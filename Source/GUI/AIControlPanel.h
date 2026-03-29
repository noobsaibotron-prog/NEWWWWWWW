#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * TDR Nova Style AI Control Panel
 * 
 * Pannello di controllo per il motore AI dell'equalizzatore.
 * Fornisce interfaccia utente per:
 * - Abilitare/disabilitare l'AI
 * - Regolare sensibilità e forza delle correzioni
 * - Visualizzare il genere rilevato
 * - Mostrare e gestire i problemi audio rilevati
 * 
 * Design ispirato a TDR Nova con sezioni organizzate, knob metallici e lista problemi.
 */
class AIControlPanel : public juce::Component, public juce::Timer
{
public:
    /**
     * Costruttore: inizializza tutti i componenti UI e le loro callback
     * @param p Riferimento al processore audio principale
     */
    explicit AIControlPanel(AIEqualizerAudioProcessor& p) : processor(p)
    {
        // === SEZIONE AI: Header e controlli principali ===
        // Etichetta principale "AI ASSISTANT"
        aiLabel.setText("AI ASSISTANT", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(12.0f));
            font.setBold(true);
            aiLabel.setFont(font);
        }
        aiLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textLabel);
        addAndMakeVisible(aiLabel);
        
        // Toggle per abilitare/disabilitare l'AI (default: ON)
        aiToggle.setButtonText("ON");
        aiToggle.setToggleState(true, juce::dontSendNotification);
        aiToggle.onClick = [this]() {
            processor.getAIEngine().setEnabled(aiToggle.getToggleState());
        };
        addAndMakeVisible(aiToggle);
        
        // Pulsante per applicare automaticamente tutte le correzioni suggerite
        autoFixBtn.setButtonText("AUTO FIX");
        autoFixBtn.onClick = [this]() {
            processor.getAIEngine().approveAllCorrections();
            processor.applyAICorrections();
        };
        addAndMakeVisible(autoFixBtn);
        
        // Pulsante per cancellare tutte le correzioni pendenti
        clearBtn.setButtonText("CLEAR");
        clearBtn.onClick = [this]() { processor.getAIEngine().clearCorrections(); };
        addAndMakeVisible(clearBtn);
        
        // === MULTI-TRACK UNMASKING: Toggle per abilitare unmasking multi-traccia ===
        unmaskingLabel.setText("MULTI-TRACK", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(9.0f));
            unmaskingLabel.setFont(font);
        }
        unmaskingLabel.setJustificationType(juce::Justification::centred);
        unmaskingLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(unmaskingLabel);
        
        // Toggle per abilitare/disabilitare Multi-Track Unmasking
        unmaskingToggle.setButtonText("UNMASKING");
        unmaskingToggle.setToggleState(false, juce::dontSendNotification);  // Default: OFF
        unmaskingToggle.setTooltip("Analyze and correct frequency masking between multiple tracks");
        unmaskingToggle.onClick = [this]() {
            processor.getAIEngine().setMultiTrackUnmaskingEnabled(unmaskingToggle.getToggleState());
            if (unmaskingToggle.getToggleState())
            {
                unmaskingToggle.setButtonText("UNMASKING ON");
                unmaskingToggle.setColour(juce::TextButton::buttonColourId, 
                                         ModernLookAndFeel::Colors::accentGreen);
            }
            else
            {
                unmaskingToggle.setButtonText("UNMASKING");
                unmaskingToggle.setColour(juce::TextButton::buttonColourId, 
                                         ModernLookAndFeel::Colors::bgLighter);
            }
        };
        addAndMakeVisible(unmaskingToggle);
        
        // === SENSITIVITÀ: Controllo della sensibilità del rilevamento ===
        // Etichetta per il knob di sensibilità
        sensitivityLabel.setText("SENSITIVITY", juce::dontSendNotification);
        sensitivityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        sensitivityLabel.setJustificationType(juce::Justification::centred);
        sensitivityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(sensitivityLabel);
        
        // Knob rotativo per regolare la sensibilità (0-100%, default: 50%)
        // La sensibilità determina quanto l'AI è "attenta" nel rilevare problemi
        sensitivityKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sensitivityKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        sensitivityKnob.setRange(0, 100, 1);
        sensitivityKnob.setValue(50);
        sensitivityKnob.setTextValueSuffix("%");
        sensitivityKnob.onValueChange = [this]() {
            processor.getAIEngine().setSensitivity((float)sensitivityKnob.getValue() / 100.0f);
            updateSensitivityDesc();
        };
        addAndMakeVisible(sensitivityKnob);

        sensitivityDesc.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
        sensitivityDesc.setJustificationType(juce::Justification::centred);
        sensitivityDesc.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(sensitivityDesc);
        updateSensitivityDesc();
        
        // === FORZA: Controllo dell'intensità delle correzioni ===
        // Etichetta per il knob di forza
        strengthLabel.setText("STRENGTH", juce::dontSendNotification);
        strengthLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        strengthLabel.setJustificationType(juce::Justification::centred);
        strengthLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(strengthLabel);
        
        // Knob rotativo per regolare la forza delle correzioni (0-100%, default: 70%)
        // La forza determina quanto aggressive sono le correzioni applicate
        strengthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        strengthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        strengthKnob.setRange(0, 100, 1);
        strengthKnob.setValue(70);
        strengthKnob.setTextValueSuffix("%");
        strengthKnob.onValueChange = [this]() {
            processor.getAIEngine().setStrength((float)strengthKnob.getValue() / 100.0f);
            updateStrengthDesc();
        };
        addAndMakeVisible(strengthKnob);

        strengthDesc.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
        strengthDesc.setJustificationType(juce::Justification::centred);
        strengthDesc.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(strengthDesc);
        updateStrengthDesc();
        
        // === GENERE: Visualizzazione del genere musicale rilevato ===
        // Etichetta "DETECTED" per il genere
        genreLabel.setText("DETECTED", juce::dontSendNotification);
        genreLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        genreLabel.setJustificationType(juce::Justification::centred);
        genreLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(genreLabel);
        
        // Label che mostra il nome del genere rilevato (aggiornato dal timer)
        genreValue.setText("--", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(11.0f));
            font.setBold(true);
            genreValue.setFont(font);
        }
        genreValue.setJustificationType(juce::Justification::centred);
        genreValue.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentBlue);
        addAndMakeVisible(genreValue);
        
        // === PROBLEMI: Sezione per visualizzare i problemi rilevati ===
        // Etichetta header per la lista dei problemi
        problemsLabel.setText("DETECTED PROBLEMS", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(10.0f));
            font.setBold(true);
            problemsLabel.setFont(font);
        }
        problemsLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textLabel);
        addAndMakeVisible(problemsLabel);
        
        // Avvia timer a 10 Hz per aggiornare il genere e ridisegnare i problemi
        startTimerHz(10);
    }
    
    /**
     * Distruttore: ferma il timer per evitare callback dopo la distruzione
     */
    ~AIControlPanel() override { stopTimer(); }

    /**
     * Metodo paint: disegna lo sfondo, i bordi e i divisori delle sezioni
     * Chiamato automaticamente da JUCE quando il componente deve essere ridisegnato
     */
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Sfondo principale del pannello con angoli arrotondati
        g.setColour(ModernLookAndFeel::Colors::bgPanel);
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Bordo sottile attorno al pannello
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
        
        // Divisori orizzontali tra le sezioni (knob section e problems section)
        g.setColour(ModernLookAndFeel::Colors::bgLighter.withAlpha(0.5f));
        g.drawHorizontalLine(knobSectionY - 5, 10, bounds.getWidth() - 10);
        g.drawHorizontalLine(problemsSectionY - 5, 10, bounds.getWidth() - 10);
        
        // Disegna la lista dei problemi rilevati
        drawProblems(g);
    }

    /**
     * Metodo resized: calcola e posiziona tutti i componenti quando il pannello viene ridimensionato
     * Usa un sistema di layout a "rimozione" dove si rimuovono porzioni di bounds per posizionare i componenti
     */
    void resized() override
    {
        auto b = getLocalBounds().reduced(10); // Bounds con margine di 10px
        
        // === HEADER AI: Etichetta e toggle ===
        auto headerRow = b.removeFromTop(24);
        aiLabel.setBounds(headerRow.removeFromLeft(100));
        aiToggle.setBounds(headerRow.removeFromRight(40).reduced(2));
        b.removeFromTop(6); // Spazio verticale
        
        // === PULSANTI: AUTO FIX e CLEAR ===
        auto btnRow = b.removeFromTop(26);
        int btnW = (btnRow.getWidth() - 6) / 2; // Larghezza pulsanti (con 6px di spazio tra loro)
        autoFixBtn.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(6); // Spazio tra i pulsanti
        clearBtn.setBounds(btnRow);
        b.removeFromTop(10); // Spazio verticale
        
        // Salva la posizione Y della sezione knob per il divisore
        knobSectionY = b.getY();
        
        // === SEZIONE KNOB: Sensibilità, Forza e Genere ===
        auto knobRow = b.removeFromTop(90);
        int knobW = knobRow.getWidth() / 3; // Ogni knob occupa 1/3 della larghezza
        
        // Knob Sensibilità (sinistra)
        auto sensArea = knobRow.removeFromLeft(knobW);
        sensitivityLabel.setBounds(sensArea.removeFromTop(14));
        sensitivityDesc.setBounds(sensArea.removeFromBottom(12));
        sensitivityKnob.setBounds(sensArea.reduced(4, 0));

        // Knob Forza (centro)
        auto strArea = knobRow.removeFromLeft(knobW);
        strengthLabel.setBounds(strArea.removeFromTop(14));
        strengthDesc.setBounds(strArea.removeFromBottom(12));
        strengthKnob.setBounds(strArea.reduced(4, 0));
        
        // Genere rilevato (destra)
        auto genreArea = knobRow;
        genreLabel.setBounds(genreArea.removeFromTop(14));
        genreValue.setBounds(genreArea.removeFromTop(50).reduced(0, 15));
        
        b.removeFromTop(10); // Spazio verticale
        problemsSectionY = b.getY(); // Salva posizione per il divisore
        
        // === SEZIONE PROBLEMI: Header e area lista ===
        problemsLabel.setBounds(b.removeFromTop(18));
        b.removeFromTop(6); // Spazio verticale
        
        // L'area rimanente viene usata per disegnare la lista dei problemi
        problemsArea = b;
    }

    /**
     * Callback del timer: aggiorna periodicamente il genere rilevato e ridisegna il componente
     * Chiamato a 10 Hz (10 volte al secondo) per mantenere l'interfaccia sincronizzata con l'AI
     */
    void timerCallback() override
    {
        // SAFETY: Skip if processor not ready
        if (!processor.isProcessorReady())
            return;
            
        // Recupera il genere attualmente rilevato dall'AI
        auto genre = processor.getAIEngine().getDetectedGenre();
        // Aggiorna il testo della label con il nome del genere
        genreValue.setText(AIEngine::getGenreName(genre), juce::dontSendNotification);
        
        // Sincronizza stato Multi-Track Unmasking toggle con AIEngine
        bool unmaskingEnabled = processor.getAIEngine().isMultiTrackUnmaskingEnabled();
        if (unmaskingToggle.getToggleState() != unmaskingEnabled)
        {
            unmaskingToggle.setToggleState(unmaskingEnabled, juce::dontSendNotification);
            if (unmaskingEnabled)
            {
                unmaskingToggle.setButtonText("UNMASKING ON");
                unmaskingToggle.setColour(juce::TextButton::buttonColourId, 
                                         ModernLookAndFeel::Colors::accentGreen);
            }
            else
            {
                unmaskingToggle.setButtonText("UNMASKING");
                unmaskingToggle.setColour(juce::TextButton::buttonColourId, 
                                         ModernLookAndFeel::Colors::bgLighter);
            }
        }
        
        // Richiede un ridisegno per aggiornare anche la lista dei problemi
        repaint();
    }

private:
    /**
     * Disegna la lista dei problemi audio rilevati dall'AI
     * Ogni problema mostra: tipo, frequenza, guadagno suggerito e severità
     * @param g Context grafico JUCE per il disegno
     */
    void drawProblems(juce::Graphics& g)
    {
        if (problemsArea.isEmpty()) return;
        
        auto bounds = problemsArea.toFloat();
        // Recupera tutte le correzioni pendenti dall'AI Engine
        const auto corrections = processor.getAIEngine().getPendingCorrections();
        
        // Se non ci sono problemi, mostra un messaggio positivo
        if (corrections.empty())
        {
            g.setColour(ModernLookAndFeel::Colors::accentGreen);
            g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
            g.drawText("No issues detected", bounds, juce::Justification::centredTop);
            return;
        }
        
        // Parametri per il layout verticale della lista
        float y = bounds.getY();
        float h = 24.0f; // Altezza di ogni item
        int shown = 0;   // Contatore degli item mostrati
        
        // Disegna ogni correzione come un item nella lista
        for (const auto& c : corrections)
        {
            // Limita a 6 item visibili o fino alla fine dell'area disponibile
            if (shown >= 6 || y + h > bounds.getBottom()) break;
            
            auto itemBounds = juce::Rectangle<float>(bounds.getX(), y, bounds.getWidth(), h - 2);
            
            // Colore basato sulla severità del problema
            juce::Colour sev = ModernLookAndFeel::Colors::getSeverity(c.severity);
            
            // Sfondo dell'item con angoli arrotondati
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.5f));
            g.fillRoundedRectangle(itemBounds, 3.0f);
            
            // Pallino colorato che indica la severità (sinistra)
            g.setColour(sev);
            g.fillEllipse(itemBounds.getX() + 6, itemBounds.getCentreY() - 4, 8, 8);
            
            // Tipo di problema (es. "Resonance", "Muddy", etc.)
            g.setColour(ModernLookAndFeel::Colors::textPrimary);
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
            g.drawText(AIEngine::getProblemTypeName(c.type),
                      (int)itemBounds.getX() + 18, (int)itemBounds.getY(),
                      70, (int)itemBounds.getHeight(),
                      juce::Justification::centredLeft);
            
            // Frequenza del problema (centro)
            // Formatta in Hz o kHz a seconda del valore
            juce::String freqTxt = c.frequency >= 1000 
                ? juce::String(c.frequency / 1000.0f, 1) + "k"
                : juce::String((int)c.frequency);
            g.setColour(ModernLookAndFeel::Colors::accentBlue);
            g.drawText(freqTxt, (int)itemBounds.getX() + 88, (int)itemBounds.getY(),
                      40, (int)itemBounds.getHeight(), juce::Justification::centred);
            
            // Guadagno suggerito (destra)
            // Arancione per tagli, verde per boost
            juce::Colour gainCol = c.suggestedGain < 0 
                ? ModernLookAndFeel::Colors::accentOrange 
                : ModernLookAndFeel::Colors::accentGreen;
            g.setColour(gainCol);
            juce::String gainTxt = (c.suggestedGain >= 0 ? "+" : "") + juce::String(c.suggestedGain, 1);
            g.drawText(gainTxt + " dB", (int)itemBounds.getRight() - 55, (int)itemBounds.getY(),
                      50, (int)itemBounds.getHeight(), juce::Justification::centredRight);
            
            y += h; // Passa all'item successivo
            shown++;
        }
        
        // Se ci sono più di 6 problemi, mostra un indicatore "+X more"
        if (corrections.size() > 6)
        {
            g.setColour(ModernLookAndFeel::Colors::textMuted);
            g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
            g.drawText("+" + juce::String(corrections.size() - 6) + " more",
                      static_cast<int>(bounds.getX()), static_cast<int>(y),
                      static_cast<int>(bounds.getWidth()), 14, juce::Justification::centred);
        }
    }

    void updateSensitivityDesc()
    {
        int val = static_cast<int>(sensitivityKnob.getValue());
        juce::String desc;
        if (val < 25)      desc = "Low - fewer detections";
        else if (val < 50)  desc = "Medium-Low";
        else if (val < 75)  desc = "Medium - balanced";
        else                desc = "High - more detections";
        sensitivityDesc.setText(desc, juce::dontSendNotification);
    }

    void updateStrengthDesc()
    {
        int val = static_cast<int>(strengthKnob.getValue());
        juce::String desc;
        if (val < 25)      desc = "Gentle";
        else if (val < 50)  desc = "Moderate";
        else if (val < 75)  desc = "Assertive";
        else                desc = "Aggressive";
        strengthDesc.setText(desc, juce::dontSendNotification);
    }

    // === MEMBER VARIABLES ===
    
    // Riferimento al processore audio principale per accedere all'AI Engine
    AIEqualizerAudioProcessor& processor;
    
    // === Componenti UI: Sezione AI ===
    juce::Label aiLabel;              // Etichetta "AI ASSISTANT"
    juce::ToggleButton aiToggle;      // Toggle ON/OFF per abilitare l'AI
    juce::TextButton autoFixBtn;      // Pulsante "AUTO FIX" per applicare tutte le correzioni
    juce::TextButton clearBtn;         // Pulsante "CLEAR" per cancellare le correzioni
    
    // === Componenti UI: Multi-Track Unmasking ===
    juce::Label unmaskingLabel;       // Etichetta "MULTI-TRACK"
    juce::TextButton unmaskingToggle; // Toggle per abilitare Multi-Track Unmasking
    
    // === Componenti UI: Knob di controllo ===
    juce::Label sensitivityLabel;     // Etichetta "SENSITIVITY"
    juce::Slider sensitivityKnob;     // Knob rotativo per regolare la sensibilità
    juce::Label sensitivityDesc;      // Descrizione contestuale (es. "Medium - balanced")
    juce::Label strengthLabel;         // Etichetta "STRENGTH"
    juce::Slider strengthKnob;        // Knob rotativo per regolare la forza
    juce::Label strengthDesc;          // Descrizione contestuale (es. "Moderate")
    
    // === Componenti UI: Genere e problemi ===
    juce::Label genreLabel;            // Etichetta "DETECTED"
    juce::Label genreValue;            // Label che mostra il nome del genere rilevato
    juce::Label problemsLabel;         // Etichetta "DETECTED PROBLEMS"
    
    // === Variabili di layout ===
    juce::Rectangle<int> problemsArea; // Area rettangolare dove disegnare la lista problemi
    int knobSectionY = 0;              // Posizione Y della sezione knob (per divisore)
    int problemsSectionY = 0;          // Posizione Y della sezione problemi (per divisore)
    
    // Macro JUCE per prevenire copia accidentale e rilevare memory leaks
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIControlPanel)
};
