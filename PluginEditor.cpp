#include "PluginEditor.h"

namespace
{
    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

TashreeqAudioProcessorEditor::TashreeqAudioProcessorEditor(TashreeqAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(920, 520);

    auto styleButton = [](juce::TextButton& b, juce::Colour bg)
    {
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    };

    styleButton(loadVstButton, juce::Colour(0xff1e293b));
    styleButton(openGuiButton, juce::Colour(0xff0f766e));
    styleButton(unloadVstButton, juce::Colour(0xff3f1d2b));
    styleButton(resetButton, juce::Colour(0xff27272a));

    addAndMakeVisible(loadVstButton);
    addAndMakeVisible(openGuiButton);
    addAndMakeVisible(unloadVstButton);
    addAndMakeVisible(resetButton);

    hostedNameLabel.setText(processor.getHostedPluginName(), juce::dontSendNotification);
    hostedNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    hostedNameLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
    hostedNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hostedNameLabel);

    loadVstButton.onClick = [this] { openVST3Chooser(); };
    openGuiButton.onClick = [this] { openHostedPluginEditor(); };
    unloadVstButton.onClick = [this]
    {
        hostedWindow.reset();
        processor.unloadHostedVST3();
        hostedNameLabel.setText(processor.getHostedPluginName(), juce::dontSendNotification);
    };

    maqamBox.addItem("Bayati", 1);
    maqamBox.addItem("Rast", 2);
    maqamBox.addItem("Saba", 3);
    maqamBox.addItem("Sikah", 4);
    maqamBox.addItem("Huzam", 5);
    maqamBox.addItem("Hijaz", 6);
    maqamBox.addItem("Off / Equal", 7);
    maqamBox.setSelectedId(1, juce::dontSendNotification);
    maqamBox.onChange = [this] { applyCurrentPreset(); };
    addAndMakeVisible(maqamBox);

    for (int i = 0; i < 12; ++i)
        rootNoteBox.addItem(kNoteNames[i], i + 1);
    rootNoteBox.setSelectedId(3, juce::dontSendNotification);
    rootNoteBox.onChange = [this] { applyCurrentPreset(); };
    addAndMakeVisible(rootNoteBox);

    styleBox.addItem("Arabic (-50c)", 1);
    styleBox.addItem("Turkish (-35c)", 2);
    styleBox.addItem("Deep (-60c)", 3);
    styleBox.setSelectedId(1, juce::dontSendNotification);
    styleBox.onChange = [this] { applyCurrentPreset(); };
    addAndMakeVisible(styleBox);

    modeBox.addItem("Poly MPE (Ch 2-16)", 1);
    modeBox.addItem("Solo / Mono (Ch 1)", 2);
    modeBox.setSelectedId(processor.getRoutingMode() == TashreeqAudioProcessor::RoutingMode::PolyMPE ? 1 : 2,
                          juce::dontSendNotification);
    modeBox.onChange = [this]
    {
        processor.setRoutingMode(modeBox.getSelectedId() == 1
            ? TashreeqAudioProcessor::RoutingMode::PolyMPE
            : TashreeqAudioProcessor::RoutingMode::MonoSingleCh);
    };
    addAndMakeVisible(modeBox);

    pbRangeBox.addItem("PB +/-2 st", 2);
    pbRangeBox.addItem("PB +/-12 st", 12);
    pbRangeBox.addItem("PB +/-24 st", 24);
    pbRangeBox.addItem("PB +/-48 st", 48);
    pbRangeBox.setSelectedId(processor.getPitchBendRange(), juce::dontSendNotification);
    pbRangeBox.onChange = [this] { processor.setPitchBendRange(pbRangeBox.getSelectedId()); };
    addAndMakeVisible(pbRangeBox);

    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setRange(0.0, 150.0, 1.0);
    depthSlider.setValue(processor.getDepth(), juce::dontSendNotification);
    depthSlider.setTextValueSuffix("%");
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
    depthSlider.onValueChange = [this] { processor.setDepth(static_cast<float>(depthSlider.getValue())); };
    addAndMakeVisible(depthSlider);

    resetButton.onClick = [this]
    {
        processor.resetAllCents();
        syncSlidersFromProcessor();
    };

    for (int i = 0; i < 12; ++i)
    {
        auto& s = noteSliders[static_cast<size_t>(i)];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setRange(-100.0, 100.0, 1.0);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setColour(juce::Slider::thumbColourId, juce::Colour(0xff38bdf8));
        s.setColour(juce::Slider::trackColourId, juce::Colour(0xff1e293b));
        s.onValueChange = [this, i]
        {
            const float val = static_cast<float>(noteSliders[static_cast<size_t>(i)].getValue());
            processor.setNoteCents(i, val);
            centValueLabels[static_cast<size_t>(i)].setText(juce::String(static_cast<int>(val)) + "c", juce::dontSendNotification);
            quickQuarterButtons[static_cast<size_t>(i)].setToggleState(std::abs(val) > 5.0f, juce::dontSendNotification);
        };
        addAndMakeVisible(s);

        auto& btn = quickQuarterButtons[static_cast<size_t>(i)];
        btn.setButtonText("-50");
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0284c7));
        btn.onClick = [this, i]
        {
            float current = processor.getNoteCents(i);
            float target = (std::abs(current) > 1.0f) ? 0.0f : (styleBox.getSelectedId() == 2 ? -35.0f : -50.0f);
            processor.setNoteCents(i, target);
            syncSlidersFromProcessor();
        };
        addAndMakeVisible(btn);

        auto& nameLbl = noteNameLabels[static_cast<size_t>(i)];
        nameLbl.setText(kNoteNames[i], juce::dontSendNotification);
        nameLbl.setJustificationType(juce::Justification::centred);
        nameLbl.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
        nameLbl.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLbl);

        auto& valLbl = centValueLabels[static_cast<size_t>(i)];
        valLbl.setJustificationType(juce::Justification::centred);
        valLbl.setFont(juce::Font(juce::FontOptions(12.0f)));
        valLbl.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(valLbl);
    }

    syncSlidersFromProcessor();
}

TashreeqAudioProcessorEditor::~TashreeqAudioProcessorEditor()
{
    hostedWindow.reset();
}

void TashreeqAudioProcessorEditor::syncSlidersFromProcessor()
{
    for (int i = 0; i < 12; ++i)
    {
        const float val = processor.getNoteCents(i);
        noteSliders[static_cast<size_t>(i)].setValue(val, juce::dontSendNotification);
        centValueLabels[static_cast<size_t>(i)].setText(juce::String(static_cast<int>(val)) + "c", juce::dontSendNotification);
        quickQuarterButtons[static_cast<size_t>(i)].setToggleState(std::abs(val) > 5.0f, juce::dontSendNotification);
    }
}

void TashreeqAudioProcessorEditor::applyCurrentPreset()
{
    const int maqamIdx = maqamBox.getSelectedId() - 1;
    const int rootIdx = rootNoteBox.getSelectedId() - 1;
    float qCents = -50.0f;
    if (styleBox.getSelectedId() == 2) qCents = -35.0f;
    else if (styleBox.getSelectedId() == 3) qCents = -60.0f;

    processor.applyMaqamPreset(maqamIdx, rootIdx, qCents);
    syncSlidersFromProcessor();
}

void TashreeqAudioProcessorEditor::openVST3Chooser()
{
    juce::File defaultVst3Dir("C:\\Program Files\\Common Files\\VST3");
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select any VST3 Instrument to Host",
        defaultVst3Dir.exists() ? defaultVst3Dir : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.vst3");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (!file.exists())
                return;

            hostedWindow.reset();
            juce::String err;
            if (processor.loadHostedVST3(file, err))
            {
                hostedNameLabel.setText("Hosted: " + processor.getHostedPluginName(), juce::dontSendNotification);
                openHostedPluginEditor();
            }
            else
            {
                hostedNameLabel.setText("Error: " + err, juce::dontSendNotification);
            }
        });
}

void TashreeqAudioProcessorEditor::openHostedPluginEditor()
{
    juce::ScopedLock lock(processor.getPluginLock());
    auto* hosted = processor.getHostedPluginInstance();
    if (hosted == nullptr)
        return;

    if (hostedWindow != nullptr)
    {
        hostedWindow->setVisible(true);
        hostedWindow->toFront(true);
        return;
    }

    if (auto* ed = hosted->createEditorIfNeeded())
    {
        hostedWindow = std::make_unique<HostedPluginWindow>(processor.getHostedPluginName(), ed);
    }
}

void TashreeqAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff08080a));

    g.setColour(juce::Colour(0xff121216));
    g.fillRoundedRectangle(16.0f, 14.0f, static_cast<float>(getWidth() - 32), 68.0f, 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(16.0f, 14.0f, static_cast<float>(getWidth() - 32), 68.0f, 12.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    g.drawText("TASHREEQ VST", 30, 22, 220, 26, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(11.5f)));
    g.drawText("Universal VST3 Microtonal Host for Mixcraft 10", 30, 48, 280, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff121216));
    g.fillRoundedRectangle(16.0f, 94.0f, static_cast<float>(getWidth() - 32), 68.0f, 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(16.0f, 94.0f, static_cast<float>(getWidth() - 32), 68.0f, 12.0f, 1.0f);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.drawText("MAQAM", 30, 100, 110, 16, juce::Justification::centredLeft);
    g.drawText("ROOT", 150, 100, 65, 16, juce::Justification::centredLeft);
    g.drawText("STYLE", 225, 100, 115, 16, juce::Justification::centredLeft);
    g.drawText("MIDI MODE", 350, 100, 135, 16, juce::Justification::centredLeft);
    g.drawText("PB RANGE", 495, 100, 95, 16, juce::Justification::centredLeft);
    g.drawText("DEPTH", 600, 100, 160, 16, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff121216));
    g.fillRoundedRectangle(16.0f, 174.0f, static_cast<float>(getWidth() - 32), 330.0f, 12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(16.0f, 174.0f, static_cast<float>(getWidth() - 32), 330.0f, 12.0f, 1.0f);
}

void TashreeqAudioProcessorEditor::resized()
{
    loadVstButton.setBounds(310, 28, 160, 38);
    openGuiButton.setBounds(480, 28, 120, 38);
    unloadVstButton.setBounds(610, 28, 75, 38);
    hostedNameLabel.setBounds(698, 28, 195, 38);

    maqamBox.setBounds(30, 118, 110, 30);
    rootNoteBox.setBounds(150, 118, 65, 30);
    styleBox.setBounds(225, 118, 115, 30);
    modeBox.setBounds(350, 118, 135, 30);
    pbRangeBox.setBounds(495, 118, 95, 30);
    depthSlider.setBounds(600, 118, 190, 30);
    resetButton.setBounds(802, 118, 88, 30);

    const int startX = 32;
    const int totalW = getWidth() - 64;
    const int colW = totalW / 12;

    for (int i = 0; i < 12; ++i)
    {
        const int x = startX + i * colW;
        noteNameLabels[static_cast<size_t>(i)].setBounds(x, 188, colW - 6, 22);
        centValueLabels[static_cast<size_t>(i)].setBounds(x, 212, colW - 6, 18);
        noteSliders[static_cast<size_t>(i)].setBounds(x + (colW - 6) / 2 - 18, 236, 36, 210);
        quickQuarterButtons[static_cast<size_t>(i)].setBounds(x + 6, 456, colW - 18, 30);
    }
}
