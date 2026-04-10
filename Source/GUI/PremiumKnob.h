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
