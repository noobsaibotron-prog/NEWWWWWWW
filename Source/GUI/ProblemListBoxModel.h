//==============================================================================
// ProblemListBoxModel.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../AI/AIEngine.h"
#include "ProblemRowComponent.h"

class ProblemListBoxModel final : public juce::ListBoxModel
{
public:
    using Correction = AIEngine::Correction;
    using FixCallback = std::function<void(int)>;

    void setCorrections(std::vector<Correction> newCorrections)
    {
        corrections = std::move(newCorrections);
        if (owner != nullptr)
            owner->updateContent();
    }

    const std::vector<Correction>& getCorrections() const noexcept
    {
        return corrections;
    }

    void setFixCallback(FixCallback cb)
    {
        onFix = std::move(cb);
    }

    void attachTo(juce::ListBox& listBox)
    {
        owner = &listBox;
        owner->setModel(this);
        owner->setRowHeight(56);
        owner->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        owner->setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        owner->setMultipleSelectionEnabled(false);
        owner->updateContent();
    }

    int getNumRows() override
    {
        return static_cast<int>(corrections.size());
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);

        g.setColour(rowIsSelected
            ? ModernLookAndFeel::Colors::amber.withAlpha(0.06f)
            : juce::Colours::transparentBlack);
        g.fillRect(r);

        g.setColour(ModernLookAndFeel::Colors::bgLighter.withAlpha(0.35f));
        g.drawHorizontalLine(height - 1, 8.0f, (float) width - 8.0f);
    }

    juce::Component* refreshComponentForRow(int rowNumber, bool, juce::Component* existingComponentToUpdate) override
    {
        if (rowNumber < 0 || rowNumber >= getNumRows())
            return nullptr;

        auto* row = dynamic_cast<ProblemRowComponent*>(existingComponentToUpdate);

        if (row == nullptr)
            row = new ProblemRowComponent();

        row->configure(corrections[(size_t) rowNumber], rowNumber, onFix);
        return row;
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        selectedRow = lastRowSelected;
    }

    int getSelectedRow() const noexcept
    {
        return selectedRow;
    }

private:
    std::vector<Correction> corrections;
    FixCallback onFix;
    juce::ListBox* owner = nullptr;
    int selectedRow = -1;
};
