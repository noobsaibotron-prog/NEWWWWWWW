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
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Wave 4A: Pre-EQ spectrum — DENSE azzurro polvere (40% alpha at the
        // top) matching the Liquid Intelligence mockup. Previous 20% alpha
        // was too diluted and the blue ocean effect was barely visible.
        if (showPre.load (std::memory_order_relaxed) && ! activeData.preDB.empty())
        {
            drawSpectrumFill (ctx, activeData.preDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x664A9FD9),   // top: azzurro polvere 40% alpha
                              juce::Colour (0x004A9FD9));  // bottom: trasparente
        }

        // Wave 4A: Post-EQ spectrum — denser verde tenue (33% top). The post
        // layer is still lighter than pre so the mix reads as "pre through a
        // coloured filter".
        if (showPost.load (std::memory_order_relaxed) && ! activeData.postDB.empty())
        {
            drawSpectrumFill (ctx, activeData.postDB, gb, minDb, maxDb, logicalW, logicalH,
                              juce::Colour (0x555ED4A0),   // top: verde tenue 33% alpha
                              juce::Colour (0x005ED4A0));  // bottom: trasparente
        }

        glDisable (GL_BLEND);
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
                           juce::Colour lineCol)
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
        glLineWidth (2.0f);
        glDrawArrays (GL_LINE_STRIP, 0, count);
        ctx.extensions.glDisableVertexAttribArray (0);
        ctx.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
    }
};
