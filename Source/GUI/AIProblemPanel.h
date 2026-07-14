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
        titleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentBlue); // Semantic panel title accent
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setTitle(tr("AI analysis title", "AI analysis title"));
        titleLabel.setDescription(tr("Heading for AI analysis results", "Heading for AI analysis results"));
        addAndMakeVisible(titleLabel);
        
        // Info labels
        genreLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        genreLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        genreLabel.setJustificationType(juce::Justification::centredLeft);
        genreLabel.setTitle(tr("Genre label", "Genre label"));
        genreLabel.setDescription(tr("Detected genre description", "Detected genre description"));
        addAndMakeVisible(genreLabel);
        
        profileLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        profileLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        profileLabel.setJustificationType(juce::Justification::centredLeft);
        profileLabel.setTitle(tr("Profile label", "Profile label"));
        profileLabel.setDescription(tr("Detected source profile", "Detected source profile"));
        addAndMakeVisible(profileLabel);
        
        // Problem list with custom row height
        problemList.setModel(this);
        problemList.setRowHeight(56);  // Compact rows — fits 3 problems in 170px visible area
        problemList.setColour(juce::ListBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark);
        problemList.setColour(juce::ListBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
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

        // UX "Diagnosi Stabile": visible capture strip — surfaces the (previously hidden)
        // CaptureService with clear state feedback and a frozen-diagnosis mode.
        captureStripBtn.setButtonText(tr("CAPTURE", "CAPTURE"));
        captureStripBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
        captureStripBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::accentBlue);
        captureStripBtn.setTooltip(tr("Record a snippet of audio and freeze the diagnosis on it",
                                      "Record a snippet of audio and freeze the diagnosis on it"));
        captureStripBtn.setTitle(tr("Capture and diagnose", "Capture and diagnose"));
        captureStripBtn.setDescription(tr("Start or stop an audio capture, then freeze the analysis results",
                                          "Start or stop an audio capture, then freeze the analysis results"));
        captureStripBtn.onClick = [this]() { onCaptureStripClicked(); };
        addAndMakeVisible(captureStripBtn);

        captureStripLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        captureStripLabel.setJustificationType(juce::Justification::centredLeft);
        captureStripLabel.setTitle(tr("Capture status", "Capture status"));
        captureStripLabel.setDescription(tr("Shows the current capture and freeze state",
                                            "Shows the current capture and freeze state"));
        addAndMakeVisible(captureStripLabel);
        
        // Action buttons
        autoFixBtn.setButtonText(tr("FIX ALL", "FIX ALL"));
        autoFixBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFFE8A030)); // Amber (Liquid Intelligence signature)
        autoFixBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFF4B84A)); // Amber bright (hover/pressed)
        autoFixBtn.setColour(juce::TextButton::textColourOnId,   juce::Colour(0xFF181A22)); // Dark text on amber
        autoFixBtn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xFF181A22));
        autoFixBtn.setTooltip(tr("Apply all suggested fixes (with confirmation)",
                                 "Apply all suggested fixes (with confirmation)"));
        autoFixBtn.setTitle(tr("Apply all fixes", "Apply all fixes"));
        autoFixBtn.setDescription(tr("Approve every suggested correction after confirmation",
                                     "Approve every suggested correction after confirmation"));
        autoFixBtn.onClick = [this]() { showAutoFixConfirmation(); };
        autoFixBtn.setExplicitFocusOrder(1);
        addAndMakeVisible(autoFixBtn);
        
        clearBtn.setButtonText(tr("CLEAR", "CLEAR"));
        clearBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLighter);
        clearBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary);
        clearBtn.setTooltip(tr("Remove all detected problems from the list", "Remove all detected problems from the list"));
        clearBtn.setTitle(tr("Clear list", "Clear list"));
        clearBtn.setDescription(tr("Dismiss every detected problem without applying fixes",
                                   "Dismiss every detected problem without applying fixes"));
        clearBtn.onClick = [this]() { 
            if (isFrozen)
                return;   // frozen diagnosis: CLEAR disabled (button is greyed out too)
            transientVisualHolds.clear();
            processor.getAIEngine().clearCorrections();
            updateProblemList();
        };
        clearBtn.setExplicitFocusOrder(2);
        addAndMakeVisible(clearBtn);
        
        undoBtn.setButtonText(tr("UNDO", "UNDO"));
        undoBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
        undoBtn.onClick = [this]() { processor.undo(); updateButtons(); };
        undoBtn.setTitle(tr("Undo last action", "Undo last action"));
        undoBtn.setExplicitFocusOrder(3);
        addAndMakeVisible(undoBtn);
        
        redoBtn.setButtonText(tr("REDO", "REDO"));
        redoBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
        redoBtn.onClick = [this]() { processor.redo(); updateButtons(); };
        redoBtn.setTitle(tr("Redo undone action", "Redo undone action"));
        redoBtn.setExplicitFocusOrder(4);
        addAndMakeVisible(redoBtn);
        
        // Status
        statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        statusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setTitle(tr("Analysis status", "Analysis status"));
        statusLabel.setDescription(tr("Shows how many problems were detected",
                                      "Shows how many problems were detected"));
        addAndMakeVisible(statusLabel);
        
        // Multi-Track Unmasking toggle — shown only if the feature is enabled (GUI-4).
        if (kMultiTrackUIEnabled)
        {
            unmaskingBtn.setButtonText("UNMASKING");
            unmaskingBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
            unmaskingBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textSecondary);
            unmaskingBtn.setTooltip("Enable Multi-Track Unmasking: Analyze frequency masking between tracks");
            unmaskingBtn.onClick = [this]() {
                bool newState = !processor.getAIEngine().isMultiTrackUnmaskingEnabled();
                processor.getAIEngine().setMultiTrackUnmaskingEnabled(newState);
                updateUnmaskingButton(newState);
            };
            addAndMakeVisible(unmaskingBtn);
        }

        startTimerHz(10);
    }
    
    ~AIProblemPanel() override { stopTimer(); }

    // Public refresh entrypoint for external callers (e.g., editor timer)
    void refreshFromProcessor()
    {
        needsUpdate.store(true, std::memory_order_release);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgMid);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Top accent line
        g.setColour(ModernLookAndFeel::Colors::accentBlue); // Semantic panel accent strip
        g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), 3.0f);
        
        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(6, 4);
        const bool rtl = isRightToLeft();

        // Hide labels we no longer show — prevents overdraw
        profileLabel.setVisible(false);
        statusLabel.setVisible(false);

        // Single title row: title + genre (+ unmasking only if enabled) (18px)
        auto titleRow = bounds.removeFromTop(18);
        if (rtl)
        {
            if (kMultiTrackUIEnabled)
                unmaskingBtn.setBounds(titleRow.removeFromLeft(80).reduced(1));
            genreLabel.setBounds(titleRow.removeFromLeft(titleRow.getWidth() - 90));
            titleLabel.setBounds(titleRow);
            titleLabel.setJustificationType(juce::Justification::centredRight);
            genreLabel.setJustificationType(juce::Justification::centredRight);
        }
        else
        {
            titleLabel.setBounds(titleRow.removeFromLeft(90));
            if (kMultiTrackUIEnabled)
            {
                genreLabel.setBounds(titleRow.removeFromLeft(titleRow.getWidth() - 80));
                unmaskingBtn.setBounds(titleRow.removeFromRight(80).reduced(1));
            }
            else
            {
                genreLabel.setBounds(titleRow);  // reclaim the freed space
            }
            titleLabel.setJustificationType(juce::Justification::centredLeft);
            genreLabel.setJustificationType(juce::Justification::centredLeft);
        }

        // UX "Diagnosi Stabile": capture strip (22px) below the title row
        bounds.removeFromTop(2);
        auto captureRow = bounds.removeFromTop(22);
        if (rtl)
        {
            captureStripBtn.setBounds(captureRow.removeFromRight(112).reduced(1));
            captureRow.removeFromRight(6);
            captureStripLabel.setBounds(captureRow);
            captureStripLabel.setJustificationType(juce::Justification::centredRight);
        }
        else
        {
            captureStripBtn.setBounds(captureRow.removeFromLeft(112).reduced(1));
            captureRow.removeFromLeft(6);
            captureStripLabel.setBounds(captureRow);
            captureStripLabel.setJustificationType(juce::Justification::centredLeft);
        }

        bounds.removeFromTop(1); // tiny gap before list

        // Bottom buttons (24px)
        auto btnRow = bounds.removeFromBottom(24);
        int btnW = (btnRow.getWidth() - 8) / 4;
        autoFixBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(1));
        clearBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(1));
        undoBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(1));
        redoBtn.setBounds(btnRow.reduced(1));

        bounds.removeFromBottom(1); // tiny gap above buttons

        // Problem list fills rest
        problemList.setBounds(bounds);
    }
    
    void timerCallback() override
    {
        // SAFETY: Skip if processor not ready
        if (!processor.isProcessorReady())
            return;
            
        auto& ai = processor.getAIEngine();

        // UX "Diagnosi Stabile": capture strip state machine.
        // Freeze trigger (re-counter-check fix, Finding #1): keyed on the DEDICATED
        // capture-completion signal (captureAnalysisCompleted, set exclusively by the
        // finish() path of analyzeCapturedAudioSnapshot), NOT on the generic
        // aiProblemsChanged/needsUpdate flag — the live analysis loop posts that flag
        // on every analyzed frame while the transport runs, which could freeze the
        // LIVE state before the capture analysis had finished.
        if (captureAwaitingResult
            && processor.consumeCaptureAnalysisCompleted())
        {
            captureAwaitingResult = false;

            if (! processor.getCaptureAnalysisResult())
            {
                // Analysis ran but failed (e.g. buffer unreadable): back to idle, no freeze
                captureStripBtn.setEnabled(true);
                captureStripBtn.setButtonText(tr("CAPTURE", "CAPTURE"));
                captureStripLabel.setText(tr("Capture analysis failed — try again", "Capture analysis failed — try again"),
                                          juce::dontSendNotification);
                captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
                return;
            }

            isFrozen = true;
            frozenProblems = ai.getPendingCorrections();
            transientVisualHolds.clear();     // diagnosis mode: no ghosts, data is static
            captureStripBtn.setEnabled(true);
            captureStripBtn.setButtonText(tr("BACK TO LIVE", "BACK TO LIVE"));
            captureStripBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::accentGreen);
            captureStripLabel.setText(tr("DIAGNOSIS FROZEN — results locked", "DIAGNOSIS FROZEN — results locked"),
                                      juce::dontSendNotification);
            captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentGreen);
            clearBtn.setEnabled(false);       // don't clear the engine while inspecting a frozen diagnosis
            juce::AccessibilityHandler::postAnnouncement(
                tr("Diagnosi congelata sui risultati della cattura", "Diagnosis frozen on capture results"),
                juce::AccessibilityHandler::AnnouncementPriority::high);
            needsUpdate.store(false, std::memory_order_release);   // consume any pending live refresh
            updateProblemList();              // show frozen results immediately
            updateButtons();
            return;
        }

        if (isFrozen)
            return;   // frozen: skip all live refresh paths (no flicker possible)

        if (processor.isCapturing())
            captureStripLabel.setText(tr("Recording...", "Recording..."), juce::dontSendNotification);
        
        // Update info labels (lightweight)
        genreLabel.setText(tr("Genre:", "Genre:") + " " + AIEngine::getGenreName(ai.getDetectedGenre()), 
                          juce::dontSendNotification);
        profileLabel.setText(tr("Profile:", "Profile:") + " " + AIEngine::getProfileName(ai.getSourceProfile()), 
                            juce::dontSendNotification);
        
        // Sync unmasking button state (only if the toggle is shown — GUI-4)
        if (kMultiTrackUIEnabled)
            updateUnmaskingButton(ai.isMultiTrackUnmaskingEnabled());
        
        bool shouldUpdate = needsUpdate.exchange(false, std::memory_order_acq_rel);
        if (ai.isNewAnalysisAvailable())
        {
            ai.clearNewAnalysisFlag();
            shouldUpdate = true;
        }

        // Detect Strength knob change → refresh gain previews in problem list
        const float curStrength = ai.getStrength();
        if (std::abs(curStrength - lastStrength) > 0.005f)
        {
            lastStrength = curStrength;
            shouldUpdate = true;  // re-render rows with updated gain preview
        }

        // Detect Sensitivity knob change → show brief "Analyzing..." feedback
        const float curSensitivity = ai.getSensitivity();
        if (std::abs(curSensitivity - lastSensitivity) > 0.005f)
        {
            lastSensitivity = curSensitivity;
            statusLabel.setText(tr("Updating analysis...", "Updating analysis..."), juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentYellow);
        }

        if (! transientVisualHolds.empty())
            shouldUpdate = true;

        if (shouldUpdate)
        {
            updateProblemList();
            updateButtons();
        }
    }
    
    void updateUnmaskingButton(bool enabled)
    {
        if (enabled)
        {
            unmaskingBtn.setButtonText("UNMASKING ON");
            unmaskingBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFFE8A030)); // Amber (Liquid Intelligence signature)
            unmaskingBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF181A22)); // Dark text on amber
        }
        else
        {
            unmaskingBtn.setButtonText("UNMASKING");
            unmaskingBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
            unmaskingBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textSecondary);
        }
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
        {
            const auto idx = static_cast<size_t>(rowNumber);
            const bool ghost = idx < problemIsGhost.size() && problemIsGhost[idx];
            rowComp->updateFromProblem(problems[idx], rowNumber, isRowSelected, isRightToLeft(), ghost);
        }

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
                    if (!canApplyNow()) return;
                    if (! isProblemLive(p)) { removeTransientVisualHold(p); updateProblemList(); return; }
                    // Apply ONLY this single correction, not all approved ones
                    processor.applySingleCorrection(p);
                    removeTransientVisualHold(p);
                    updateProblemList();
                } else if (result == 2) {
                    rejectCorrectionByMatch(p);
                    removeTransientVisualHold(p);
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
            if (!canApplyNow()) return;
            const auto& p = problems[row];
            if (! isProblemLive(p)) { removeTransientVisualHold(p); updateProblemList(); return; }
            // Apply ONLY this single correction, not all approved ones
            processor.applySingleCorrection(p);
            removeTransientVisualHold(p);
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
                tr("Selected row: ", "Selected row: ") + getRowAnnouncementFromCorrection(problems[static_cast<size_t>(row)]),
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
                setupLabel(typeLabel, f, ModernLookAndFeel::Colors::textBright);
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(14.0f));
                f.setBold(true);
                setupLabel(freqLabel, f, ModernLookAndFeel::Colors::textBright);
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(9.0f));
                f.setBold(true);
                setupLabel(sevLabel, f, ModernLookAndFeel::Colors::textBright, true);
            }
            setupLabel(explanationLabel, juce::Font(juce::FontOptions().withHeight(11.0f)), ModernLookAndFeel::Colors::textPrimary);
            setupLabel(causeLabel, juce::Font(juce::FontOptions().withHeight(10.0f)), ModernLookAndFeel::Colors::textLabel);
            auto italic = juce::Font(juce::FontOptions().withHeight(10.0f));
            italic.setItalic(true);
            setupLabel(impactLabel, italic, ModernLookAndFeel::Colors::textSecondary);
            {
                auto f = juce::Font(juce::FontOptions().withHeight(11.0f));
                f.setBold(true);
                setupLabel(fixLabel, f, ModernLookAndFeel::Colors::accentGreen);
            }
            {
                auto f = juce::Font(juce::FontOptions().withHeight(9.0f));
                f.setBold(true);
                setupLabel(confidenceLabel, f, juce::Colours::white, true);
            }
            setupLabel(bandLabel, juce::Font(juce::FontOptions().withHeight(9.0f)), ModernLookAndFeel::Colors::textMuted);
            setupLabel(hintLabel, juce::Font(juce::FontOptions().withHeight(10.0f)), ModernLookAndFeel::Colors::accentBlue); // Semantic hint accent
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

        void updateFromProblem(const AIEngine::Correction& p, int rowIndex, bool isSelected, bool rtlFlag, bool isGhost = false)
        {
            currentIndex = rowIndex;
            rtl = rtlFlag;
            selected = isSelected;
            // UX "Diagnosi Stabile": ghost rows (visual exit hold, no longer live in the
            // engine) fade instead of vanishing. Flag pre-computed in updateProblemList().
            setAlpha(isGhost ? 0.45f : 1.0f);
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

            // Scale gain by current Strength for live preview
            const float strength = owner.processor.getAIEngine().getStrength();
            const float scaledGain = p.suggestedGain * strength;
            juce::String action = scaledGain < 0 ? owner.tr("CUT", "CUT") : owner.tr("BOOST", "BOOST");
            juce::String gainStr = (scaledGain > 0 ? "+" : "") + juce::String(scaledGain, 1) + " dB";
            juce::String qStr = owner.tr("Q=", "Q=") + juce::String(p.suggestedQ, 1);
            juce::String filterStr = owner.getFilterTypeName(p.suggestedFilter);
            const auto fixText = owner.tr("FIX:", "FIX:") + " " + action + " " + gainStr + "  " + qStr + "  " + filterStr;
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

            juce::Colour bgCol = selected ? ModernLookAndFeel::Colors::bgPanel
                                          : ModernLookAndFeel::Colors::bgMid;
            g.setColour(bgCol);
            g.fillRoundedRectangle(4.0f, 2.0f, w - 8.0f, h - 4.0f, corner);

            g.setColour(severityColour);
            g.fillRoundedRectangle(barX, 2.0f, barWidth, h - 4.0f, 3.0f);

            // Severity badge (compact: y=4)
            const float badgeX = rtl ? 12.0f : w - badgeW - 12.0f;
            g.setColour(severityColour.withAlpha(0.3f));
            g.fillRoundedRectangle(badgeX, 4.0f, badgeW, 18.0f, 4.0f);
            g.setColour(severityColour);
            g.drawRoundedRectangle(badgeX, 4.0f, badgeW, 18.0f, 4.0f, 1.0f);

            // Bottom separator
            g.setColour(ModernLookAndFeel::Colors::bgLighter);
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

            // Compact 2-row layout (56px row height)
            // Row 1 (y=4): type + freq + severity badge
            // Row 2 (y=28): fix suggestion (one line)
            const int contentW = w - xBase - 10;

            if (! rtl)
            {
                place(typeLabel, xBase, 4, 120, 18);
                place(freqLabel, xBase + 125, 4, 80, 18);
                place(sevLabel, w - badgeW - 12, 4, badgeW, 18);
            }
            else
            {
                place(sevLabel, 12, 4, badgeW, 18);
                place(freqLabel, w - xBase - 80, 4, 80, 18);
                place(typeLabel, w - xBase - 80 - 120, 4, 120, 18);
            }

            place(explanationLabel, xBase, 26, contentW, 16);

            // Hide detailed rows in compact mode — tooltip has full detail
            causeLabel.setVisible(false);
            impactLabel.setVisible(false);
            fixLabel.setBounds(xBase, 42, contentW - 80, 14);
            confidenceLabel.setBounds(w - 90, 42, 65, 14);
            bandLabel.setVisible(false);
            hintLabel.setVisible(false);
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
        if (sev > 0.7f) return ModernLookAndFeel::Colors::accentRed;
        if (sev > 0.4f) return ModernLookAndFeel::Colors::accentOrange;
        return ModernLookAndFeel::Colors::accentGreen;
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
    
    juce::String getFilterTypeName(AIEngine::Correction::FilterType ft) const
    {
        switch (ft)
        {
            case AIEngine::Correction::FilterType::Peak:      return "Peak";
            case AIEngine::Correction::FilterType::LowShelf:  return "Lo Shelf";
            case AIEngine::Correction::FilterType::HighShelf:  return "Hi Shelf";
            case AIEngine::Correction::FilterType::Notch:     return "Notch";
            case AIEngine::Correction::FilterType::LowCut:    return "Lo Cut";
            case AIEngine::Correction::FilterType::HighCut:   return "Hi Cut";
            default:                                          return "Peak";
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
        // Frozen diagnosis mode: display the captured snapshot, ignore live updates
        // (they otherwise overwrite capture results within ~100-200ms of playback).
        auto raw = isFrozen ? frozenProblems : processor.getAIEngine().getPendingCorrections();
        const auto liveProblemCount = raw.size();
        if (! isFrozen)
            mergeTransientVisualHolds(raw);   // appends ghost entries AFTER the live ones

        // Ghost flag computed ONCE here (counter-exam fix A/B: never call isProblemLive()
        // per row — it locks + copies the corrections vector on every visible row).
        // Entries at index >= liveProblemCount were appended by mergeTransientVisualHolds
        // and are therefore no longer live in the engine.
        std::vector<std::pair<AIEngine::Correction, bool>> merged;
        merged.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i)
            merged.emplace_back(raw[i], i >= liveProblemCount);

        // Sort by priority (severity * confidence) for display — ghost flag travels with row
        std::sort(merged.begin(), merged.end(), [](const auto& a, const auto& b) {
            float priorityA = a.first.severity * a.first.confidence;
            float priorityB = b.first.severity * b.first.confidence;
            if (std::abs(priorityA - priorityB) < 0.01f)
                return a.first.severity > b.first.severity;
            return priorityA > priorityB;
        });

        // Limit to top 50 for performance (but show ALL if less than 50)
        if (merged.size() > 50)
            merged.resize(50);

        problems.clear();
        problems.reserve(merged.size());
        problemIsGhost.clear();
        problemIsGhost.reserve(merged.size());
        for (const auto& [corr, ghost] : merged)
        {
            problems.push_back(corr);
            problemIsGhost.push_back(ghost);
        }
        problemList.updateContent();
        if (auto* h = problemList.getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
        const int n = static_cast<int>(problems.size());
        const auto newIssuesMsg = (n == 1)
            ? juce::String::formatted(tr("%d nuovo problema rilevato", "%d new problem detected").toRawUTF8(), n)
            : juce::String::formatted(tr("%d nuovi problemi rilevati", "%d new problems detected").toRawUTF8(), n);
        if (n != lastAnnouncedProblemCount)
        {
            lastAnnouncedProblemCount = n;
            juce::AccessibilityHandler::postAnnouncement(newIssuesMsg, juce::AccessibilityHandler::AnnouncementPriority::high);
        }
        
        if (n == 0)
        {
            statusLabel.setText(juce::String::fromUTF8("\xe2\x96\xb6 ") + tr("Play audio to start analysis", "Play audio to start analysis"), juce::dontSendNotification);
        }
        else
        {
            auto fmt = (n == 1) ? tr("%d issue detected", "%d issue detected")
                                : tr("%d issues detected", "%d issues detected");
            statusLabel.setText(juce::String::formatted(fmt.toRawUTF8(), n), juce::dontSendNotification);
        }
        
        // Finding #2 (counter-check): frozen diagnosis is read-only. FIX/FIX ALL would
        // route through the isProblemLive() guards, which compare against the LIVE
        // engine state — not the frozen snapshot on screen — so the action could be a
        // silent no-op or apply to data different from what is displayed. Disable it.
        autoFixBtn.setEnabled(! isFrozen && liveProblemCount > 0);
    }

    static bool shouldHoldVisually(const AIEngine::Correction&) noexcept
    {
        // UX "Diagnosi Stabile": every problem type benefits from the visual exit hold
        // (was Sibilance-only). Safety guards (isProblemLive before apply) are unchanged,
        // so FIX on a ghost that vanished from the engine remains a safe no-op.
        return true;
    }

    static bool isSameDisplayedProblem(const AIEngine::Correction& a,
                                       const AIEngine::Correction& b) noexcept
    {
        if (a.type != b.type)
            return false;

        const float freqRatio = std::abs(std::log2(a.frequency / juce::jmax(20.0f, b.frequency)));
        const bool freqMatch = freqRatio < 0.05f;
        const bool gainMatch = std::abs(a.suggestedGain - b.suggestedGain) < 0.5f;
        return freqMatch && gainMatch;
    }

    void mergeTransientVisualHolds(std::vector<AIEngine::Correction>& raw)
    {
        const auto now = juce::Time::currentTimeMillis();

        for (const auto& c : raw)
        {
            if (! shouldHoldVisually(c))
                continue;

            auto it = std::find_if(transientVisualHolds.begin(), transientVisualHolds.end(),
                [&c](const TransientVisualHold& hold) { return isSameDisplayedProblem(hold.correction, c); });

            if (it != transientVisualHolds.end())
            {
                it->correction = c;
                it->expiresAtMs = now + kTransientVisualHoldMs;
            }
            else
            {
                transientVisualHolds.push_back({ c, now + kTransientVisualHoldMs });
            }
        }

        transientVisualHolds.erase(
            std::remove_if(transientVisualHolds.begin(), transientVisualHolds.end(),
                [now](const TransientVisualHold& hold) { return hold.expiresAtMs <= now; }),
            transientVisualHolds.end());

        for (const auto& hold : transientVisualHolds)
        {
            const auto alreadyLive = std::any_of(raw.begin(), raw.end(),
                [&hold](const AIEngine::Correction& c) { return isSameDisplayedProblem(c, hold.correction); });

            if (! alreadyLive)
                raw.push_back(hold.correction);
        }
    }

    void removeTransientVisualHold(const AIEngine::Correction& target)
    {
        transientVisualHolds.erase(
            std::remove_if(transientVisualHolds.begin(), transientVisualHolds.end(),
                [&target](const TransientVisualHold& hold) { return isSameDisplayedProblem(hold.correction, target); }),
            transientVisualHolds.end());
    }

    bool isProblemLive(const AIEngine::Correction& target) const
    {
        const auto pending = processor.getAIEngine().getPendingCorrections();
        return std::any_of(pending.begin(), pending.end(),
            [&target](const AIEngine::Correction& p) { return isSameDisplayedProblem(p, target); });
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
        auto actionableProblems = processor.getAIEngine().getPendingCorrections();
        if (actionableProblems.empty()) return;
        if (fixAllInProgress) return;
        fixAllInProgress = true;
        autoFixBtn.setEnabled(false);

        juce::String msg = tr("Apply ", "Apply ") + juce::String(actionableProblems.size()) + tr(" corrections?\n\n", " corrections?\n\n");

        for (size_t i = 0; i < juce::jmin(actionableProblems.size(), size_t(5)); ++i)
        {
            const auto& p = actionableProblems[i];
            msg += "• " + AIEngine::getProblemTypeName(p.type) + " @ " + formatFreq(p.frequency);
            msg += " → " + juce::String(p.suggestedGain, 1) + " dB\n";
        }

        if (actionableProblems.size() > 5)
            msg += "\n" + tr("...and ", "...and ") + juce::String(actionableProblems.size() - 5) + tr(" more\n", " more\n");

        msg += "\n" + tr("You can UNDO these changes.", "You can UNDO these changes.");

        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
            tr("Apply AI Corrections", "Apply AI Corrections"), msg, tr("Apply", "Apply"), tr("Cancel", "Cancel"), nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                fixAllInProgress = false;
                // Finding #2: do not re-enable unconditionally — updateProblemList()
                // re-applies the frozen-aware enable logic
                // ("autoFixBtn.setEnabled(!isFrozen && liveProblemCount > 0)").
                updateProblemList();
                if (result == 1) {
                    if (!canApplyNow()) { updateProblemList(); return; }
                    processor.getAIEngine().approveAllCorrections();
                    processor.applyAICorrections();
                    transientVisualHolds.clear();
                    updateProblemList();
                    juce::AccessibilityHandler::postAnnouncement(
                        tr("Corrections applied", "Corrections applied"),
                        juce::AccessibilityHandler::AnnouncementPriority::high);
                } else {
                    juce::AccessibilityHandler::postAnnouncement(
                        tr("Correction application cancelled", "Correction application cancelled"),
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
                tr("Selected row: ", "Selected row: ") + getRowAnnouncementFromCorrection(problems[static_cast<size_t>(row)]),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
        }
    }

    void activateRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return;
        if (!canApplyNow()) return;

        const auto& p = problems[row];
        if (! isProblemLive(p)) { removeTransientVisualHold(p); updateProblemList(); return; }
        // Apply ONLY this single correction, not all approved ones
        processor.applySingleCorrection(p);
        removeTransientVisualHold(p);
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
            const bool freqMatch = freqRatio < 0.05f;  // Within ~5%
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
            const bool freqMatch = freqRatio < 0.05f;  // Within ~5%
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
    juce::TextButton unmaskingBtn;  // Multi-Track Unmasking toggle
    // GUI-4: the multi-track unmasking feature is DISABLED (requires multi-instance
    // host support; MultiTrackUnmasking has no single-instance effect). The toggle
    // is not shown. Flip to true only when the feature actually works end-to-end.
    static constexpr bool kMultiTrackUIEnabled = false;
    
    std::vector<AIEngine::Correction> problems;
    std::vector<bool> problemIsGhost;        // parallel to `problems`: true = exit-hold ghost

    // UX "Diagnosi Stabile" — capture strip + frozen diagnosis mode.
    // Message-thread-only state (timer + clicks): no atomics needed.
    juce::TextButton captureStripBtn;
    juce::Label captureStripLabel;
    bool isFrozen { false };                 // panel shows frozenProblems, ignores live updates
    bool captureAwaitingResult { false };    // between STOP+ANALYZE and async results arrival
    std::vector<AIEngine::Correction> frozenProblems;
    struct TransientVisualHold
    {
        AIEngine::Correction correction;
        juce::int64 expiresAtMs = 0;
    };
    // UX "Diagnosi Stabile": exit hold-time so problems fade out instead of vanishing
    // the instant the engine drops them (engine persistence is asymmetric: slow-in ~0.8s,
    // instant-out). Engine gates (60%/8-frame) are UNTOUCHED — AIAccuracyTest depends on them.
    static constexpr juce::int64 kTransientVisualHoldMs = 3500;
    std::vector<TransientVisualHold> transientVisualHolds;
    std::atomic<bool> needsUpdate { true };
    juce::int64 lastApplyTimeMs { 0 };      // throttle rapid apply clicks (300ms window)
    bool fixAllInProgress { false };         // guard for FIX ALL re-entry
    float lastStrength { -1.0f };            // detect strength knob changes for live gain preview
    float lastSensitivity { -1.0f };         // detect sensitivity changes for re-analyze feedback
    int lastAnnouncedProblemCount { -1 };

    /** UX "Diagnosi Stabile": capture strip click handler (start / stop+analyze / unfreeze). */
    void onCaptureStripClicked()
    {
        if (isFrozen)
        {
            // BACK TO LIVE: unfreeze and resume live updates
            isFrozen = false;
            frozenProblems.clear();
            captureStripBtn.setButtonText(tr("CAPTURE", "CAPTURE"));
            captureStripBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::accentBlue);
            captureStripLabel.setText("", juce::dontSendNotification);
            captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
            clearBtn.setEnabled(true);
            juce::AccessibilityHandler::postAnnouncement(
                tr("Tornato all'analisi live", "Back to live analysis"),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
            updateProblemList();      // re-applies frozen-aware FIX ALL enable state now
            refreshFromProcessor();   // and schedule a fresh live refresh on next tick
            return;
        }

        if (! processor.isCapturing())
        {
            // START: lock-free CAS in CaptureService — safe from the message thread
            if (processor.startManualCapture())
            {
                captureStripBtn.setButtonText(tr("STOP + ANALYZE", "STOP + ANALYZE"));
                captureStripLabel.setText(tr("Recording...", "Recording..."), juce::dontSendNotification);
                captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentYellow);
            }
            return;
        }

        // STOP + ANALYZE: async analysis on the processor's dedicated thread;
        // completion posts aiProblemsChanged → freeze trigger in timerCallback().
        processor.stopManualCapture();
        if (processor.analyzeCapturedAudioSnapshot())
        {
            captureAwaitingResult = true;
            captureStripBtn.setEnabled(false);
            captureStripLabel.setText(tr("Analyzing...", "Analyzing..."), juce::dontSendNotification);
            captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentYellow);
        }
        else
        {
            captureStripBtn.setButtonText(tr("CAPTURE", "CAPTURE"));
            captureStripLabel.setText(tr("Capture too short — try again", "Capture too short — try again"),
                                      juce::dontSendNotification);
            captureStripLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        }
    }

    /** Returns true if applying is currently allowed: not in frozen-diagnosis mode
        and enough time has passed since the last apply (300ms debounce).
        Finding #2 (counter-check): while frozen, the list shows the capture snapshot
        but the apply guards (isProblemLive) compare against the LIVE engine state —
        applying could silently no-op or act on data different from what is shown.
        All apply paths (single FIX, double-click, keyboard activate, FIX ALL) funnel
        through this gate, so blocking here makes frozen mode consistently read-only. */
    bool canApplyNow()
    {
        if (isFrozen)
        {
            juce::AccessibilityHandler::postAnnouncement(
                tr("Diagnosi congelata: torna a LIVE per applicare le correzioni",
                   "Diagnosis frozen: go back to LIVE to apply fixes"),
                juce::AccessibilityHandler::AnnouncementPriority::medium);
            return false;
        }
        const juce::int64 now = juce::Time::currentTimeMillis();
        if (now - lastApplyTimeMs < 300)
            return false;
        lastApplyTimeMs = now;
        return true;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIProblemPanel)
};
