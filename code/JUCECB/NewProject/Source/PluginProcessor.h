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
        double basePlaybackRate;
        double playbackRate;
        float velocity;
        bool isActive;
        
        // Envelope parameters
        float attackTime = 0.01f;     // Attack time in seconds
        float releaseTime = 0.15f;    // Release time in seconds
        float releaseLevel = 1.0f;    // Level when note off was triggered
        double releaseStart = 0;      // Sample position when note off was triggered
        double attackStart = 0;       // Sample position when note started
        bool isReleasing = false;
        double sampleRate = 44100.0;
        int bufferLength = 0;         // Add this to store buffer length
        static constexpr float crossfadeLength = 64; // samples
        static constexpr int XFADE_LENGTH = 512; // Longer crossfade for smoother transitions
        float previousSample = 0.0f; // Store last sample for interpolation
        
        Voice(int note, double rate, float vel, double sr, int buffLen)
        : midiNote(note),
        samplePosition(0.0),
        basePlaybackRate(rate),
        playbackRate(rate),
        velocity(vel),
        isActive(true),
        attackStart(0),
        sampleRate(sr),
        bufferLength(buffLen) {}
        
        float getEnvelopeGain(double currentSamplePos) {
            // Attack phase
            float attackGain = 1.0f;
            double timeSinceAttack = (currentSamplePos - attackStart) / sampleRate;
            if (timeSinceAttack < 0) {
                timeSinceAttack += bufferLength / sampleRate;
            }
            
            if (timeSinceAttack < attackTime) {
                // Smoother cubic curve for attack
                float t = timeSinceAttack / attackTime;
                attackGain = t * t * (3.0f - 2.0f * t); // Smooth cubic interpolation
            }
            
            // Release phase
            float releaseGain = 1.0f;
            if (isReleasing) {
                double timeSinceRelease = (currentSamplePos - releaseStart) / sampleRate;
                if (timeSinceRelease < 0) {
                    timeSinceRelease += bufferLength / sampleRate;
                }
                
                if (timeSinceRelease >= releaseTime) {
                    isActive = false;
                    return 0.0f;
                }
                
                // Smoother release curve
                float t = timeSinceRelease / releaseTime;
                releaseGain = std::pow(1.0f - t, 2.0f); // Quadratic decay
            }
            
            return attackGain * releaseGain;
        }
        
        void triggerRelease() {
            isReleasing = true;
            releaseStart = samplePosition;
            // If releaseStart is beyond the buffer length, wrap it
            while (releaseStart >= bufferLength) {
                releaseStart -= bufferLength;
            }
        }
    };
    
    class TextParameter : public juce::AudioProcessorParameter
    {
        public:
        TextParameter(const String& paramId, const String& name, const String& defaultValue)
        : parameterID(paramId), parameterName(name), value(defaultValue) {}
        
        float getValue() const override { return 0.0f; }
        void setValue(float newValue) override {}
        float getDefaultValue() const override { return 0.0f; }
        String getName(int maximumStringLength) const override { return parameterName; }
        String getLabel() const override { return {}; }
        
        // Required virtual methods
        float getValueForText(const String& text) const override { return 0.0f; }
        String getText(float v, int maximumLength) const override
        {
            return value.substring(0, maximumLength);
        }
        bool isDiscrete() const override { return false; }
        bool isBoolean() const override { return false; }
        int getNumSteps() const override { return 0; }
        bool isMetaParameter() const override { return false; }
        Category getCategory() const override { return Category::genericParameter; }
        
        void setKeyText(const String& newText)
        {
            value = newText;
            sendValueChangedMessageToListeners(0.0f);
        }
        
        String getKeyText() const { return value; }
        
        private:
        String parameterID;
        String parameterName;
        String value;
    };
    
    void parameterValueChanged(int parameterIndex, float newValue) override
    {
        if (auto* param = dynamic_cast<TextParameter*>(parameters.getParameter("enckey"))) {
            if (param->getParameterIndex() == parameterIndex) {
                encryptionKey = param->getKeyText();
                reloadWithNewKey();
            }
        }
    }
    
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}
    
    // Audio parameters
    juce::AudioProcessorValueTreeState parameters;
    
    private:
    //==============================================================================
    // Audio format handling
    juce::AudioFormatManager formatManager;
    
    // Encryption methods
    void encryptAudioECB(AudioBuffer<float>& buffer, const String& key);
    std::vector<uint8_t> encryptBlockECB(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key);
    void reloadWithNewKey();
    
    // File handling methods
    AudioBuffer<float> getAudioBufferFromFile(juce::File file);
    bool isValidWavFile(const File& file);
    std::unique_ptr<FileChooser> fileChooser;
    
    // Playback state
    bool hasLoadedFile = false;
    int currentSamplePosition = 0;
    AudioBuffer<float> originalBuffer;
    AudioBuffer<float> encryptedBuffer;
    
    // Pitch control
    double playbackRate = 1.0;
    const int midiRootNote = 69; // The note at which we play at normal speed, A4
    
    // Plugin state
    std::atomic<float>* wetDryParameter = nullptr;
    
    // Polyphony
    std::vector<Voice> voices;
    
    // Quantization
    std::atomic<float>* quantizationParameter = nullptr;
    
    // Pitch wheel
    std::atomic<float>* pitchBendRangeParameter = nullptr;
    
    // Encryption key
    TextParameter* encKeyParameter = nullptr;
    String encryptionKey = "DefaultKey123";
    
    // Envelope
    std::atomic<float>* releaseTimeParameter = nullptr;
    
    // Gain
    std::atomic<float>* gainParameter = nullptr;
    
    // Looping
    std::atomic<float>* loopEnabledParameter = nullptr;
    
    // Logging
    std::unique_ptr<FileLogger> fileLogger;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JUCECB)
};
