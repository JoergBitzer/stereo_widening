#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StereoWidenerAudioProcessor::StereoWidenerAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),m_algo(this), m_parameterVTS(nullptr)
{

    m_algo.addParameter(m_paramVector);

    m_parameterVTS = std::make_unique<AudioProcessorValueTreeState>(*this, nullptr, Identifier("StereoWidenerVTS"),
        AudioProcessorValueTreeState::ParameterLayout(m_paramVector.begin(), m_paramVector.end()));

    m_algo.prepareParameter(m_parameterVTS);

	m_presets.setAudioValueTreeState(m_parameterVTS.get());
    // if needed add categories, if g_PresetCategories contains one empty string "", nothing happened
    m_presets.addCategory(g_PresetCategories);

#ifdef FACTORY_PRESETS
    m_presets.DeployFactoryPresets();
#endif

	m_presets.loadfromFileAllUserPresets();

    setLatencySamples(m_algo.getLatency());
}

StereoWidenerAudioProcessor::~StereoWidenerAudioProcessor()
{
    // Persist only the GUI scale factor (a UI convenience, not a processing default) --
    // see GlobalSettings.h for why parameter values themselves are no longer persisted
    // here; the init.xml preset is the supported way to restore a previous session's
    // settings.
    m_algo.getGlobalSettings().saveGuiScaleFactor(m_pluginScaleFactor);
}

//==============================================================================
const juce::String StereoWidenerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StereoWidenerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool StereoWidenerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool StereoWidenerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}


double StereoWidenerAudioProcessor::getTailLengthSeconds() const
{

    return 0.0;
}

int StereoWidenerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int StereoWidenerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StereoWidenerAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String StereoWidenerAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void StereoWidenerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void StereoWidenerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Since we only support if in and output is the same, we can just ask for input
    int nrofchannels = this->getMainBusNumOutputChannels();
    jassert(nrofchannels > 0); // number of channels should never be zero

    juce::ignoreUnused (samplesPerBlock);
    m_fs = static_cast<float>(sampleRate);
    m_algo.prepareToPlay(sampleRate,samplesPerBlock,nrofchannels);
}

void StereoWidenerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool StereoWidenerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void StereoWidenerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
 #if WITH_MIDIKEYBOARD
	m_keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    m_wheelState.processNextMidiBuffer(midiMessages,true);
#else
    juce::ignoreUnused (midiMessages);
#endif

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    m_algo.processBlock(buffer,midiMessages);

#if WITH_MIDIKEYBOARD
    midiMessages.clear(); // except you want to create new midi messages, but than say so
    // by setting NEEDS_MIDI_OUTPUT in CMakeLists.txt
#endif
}

//==============================================================================
bool StereoWidenerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* StereoWidenerAudioProcessor::createEditor()
{
    return new StereoWidenerAudioProcessorEditor (*this);
}

//==============================================================================
void StereoWidenerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
	auto state = m_parameterVTS->copyState();
    ValueTree vtpluginsize("PluginSize");
    vtpluginsize.setProperty("ScaleFactor",m_pluginScaleFactor,nullptr);
    state.appendChild(vtpluginsize,nullptr);


	std::unique_ptr<XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);

}

void StereoWidenerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
 	std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(m_parameterVTS->state.getType()))
        {
            ValueTree vt = ValueTree::fromXml(*xmlState);
            ValueTree subvt = vt.getChildWithName("PluginSize");
            if (subvt.isValid())
            {
                float val = subvt.getProperty("ScaleFactor");
                m_pluginScaleFactor = val;
                vt.removeChild(subvt, nullptr);

            }
            juce::String presetname(xmlState->getStringAttribute("presetname"));
            m_presets.setCurrentPresetName(presetname);

			m_parameterVTS->replaceState(vt);
        }

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StereoWidenerAudioProcessor();
}
