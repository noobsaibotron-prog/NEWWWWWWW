#pragma once
/**
 * Strato 4 — OpenGL Spectrum Rendering Helper.
 *
 * NOT a Component. NOT an OpenGLRenderer.
 * Pure rendering helper called from Editor::renderOpenGL().
 *
 * Architecture:
 *   - PluginEditor implements juce::OpenGLRenderer
 *   - Editor's OpenGLContext calls setRenderer(&editor)
 *   - JUCE calls renderOpenGL() BEFORE painting child components
 *   - renderOpenGL() calls this helper's renderShader()
 *   - Software paint() (grid, EQ, bands) draws ON TOP
 *
 * This gives us: GPU spectrum (bottom) -> software UI (top) in one context.
 *
 * Thread safety:
 *   - updateSpectrumData() called from GUI thread
 *   - renderShader() called from GL thread
 *   - SpinLock protects the pending->active handoff
 *
 * GL conventions (JUCE on macOS):
 *   - All GL types/constants/functions live in juce::gl:: namespace
 *   - VBO extension calls go through context.extensions.glXxx()
 *   - We use 'using namespace juce::gl' for readability
 */

#include <juce_opengl/juce_opengl.h>
#include <vector>
#include <atomic>

using namespace juce::gl;

class GLSpectrumHelper
{
public:
    GLSpectrumHelper() = default;
    ~GLSpectrumHelper() = default;

    //==========================================================================
    // GUI THREAD: push new per-pixel dB data
    //==========================================================================
    void updateSpectrumData (const std::vector<float>& prePixelDB,
                             const std::vector<float>& postPixelDB,
                             juce::Rectangle<float> bounds,
                             float minDb, float maxDb)
    {
        const juce::SpinLock::ScopedLockType lock (renderLock);
        pendingData.preDB   = prePixelDB;
        pendingData.postDB  = postPixelDB;
        pendingData.bounds  = bounds;
        pendingData.minDb   = minDb;
        pendingData.maxDb   = maxDb;
        pendingDirty = true;
    }

    void setShowPre (bool v) noexcept  { showPre.store (v, std::memory_order_relaxed); }
    void setShowPost (bool v) noexcept { showPost.store (v, std::memory_order_relaxed); }

    //==========================================================================
    // Called from Editor::newOpenGLContextCreated()
    //==========================================================================
    void initGL (juce::OpenGLContext& ctx)
    {
        compileShaders (ctx);
        ctx.extensions.glGenBuffers (1, &vbo);
    }

    //==========================================================================
    // Called from Editor::openGLContextClosing()
    //==========================================================================
    void cleanupGL (juce::OpenGLContext& ctx)
    {
        fillShader.reset();
        lineShader.reset();

        if (vbo != 0)
        {
            ctx.extensions.glDeleteBuffers (1, &vbo);
            vbo = 0;
        }
    }

    //==========================================================================
    // Called from Editor::renderOpenGL()
    // ctx = editor's OpenGLContext, compW/compH = full component pixel dimensions
    //==========================================================================
    /** Render the spectrum.
     *  vpX/vpY/vpW/vpH = physical-pixel viewport (GL coords, Y-flipped).
     *  logicalW/logicalH = component logical dimensions (for NDC conversion). */
    void renderShader (juce::OpenGLContext& ctx,
                       int vpX, int vpY, int vpW, int vpH,
                       int logicalW, int logicalH)
    {
        // Swap pending -> active under lock
        {
            const juce::SpinLock::ScopedLockType lock (renderLock);
            if (pendingDirty)
            {
                activeData = pendingData;
                pendingDirty = false;
            }
        }

        if (activeData.preDB.empty() && activeData.postDB.empty())
            return;

        if (! fillShader || ! lineShader)
            return;

        if (logicalW <= 0 || logicalH <= 0 || vpW <= 0 || vpH <= 0)
            return;

        const auto& gb = activeData.bounds;
        const float minDb = activeData.minDb;
        const float maxDb = activeData.maxDb;

        // Physical-pixel viewport positioned at the spectrum component's location
        glViewport (vpX, vpY, vpW, vpH);
        glEnable (GL_BLEND);

        // =============================================================
        // RAII guard — ALWAYS restores JUCE's canonical blend state on
        // scope exit, even if an exception propagates out of a draw*
        // call (e.g. std::bad_alloc from a vector resize in fillVerts).
        // Mandatory per the Tribunale's "blend-state leak is the #1 risk"
        // warning: if we ever leave the state on an additive func, the
        // next JUCE 2D draw through the same GL context corrupts.
        // =============================================================
        struct BlendStateGuard
        {
            ~BlendStateGuard()
            {
                glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                glDisable (GL_BLEND);
            }
        } blendGuard;

        // =============================================================
        // Wave 5 Premium Spectrum: 3 passes per spectrum
        // 1) alpha-blend gradient fill (soft base tint)
        // 2) additive wide stroke  ("glow" halo, 6 px)
        // 3) alpha-blend crisp stroke ("main" line, 1.5 px)
        //
        // Pre-EQ = azzurro polvere (reference signal, secondary)
        // Post-EQ = luminous cyan  (hero curve, dominant)
        // Additive blend makes the halo pile up brightness without
        // ever going milky — the key trick behind premium analyzers.
        // =============================================================

        // ---------- Pre-EQ spectrum --------------------------------
        if (showPre.load (std::memory_order_relaxed) && ! activeData.preDB.empty())
        {
            // Pass 1: SOLID dusty-azure fill — full body, glossy edge comes from passes 2+3
            // Top brighter, bottom slightly dimmer but NEVER transparent → filled look
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawSpectrumFill (ctx, activeData.preDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x504A9FD9),   // top ~31 % alpha azzurro polvere
                              juce::Colour (0x284A9FD9));  // bottom ~16 % alpha — still visible, solid

            // Pass 2: wide additive halo (6 px) — glossy glow on the edge
            glBlendFunc (GL_SRC_ALPHA, GL_ONE);
            drawSpectrumLine (ctx, activeData.preDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x404A9FD9),   // 25 % alpha → additive pile-up
                              6.0f);

            // Pass 3: crisp main stroke (1.5 px) — glossy highlight
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawSpectrumLine (ctx, activeData.preDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0xD04A9FD9),   // 82 % alpha → crisp azure contour
                              1.5f);
        }

        // ---------- Post-EQ spectrum -------------------------------
        if (showPost.load (std::memory_order_relaxed) && ! activeData.postDB.empty())
        {
            // Pass 1: SOLID luminous cyan fill — distinguishable from pre-EQ via colour + higher alpha
            // Filled body, NOT transparent at the bottom
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawSpectrumFill (ctx, activeData.postDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x4400E5FF),   // top ~27 % alpha cyan fill
                              juce::Colour (0x2000E5FF));  // bottom ~12 % alpha — still filled

            // Pass 2: wide additive cyan halo (6 px) — glossy hero glow
            glBlendFunc (GL_SRC_ALPHA, GL_ONE);
            drawSpectrumLine (ctx, activeData.postDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x5500E5FF),   // 33 % alpha → bright additive pile-up
                              6.0f);

            // Pass 3: crisp main cyan stroke (1.5 px) — glossy highlight
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawSpectrumLine (ctx, activeData.postDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0xE600E5FF),   // 90 % alpha → vivid cyan line
                              1.5f);
        }

        // blendGuard destructor runs here — no manual reset needed.
    }

private:
    //==========================================================================
    // Double-buffered spectrum data
    //==========================================================================
    struct SpectrumBuffer
    {
        std::vector<float> preDB;
        std::vector<float> postDB;
        juce::Rectangle<float> bounds;
        float minDb = -90.0f;
        float maxDb = 12.0f;
    };

    SpectrumBuffer pendingData, activeData;
    bool pendingDirty = false;             // protected by renderLock (no longer atomic)
    juce::SpinLock renderLock;             // protects pendingData + pendingDirty handoff
    std::atomic<bool> showPre  { true };
    std::atomic<bool> showPost { true };

    // GL resources (created/destroyed by initGL/cleanupGL)
    std::unique_ptr<juce::OpenGLShaderProgram> fillShader;
    std::unique_ptr<juce::OpenGLShaderProgram> lineShader;
    GLuint vbo = 0;

    // Vertex scratch buffers (reused per frame, avoid allocations)
    std::vector<float> fillVerts;
    std::vector<float> lineVerts;

    //==========================================================================
    // Shader sources — GLSL 1.20 compatible (macOS compatibility profile)
    //==========================================================================
    static const char* fillVertexShader()
    {
        return R"(
            attribute vec2 position;
            varying float vYNorm;
            void main() {
                vYNorm = position.y * 0.5 + 0.5;
                gl_Position = vec4(position, 0.0, 1.0);
            }
        )";
    }

    static const char* fillFragmentShader()
    {
        return R"(
            varying float vYNorm;
            uniform vec4 topColour;
            uniform vec4 bottomColour;
            void main() {
                gl_FragColor = mix(bottomColour, topColour, vYNorm);
            }
        )";
    }

    static const char* lineVertexShader()
    {
        return R"(
            attribute vec2 position;
            void main() {
                gl_Position = vec4(position, 0.0, 1.0);
            }
        )";
    }

    static const char* lineFragmentShader()
    {
        return R"(
            uniform vec4 colour;
            void main() {
                gl_FragColor = colour;
            }
        )";
    }

    //==========================================================================
    // Setup — with explicit error logging
    //==========================================================================
    void compileShaders (juce::OpenGLContext& ctx)
    {
        fillShader = std::make_unique<juce::OpenGLShaderProgram> (ctx);
        if (! fillShader->addVertexShader (fillVertexShader()))
        {
            DBG ("GLSpectrumHelper: fill VERTEX shader error: " + fillShader->getLastError());
            fillShader.reset();
            return;
        }
        if (! fillShader->addFragmentShader (fillFragmentShader()))
        {
            DBG ("GLSpectrumHelper: fill FRAGMENT shader error: " + fillShader->getLastError());
            fillShader.reset();
            return;
        }
        if (! fillShader->link())
        {
            DBG ("GLSpectrumHelper: fill shader LINK error: " + fillShader->getLastError());
            fillShader.reset();
            return;
        }

        lineShader = std::make_unique<juce::OpenGLShaderProgram> (ctx);
        if (! lineShader->addVertexShader (lineVertexShader()))
        {
            DBG ("GLSpectrumHelper: line VERTEX shader error: " + lineShader->getLastError());
            lineShader.reset();
            return;
        }
        if (! lineShader->addFragmentShader (lineFragmentShader()))
        {
            DBG ("GLSpectrumHelper: line FRAGMENT shader error: " + lineShader->getLastError());
            lineShader.reset();
            return;
        }
        if (! lineShader->link())
        {
            DBG ("GLSpectrumHelper: line shader LINK error: " + lineShader->getLastError());
            lineShader.reset();
            return;
        }
    }

    //==========================================================================
    // Rendering helpers
    //==========================================================================
    static float toNdcX (float x, float compW) { return  2.0f * x / compW - 1.0f; }
    static float toNdcY (float y, float compH) { return -(2.0f * y / compH - 1.0f); }

    static float dbToPixelY (float db, float minDb, float maxDb,
                              float graphTop, float graphBottom)
    {
        const float norm = (db - minDb) / (maxDb - minDb);
        return graphBottom - norm * (graphBottom - graphTop);
    }

    void drawSpectrumFill (juce::OpenGLContext& ctx,
                           const std::vector<float>& pixelDB,
                           const juce::Rectangle<float>& gb,
                           float minDb, float maxDb,
                           int compW, int compH,
                           juce::Colour topCol, juce::Colour bottomCol)
    {
        const int dataW  = static_cast<int> (pixelDB.size());
        const int graphW = static_cast<int> (gb.getWidth());
        const int count  = std::min (dataW, graphW);
        if (count < 2) return;

        const float gX   = gb.getX();
        const float gTop = gb.getY();
        const float gBot = gb.getBottom();
        const float cW   = static_cast<float> (compW);
        const float cH   = static_cast<float> (compH);

        // Triangle strip: for each x, two vertices (spectrum top, graph bottom)
        fillVerts.resize (static_cast<size_t> (count * 4));
        for (int i = 0; i < count; ++i)
        {
            const float xPx = gX + static_cast<float> (i);
            const float yTop = dbToPixelY (pixelDB[static_cast<size_t> (i)], minDb, maxDb, gTop, gBot);
            const float ndcX = toNdcX (xPx, cW);

            const auto idx = static_cast<size_t> (i * 4);
            fillVerts[idx + 0] = ndcX;
            fillVerts[idx + 1] = toNdcY (yTop, cH);
            fillVerts[idx + 2] = ndcX;
            fillVerts[idx + 3] = toNdcY (gBot, cH);
        }

        fillShader->use();
        fillShader->setUniform ("topColour",
            topCol.getFloatRed(), topCol.getFloatGreen(),
            topCol.getFloatBlue(), topCol.getFloatAlpha());
        fillShader->setUniform ("bottomColour",
            bottomCol.getFloatRed(), bottomCol.getFloatGreen(),
            bottomCol.getFloatBlue(), bottomCol.getFloatAlpha());

        ctx.extensions.glBindBuffer (GL_ARRAY_BUFFER, vbo);
        ctx.extensions.glBufferData (GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr> (fillVerts.size() * sizeof (float)),
                      fillVerts.data(), GL_STREAM_DRAW);

        ctx.extensions.glEnableVertexAttribArray (0);
        ctx.extensions.glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, count * 2);
        ctx.extensions.glDisableVertexAttribArray (0);
        ctx.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
    }

    void drawSpectrumLine (juce::OpenGLContext& ctx,
                           const std::vector<float>& pixelDB,
                           const juce::Rectangle<float>& gb,
                           float minDb, float maxDb,
                           int compW, int compH,
                           juce::Colour lineCol,
                           float lineWidth = 2.0f)
    {
        const int dataW  = static_cast<int> (pixelDB.size());
        const int graphW = static_cast<int> (gb.getWidth());
        const int count  = std::min (dataW, graphW);
        if (count < 2) return;

        const float gX   = gb.getX();
        const float gTop = gb.getY();
        const float gBot = gb.getBottom();
        const float cW   = static_cast<float> (compW);
        const float cH   = static_cast<float> (compH);

        lineVerts.resize (static_cast<size_t> (count * 2));
        for (int i = 0; i < count; ++i)
        {
            const float xPx = gX + static_cast<float> (i);
            const float yTop = dbToPixelY (pixelDB[static_cast<size_t> (i)], minDb, maxDb, gTop, gBot);
            const auto idx = static_cast<size_t> (i * 2);
            lineVerts[idx + 0] = toNdcX (xPx, cW);
            lineVerts[idx + 1] = toNdcY (yTop, cH);
        }

        lineShader->use();
        lineShader->setUniform ("colour",
            lineCol.getFloatRed(), lineCol.getFloatGreen(),
            lineCol.getFloatBlue(), lineCol.getFloatAlpha());

        ctx.extensions.glBindBuffer (GL_ARRAY_BUFFER, vbo);
        ctx.extensions.glBufferData (GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr> (lineVerts.size() * sizeof (float)),
                      lineVerts.data(), GL_STREAM_DRAW);

        ctx.extensions.glEnableVertexAttribArray (0);
        ctx.extensions.glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glLineWidth (lineWidth);
        glDrawArrays (GL_LINE_STRIP, 0, count);
        ctx.extensions.glDisableVertexAttribArray (0);
        ctx.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
    }
};
