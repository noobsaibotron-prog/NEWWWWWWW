#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * AI Problem Panel - Enhanced v2.0
 * 
 * Clear, professional display of detected audio problems with:
 * - Expandable problem cards with full details
 * - Click-to-highlight on spectrum integration
 * - Clear visual hierarchy and descriptions
 */
class AIProblemPanel : public juce::Component,
                       public juce::Timer,
                       public juce::ListBoxModel
{
public:
    std::function<void(float frequency, float q, float severity, AIEngine::ProblemType type)> onProblemSelected;

    explicit AIProblemPanel(AIEqualizerAudioProcessor& p)
        : processor(p),
          problemList(*this)
    {
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        setWantsKeyboardFocus(true);
        setTitle(tr("AI Analysis Panel", "AI Analysis Panel"));
        setDescription(tr("Detected problems with keyboard and screen-reader support",
                           "Detected problems with keyboard and screen-reader support"));

        // Header
        titleLabel.setText(tr("AI ANALYSIS", "AI ANALYSIS"), juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(15.0f));
            font.setBold(true);
            titleLabel.setFont(font);
        }
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4A9FD9));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setTitle(tr("AI analysis title", "AI analysis title"));
        titleLabel.setDescription(tr("Heading for AI analysis results", "Heading for AI analysis results"));
        addAndMakeVisible(titleLabel);
        
        // Info labels
        genreLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        genreLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        genreLabel.setJustificationType(juce::Justification::centredLeft);
        genreLabel.setTitle(tr("Genre label", "Genre label"));
        genreLabel.setDescription(tr("Detected genre description", "Detected genre description"));
        addAndMakeVisible(genreLabel);
        
        profileLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        profileLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF666666));
        profileLabel.setJustificationType(juce::Justification::centredLeft);
        profileLabel.setTitle(tr("Profile label", "Profile label"));
        profileLabel.setDescription(tr("Detected source profile", "Detected source profile"));
        addAndMakeVisible(profileLabel);
        
        // Problem list with custom row height
        problemList.setModel(this);
        problemList.setRowHeight(140);  // Tall rows for detailed info
        problemList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF1A1A1A));
        problemList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF333333));
        problemList.setOutlineThickness(1);
        problemList.setWantsKeyboardFocus(true);
        problemList.setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        problemList.setMouseMoveSelectsRows(false);
        problemList.setTitle(tr("Detected problems list", "Detected problems list"));
        problemList.setDescription(tr("Use arrows to move, Enter to apply, Space to preview on spectrum",
                                      "Use arrows to move, Enter to apply, Space to preview on spectrum"));
        problemList.setTooltip(tr("Keyboard: Up/Down to navigate • Enter to apply • Space to highlight",
                                  "Keyboard: Up/Down to navigate • Enter to apply • Space to highlight"));
        addAndMakeVisible(problemList);
        
        // Action buttons
        autoFixBtn.setButtonText(tr("FIX ALL", "FIX ALL"));
        autoFixBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
        autoFixBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3D7A37));
        autoFixBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        autoFixBtn.setTooltip(tr("Apply all suggested fixes (with confirmation)",
                                 "Apply all suggested fixes (with confirmation)"));
        autoFixBtn.setTitle(tr("Apply all fixes", "Apply all fixes"));
        autoFixBtn.setDescription(tr("Approve every suggested correction after confirmation",
                                     "Approve every suggested correction after confirmation"));
        autoFixBtn.onClick = [this]() { showAutoFixConfirmation(); };
        autoFixBtn.setExplicitFocusOrder(1);
        addAndMakeVisible(autoFixBtn);
        
        clearBtn.setButtonText(tr("CLEAR", "CLEAR"));
        clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        clearBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
        clearBtn.setTooltip(tr("Remove all detected problems from the list", "Remove all detected problems from the list"));
        clearBtn.setTitle(tr("Clear list", "Clear list"));
        clearBtn.setDescription(tr("Dismiss every detected problem without applying fixes",
                                   "Dismiss every detected problem without applying fixes"));
        clearBtn.onClick = [this]() { 
            processor.getAIEngine().clearCorrections();
            updateProblemList();
        };
        clearBtn.setExplicitFocusOrder(2);
        addAndMakeVisible(clearBtn);
        
        undoBtn.setButtonText(tr("UNDO", "UNDO"));
        undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        undoBtn.onClick = [this]() { processor.undo(); updateButtons(); };
        undoBtn.setTitle(tr("Undo last action", "Undo last action"));
        undoBtn.setExplicitFocusOrder(3);
        addAndMakeVisible(undoBtn);
        
        redoBtn.setButtonText(tr("REDO", "REDO"));
        redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        redoBtn.onClick = [this]() { processor.redo(); updateButtons(); };
        redoBtn.setTitle(tr("Redo undone action", "Redo undone action"));
        redoBtn.setExplicitFocusOrder(4);
        addAndMakeVisible(redoBtn);
        
        // Status
        statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF777777));
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setTitle(tr("Analysis status", "Analysis status"));
        statusLabel.setDescription(tr("Shows how many problems were detected",
                                      "Shows how many problems were detected"));
        addAndMakeVisible(statusLabel);
        
        startTimerHz(10);
    }
    
    ~AIProblemPanel() override { stopTimer(); }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Top accent line
        g.setColour(juce::Colour(0xFF4A9FD9));
        g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), 3.0f);
        
        // Border
        g.setColour(juce::Colour(0xFF333333));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);
        const bool rtl = isRightToLeft();
        
        // Title area
        auto titleRow = bounds.removeFromTop(24);
        if (rtl)
        {
            genreLabel.setBounds(titleRow.removeFromLeft(titleRow.getWidth() - 120));
            titleLabel.setBounds(titleRow);
            titleLabel.setJustificationType(juce::Justification::centredRight);
            genreLabel.setJustificationType(juce::Justification::centredRight);
            profileLabel.setJustificationType(juce::Justification::centredRight);
        }
        else
        {
            titleLabel.setBounds(titleRow.removeFromLeft(120));
            genreLabel.setBounds(titleRow);
            titleLabel.setJustificationType(juce::Justification::centredLeft);
            genreLabel.setJustificationType(juce::Justification::centredLeft);
            profileLabel.setJustificationType(juce::Justification::centredLeft);
        }
        
        profileLabel.setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(8);
        
        // Bottom buttons
        auto btnRow = bounds.removeFromBottom(32);
        int btnW = (btnRow.getWidth() - 12) / 4;
        autoFixBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(2));
        clearBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(2));
        undoBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(2));
        redoBtn.setBounds(btnRow.reduced(2));
        
        bounds.removeFromBottom(4);
        statusLabel.setBounds(bounds.removeFromBottom(18));
        bounds.removeFromBottom(6);
        
        // Problem list fills rest
        problemList.setBounds(bounds);
    }
    
    void timerCallback() override
    {
        auto& ai = processor.getAIEngine();
        
        // Update info labels
        genreLabel.setText(tr("Genre:", "Genre:") + " " + AIEngine::getGenreName(ai.getDetectedGenre()), 
                          juce::dontSendNotification);
        profileLabel.setText(tr("Profile:", "Profile:") + " " + AIEngine::getProfileName(ai.getSourceProfile()), 
                            juce::dontSendNotification);
        
        // FORCE: ALWAYS update problem list EVERY timer tick
        // This ensures problems appear immediately, no conditions
        if (ai.isNewAnalysisAvailable())
            ai.clearNewAnalysisFlag();
        
        // ALWAYS update - no conditions, no delays
        updateProblemList();
        
        updateButtons();
    }
    
    //==========================================================================
    // ListBoxModel Implementation
    //==========================================================================
    
    int getNumRows() override { return static_cast<int>(problems.size()); }

    juce::String getNameForRow(int rowNumber) override
    {
        return getRowSummary(rowNumber);
    }

    juce::String getTooltipForRow(int row) override
    {
        return getRowSummary(row);
    }

    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponent) override
    {
        auto* rowComp = dynamic_cast<ProblemRowComponent*>(existingComponent);
        if (rowComp == nullptr)
            rowComp = new ProblemRowComponent(*this);

        if (rowNumber >= 0 && rowNumber < static_cast<int>(problems.size()))
            rowComp->updateFromProblem(problems[static_cast<size_t>(rowNumber)], rowNumber, isRowSelected, isRightToLeft());

        return rowComp;
    }
    
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return;
        
        const auto& p = problems[row];
        
        // Highlight on spectrum
        if (onProblemSelected)
            onProblemSelected(p.frequency, p.suggestedQ, p.severity, p.type);
        
        // Right-click context menu
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, tr("Apply This Fix", "Apply This Fix"));
            menu.addItem(2, tr("Dismiss Problem", "Dismiss Problem"));
            menu.addSeparator();
            menu.addItem(3, tr("Show Full Analysis...", "Show Full Analysis..."));
            
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, row, p](int result) {
                if (result == 1) {
                    // Apply ONLY this single correction, not all approved ones
                    processor.applySingleCorrection(p);
                    updateProblemList();
                } else if (result == 2) {
                    rejectCorrectionByMatch(p);
                    updateProblemList();
                } else if (result == 3) {
                    showFullAnalysis(row);
                }
            });
        }
    }
    
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < static_cast<int>(problems.size()))
        {
            const auto& p = problems[row];
            // Apply ONLY this single correction, not all approved ones
            processor.applySingleCorrection(p);
            updateProblemList();
        }
    }
    
    void selectedRowsChanged(int lastRowSelected) override
    {
        juce::ignoreUnused(lastRowSelected);
        if (auto* h = problemList.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::rowSelectionChanged);

        const int row = problemList.getSelectedRow();
        if (row >= 0 && row < static_cast<int>(problems.size()))
        {
            juce::AccessibilityHandler::postAnnouncement(
                tr("Riga selezionata: ", "Selected row: ") + getRowAnnouncementFromCorrection(problems[static_cast<size_t>(row)]),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (problems.empty())
            return false;

        const auto lastIndex = static_cast<int>(problems.size()) - 1;
        const int current = juce::jlimit(0, lastIndex, problemList.getSelectedRow());

        if (key == juce::KeyPress::upKey)
        {
            focusListRow(juce::jlimit(0, lastIndex, current - 1));
            return true;
        }

        if (key == juce::KeyPress::downKey)
        {
            focusListRow(juce::jlimit(0, lastIndex, current + 1));
            return true;
        }

        if (key == juce::KeyPress::pageUpKey)
        {
            focusListRow(juce::jmax(0, current - 3));
            return true;
        }

        if (key == juce::KeyPress::pageDownKey)
        {
            focusListRow(juce::jmin(lastIndex, current + 3));
            return true;
        }

        if (key == juce::KeyPress::homeKey)
        {
            focusListRow(0);
            return true;
        }

        if (key == juce::KeyPress::endKey)
        {
            focusListRow(lastIndex);
            return true;
        }

        if (key == juce::KeyPress::returnKey)
        {
            activateRow(current);
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::spaceKey)
        {
            highlightRow(current);
            return true;
        }

        return false;
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this,
                                                            juce::AccessibilityRole::group);
    }

private:
    //==========================================================================
    class ProblemListBox : public juce::ListBox
    {
    public:
        explicit ProblemListBox(AIProblemPanel& ownerRef)
            : juce::ListBox("problemList", &ownerRef), owner(ownerRef)
        {
            setAccessible(true);
        }

    private:

        AIProblemPanel& owner;
    };

    class ProblemRowComponent : public juce::Component,
                                public juce::TooltipClient
    {
    public:
        explicit ProblemRowComponent(AIProblemPanel& ownerRef) : owner(ownerRef)
        {
            setInterceptsMouseClicks(false, true);
            setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
            setWantsKeyboardFocus(true);
            setAccessible(true);

            {
                auto f = juce::Font(juce::FontOptions().withHeight(13.0f));
                f.setBold(true);
                setupLabel(typeLabel, f, juce::Colour(0xFFFFFFFF));
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(14.0f));
                f.setBold(true);
                setupLabel(freqLabel, f, juce::Colour(0xFFFFFFFF));
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(9.0f));
                f.setBold(true);
                setupLabel(sevLabel, f, juce::Colour(0xFFFFFFFF), true);
            }
            setupLabel(explanationLabel, juce::Font(juce::FontOptions().withHeight(11.0f)), juce::Colour(0xFFDDDDDD));
            setupLabel(causeLabel, juce::Font(juce::FontOptions().withHeight(10.0f)), juce::Colour(0xFF999999));
            auto italic = juce::Font(juce::FontOptions().withHeight(10.0f));
            italic.setItalic(true);
            setupLabel(impactLabel, italic, juce::Colour(0xFF888888));
            {
                auto f = juce::Font(juce::FontOptions().withHeight(11.0f));
                f.setBold(true);
                setupLabel(fixLabel, f, juce::Colour(0xFF66FF66));
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(9.0f));
                f.setBold(true);
                setupLabel(confidenceLabel, f, juce::Colours::white, true);
            }
            setupLabel(bandLabel, juce::Font(juce::FontOptions().withHeight(9.0f)), juce::Colour(0xFF666666));
            setupLabel(hintLabel, juce::Font(juce::FontOptions().withHeight(9.0f)), juce::Colour(0xFF4A9FD9));
            hintLabel.setVisible(false);

            // Focus order within the row
            typeLabel.setExplicitFocusOrder(1);
            freqLabel.setExplicitFocusOrder(2);
            sevLabel.setExplicitFocusOrder(3);
            explanationLabel.setExplicitFocusOrder(4);
            causeLabel.setExplicitFocusOrder(5);
            impactLabel.setExplicitFocusOrder(6);
            fixLabel.setExplicitFocusOrder(7);
            bandLabel.setExplicitFocusOrder(8);
            hintLabel.setExplicitFocusOrder(9);
            confidenceLabel.setExplicitFocusOrder(10);

            addAndMakeVisible(typeLabel);
            addAndMakeVisible(freqLabel);
            addAndMakeVisible(sevLabel);
            addAndMakeVisible(explanationLabel);
            addAndMakeVisible(causeLabel);
            addAndMakeVisible(impactLabel);
            addAndMakeVisible(fixLabel);
            addAndMakeVisible(confidenceLabel);
            addAndMakeVisible(bandLabel);
            addAndMakeVisible(hintLabel);
        }

        void updateFromProblem(const AIEngine::Correction& p, int rowIndex, bool isSelected, bool rtlFlag)
        {
            currentIndex = rowIndex;
            rtl = rtlFlag;
            selected = isSelected;
            severityColour = owner.getSeverityColor(p.severity);
            confidenceValue = p.confidence;

            const int confidencePercent = static_cast<int>(p.confidence * 100.0f);

            setTitle(owner.getRowSummaryFromCorrection(p));
            setDescription(owner.getRowAnnouncementFromCorrection(p));
            setHelpText(owner.tr("Press Enter to apply or Space to preview on spectrum",
                                 "Press Enter to apply or Space to preview on spectrum"));

            sevLabel.setColour(juce::Label::backgroundColourId, severityColour.withAlpha(0.3f));
            sevLabel.setColour(juce::Label::textColourId, severityColour);
            const auto severityLabel = owner.getSeverityLabel(p.severity);
            sevLabel.setText(severityLabel, juce::dontSendNotification);
            sevLabel.setTitle(owner.tr("Severity", "Severity"));
            sevLabel.setDescription(severityLabel + " (" + juce::String(confidencePercent) + "%)");

            const auto typeName = AIEngine::getProblemTypeName(p.type).toUpperCase();
            typeLabel.setText(typeName, juce::dontSendNotification);
            typeLabel.setTitle(owner.tr("Problem type", "Problem type"));
            typeLabel.setDescription(typeName);

            const auto freqText = owner.formatFreq(p.frequency);
            freqLabel.setText(freqText, juce::dontSendNotification);
            freqLabel.setTitle(owner.tr("Frequency", "Frequency"));
            freqLabel.setDescription(owner.tr("Centre frequency", "Centre frequency") + ": " + freqText);

            const auto explanation = owner.getExplanation(p);
            explanationLabel.setText(explanation, juce::dontSendNotification);
            explanationLabel.setTitle(owner.tr("Explanation", "Explanation"));
            explanationLabel.setDescription(explanation);

            const auto cause = owner.getCause(p.type);
            causeLabel.setText(owner.tr("CAUSE:", "CAUSE:") + " " + cause, juce::dontSendNotification);
            causeLabel.setTitle(owner.tr("Probable cause", "Probable cause"));
            causeLabel.setDescription(cause);

            const auto impact = owner.getImpact(p.type);
            impactLabel.setText(owner.tr("SOUNDS:", "SOUNDS:") + " " + impact, juce::dontSendNotification);
            impactLabel.setTitle(owner.tr("Sound impact", "Sound impact"));
            impactLabel.setDescription(impact);

            juce::String action = p.suggestedGain < 0 ? owner.tr("CUT", "CUT") : owner.tr("BOOST", "BOOST");
            juce::String gainStr = (p.suggestedGain > 0 ? "+" : "") + juce::String(p.suggestedGain, 1) + " dB";
            juce::String qStr = owner.tr("Q=", "Q=") + juce::String(p.suggestedQ, 1);
            const auto fixText = owner.tr("FIX:", "FIX:") + " " + action + " " + gainStr + "  " + qStr;
            fixLabel.setText(fixText, juce::dontSendNotification);
            fixLabel.setTitle(owner.tr("Suggested fix", "Suggested fix"));
            fixLabel.setDescription(fixText);

            const auto confidenceText = juce::String(confidencePercent) + "%";
            confidenceLabel.setText(confidenceText, juce::dontSendNotification);
            confidenceLabel.setTitle(owner.tr("Confidence", "Confidence"));
            confidenceLabel.setDescription(owner.tr("AI confidence level", "AI confidence level") + ": " + confidenceText);

            juce::String bandName = AIEngine::getBandName(p.frequency);
            juce::String bwDesc = p.suggestedQ > 5.0f ? owner.tr("Narrow surgical cut", "Narrow surgical cut")
                                : (p.suggestedQ > 2.0f ? owner.tr("Focused correction", "Focused correction")
                                                        : owner.tr("Wide musical adjustment", "Wide musical adjustment"));
            const auto bandText = owner.tr("Region:", "Region:") + " " + bandName + "  •  " + bwDesc;
            bandLabel.setText(bandText, juce::dontSendNotification);
            bandLabel.setTitle(owner.tr("Frequency region", "Frequency region"));
            bandLabel.setDescription(bandText);

            const auto hintText = owner.tr("Click to highlight on spectrum • Double-click to apply fix",
                                           "Click to highlight on spectrum • Double-click to apply fix");
            hintLabel.setText(hintText, juce::dontSendNotification);
            hintLabel.setTitle(owner.tr("Row actions", "Row actions"));
            hintLabel.setDescription(hintText);
            hintLabel.setVisible(selected);

            auto justify = rtl ? juce::Justification::centredRight : juce::Justification::left;
            for (auto* lbl : { &typeLabel, &freqLabel, &sevLabel, &explanationLabel, &causeLabel,
                               &impactLabel, &fixLabel, &bandLabel, &hintLabel, &confidenceLabel })
            {
                lbl->setJustificationType(justify);
            }

            rowTooltip = owner.getRowSummaryFromCorrection(p);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const auto w = static_cast<float>(getWidth());
            const auto h = static_cast<float>(getHeight());
            const float badgeW = 70.0f;
            const float corner = 6.0f;
            const float barWidth = 6.0f;
            const float barX = rtl ? w - barWidth - 4.0f : 4.0f;

            juce::Colour bgCol = selected ? juce::Colour(0xFF2A3040) : juce::Colour(0xFF222222);
            g.setColour(bgCol);
            g.fillRoundedRectangle(4.0f, 2.0f, w - 8.0f, h - 4.0f, corner);

            g.setColour(severityColour);
            g.fillRoundedRectangle(barX, 2.0f, barWidth, h - 4.0f, 3.0f);

            const float badgeX = rtl ? 12.0f : w - badgeW - 12.0f;
            g.setColour(severityColour.withAlpha(0.3f));
            g.fillRoundedRectangle(badgeX, 8.0f, badgeW, 18.0f, 4.0f);
            g.setColour(severityColour);
            g.drawRoundedRectangle(badgeX, 8.0f, badgeW, 18.0f, 4.0f, 1.0f);

            const float x = 16.0f;
            const float contentW = w - x - 10.0f;
            const float fixBoxY = 82.0f;
            g.setColour(juce::Colour(0xFF1A2A1A));
            g.fillRoundedRectangle(x, fixBoxY, contentW - 80.0f, 26.0f, 4.0f);

            const float confX = rtl ? x + 10.0f : w - 90.0f;
            const float confW = 65.0f;
            g.setColour(juce::Colour(0xFF333333));
            g.fillRoundedRectangle(confX, fixBoxY + 6.0f, confW, 14.0f, 4.0f);

            juce::Colour confCol = confidenceValue > 0.7f ? juce::Colour(0xFF44AA44)
                                      : (confidenceValue > 0.4f ? juce::Colour(0xFFAAAA44) : juce::Colour(0xFFAA4444));
            g.setColour(confCol);
            g.fillRoundedRectangle(confX, fixBoxY + 6.0f, confW * confidenceValue, 14.0f, 4.0f);

            g.setColour(juce::Colour(0xFF333333));
            g.drawHorizontalLine(static_cast<int>(h) - 2, 10.0f, w - 10.0f);
        }

        void resized() override
        {
            const int w = getWidth();
            const int badgeW = 70;
            const int xBase = 16;

            auto place = [this](juce::Label& lbl, int x, int y, int width, int height)
            {
                lbl.setBounds(x, y, width, height);
            };

            if (! rtl)
            {
                place(typeLabel, xBase, 8, 120, 18);
                place(freqLabel, xBase + 125, 8, 80, 18);
                place(sevLabel, w - badgeW - 12, 8, badgeW, 18);
            }
            else
            {
                place(sevLabel, 12, 8, badgeW, 18);
                place(freqLabel, w - xBase - 80, 8, 80, 18);
                place(typeLabel, w - xBase - 80 - 120, 8, 120, 18);
            }

            const int contentW = w - xBase - 10;
            place(explanationLabel, xBase, 30, contentW, 16);
            place(causeLabel, xBase, 48, contentW, 14);
            place(impactLabel, xBase, 64, contentW, 14);

            const float fixBoxY = 82.0f;
            place(fixLabel, xBase + 10, static_cast<int>(fixBoxY) + 2, contentW - 110, 22);
            place(confidenceLabel, w - 90, static_cast<int>(fixBoxY) + 6, 65, 14);

            place(bandLabel, xBase, 112, contentW, 14);
            place(hintLabel, xBase, 126, contentW, 12);
        }

        juce::String getTooltip() override { return rowTooltip; }

    private:
        void setupLabel(juce::Label& lbl, juce::Font font, juce::Colour colour, bool roundedBackground = false)
        {
            lbl.setFont(font);
            lbl.setColour(juce::Label::textColourId, colour);
            if (roundedBackground)
                lbl.setColour(juce::Label::backgroundColourId, juce::Colour(0x33000000));
            lbl.setInterceptsMouseClicks(false, false);
            lbl.setWantsKeyboardFocus(true);
            lbl.setAccessible(true);
        }

        AIProblemPanel& owner;
        bool rtl = false;
        bool selected = false;
        int currentIndex = -1;
        float confidenceValue = 0.0f;
        juce::Colour severityColour = juce::Colours::transparentBlack;
        juce::String rowTooltip;

        juce::Label typeLabel, freqLabel, sevLabel, explanationLabel, causeLabel, impactLabel,
                    fixLabel, confidenceLabel, bandLabel, hintLabel;
    };

    //==========================================================================
    // Helper Functions
    //==========================================================================
    
    juce::String formatFreq(float f) const
    {
        if (f >= 10000.0f) return juce::String(f / 1000.0f, 1) + " kHz";
        if (f >= 1000.0f) return juce::String(f / 1000.0f, 2) + " kHz";
        return juce::String(static_cast<int>(f)) + " Hz";
    }
    
    juce::Colour getSeverityColor(float sev) const
    {
        if (sev > 0.7f) return juce::Colour(0xFFFF4444);  // Red
        if (sev > 0.4f) return juce::Colour(0xFFFFAA00);  // Orange
        return juce::Colour(0xFF44BB44);  // Green
    }

    juce::String getSeverityLabel(float sev) const
    {
        if (sev > 0.7f) return tr("CRITICAL", "CRITICAL");
        if (sev > 0.4f) return tr("MODERATE", "MODERATE");
        return tr("MINOR", "MINOR");
    }
    
    juce::String getExplanation(const AIEngine::Correction& p) const
    {
        float gain = std::abs(p.suggestedGain);
        juce::String freqStr = formatFreq(p.frequency);
        
        switch (p.type)
        {
            case AIEngine::ProblemType::Resonance:
                return tr("Sharp resonant peak +", "Sharp resonant peak +") + juce::String(gain, 1) + tr(" dB above surrounding frequencies",
                                                                                                          " dB above surrounding frequencies");
            case AIEngine::ProblemType::Harshness:
                return tr("Excessive upper-mid energy causing listener fatigue at ", "Excessive upper-mid energy causing listener fatigue at ") + freqStr;
            case AIEngine::ProblemType::Muddiness:
                return tr("Low-mid buildup masking clarity and detail around ", "Low-mid buildup masking clarity and detail around ") + freqStr;
            case AIEngine::ProblemType::Boxyness:
                return tr("Room/cabinet resonance creating hollow coloration at ", "Room/cabinet resonance creating hollow coloration at ") + freqStr;
            case AIEngine::ProblemType::Sibilance:
                return tr("Harsh sibilant frequencies (S/T/F sounds) peaking at ", "Harsh sibilant frequencies (S/T/F sounds) peaking at ") + freqStr;
            case AIEngine::ProblemType::LowEndBoom:
                return tr("Excessive sub-bass energy causing rumble and masking at ", "Excessive sub-bass energy causing rumble and masking at ") + freqStr;
            case AIEngine::ProblemType::ThinSound:
                return tr("Deficient low-mid body - sound lacks warmth around ", "Deficient low-mid body - sound lacks warmth around ") + freqStr;
            case AIEngine::ProblemType::DullSound:
                return tr("Missing high-frequency air and sparkle above ", "Missing high-frequency air and sparkle above ") + freqStr;
            default:
                return tr("Audio issue detected at ", "Audio issue detected at ") + freqStr;
        }
    }
    
    juce::String getCause(AIEngine::ProblemType type) const
    {
        switch (type)
        {
            case AIEngine::ProblemType::Resonance:
                return tr("Room mode, mic resonance, or instrument characteristic",
                          "Room mode, mic resonance, or instrument characteristic");
            case AIEngine::ProblemType::Harshness:
                return tr("Overdriven preamp, bright mic, or aggressive compression",
                          "Overdriven preamp, bright mic, or aggressive compression");
            case AIEngine::ProblemType::Muddiness:
                return tr("Proximity effect, room reflections, stacked instruments",
                          "Proximity effect, room reflections, stacked instruments");
            case AIEngine::ProblemType::Boxyness:
                return tr("Small room recording or cabinet resonance",
                          "Small room recording or cabinet resonance");
            case AIEngine::ProblemType::Sibilance:
                return tr("Close mic technique or vocalist characteristics",
                          "Close mic technique or vocalist characteristics");
            case AIEngine::ProblemType::LowEndBoom:
                return tr("Room bass buildup or uncontrolled low frequencies",
                          "Room bass buildup or uncontrolled low frequencies");
            case AIEngine::ProblemType::ThinSound:
                return tr("High-pass too aggressive or phase cancellation",
                          "High-pass too aggressive or phase cancellation");
            case AIEngine::ProblemType::DullSound:
                return tr("Low-pass filtering or absorption in recording",
                          "Low-pass filtering or absorption in recording");
            default:
                return tr("Multiple possible causes", "Multiple possible causes");
        }
    }
    
    juce::String getImpact(AIEngine::ProblemType type) const
    {
        switch (type)
        {
            case AIEngine::ProblemType::Resonance:
                return tr("Ringing, feedback-prone, fatiguing",
                          "Ringing, feedback-prone, fatiguing");
            case AIEngine::ProblemType::Harshness:
                return tr("Ear fatigue, painful at volume",
                          "Ear fatigue, painful at volume");
            case AIEngine::ProblemType::Muddiness:
                return tr("Unclear, obscured detail",
                          "Unclear, obscured detail");
            case AIEngine::ProblemType::Boxyness:
                return tr("Cheap, amateur sound quality",
                          "Cheap, amateur sound quality");
            case AIEngine::ProblemType::Sibilance:
                return tr("Distracting, piercing consonants",
                          "Distracting, piercing consonants");
            case AIEngine::ProblemType::LowEndBoom:
                return tr("Muddy bass, wasted headroom",
                          "Muddy bass, wasted headroom");
            case AIEngine::ProblemType::ThinSound:
                return tr("Weak, lacks emotional weight",
                          "Weak, lacks emotional weight");
            case AIEngine::ProblemType::DullSound:
                return tr("Lifeless, poor translation",
                          "Lifeless, poor translation");
            default:
                return tr("May affect mix quality", "May affect mix quality");
        }
    }
    
    void showFullAnalysis(int row)
    {
        if (row < 0 || row >= static_cast<int>(problems.size())) return;
        const auto& p = problems[row];
        
        juce::String msg;
        msg += tr("PROBLEM TYPE: ", "PROBLEM TYPE: ") + AIEngine::getProblemTypeName(p.type) + "\n";
        msg += tr("FREQUENCY: ", "FREQUENCY: ") + formatFreq(p.frequency) + "\n";
        msg += tr("REGION: ", "REGION: ") + AIEngine::getBandName(p.frequency) + "\n\n";
        msg += tr("SEVERITY: ", "SEVERITY: ") + juce::String(static_cast<int>(p.severity * 100)) + "%\n";
        msg += tr("CONFIDENCE: ", "CONFIDENCE: ") + juce::String(static_cast<int>(p.confidence * 100)) + "%\n\n";
        msg += tr("EXPLANATION:", "EXPLANATION:") + "\n" + getExplanation(p) + "\n\n";
        msg += tr("PROBABLE CAUSE:", "PROBABLE CAUSE:") + "\n" + getCause(p.type) + "\n\n";
        msg += tr("SOUND IMPACT:", "SOUND IMPACT:") + "\n" + getImpact(p.type) + "\n\n";
        msg += tr("SUGGESTED FIX:", "SUGGESTED FIX:") + "\n";
        msg += "  " + tr("Gain: ", "Gain: ") + juce::String(p.suggestedGain, 1) + " dB\n";
        msg += "  " + tr("Q: ", "Q: ") + juce::String(p.suggestedQ, 1) + "\n";
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            tr("Full Analysis: ", "Full Analysis: ") + AIEngine::getProblemTypeName(p.type), msg, tr("OK", "OK"),
            nullptr, juce::ModalCallbackFunction::create([this, p](int)
        {
            juce::AccessibilityHandler::postAnnouncement(
                tr("Analisi completa chiusa per ", "Full analysis closed for ") + AIEngine::getProblemTypeName(p.type),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
        }));
    }
    
    void updateProblemList()
    {
        // FORCE SHOW: Get ALL pending corrections - NO CONDITIONS
        auto raw = processor.getAIEngine().getPendingCorrections();
        
        // ALWAYS USE RAW - NO FILTERING, NO MERGING, NO CONDITIONS
        problems = raw;
        
        // Sort by priority (severity * confidence) for display
        std::sort(problems.begin(), problems.end(), [](const AIEngine::Correction& a, const AIEngine::Correction& b) {
            float priorityA = a.severity * a.confidence;
            float priorityB = b.severity * b.confidence;
            if (std::abs(priorityA - priorityB) < 0.01f)
                return a.severity > b.severity;
            return priorityA > priorityB;
        });
        
        // Limit to top 50 for performance (but show ALL if less than 50)
        if (problems.size() > 50)
            problems.resize(50);
        problemList.updateContent();
        if (auto* h = problemList.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
        const int n = static_cast<int>(problems.size());
        const auto newIssuesMsg = (n == 1)
            ? juce::String::formatted(tr("%d nuovo problema rilevato", "%d new problem detected").toRawUTF8(), n)
            : juce::String::formatted(tr("%d nuovi problemi rilevati", "%d new problems detected").toRawUTF8(), n);
        juce::AccessibilityHandler::postAnnouncement(newIssuesMsg, juce::AccessibilityHandler::AnnouncementPriority::high);
        
        if (n == 0)
        {
            statusLabel.setText(tr("No issues detected", "No issues detected"), juce::dontSendNotification);
        }
        else
        {
            auto fmt = (n == 1) ? tr("%d issue detected", "%d issue detected")
                                : tr("%d issues detected", "%d issues detected");
            statusLabel.setText(juce::String::formatted(fmt.toRawUTF8(), n), juce::dontSendNotification);
        }
        
        autoFixBtn.setEnabled(n > 0);
    }
    
    void updateButtons()
    {
        undoBtn.setEnabled(processor.canUndo());
        redoBtn.setEnabled(processor.canRedo());
        
        if (processor.canUndo())
            undoBtn.setTooltip(tr("Undo: ", "Undo: ") + processor.getUndoDescription());
        if (processor.canRedo())
            redoBtn.setTooltip(tr("Redo: ", "Redo: ") + processor.getRedoDescription());

        if (auto* h = undoBtn.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        if (auto* h = redoBtn.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
    }

    void showAutoFixConfirmation()
    {
        if (problems.empty()) return;
        
        juce::String msg = tr("Apply ", "Apply ") + juce::String(problems.size()) + tr(" corrections?\n\n", " corrections?\n\n");
        
        for (size_t i = 0; i < juce::jmin(problems.size(), size_t(5)); ++i)
        {
            const auto& p = problems[i];
            msg += "• " + AIEngine::getProblemTypeName(p.type) + " @ " + formatFreq(p.frequency);
            msg += " → " + juce::String(p.suggestedGain, 1) + " dB\n";
        }
        
        if (problems.size() > 5)
            msg += "\n" + tr("...and ", "...and ") + juce::String(problems.size() - 5) + tr(" more\n", " more\n");
        
        msg += "\n" + tr("You can UNDO these changes.", "You can UNDO these changes.");
        
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
            tr("Apply AI Corrections", "Apply AI Corrections"), msg, tr("Apply", "Apply"), tr("Cancel", "Cancel"), nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1) {
                    processor.getAIEngine().approveAllCorrections();
                    processor.applyAICorrections();
                    updateProblemList();
                    juce::AccessibilityHandler::postAnnouncement(
                        tr("Correzioni applicate", "Corrections applied"),
                        juce::AccessibilityHandler::AnnouncementPriority::high);
                } else {
                    juce::AccessibilityHandler::postAnnouncement(
                        tr("Applicazione correttivi annullata", "Correction application cancelled"),
                        juce::AccessibilityHandler::AnnouncementPriority::medium);
                }
            }));
    }
    
    juce::String getCurrentLocaleTag() const
    {
        if (auto* loc = juce::LocalisedStrings::getCurrentMappings())
            return loc->getLanguageName();

        return juce::SystemStats::getDisplayLanguage();
    }

    bool isRightToLeft() const
    {
        const auto lang = getCurrentLocaleTag();
        return lang.startsWithIgnoreCase("ar")
            || lang.startsWithIgnoreCase("he")
            || lang.startsWithIgnoreCase("fa")
            || lang.startsWithIgnoreCase("ur");
    }

    juce::String tr(const juce::String& key, const juce::String& fallback = {}) const
    {
        return fallback.isNotEmpty() ? juce::translate(key, fallback) : juce::translate(key);
    }

    void focusListRow(int row)
    {
        problemList.selectRow(row);
        problemList.scrollToEnsureRowIsOnscreen(row);
        problemList.grabKeyboardFocus();
        if (auto* h = problemList.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::rowSelectionChanged);
        if (row >= 0 && row < static_cast<int>(problems.size()))
        {
            juce::AccessibilityHandler::postAnnouncement(
                tr("Riga selezionata: ", "Selected row: ") + getRowAnnouncementFromCorrection(problems[static_cast<size_t>(row)]),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
        }
    }

    void activateRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return;

        const auto& p = problems[row];
        // Apply ONLY this single correction, not all approved ones
        processor.applySingleCorrection(p);
        updateProblemList();
    }

    void highlightRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return;

        if (onProblemSelected)
        {
            const auto& p = problems[static_cast<size_t>(row)];
            onProblemSelected(p.frequency, p.suggestedQ, p.severity, p.type);
        }
    }

    juce::String getRowSummary(int row) const
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return {};

        return getRowSummaryFromCorrection(problems[static_cast<size_t>(row)]);
    }

    juce::String getRowSummaryFromCorrection(const AIEngine::Correction& p) const
    {
        juce::String summary;
        summary << tr("Type:", "Type:") << " " << AIEngine::getProblemTypeName(p.type)
                << " • " << tr("Freq:", "Freq:") << " " << formatFreq(p.frequency)
                << " • " << tr("Gain:", "Gain:") << " " << juce::String(p.suggestedGain, 1) << " dB";
        return summary;
    }

    juce::String getRowAnnouncementFromCorrection(const AIEngine::Correction& p) const
    {
        juce::String announcement;
        announcement << AIEngine::getProblemTypeName(p.type) << " @ " << formatFreq(p.frequency) << ". ";
        announcement << tr("Severity", "Severity") << " " << getSeverityLabel(p.severity)
                     << " (" << juce::String(static_cast<int>(p.severity * 100.0f)) << "%). ";
        announcement << tr("Confidence", "Confidence") << " "
                     << juce::String(static_cast<int>(p.confidence * 100.0f)) << "%. ";
        const bool isCut = p.suggestedGain < 0;
        announcement << tr("Suggested", "Suggested") << " "
                     << (isCut ? tr("cut", "cut") : tr("boost", "boost")) << " "
                     << juce::String(p.suggestedGain, 1) << " dB, Q "
                     << juce::String(p.suggestedQ, 1) << ".";
        return announcement;
    }
    
    // Find and approve/reject correction by matching frequency, type, and gain
    // (because UI index doesn't match pendingCorrections index after filtering/merging)
    void approveCorrectionByMatch(const AIEngine::Correction& target)
    {
        auto& ai = processor.getAIEngine();
        auto pending = ai.getPendingCorrections();
        
        // Find matching correction in pendingCorrections
        for (int i = 0; i < static_cast<int>(pending.size()); ++i)
        {
            const auto& p = pending[i];
            // Match by frequency (within 1%), type, and similar gain
            const float freqRatio = std::abs(std::log2(target.frequency / juce::jmax(20.0f, p.frequency)));
            const bool freqMatch = freqRatio < 0.01f;  // Within 1%
            const bool typeMatch = (p.type == target.type);
            const bool gainMatch = std::abs(p.suggestedGain - target.suggestedGain) < 0.5f;  // Within 0.5dB
            
            if (freqMatch && typeMatch && gainMatch)
            {
                ai.approveCorrection(i);
                return;
            }
        }
    }
    
    void rejectCorrectionByMatch(const AIEngine::Correction& target)
    {
        auto& ai = processor.getAIEngine();
        auto pending = ai.getPendingCorrections();
        
        // Find matching correction in pendingCorrections
        for (int i = 0; i < static_cast<int>(pending.size()); ++i)
        {
            const auto& p = pending[i];
            // Match by frequency (within 1%), type, and similar gain
            const float freqRatio = std::abs(std::log2(target.frequency / juce::jmax(20.0f, p.frequency)));
            const bool freqMatch = freqRatio < 0.01f;  // Within 1%
            const bool typeMatch = (p.type == target.type);
            const bool gainMatch = std::abs(p.suggestedGain - target.suggestedGain) < 0.5f;  // Within 0.5dB
            
            if (freqMatch && typeMatch && gainMatch)
            {
                ai.rejectCorrection(i);
                return;
            }
        }
    }

    //==========================================================================
    AIEqualizerAudioProcessor& processor;
    
    juce::Label titleLabel, genreLabel, profileLabel, statusLabel;
    ProblemListBox problemList;
    juce::TextButton autoFixBtn, clearBtn, undoBtn, redoBtn;
    
    std::vector<AIEngine::Correction> problems;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIProblemPanel)
};
