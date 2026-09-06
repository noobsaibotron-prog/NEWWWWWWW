#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EmberTokens.h"

/** Continuous 3px stereo columns + peak-hold hairline. No LED segments. */
class EmberMeters : public juce::Component
{
public:
    void setLevels(float leftDb, float rightDb)
    {
        auto attackRelease = [](float cur, float target)
        {
            return target > cur ? target : (cur * 0.88f + target * 0.12f);
        };
        left = attackRelease(left, leftDb);
        right = attackRelease(right, rightDb);

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (leftDb >= peakL)
        {
            peakL = leftDb;
            peakHoldUntilL = now + 1100.0;
        }
        else if (now > peakHoldUntilL)
        {
            peakL = juce::jmax(leftDb, peakL - 0.35f);
        }

        if (rightDb >= peakR)
        {
            peakR = rightDb;
            peakHoldUntilR = now + 1100.0;
        }
        else if (now > peakHoldUntilR)
        {
            peakR = juce::jmax(rightDb, peakR - 0.35f);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float colW = 3.0f;
        const float gap = 6.0f;
        const float total = colW * 2.0f + gap;
        const float x0 = b.getCentreX() - total * 0.5f;
        paintColumn(g, { x0, b.getY(), colW, b.getHeight() }, left, peakL);
        paintColumn(g, { x0 + colW + gap, b.getY(), colW, b.getHeight() }, right, peakR);
    }

private:
    static float dbToY(float db, float top, float bottom)
    {
        const float n = juce::jlimit(0.0f, 1.0f, (db - EmberTokens::specMinDb)
                                                  / (EmberTokens::specMaxDb - EmberTokens::specMinDb));
        return juce::jmap(n, bottom, top);
    }

    void paintColumn(juce::Graphics& g, juce::Rectangle<float> col, float levelDb, float peakDb)
    {
        g.setColour(EmberTokens::sunken);
        g.fillRect(col);

        const float y = dbToY(levelDb, col.getY(), col.getBottom());
        auto fill = juce::Rectangle<float>(col.getX(), y, col.getWidth(), col.getBottom() - y);
        g.setColour(EmberTokens::meter.withAlpha(0.85f));
        g.fillRect(fill);

        const float py = dbToY(peakDb, col.getY(), col.getBottom());
        g.setColour(EmberTokens::cyan.withAlpha(0.55f));
        g.fillRect(col.getX(), py, col.getWidth(), 1.0f);
    }

    float left = -100.0f, right = -100.0f;
    float peakL = -100.0f, peakR = -100.0f;
    double peakHoldUntilL = 0.0, peakHoldUntilR = 0.0;
};
