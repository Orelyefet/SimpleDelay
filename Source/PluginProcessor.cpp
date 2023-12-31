/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleDelayAudioProcessor::SimpleDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
: AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                  )
#endif
{
}

SimpleDelayAudioProcessor::~SimpleDelayAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleDelayAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool SimpleDelayAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool SimpleDelayAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double SimpleDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto delayBufferSize = sampleRate * 2.0;
    DBG("delayBufferSize: " << delayBufferSize);
    delayBuffer.setSize(getTotalNumOutputChannels(), static_cast<int>(delayBufferSize));
    delayBuffer.clear();
}

void SimpleDelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
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
#endif

void SimpleDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    {
        buffer.clear (i, 0, buffer.getNumSamples());
    }
    
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        
        fillBuffer(channel, bufferSize, delayBufferSize, channelData);
        
        // One second of audio from in the past
        auto readPosition = writePosition - getSampleRate();
        
        if (readPosition < 0)
            readPosition += delayBufferSize;
        
        if (readPosition + bufferSize < delayBufferSize)
        {
            buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), bufferSize, 0.7f, 0.7f);
        }
        else
        {
            auto numSamplesToEnd = delayBufferSize - readPosition;
            buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), numSamplesToEnd, 0.7f, 0.7f);
            
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            buffer.addFromWithRamp(channel, numSamplesToEnd, delayBuffer.getReadPointer(channel, 0), numSamplesAtStart, 0.7f, 0.7f);
        }
    }
    
    DBG("Delay Buffer Size: " << delayBufferSize);
    DBG("Buffer Size: " << bufferSize);
    DBG("Write Position: " << writePosition);
    
    writePosition += bufferSize;
    writePosition %= delayBufferSize;
}

void SimpleDelayAudioProcessor::fillBuffer(int channel, int bufferSize, int delayBufferSize, float* channelData)
{
    // Check to see if main buffer copies to delay buffer without needing to wrap
    if (delayBufferSize > bufferSize + writePosition)
    {
        // copy main buffer contents to delay buffer
        delayBuffer.copyFromWithRamp(channel, writePosition, channelData, bufferSize, 0.1f, 0.1f);
    }
    else
    {
        // Determine how much space is left at the end of the delay buffer
        auto numSamplesToEnd = delayBufferSize - writePosition;
        
        // Copy that amount of contents to the end
        delayBuffer.copyFromWithRamp(channel, writePosition, channelData, numSamplesToEnd, 0.1f, 0.1f);
        
        // Calculate how much contetns is remaining to copy
        auto numSamplesAtStart = bufferSize - numSamplesToEnd;
        
        delayBuffer.copyFromWithRamp(channel, 0, channelData + numSamplesToEnd, numSamplesAtStart, 0.1f, 0.1f);
}

//==============================================================================
bool SimpleDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleDelayAudioProcessor::createEditor()
{
    return new SimpleDelayAudioProcessorEditor (*this);
}

//==============================================================================
void SimpleDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleDelayAudioProcessor();
}
