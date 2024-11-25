/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JUCECB::JUCECB()
    : AudioProcessor(BusesProperties()
        #if ! JucePlugin_IsMidiEffect
         #if ! JucePlugin_IsSynth
          .withInput("Input", juce::AudioChannelSet::mono(), true)
         #endif
          .withOutput("Output", juce::AudioChannelSet::mono(), true)
        #endif
    ),
    parameters(*this, nullptr, "Parameters",
        {
            std::make_unique<juce::AudioParameterFloat>(
                "wetdry",    // parameter ID
                "Mix",       // parameter name
                0.0f,        // minimum value
                1.0f,        // maximum value
                0.0f),       // default value
            std::make_unique<juce::AudioParameterInt>(
                "quantize",  // parameter ID
                "Quantize",  // parameter name
                2,          // minimum value (2 levels)
                64,         // maximum value
                16),        // default value
            std::make_unique<juce::AudioParameterFloat>(
                "pbrange",   // parameter ID
                "Pitch Bend Range",  // parameter name
                1.0f,       // minimum value (1 semitone)
                24.0f,      // maximum value (2 octaves)
                2.0f),      // default value (2 semitones)
            std::make_unique<juce::AudioParameterFloat>(
                "release",   // parameter ID
                "Release Time", // parameter name
                0.01f,      // minimum value (10ms)
                2.0f,       // maximum value (2 seconds)
                0.1f)       // default value (100ms)
        })
{
    wetDryParameter = parameters.getRawParameterValue("wetdry");
    quantizationParameter = parameters.getRawParameterValue("quantize");
    pitchBendRangeParameter = parameters.getRawParameterValue("pbrange");
    releaseTimeParameter = parameters.getRawParameterValue("release");
    
    // Create text parameter for encryption key separately
    encKeyParameter = new TextParameter("enckey", "Encryption Key", "DefaultKey123");
    addParameter(encKeyParameter);
    
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    formatManager.registerBasicFormats();
}

JUCECB::~JUCECB()
{
    formatReader = nullptr;
    EVP_cleanup();
    ERR_free_strings();
}

//==============================================================================
const juce::String JUCECB::getName() const
{
    return JucePlugin_Name;
}

bool JUCECB::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JUCECB::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JUCECB::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JUCECB::getTailLengthSeconds() const
{
    return 0.0;
}

int JUCECB::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int JUCECB::getCurrentProgram()
{
    return 0;
}

void JUCECB::setCurrentProgram (int index)
{
}

const String JUCECB::getProgramName (int index)
{
    return {};
}

void JUCECB::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void JUCECB::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void JUCECB::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JUCECB::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
   #else
    // This is the place where you check if the layout is supported.
    // We only support mono in this version
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono())
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

void JUCECB::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (!hasLoadedFile) {
        buffer.clear();
        return;
    }

    ScopedNoDenormals noDenormals;
    
    // Process MIDI messages
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        
        if (msg.isNoteOn()) {
            double playbackRate = std::pow(2.0, (msg.getNoteNumber() - midiRootNote) / 12.0);
            float velocity = msg.getVelocity() / 127.0f;
            voices.push_back(Voice(msg.getNoteNumber(), playbackRate, velocity, getSampleRate()));
        }
        else if (msg.isNoteOff()) {
            for (auto& voice : voices) {
                if (voice.midiNote == msg.getNoteNumber() && !voice.isReleasing) {
                    voice.triggerRelease();
                }
            }
        }
        else if (msg.isPitchWheel()) {
            const double pitchWheelValue = (msg.getPitchWheelValue() - 8192) / 8192.0;
            const double pitchBendRange = pitchBendRangeParameter->load();
            const double pitchBendFactor = std::pow(2.0, pitchWheelValue * pitchBendRange / 12.0);
            
            for (auto& voice : voices) {
                voice.playbackRate = voice.basePlaybackRate * pitchBendFactor;
            }
        }
    }
    
    voices.erase(
        std::remove_if(voices.begin(), voices.end(),
            [](const Voice& voice) { return !voice.isActive; }),
        voices.end()
    );
    
    buffer.clear();
    
    float wetMix = parameters.getRawParameterValue("wetdry")->load();
    float dryMix = 1.0f - wetMix;
    
    AudioBuffer<float> tempBuffer(1, buffer.getNumSamples());
    
    for (auto& voice : voices) {
        tempBuffer.clear();
        
        float* channelData = tempBuffer.getWritePointer(0);
        const float* originalData = originalBuffer.getReadPointer(0);
        const float* encryptedData = encryptedBuffer.getReadPointer(0);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
            double readPosition = voice.samplePosition + (sample * voice.playbackRate);
            
            while (readPosition >= originalBuffer.getNumSamples()) {
                readPosition -= originalBuffer.getNumSamples();
            }
            
            int pos1 = static_cast<int>(readPosition);
            int pos2 = (pos1 + 1) % originalBuffer.getNumSamples();
            float fraction = static_cast<float>(readPosition - pos1);
            
            float drySample = originalData[pos1] +
                (originalData[pos2] - originalData[pos1]) * fraction;
            
            float wetSample = encryptedData[pos1] +
                (encryptedData[pos2] - encryptedData[pos1]) * fraction;
            
            // Apply velocity and envelope
            float envelopeGain = voice.getEnvelopeGain(voice.samplePosition + sample * voice.playbackRate);
            drySample *= voice.velocity * envelopeGain;
            wetSample *= voice.velocity * envelopeGain;
            
            channelData[sample] = (drySample * dryMix) + (wetSample * wetMix);
        }
        
        buffer.addFrom(0, 0, tempBuffer, 0, 0, buffer.getNumSamples());
        
        voice.samplePosition += buffer.getNumSamples() * voice.playbackRate;
        while (voice.samplePosition >= originalBuffer.getNumSamples()) {
            voice.samplePosition -= originalBuffer.getNumSamples();
        }
    }
    
    if (!voices.empty()) {
        float scale = 1.0f / std::sqrt(static_cast<float>(voices.size()));
        buffer.applyGain(scale);
    }
}
//==============================================================================
bool JUCECB::hasEditor() const
{
    return true;
}

AudioProcessorEditor* JUCECB::createEditor()
{
    return new JUCECBEditor (*this);
}

//==============================================================================
void JUCECB::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JUCECB::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        parameters.replaceState(ValueTree::fromXml(*xmlState));
}

void JUCECB::encryptAudioECB(AudioBuffer<float>& buffer, const String& key)
{
    // Calculate RMS of original signal first
    float originalRMS = 0.0f;
    int totalSamples = buffer.getNumSamples() * buffer.getNumChannels();
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
        const float* data = buffer.getReadPointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); i++) {
            originalRMS += data[i] * data[i];
        }
    }
    originalRMS = std::sqrt(originalRMS / totalSamples);
    
    // First quantize the audio
    const int numLevels = static_cast<int>(quantizationParameter->load());
    const float quantizationStep = 2.0f / numLevels; // Range is -1 to 1, so total range is 2
    
    // Quantize the buffer first
    for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
        float* data = buffer.getWritePointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); i++) {
            // Quantize each sample to nearest level
            float level = std::round(data[i] / quantizationStep);
            data[i] = level * quantizationStep;
        }
    }
    
    // Generate a fixed key from the string
    std::vector<uint8_t> aesKey(32, 0); // 256-bit key
    for (size_t i = 0; i < static_cast<size_t>(key.length()) && i < 32; i++) {
        aesKey[i] = static_cast<uint8_t>(key[i]);
    }
    
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    
    // Process each channel
    for (int channel = 0; channel < numChannels; channel++) {
        float* data = buffer.getWritePointer(channel);
        
        // Convert float samples to 16-bit integers
        std::vector<int16_t> intSamples(numSamples);
        for (int i = 0; i < numSamples; i++) {
            intSamples[i] = static_cast<int16_t>(data[i] * 32767.0f);
        }
        
        // Convert to bytes for encryption
        std::vector<uint8_t> bytes(intSamples.size() * sizeof(int16_t));
        memcpy(bytes.data(), intSamples.data(), bytes.size());
        
        // Pad to AES block size
        const size_t padding = bytes.size() % AES_BLOCK_SIZE;
        if (padding != 0) {
            const size_t paddingSize = AES_BLOCK_SIZE - padding;
            bytes.resize(bytes.size() + paddingSize, static_cast<uint8_t>(paddingSize));
        }
        
        // Encrypt the bytes
        std::vector<uint8_t> encryptedBytes = encryptBlockECB(bytes, aesKey);
        
        // Convert encrypted bytes back to int16_t samples
        std::vector<int16_t> encryptedSamples(numSamples);
        memcpy(encryptedSamples.data(), encryptedBytes.data(),
               std::min(encryptedBytes.size(), encryptedSamples.size() * sizeof(int16_t)));
        
        // Store encrypted samples back in buffer
        for (int i = 0; i < numSamples; i++) {
            data[i] = static_cast<float>(encryptedSamples[i]) / 32767.0f;
        }
    }
    
    // Calculate RMS of encrypted signal
    float encryptedRMS = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
        const float* data = buffer.getReadPointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); i++) {
            encryptedRMS += data[i] * data[i];
        }
    }
    encryptedRMS = std::sqrt(encryptedRMS / totalSamples);
    
    // Apply normalization
    if (encryptedRMS > 0.0f) {
        const float gainFactor = originalRMS / encryptedRMS;
        for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
            float* data = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); i++) {
                data[i] *= gainFactor;
            }
        }
    }
}

std::vector<uint8_t> JUCECB::encryptBlockECB(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key)
{
    std::vector<uint8_t> encrypted(data.size() + AES_BLOCK_SIZE);
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return encrypted;
    }
    
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key.data(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return encrypted;
    }
    
    int outlen1 = 0;
    int outlen2 = 0;
    
    if (EVP_EncryptUpdate(ctx,
                         encrypted.data(),
                         &outlen1,
                         data.data(),
                         static_cast<int>(data.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return encrypted;
    }
    
    if (EVP_EncryptFinal_ex(ctx, encrypted.data() + outlen1, &outlen2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return encrypted;
    }
    
    EVP_CIPHER_CTX_free(ctx);
    encrypted.resize(static_cast<size_t>(outlen1 + outlen2));
    return encrypted;
}

void JUCECB::loadFile()
{
    WildcardFileFilter wavFilter("*.wav", String(), "WAV files");
    
    FileChooser chooser("Please select a WAV file",
                       File::getSpecialLocation(File::userHomeDirectory),
                       "*.wav",
                       true);
    
    if (chooser.browseForFileToOpen())
    {
        auto file = chooser.getResult();
        
        if (!isValidWavFile(file)) {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                      "Invalid File",
                                      "Please select a valid WAV file.");
            return;
        }
        
        std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));
        
        if (reader != nullptr)
        {
            // Convert to mono if necessary
            originalBuffer.setSize(1, reader->lengthInSamples);
            if (reader->numChannels == 1) {
                reader->read(&originalBuffer, 0, reader->lengthInSamples, 0, true, true);
            }
            else {
                // Mix down to mono
                AudioBuffer<float> tempBuffer(reader->numChannels, reader->lengthInSamples);
                reader->read(&tempBuffer, 0, reader->lengthInSamples, 0, true, true);
                
                // Average all channels
                originalBuffer.clear();
                for (int channel = 0; channel < reader->numChannels; channel++) {
                    originalBuffer.addFrom(0, 0, tempBuffer, channel, 0, reader->lengthInSamples, 1.0f/reader->numChannels);
                }
            }
            
            // Create encrypted buffer (mono)
            encryptedBuffer.setSize(1, reader->lengthInSamples);
            encryptedBuffer.copyFrom(0, 0, originalBuffer, 0, 0, originalBuffer.getNumSamples());
            
            // Encrypt the buffer with quantization
            encryptAudioECB(encryptedBuffer, encryptionKey);
            
            hasLoadedFile = true;
            currentSamplePosition = 0;
            DBG("File loaded successfully in mono");
        }
    }
}


bool JUCECB::isValidWavFile(const File& file)
{
    if (!file.existsAsFile()) return false;
    if (file.getFileExtension().toLowerCase() != ".wav") return false;
    if (!file.hasReadAccess()) return false;
    if (file.getSize() < 44) return false;
    return true;
}

bool JUCECB::checkWavProperties(AudioFormatReader* reader)
{
    if (reader == nullptr) return false;
    
    // Check basic properties
    if (reader->numChannels <= 0 ||
        reader->numChannels > 2 ||  // Only allow mono or stereo
        reader->lengthInSamples <= 0 ||
        reader->sampleRate <= 0) {
        return false;
    }
    
    // Check if sample rate is standard
    const float standardRates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f, 192000.0f };
    bool validRate = false;
    for (float rate : standardRates) {
        if (std::abs(reader->sampleRate - rate) < 1.0f) {
            validRate = true;
            break;
        }
    }
    
    return validRate;
}

void JUCECB::stopNote()
{
    if (noteIsPlaying)
    {
        MidiBuffer midiBuffer;
        midiBuffer.addEvent(MidiMessage::noteOff(1, currentMidiNote), 0);
        isNotePlaying = false;
        noteIsPlaying = false;
    }
}

void JUCECB::startNote()
{
    if (!noteIsPlaying && hasLoadedFile)
    {
        MidiBuffer midiBuffer;
        midiBuffer.addEvent(MidiMessage::noteOn(1, currentMidiNote, (uint8_t)127), 0);
        isNotePlaying = true;
        currentSamplePosition = 0;
        noteIsPlaying = true;
    }
}

void JUCECB::reloadWithNewKey()
{
    // Only proceed if we have a file loaded
    if (!hasLoadedFile) {
        return;
    }

    // Stop any playing notes
    for (auto& voice : voices) {
        voice.isActive = false;
    }
    voices.clear();

    // Create new encrypted version with the new key
    encryptedBuffer.clear();
    encryptedBuffer.setSize(originalBuffer.getNumChannels(), originalBuffer.getNumSamples());
    encryptedBuffer.copyFrom(0, 0, originalBuffer, 0, 0, originalBuffer.getNumSamples());
    
    // Encrypt with new key
    encryptAudioECB(encryptedBuffer, encryptionKey);
    
    currentSamplePosition = 0;
    DBG("Reloaded with new key: " + encryptionKey);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JUCECB();
}
