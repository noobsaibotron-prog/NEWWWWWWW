#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <vector>
#include <mutex>
#include <atomic>

/**
 * OpenGLSpectrumRenderer
 *
 * Renders the spectrum fill + line using OpenGL shaders instead of JUCE software paths.
 * Designed to be embedded inside AdvancedSpectrumDisplay as an overlay child component.
 *
 * Strategy:
 *   - Attaches an OpenGLContext to itself
 *   - Draws spectrum fill as a triangle strip (GPU)
 *   - Draws spectrum line as a line strip (GPU)
 *   - Draws peak-hold line as a separate line strip (GPU)
 *   - Software paint() in parent handles grid, labels, EQ curve, band nodes
 *
 * Thread safety:
 *   - updateSpectrum() called from GUI thread (timerCallback)
 *   - render() called from OpenGL thread
 *   - spinlock-protected double buffer for spectrum data
 */
class OpenGLSpectrumRenderer : public juce::Component,
                                public juce::OpenGLRenderer
{
public:
    OpenGLSpectrumRenderer()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);

        openGLContext.setRenderer(this);
        openGLContext.attachTo(*this);
        openGLContext.setContinuousRepainting(true);
    }

    ~OpenGLSpectrumRenderer() override
    {
        openGLContext.detach();
    }

    // ─── Called from GUI thread (timerCallback) ───────────────────────────────
    void updateSpectrum(const std::vector<float>& spectrum,
                        const std::vector<float>& peakHold,
                        juce::Rectangle<float> graphBounds,
                        float minDb, float maxDb)
    {
        // Write to pending buffer; OpenGL thread reads from active buffer
        const int w = static_cast<int>(graphBounds.getWidth());
        if (w <= 0) return;

        auto& buf = pending;
        buf.spectrum   = spectrum;
        buf.peakHold   = peakHold;
        buf.bounds     = graphBounds;
        buf.minDb      = minDb;
        buf.maxDb      = maxDb;
        buf.width      = w;

        pendingDirty.store(true, std::memory_order_release);
    }

    void setShowPre(bool v)  { showPre.store(v);  }
    void setShowPost(bool v) { showPost.store(v); }

    // ─── OpenGLRenderer callbacks ─────────────────────────────────────────────
    void newOpenGLContextCreated() override
    {
        createShaders();
        createBuffers();
    }

    void openGLContextClosing() override
    {
        shader.reset();
        fillShader.reset();
        vbo = 0;
    }

    void renderOpenGL() override
    {
        // Swap pending → active
        if (pendingDirty.exchange(false, std::memory_order_acq_rel))
            active = pending;

        if (active.spectrum.empty() || !shader || !fillShader)
            return;

        juce::OpenGLHelpers::clear(juce::Colours::transparentBlack);

        const int w = active.width;
        const float minDb = active.minDb;
        const float maxDb = active.maxDb;
        const float range = maxDb - minDb;

        const int sw = getWidth();
        const int sh = getHeight();
        if (sw <= 0 || sh <= 0) return;

        // Build vertex arrays
        buildVertices(active.spectrum, active.bounds, minDb, range, sw, sh);

        if (showPre.load())
            drawFill();

        if (!active.peakHold.empty())
            drawPeakHold(active.peakHold, active.bounds, minDb, range, sw, sh);
    }

private:
    // ─── Shaders ──────────────────────────────────────────────────────────────
    static const char* lineVertSrc()
    {
        return R"(
            attribute vec2 position;
            void main() {
                gl_Position = vec4(position, 0.0, 1.0);
            }
        )";
    }

    static const char* lineFragSrc()
    {
        return R"(
            uniform vec4 colour;
            void main() {
                gl_FragColor = colour;
            }
        )";
    }

    static const char* fillVertSrc()
    {
        return R"(
            attribute vec2 position;
            varying float yNorm;
            void main() {
                yNorm = position.y * 0.5 + 0.5;
                gl_Position = vec4(position, 0.0, 1.0);
            }
        )";
    }

    static const char* fillFragSrc()
    {
        return R"(
            varying float yNorm;
            uniform vec4 topColour;
            uniform vec4 bottomColour;
            void main() {
                gl_FragColor = mix(bottomColour, topColour, yNorm);
            }
        )";
    }

    void createShaders()
    {
        shader = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
        if (!shader->addVertexShader(lineVertSrc()) ||
            !shader->addFragmentShader(lineFragSrc()) ||
            !shader->link())
        {
            shader.reset();
        }

        fillShader = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
        if (!fillShader->addVertexShader(fillVertSrc()) ||
            !fillShader->addFragmentShader(fillFragSrc()) ||
            !fillShader->link())
        {
            fillShader.reset();
        }
    }

    void createBuffers()
    {
        juce::gl::glGenBuffers(1, &vbo);
    }

    // ─── Vertex building ──────────────────────────────────────────────────────

    // Converts pixel coords to NDC [-1,1]
    static float toNdcX(float px, float sw) { return  2.0f * px / sw - 1.0f; }
    static float toNdcY(float py, float sh) { return -2.0f * py / sh + 1.0f; }

    void buildVertices(const std::vector<float>& spectrum,
                       juce::Rectangle<float> bounds,
                       float minDb, float range,
                       int sw, int sh)
    {
        lineVerts.clear();
        fillVerts.clear();

        const int n = static_cast<int>(spectrum.size());
        if (n == 0) return;

        const float bx = bounds.getX();
        const float by = bounds.getY();
        const float bw = bounds.getWidth();
        const float bh = bounds.getHeight();
        const float bottom = by + bh;

        lineVerts.reserve(static_cast<size_t>(n) * 2);
        fillVerts.reserve(static_cast<size_t>(n) * 4);

        for (int i = 0; i < n; ++i)
        {
            float px = bx + static_cast<float>(i) * bw / static_cast<float>(n - 1);
            float t  = juce::jlimit(0.0f, 1.0f, (spectrum[static_cast<size_t>(i)] - minDb) / range);
            float py = bottom - t * bh;

            float nx = toNdcX(px, static_cast<float>(sw));
            float ny = toNdcY(py, static_cast<float>(sh));
            float nb = toNdcY(bottom, static_cast<float>(sh));

            lineVerts.push_back(nx);
            lineVerts.push_back(ny);

            // Triangle strip: alternate top/bottom for fill
            fillVerts.push_back(nx); fillVerts.push_back(ny);
            fillVerts.push_back(nx); fillVerts.push_back(nb);
        }
    }

    void drawFill()
    {
        if (!fillShader || fillVerts.empty()) return;

        fillShader->use();

        // Top color: blue semi-transparent
        fillShader->setUniform("topColour",
            0.29f, 0.56f, 0.85f, 0.28f);
        // Bottom color: transparent
        fillShader->setUniform("bottomColour",
            0.29f, 0.56f, 0.85f, 0.04f);

        juce::gl::glEnable(juce::gl::GL_BLEND);
        juce::gl::glBlendFunc(juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
        juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER,
                               static_cast<juce::gl::GLsizeiptr>(fillVerts.size() * sizeof(float)),
                               fillVerts.data(),
                               juce::gl::GL_STREAM_DRAW);

        auto posAttrib = (juce::gl::GLuint)fillShader->getAttributeID("position");
        juce::gl::glEnableVertexAttribArray(posAttrib);
        juce::gl::glVertexAttribPointer(posAttrib, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

        juce::gl::glDrawArrays(juce::gl::GL_TRIANGLE_STRIP,
                               0,
                               static_cast<juce::gl::GLsizei>(fillVerts.size() / 2));

        juce::gl::glDisableVertexAttribArray(posAttrib);
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);

        // Draw spectrum line
        if (shader && !lineVerts.empty())
        {
            shader->use();
            shader->setUniform("colour", 0.41f, 0.65f, 0.91f, 0.75f);  // accentBlue

            juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
            juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER,
                                   static_cast<juce::gl::GLsizeiptr>(lineVerts.size() * sizeof(float)),
                                   lineVerts.data(),
                                   juce::gl::GL_STREAM_DRAW);

            auto lpos = (juce::gl::GLuint)shader->getAttributeID("position");
            juce::gl::glEnableVertexAttribArray(lpos);
            juce::gl::glVertexAttribPointer(lpos, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);
            juce::gl::glLineWidth(1.8f);
            juce::gl::glDrawArrays(juce::gl::GL_LINE_STRIP,
                                   0,
                                   static_cast<juce::gl::GLsizei>(lineVerts.size() / 2));
            juce::gl::glDisableVertexAttribArray(lpos);
            juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);
        }
    }

    void drawPeakHold(const std::vector<float>& peakHold,
                      juce::Rectangle<float> bounds,
                      float minDb, float range,
                      int sw, int sh)
    {
        if (!shader) return;

        std::vector<float> pkVerts;
        pkVerts.reserve(peakHold.size() * 2);

        const float bx = bounds.getX();
        const float by = bounds.getY();
        const float bw = bounds.getWidth();
        const float bh = bounds.getHeight();
        const float bottom = by + bh;
        const int n = static_cast<int>(peakHold.size());

        for (int i = 0; i < n; ++i)
        {
            float px = bx + static_cast<float>(i) * bw / static_cast<float>(n - 1);
            float t  = juce::jlimit(0.0f, 1.0f, (peakHold[static_cast<size_t>(i)] - minDb) / range);
            float py = bottom - t * bh;
            pkVerts.push_back(toNdcX(px, static_cast<float>(sw)));
            pkVerts.push_back(toNdcY(py, static_cast<float>(sh)));
        }

        shader->use();
        shader->setUniform("colour", 1.0f, 0.6f, 0.0f, 0.85f);  // accentOrange

        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
        juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER,
                               static_cast<juce::gl::GLsizeiptr>(pkVerts.size() * sizeof(float)),
                               pkVerts.data(),
                               juce::gl::GL_STREAM_DRAW);

        auto pos = (juce::gl::GLuint)shader->getAttributeID("position");
        juce::gl::glEnableVertexAttribArray(pos);
        juce::gl::glVertexAttribPointer(pos, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);
        juce::gl::glLineWidth(1.2f);
        juce::gl::glDrawArrays(juce::gl::GL_LINE_STRIP,
                               0,
                               static_cast<juce::gl::GLsizei>(pkVerts.size() / 2));
        juce::gl::glDisableVertexAttribArray(pos);
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);
    }

    // ─── Data ─────────────────────────────────────────────────────────────────
    struct SpectrumBuffer
    {
        std::vector<float> spectrum;
        std::vector<float> peakHold;
        juce::Rectangle<float> bounds;
        float minDb = -90.0f;
        float maxDb =  12.0f;
        int width   = 0;
    };

    SpectrumBuffer pending, active;
    std::atomic<bool> pendingDirty { false };
    std::atomic<bool> showPre  { true  };
    std::atomic<bool> showPost { false };

    juce::OpenGLContext openGLContext;

    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram> fillShader;
    juce::gl::GLuint vbo = 0;

    std::vector<float> lineVerts;
    std::vector<float> fillVerts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLSpectrumRenderer)
};
