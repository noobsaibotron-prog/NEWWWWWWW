#pragma once
/**
 * PremiumKnob — Filmstrip-based rotary knob for "Liquid Intelligence" design.
 *
 * Rendering: a single juce::Image filmstrip (128 vertical frames) pre-rendered
 * by Manus. The paint() method picks the frame index based on the knob value
 * and does a single drawImage() call — no procedural arcs, no fillEllipse.
 *
 * Performance: eliminates sin/cos / Path overhead from the message thread,
 * replacing it with an O(1) blit. Filmstrips are cached globally via
 * juce::ImageCache::getFromMemory (shared across all knob instances).
 *
 * HiDPI: the Large Amber filmstrip is exported at 256×256 per frame (2x),
 * the Small Blue at 128×128 per frame (1x or 2x depending on display size).
 * juce::Image + drawImage handle Retina scaling automatically via the
 * graphics context's transform.
 */

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

class PremiumKnob : public juce::Slider
{
public:
    enum class Style { LargeAmber, SmallBlue };

    explicit PremiumKnob(const juce::String& labelText = {}, Style s = Style::LargeAmber)
        : juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
          label(labelText),
          style(s)
    {
        setPopupDisplayEnabled(true, false, nullptr);
        setRange(0.0, 1.0, 0.0);
    }

    void setStyle(Style s) noexcept
    {
        if (style != s)
        {
            style = s;
            repaint();
        }
    }

    Style getStyle() const noexcept { return style; }

    void paint(juce::Graphics& g) override
    {
        auto& film = getFilmstrip(style);
        if (film.isNull())
            return;

        // Filmstrip is a vertical strip of 128 frames. Frame height = total height / 128.
        const int numFrames = 128;
        const int frameW = film.getWidth();
        const int frameH = film.getHeight() / numFrames;
        if (frameH <= 0)
            return;

        // Map slider value [min..max] → frame index [0..127]
        const double norm = getNormalisableRange().convertTo0to1(getValue());
        const int frameIdx = juce::jlimit(0, numFrames - 1, (int) std::round(norm * (numFrames - 1)));

        // Compute the "face" area (excluding any native text box). juce::Slider
        // positions the text box as a child component; paint() here must not
        // overlap it, otherwise the value string gets covered by the filmstrip.
        int faceTop = 0;
        int faceH   = getHeight();
        const int tbH = getTextBoxHeight();
        const auto tbPos = getTextBoxPosition();
        if (tbPos == juce::Slider::TextBoxAbove)
        {
            faceTop = tbH;
            faceH   = juce::jmax (0, getHeight() - tbH);
        }
        else if (tbPos == juce::Slider::TextBoxBelow)
        {
            faceH = juce::jmax (0, getHeight() - tbH);
        }

        // Reserve space for the optional custom label under the knob face.
        const bool hasLabel = label.isNotEmpty() && faceH > 40;
        const int labelH   = hasLabel ? 14 : 0;
        const int availH   = juce::jmax (0, faceH - labelH);

        // CRITICAL FIX: always draw the filmstrip frame in a SQUARE centered region.
        // Without this, setBounds() with a non-square rect stretches the circular
        // frame into an ellipse. The knob must remain visually round regardless
        // of the parent component's aspect ratio.
        const int knobSize = juce::jmin (getWidth(), availH);
        if (knobSize <= 0)
            return;

        const int knobX = (getWidth() - knobSize) / 2;
        const int knobY = faceTop + (availH - knobSize) / 2;

        // High-quality resampling: prevents aliasing/shimmer on filmstrip reflections
        // when the knob is rendered smaller than the native frame size (256px amber,
        // 128px blue). Bilinear→bicubic upgrade — negligible cost for a single blit.
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        // Contact shadow / ambient occlusion: drawn BEFORE the filmstrip so the
        // knob sits "above" the panel. Static cached radial gradient — no per-frame
        // allocation. Light source is fixed above → shadow offset +2px Y.
        {
            auto& shadowImg = getShadowImage();
            if (!shadowImg.isNull())
            {
                const int shadowPad = juce::jmax(2, knobSize / 12);
                g.drawImage(shadowImg,
                            knobX - shadowPad, knobY - shadowPad + 2,
                            knobSize + shadowPad * 2, knobSize + shadowPad * 2,
                            0, 0, shadowImg.getWidth(), shadowImg.getHeight(),
                            false);
            }
        }

        g.drawImage(film,
                    knobX, knobY, knobSize, knobSize,          // square dest rect
                    0, frameIdx * frameH, frameW, frameH,      // source rect
                    false);

        if (hasLabel)
        {
            g.setColour(juce::Colour(0xFF8888A0));  // textSecondary
            auto labelBounds = juce::Rectangle<int>(0, faceTop + faceH - labelH, getWidth(), labelH - 2);
            g.drawFittedText(label, labelBounds, juce::Justification::centred, 1);
        }
    }

private:
    juce::String label;
    Style style;

    /** Cached radial shadow image — generated once, shared across all knob instances.
     *  128×128 ARGB radial gradient: black 35% at center → transparent at edge.
     *  Drawn behind the filmstrip to simulate ambient occlusion / contact shadow. */
    static juce::Image& getShadowImage()
    {
        static juce::Image img = []() {
            const int sz = 128;
            juce::Image shadow(juce::Image::ARGB, sz, sz, true);
            juce::Graphics sg(shadow);
            const float cx = static_cast<float>(sz) * 0.5f;
            const float cy = static_cast<float>(sz) * 0.5f;
            const float radius = static_cast<float>(sz) * 0.46f;
            // Radial gradient: dark center → transparent edge
            juce::ColourGradient grad(
                juce::Colours::black.withAlpha(0.35f), cx, cy,
                juce::Colours::transparentBlack,       cx + radius, cy,
                true); // radial
            sg.setGradientFill(grad);
            sg.fillRect(0, 0, sz, sz);
            return shadow;
        }();
        return img;
    }

    /** Shared filmstrip cache. ImageCache refcounts and auto-frees on plugin unload. */
    static juce::Image& getFilmstrip(Style s)
    {
        if (s == Style::LargeAmber)
        {
            static juce::Image img = juce::ImageCache::getFromMemory(
                BinaryData::knob_large_amber_png,
                BinaryData::knob_large_amber_pngSize);
            return img;
        }
        else
        {
            static juce::Image img = juce::ImageCache::getFromMemory(
                BinaryData::knob_small_blue_png,
                BinaryData::knob_small_blue_pngSize);
            return img;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PremiumKnob)
};
