/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

//==============================================================================
/**
*/
class JUCECB  : public juce::AudioProcessor
{
public:
    //==============================================================================
    JUCECB();
    ~JUCECB() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Custom public methods
    void loadFile();
    void setEncryptionKey(const String& newKey) {
        if (newKey != encryptionKey) {
            encryptionKey = newKey;
            if (hasLoadedFile) {
                reloadWithNewKey();
            }
        }
    }
    String getCurrentKey() const { return encryptionKey; }
    void stopNote();
    void startNote();

    // Audio parameters
    juce::AudioProcessorValueTreeState parameters;

private:
    //==============================================================================
    // Sampler
    juce::Synthesiser sampler;
    const int numVoices = 4;
    
    // Audio format handling
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReader> formatReader;
    
    // Encryption methods
    void encryptAudioECB(AudioBuffer<float>& buffer, const String& key);
    std::vector<uint8_t> encryptBlockECB(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key);
    void reloadWithNewKey();
    
    // File handling methods
    AudioBuffer<float> getAudioBufferFromFile(juce::File file);
    bool isValidWavFile(const File& file);
    bool checkWavProperties(AudioFormatReader* reader);
    
    // Playback state
    bool hasLoadedFile = false;
    bool noteIsPlaying = false;
    bool isNotePlaying = false;
    int currentSamplePosition = 0;
    AudioBuffer<float> originalBuffer;
    AudioBuffer<float> dryBuffer;
    
    // Pitch control
    double playbackRate = 1.0;
    int currentMidiNote = 60; // Middle C
    const int midiRootNote = 60; // The note at which we play at normal speed
    
    // Plugin state
    String encryptionKey = "DefaultKey123";
    std::atomic<float>* wetDryParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JUCECB)
};
