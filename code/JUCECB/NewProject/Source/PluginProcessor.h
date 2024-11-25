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
class JUCECB  : public juce::AudioProcessor, public AudioProcessorParameter::Listener
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
    
    struct Voice {
        int midiNote;
        double samplePosition;
        double basePlaybackRate;  // Store the original rate without pitchbend
        double playbackRate;      // Actual rate including pitchbend
        float velocity;           // Store velocity (0.0 to 1.0)
        bool isActive;
        
        Voice(int note, double rate, float vel)
            : midiNote(note),
              samplePosition(0.0),
              basePlaybackRate(rate),
              playbackRate(rate),
              velocity(vel),
              isActive(true) {}
    };
    
    void parameterValueChanged(int parameterIndex, float newValue) override
        {
            // Check if it's the encryption key parameter
            if (auto* param = parameters.getParameter("enckey")) {
                if (param->getParameterIndex() == parameterIndex) {
                    // Convert the parameter value to a key string
                    juce::String newKey;
                    int keyIndex = static_cast<int>(newValue * 3.0f); // 4 choices, so multiply by 3
                    switch(keyIndex) {
                        case 0: newKey = "key1"; break;
                        case 1: newKey = "key2"; break;
                        case 2: newKey = "key3"; break;
                        case 3: newKey = "key4"; break;
                        default: newKey = "key1";
                    }
                    encryptionKey = newKey;
                    reloadWithNewKey();
                }
            }
        }
        
        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

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
    AudioBuffer<float> encryptedBuffer;
    
    // Pitch control
    double playbackRate = 1.0;
    int currentMidiNote = 60; // Middle C
    const int midiRootNote = 60; // The note at which we play at normal speed
    
    // Plugin state
    std::atomic<float>* wetDryParameter = nullptr;
    
    // Polyphony
    std::vector<Voice> voices;
    
    // Quantization
    std::atomic<float>* quantizationParameter = nullptr;
    
    // Pitch wheel
    std::atomic<float>* pitchBendRangeParameter = nullptr;
    
    // Encryption key
    std::atomic<float>* encryptionKeyParameter = nullptr;
    juce::String encryptionKey = "DefaultKey123";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JUCECB)
};
