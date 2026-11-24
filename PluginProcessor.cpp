#include "PluginProcessor.h"
#include "PluginEditor.h"

TashreeqAudioProcessor::TashreeqAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& c : noteCents)
        c.store(0.0f);

    activeNoteChannel.fill(1);
    formatManager.addDefaultFormats();

    applyMaqamPreset(0, 2, -50.0f);
}

TashreeqAudioProcessor::~TashreeqAudioProcessor()
{
    unloadHostedVST3();
}

void TashreeqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentBlockSize = samplesPerBlock > 0 ? samplesPerBlock : 512;

    juce::ScopedLock lock(pluginLock);
    if (hostedPlugin != nullptr)
    {
        hostedPlugin->setPlayConfigDetails(
            getTotalNumInputChannels(),
            getTotalNumOutputChannels(),
            currentSampleRate,
            currentBlockSize);
        hostedPlugin->prepareToPlay(currentSampleRate, currentBlockSize);
    }
}

void TashreeqAudioProcessor::releaseResources()
{
    juce::ScopedLock lock(pluginLock);
    if (hostedPlugin != nullptr)
        hostedPlugin->releaseResources();
}

bool TashreeqAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

double TashreeqAudioProcessor::getTailLengthSeconds() const
{
    juce::ScopedLock lock(pluginLock);
    if (hostedPlugin != nullptr)
        return hostedPlugin->getTailLengthSeconds();
    return 0.0;
}

void TashreeqAudioProcessor::setNoteCents(int pitchClass, float cents)
{
    if (pitchClass >= 0 && pitchClass < 12)
        noteCents[static_cast<size_t>(pitchClass)].store(juce::jlimit(-100.0f, 100.0f, cents));
}

float TashreeqAudioProcessor::getNoteCents(int pitchClass) const
{
    if (pitchClass >= 0 && pitchClass < 12)
        return noteCents[static_cast<size_t>(pitchClass)].load();
    return 0.0f;
}

void TashreeqAudioProcessor::setDepth(float newDepthPercent)
{
    depthPercent.store(juce::jlimit(0.0f, 150.0f, newDepthPercent));
}

void TashreeqAudioProcessor::setPitchBendRange(int semitones)
{
    pitchBendRange.store(juce::jlimit(1, 48, semitones));
}

void TashreeqAudioProcessor::setRoutingMode(RoutingMode mode)
{
    routingMode.store(static_cast<int>(mode));
}

void TashreeqAudioProcessor::resetAllCents()
{
    for (auto& c : noteCents)
        c.store(0.0f);
}

void TashreeqAudioProcessor::applyMaqamPreset(int maqamIndex, int rootNote, float quarterToneCents)
{
    resetAllCents();
    const int r = ((rootNote % 12) + 12) % 12;

    auto setInterval = [&](int semitoneOffset, float cents)
    {
        int pc = (r + semitoneOffset) % 12;
        setNoteCents(pc, cents);
    };

    switch (maqamIndex)
    {
        case 0:
            setInterval(2, quarterToneCents);
            break;
        case 1:
            setInterval(4, quarterToneCents);
            setInterval(11, quarterToneCents);
            break;
        case 2:
            setInterval(2, quarterToneCents);
            setInterval(9, quarterToneCents);
            break;
        case 3:
            setInterval(0, quarterToneCents);
            setInterval(7, quarterToneCents);
            break;
        case 4:
            setInterval(0, quarterToneCents);
            setInterval(5, quarterToneCents);
            break;
        case 5:
            setInterval(1, quarterToneCents * 0.25f);
            break;
        case 6:
        default:
            break;
    }
}

int TashreeqAudioProcessor::calculatePitchBendValue(float cents) const
{
    const float rangeCents = static_cast<float>(pitchBendRange.load()) * 100.0f;
    const float normalized = juce::jlimit(-1.0f, 1.0f, cents / rangeCents);
    const int pbValue = static_cast<int>(std::round(8192.0f + normalized * 8191.0f));
    return juce::jlimit(0, 16383, pbValue);
}

void TashreeqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    juce::MidiBuffer processedMidi;
    const float depthScale = depthPercent.load() / 100.0f;
    const auto mode = getRoutingMode();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int samplePos = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            const int noteNum = msg.getNoteNumber();
            const int pitchClass = noteNum % 12;
            const float cents = noteCents[static_cast<size_t>(pitchClass)].load() * depthScale;
            const int pbVal = calculatePitchBendValue(cents);

            int targetChannel = 1;
            if (mode == RoutingMode::PolyMPE)
            {
                if (std::abs(cents) < 0.01f)
                {
                    targetChannel = 1;
                }
                else
                {
                    targetChannel = nextMpeChannel;
                    nextMpeChannel++;
                    if (nextMpeChannel > 16)
                        nextMpeChannel = 2;
                }
            }
            else
            {
                targetChannel = 1;
            }

            activeNoteChannel[static_cast<size_t>(noteNum)] = targetChannel;

            processedMidi.addEvent(juce::MidiMessage::pitchWheel(targetChannel, pbVal), samplePos);
            processedMidi.addEvent(juce::MidiMessage::noteOn(targetChannel, noteNum, msg.getVelocity()), samplePos);
        }
        else if (msg.isNoteOff())
        {
            const int noteNum = msg.getNoteNumber();
            const int targetChannel = activeNoteChannel[static_cast<size_t>(noteNum)];
            processedMidi.addEvent(juce::MidiMessage::noteOff(targetChannel, noteNum, msg.getVelocity()), samplePos);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int ch = 1; ch <= 16; ++ch)
            {
                processedMidi.addEvent(juce::MidiMessage::allNotesOff(ch), samplePos);
                processedMidi.addEvent(juce::MidiMessage::pitchWheel(ch, 8192), samplePos);
            }
        }
        else
        {
            processedMidi.addEvent(msg, samplePos);
        }
    }

    {
        juce::ScopedTryLock tryLock(pluginLock);
        if (tryLock.isLocked() && hostedPlugin != nullptr)
        {
            hostedPlugin->processBlock(buffer, processedMidi);
        }
    }

    midiMessages.swapWith(processedMidi);
}

bool TashreeqAudioProcessor::loadHostedVST3(const juce::File& pluginFile, juce::String& errorOut)
{
    if (!pluginFile.exists())
    {
        errorOut = "Plugin file does not exist.";
        return false;
    }

    juce::OwnedArray<juce::PluginDescription> foundTypes;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* fmt = formatManager.getFormat(i);
        if (fmt->fileMightContainThisPluginType(pluginFile.getFullPathName()))
        {
            fmt->findAllTypesForFile(foundTypes, pluginFile.getFullPathName());
            if (!foundTypes.isEmpty())
                break;
        }
    }

    if (foundTypes.isEmpty())
    {
        errorOut = "No valid VST3 instrument found in the selected file.";
        return false;
    }

    auto desc = *foundTypes[0];
    auto instance = formatManager.createPluginInstance(desc, currentSampleRate, currentBlockSize, errorOut);

    if (instance == nullptr)
        return false;

    instance->setPlayConfigDetails(
        getTotalNumInputChannels(),
        getTotalNumOutputChannels(),
        currentSampleRate,
        currentBlockSize);
    instance->prepareToPlay(currentSampleRate, currentBlockSize);

    {
        juce::ScopedLock lock(pluginLock);
        hostedPlugin = std::move(instance);
        hostedDescription = desc;
        hostedPluginPath = pluginFile.getFullPathName();
    }

    return true;
}

void TashreeqAudioProcessor::unloadHostedVST3()
{
    juce::ScopedLock lock(pluginLock);
    if (hostedPlugin != nullptr)
    {
        hostedPlugin->releaseResources();
        hostedPlugin.reset();
    }
    hostedPluginPath.clear();
}

bool TashreeqAudioProcessor::hasHostedPlugin() const
{
    return hostedPlugin != nullptr;
}

juce::String TashreeqAudioProcessor::getHostedPluginName() const
{
    if (hostedPlugin != nullptr)
        return hostedDescription.name;
    return "No VST3 Loaded";
}

void TashreeqAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = std::make_unique<juce::XmlElement>("TashreeqVSTState");
    state->setAttribute("depth", static_cast<double>(depthPercent.load()));
    state->setAttribute("pbRange", pitchBendRange.load());
    state->setAttribute("routingMode", routingMode.load());
    state->setAttribute("hostedPath", hostedPluginPath);

    for (int i = 0; i < 12; ++i)
        state->setAttribute("c" + juce::String(i), static_cast<double>(noteCents[static_cast<size_t>(i)].load()));

    {
        juce::ScopedLock lock(pluginLock);
        if (hostedPlugin != nullptr)
        {
            juce::MemoryBlock pluginState;
            hostedPlugin->getStateInformation(pluginState);
            state->setAttribute("hostedState", pluginState.toBase64Encoding());
        }
    }

    copyXmlToBinary(*state, destData);
}

void TashreeqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("TashreeqVSTState"))
        return;

    setDepth(static_cast<float>(xml->getDoubleAttribute("depth", 100.0)));
    setPitchBendRange(xml->getIntAttribute("pbRange", 2));
    setRoutingMode(static_cast<RoutingMode>(xml->getIntAttribute("routingMode", 0)));

    for (int i = 0; i < 12; ++i)
        setNoteCents(i, static_cast<float>(xml->getDoubleAttribute("c" + juce::String(i), 0.0)));

    auto path = xml->getStringAttribute("hostedPath");
    if (path.isNotEmpty())
    {
        juce::String err;
        if (loadHostedVST3(juce::File(path), err))
        {
            auto b64 = xml->getStringAttribute("hostedState");
            if (b64.isNotEmpty())
            {
                juce::MemoryBlock pluginState;
                if (pluginState.fromBase64Encoding(b64))
                {
                    juce::ScopedLock lock(pluginLock);
                    if (hostedPlugin != nullptr)
                        hostedPlugin->setStateInformation(pluginState.getData(), static_cast<int>(pluginState.getSize()));
                }
            }
        }
    }
}

juce::AudioProcessorEditor* TashreeqAudioProcessor::createEditor()
{
    return new TashreeqAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TashreeqAudioProcessor();
}
