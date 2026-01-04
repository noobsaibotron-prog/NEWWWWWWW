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
    //==========================================================================
    // CALLBACK: Called when user clicks a problem - use to highlight on spectrum
    //==========================================================================
    std::function<void(float frequency, float q, float severity, AIEngine::ProblemType type)> onProblemSelected;

    explicit AIProblemPanel(AIEqualizerAudioProcessor& p) : processor(p)
    {
        // Header
        titleLabel.setText("AI ANALYSIS", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(15.0f));
            font.setBold(true);
            titleLabel.setFont(font);
        }
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4A9FD9));
        addAndMakeVisible(titleLabel);
        
        // Info labels
        genreLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        genreLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        addAndMakeVisible(genreLabel);
        
        profileLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        profileLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF666666));
        addAndMakeVisible(profileLabel);
        
        // Problem list with custom row height
        problemList.setModel(this);
        problemList.setRowHeight(140);  // Tall rows for detailed info
        problemList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF1A1A1A));
        problemList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF333333));
        problemList.setOutlineThickness(1);
        addAndMakeVisible(problemList);
        
        // Action buttons
        autoFixBtn.setButtonText("FIX ALL");
        autoFixBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
        autoFixBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3D7A37));
        autoFixBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        autoFixBtn.setTooltip("Apply all suggested fixes (with confirmation)");
        autoFixBtn.onClick = [this]() { showAutoFixConfirmation(); };
        addAndMakeVisible(autoFixBtn);
        
        clearBtn.setButtonText("CLEAR");
        clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        clearBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
        clearBtn.onClick = [this]() { 
            processor.getAIEngine().clearCorrections();
            updateProblemList();
        };
        addAndMakeVisible(clearBtn);
        
        undoBtn.setButtonText("UNDO");
        undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        undoBtn.onClick = [this]() { processor.undo(); updateButtons(); };
        addAndMakeVisible(undoBtn);
        
        redoBtn.setButtonText("REDO");
        redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        redoBtn.onClick = [this]() { processor.redo(); updateButtons(); };
        addAndMakeVisible(redoBtn);
        
        // Status
        statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF777777));
        statusLabel.setJustificationType(juce::Justification::centred);
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
        
        // Title area
        auto titleRow = bounds.removeFromTop(24);
        titleLabel.setBounds(titleRow.removeFromLeft(120));
        genreLabel.setBounds(titleRow);
        
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
        genreLabel.setText("Genre: " + AIEngine::getGenreName(ai.getDetectedGenre()), 
                          juce::dontSendNotification);
        profileLabel.setText("Profile: " + AIEngine::getProfileName(ai.getSourceProfile()), 
                            juce::dontSendNotification);
        
        if (ai.isNewAnalysisAvailable())
        {
            ai.clearNewAnalysisFlag();
            updateProblemList();
        }
        
        updateButtons();
    }
    
    //==========================================================================
    // ListBoxModel Implementation
    //==========================================================================
    
    int getNumRows() override { return static_cast<int>(problems.size()); }
    
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        if (row < 0 || row >= static_cast<int>(problems.size()))
            return;
        
        const auto& p = problems[row];
        
        // Card background
        juce::Colour bgCol = isSelected ? juce::Colour(0xFF2A3040) : juce::Colour(0xFF222222);
        g.setColour(bgCol);
        g.fillRoundedRectangle(4.0f, 2.0f, width - 8.0f, height - 4.0f, 6.0f);
        
        // Left severity bar
        juce::Colour sevCol = getSeverityColor(p.severity);
        g.setColour(sevCol);
        g.fillRoundedRectangle(4.0f, 2.0f, 6.0f, height - 4.0f, 3.0f);
        
        int x = 16;  // Left margin after severity bar
        int contentW = width - x - 10;
        
        // === LINE 1: Type + Frequency + Severity Badge ===
        // Problem type (large, colored)
        g.setColour(sevCol);
        auto fontTitle = juce::Font(juce::FontOptions().withHeight(13.0f));
        fontTitle.setBold(true);
        g.setFont(fontTitle);
        juce::String typeName = AIEngine::getProblemTypeName(p.type).toUpperCase();
        g.drawText(typeName, x, 8, 120, 18, juce::Justification::left);
        
        // Frequency (prominent)
        g.setColour(juce::Colours::white);
        auto fontHeading = juce::Font(juce::FontOptions().withHeight(14.0f));
        fontHeading.setBold(true);
        g.setFont(fontHeading);
        juce::String freqStr = formatFreq(p.frequency);
        g.drawText(freqStr, x + 125, 8, 80, 18, juce::Justification::left);
        
        // Severity badge (right side)
        juce::String sevLabel = p.severity > 0.7f ? "CRITICAL" : (p.severity > 0.4f ? "MODERATE" : "MINOR");
        int badgeW = 70;
        g.setColour(sevCol.withAlpha(0.3f));
        g.fillRoundedRectangle(static_cast<float>(width - badgeW - 12), 8.0f, static_cast<float>(badgeW), 18.0f, 4.0f);
        g.setColour(sevCol);
        auto fontBadge = juce::Font(juce::FontOptions().withHeight(9.0f));
        fontBadge.setBold(true);
        g.setFont(fontBadge);
        g.drawText(sevLabel, width - badgeW - 12, 8, badgeW, 18, juce::Justification::centred);
        
        // === LINE 2: Problem explanation ===
        g.setColour(juce::Colour(0xFFDDDDDD));
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        juce::String explanation = getExplanation(p);
        g.drawText(explanation, x, 30, contentW, 16, juce::Justification::left);
        
        // === LINE 3: Cause ===
        g.setColour(juce::Colour(0xFF999999));
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.drawText("CAUSE: " + getCause(p.type), x, 48, contentW, 14, juce::Justification::left);
        
        // === LINE 4: Sound impact ===
        g.setColour(juce::Colour(0xFF888888));
        auto fontItalic = juce::Font(juce::FontOptions().withHeight(10.0f));
        fontItalic.setItalic(true);
        g.setFont(fontItalic);
        g.drawText("SOUNDS: " + getImpact(p.type), x, 64, contentW, 14, juce::Justification::left);
        
        // === LINE 5: Fix suggestion box ===
        float fixBoxY = 82.0f;
        g.setColour(juce::Colour(0xFF1A2A1A));
        g.fillRoundedRectangle(static_cast<float>(x), fixBoxY, contentW - 80.0f, 26.0f, 4.0f);
        
        // Fix details
        juce::String action = p.suggestedGain < 0 ? "CUT" : "BOOST";
        juce::String gainStr = (p.suggestedGain > 0 ? "+" : "") + juce::String(p.suggestedGain, 1) + " dB";
        juce::String qStr = "Q=" + juce::String(p.suggestedQ, 1);
        
        juce::Colour actionCol = p.suggestedGain < 0 ? juce::Colour(0xFFFF6666) : juce::Colour(0xFF66FF66);
        
        g.setColour(juce::Colour(0xFF448844));
        auto fontFooter = juce::Font(juce::FontOptions().withHeight(9.0f));
        fontFooter.setBold(true);
        g.setFont(fontFooter);
        g.drawText("FIX:", x + 6, static_cast<int>(fixBoxY) + 2, 30, 22, juce::Justification::left);
        
        g.setColour(actionCol);
        auto fontBtn = juce::Font(juce::FontOptions().withHeight(11.0f));
        fontBtn.setBold(true);
        g.setFont(fontBtn);
        g.drawText(action + " " + gainStr + "  " + qStr, x + 38, static_cast<int>(fixBoxY) + 2, 180, 22, juce::Justification::left);
        
        // Confidence meter
        float confX = static_cast<float>(width - 90);
        float confW = 65.0f;
        g.setColour(juce::Colour(0xFF333333));
        g.fillRoundedRectangle(confX, fixBoxY + 6.0f, confW, 14.0f, 4.0f);
        
        juce::Colour confCol = p.confidence > 0.7f ? juce::Colour(0xFF44AA44) 
                             : (p.confidence > 0.4f ? juce::Colour(0xFFAAAA44) : juce::Colour(0xFFAA4444));
        g.setColour(confCol);
        g.fillRoundedRectangle(confX, fixBoxY + 6.0f, confW * p.confidence, 14.0f, 4.0f);
        
        g.setColour(juce::Colours::white);
        auto fontItem = juce::Font(juce::FontOptions().withHeight(9.0f));
        fontItem.setBold(true);
        g.setFont(fontItem);
        g.drawText(juce::String(static_cast<int>(p.confidence * 100)) + "%", 
                   static_cast<int>(confX), static_cast<int>(fixBoxY) + 6, static_cast<int>(confW), 14, 
                   juce::Justification::centred);
        
        // === LINE 6: Band info ===
        g.setColour(juce::Colour(0xFF666666));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        juce::String bandName = AIEngine::getBandName(p.frequency);
        juce::String bwDesc = p.suggestedQ > 5.0f ? "Narrow surgical cut" 
                            : (p.suggestedQ > 2.0f ? "Focused correction" : "Wide musical adjustment");
        g.drawText("Region: " + bandName + "  •  " + bwDesc, x, 112, contentW, 14, juce::Justification::left);
        
        // Click hint if selected
        if (isSelected)
        {
            g.setColour(juce::Colour(0xFF4A9FD9).withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
            g.drawText("Click to highlight on spectrum • Double-click to apply fix", 
                       x, 126, contentW, 12, juce::Justification::left);
        }
        
        // Bottom separator
        g.setColour(juce::Colour(0xFF333333));
        g.drawHorizontalLine(height - 2, 10.0f, static_cast<float>(width - 10));
    }
    
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
            menu.addItem(1, "Apply This Fix");
            menu.addItem(2, "Dismiss Problem");
            menu.addSeparator();
            menu.addItem(3, "Show Full Analysis...");
            
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result) {
                if (result == 1) {
                    processor.getAIEngine().approveCorrection(row);
                    processor.applyAICorrections();
                    updateProblemList();
                } else if (result == 2) {
                    processor.getAIEngine().rejectCorrection(row);
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
            processor.getAIEngine().approveCorrection(row);
            processor.applyAICorrections();
            updateProblemList();
        }
    }
    
    void selectedRowsChanged(int) override {}

private:
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
    
    juce::String getExplanation(const AIEngine::Correction& p) const
    {
        float gain = std::abs(p.suggestedGain);
        juce::String freqStr = formatFreq(p.frequency);
        
        switch (p.type)
        {
            case AIEngine::ProblemType::Resonance:
                return "Sharp resonant peak +" + juce::String(gain, 1) + " dB above surrounding frequencies";
            case AIEngine::ProblemType::Harshness:
                return "Excessive upper-mid energy causing listener fatigue at " + freqStr;
            case AIEngine::ProblemType::Muddiness:
                return "Low-mid buildup masking clarity and detail around " + freqStr;
            case AIEngine::ProblemType::Boxyness:
                return "Room/cabinet resonance creating hollow coloration at " + freqStr;
            case AIEngine::ProblemType::Sibilance:
                return "Harsh sibilant frequencies (S/T/F sounds) peaking at " + freqStr;
            case AIEngine::ProblemType::LowEndBoom:
                return "Excessive sub-bass energy causing rumble and masking at " + freqStr;
            case AIEngine::ProblemType::ThinSound:
                return "Deficient low-mid body - sound lacks warmth around " + freqStr;
            case AIEngine::ProblemType::DullSound:
                return "Missing high-frequency air and sparkle above " + freqStr;
            default:
                return "Audio issue detected at " + freqStr;
        }
    }
    
    juce::String getCause(AIEngine::ProblemType type) const
    {
        switch (type)
        {
            case AIEngine::ProblemType::Resonance:
                return "Room mode, mic resonance, or instrument characteristic";
            case AIEngine::ProblemType::Harshness:
                return "Overdriven preamp, bright mic, or aggressive compression";
            case AIEngine::ProblemType::Muddiness:
                return "Proximity effect, room reflections, stacked instruments";
            case AIEngine::ProblemType::Boxyness:
                return "Small room recording or cabinet resonance";
            case AIEngine::ProblemType::Sibilance:
                return "Close mic technique or vocalist characteristics";
            case AIEngine::ProblemType::LowEndBoom:
                return "Room bass buildup or uncontrolled low frequencies";
            case AIEngine::ProblemType::ThinSound:
                return "High-pass too aggressive or phase cancellation";
            case AIEngine::ProblemType::DullSound:
                return "Low-pass filtering or absorption in recording";
            default:
                return "Multiple possible causes";
        }
    }
    
    juce::String getImpact(AIEngine::ProblemType type) const
    {
        switch (type)
        {
            case AIEngine::ProblemType::Resonance:
                return "Ringing, feedback-prone, fatiguing";
            case AIEngine::ProblemType::Harshness:
                return "Ear fatigue, painful at volume";
            case AIEngine::ProblemType::Muddiness:
                return "Unclear, obscured detail";
            case AIEngine::ProblemType::Boxyness:
                return "Cheap, amateur sound quality";
            case AIEngine::ProblemType::Sibilance:
                return "Distracting, piercing consonants";
            case AIEngine::ProblemType::LowEndBoom:
                return "Muddy bass, wasted headroom";
            case AIEngine::ProblemType::ThinSound:
                return "Weak, lacks emotional weight";
            case AIEngine::ProblemType::DullSound:
                return "Lifeless, poor translation";
            default:
                return "May affect mix quality";
        }
    }
    
    void showFullAnalysis(int row)
    {
        if (row < 0 || row >= static_cast<int>(problems.size())) return;
        const auto& p = problems[row];
        
        juce::String msg;
        msg += "PROBLEM TYPE: " + AIEngine::getProblemTypeName(p.type) + "\n";
        msg += "FREQUENCY: " + formatFreq(p.frequency) + "\n";
        msg += "REGION: " + AIEngine::getBandName(p.frequency) + "\n\n";
        msg += "SEVERITY: " + juce::String(static_cast<int>(p.severity * 100)) + "%\n";
        msg += "CONFIDENCE: " + juce::String(static_cast<int>(p.confidence * 100)) + "%\n\n";
        msg += "EXPLANATION:\n" + getExplanation(p) + "\n\n";
        msg += "PROBABLE CAUSE:\n" + getCause(p.type) + "\n\n";
        msg += "SOUND IMPACT:\n" + getImpact(p.type) + "\n\n";
        msg += "SUGGESTED FIX:\n";
        msg += "  Gain: " + juce::String(p.suggestedGain, 1) + " dB\n";
        msg += "  Q: " + juce::String(p.suggestedQ, 1) + "\n";
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Full Analysis: " + AIEngine::getProblemTypeName(p.type), msg, "OK");
    }
    
    void updateProblemList()
    {
        problems = processor.getAIEngine().getPendingCorrections();
        problemList.updateContent();
        
        int n = static_cast<int>(problems.size());
        if (n == 0)
            statusLabel.setText("No issues detected", juce::dontSendNotification);
        else
            statusLabel.setText(juce::String(n) + (n == 1 ? " issue" : " issues") + " detected", 
                               juce::dontSendNotification);
        
        autoFixBtn.setEnabled(n > 0);
    }
    
    void updateButtons()
    {
        undoBtn.setEnabled(processor.canUndo());
        redoBtn.setEnabled(processor.canRedo());
        
        if (processor.canUndo())
            undoBtn.setTooltip("Undo: " + processor.getUndoDescription());
        if (processor.canRedo())
            redoBtn.setTooltip("Redo: " + processor.getRedoDescription());
    }
    
    void showAutoFixConfirmation()
    {
        if (problems.empty()) return;
        
        juce::String msg = "Apply " + juce::String(problems.size()) + " corrections?\n\n";
        
        for (size_t i = 0; i < juce::jmin(problems.size(), size_t(5)); ++i)
        {
            const auto& p = problems[i];
            msg += "• " + AIEngine::getProblemTypeName(p.type) + " @ " + formatFreq(p.frequency);
            msg += " → " + juce::String(p.suggestedGain, 1) + " dB\n";
        }
        
        if (problems.size() > 5)
            msg += "\n...and " + juce::String(problems.size() - 5) + " more\n";
        
        msg += "\nYou can UNDO these changes.";
        
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
            "Apply AI Corrections", msg, "Apply", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1) {
                    processor.getAIEngine().approveAllCorrections();
                    processor.applyAICorrections();
                    updateProblemList();
                }
            }));
    }
    
    //==========================================================================
    AIEqualizerAudioProcessor& processor;
    
    juce::Label titleLabel, genreLabel, profileLabel, statusLabel;
    juce::ListBox problemList;
    juce::TextButton autoFixBtn, clearBtn, undoBtn, redoBtn;
    
    std::vector<AIEngine::Correction> problems;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIProblemPanel)
};
