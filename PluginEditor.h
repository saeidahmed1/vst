#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <array>

class HostedPluginWindow : public juce::DocumentWindow
{
public:
    HostedPluginWindow(const juce::String& name, juce::AudioProcessorEditor* editor)
        : DocumentWindow(name, juce::Colour(0xff0b0b0e), DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};

class TashreeqAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TashreeqAudioProcessorEditor(TashreeqAudioProcessor&);
    ~TashreeqAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void syncSlidersFromProcessor();
    void applyCurrentPreset();
    void openVST3Chooser();
    void openHostedPluginEditor();

    TashreeqAudioProcessor& processor;

    juce::TextButton loadVstButton { "Load VST3 Instrument" };
    juce::TextButton openGuiButton { "Open VST GUI" };
    juce::TextButton unloadVstButton { "Unload" };
    juce::Label hostedNameLabel;

    juce::ComboBox maqamBox;
    juce::ComboBox rootNoteBox;
    juce::ComboBox styleBox;
    juce::ComboBox modeBox;
    juce::ComboBox pbRangeBox;
    juce::Slider depthSlider;
    juce::TextButton resetButton { "Reset 0c" };

    std::array<juce::Slider, 12> noteSliders;
    std::array<juce::TextButton, 12> quickQuarterButtons;
    std::array<juce::Label, 12> noteNameLabels;
    std::array<juce::Label, 12> centValueLabels;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<HostedPluginWindow> hostedWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TashreeqAudioProcessorEditor)
};
