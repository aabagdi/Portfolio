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
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
         #endif
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        #endif
    ),
    parameters(*this, nullptr, "Parameters",
        {
            std::make_unique<AudioParameterFloat>(
                "wetdry",    // parameter ID
                "Mix",       // parameter name
                0.0f,        // minimum value
                1.0f,        // maximum value
                0.0f)        // default value (start with dry signal)
        })
{
    wetDryParameter = parameters.getRawParameterValue("wetdry");
    
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    formatManager.registerBasicFormats();
    
    for(int i = 0; i < numVoices; i++) {
        sampler.addVoice(new SamplerVoice());
    }
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

const juce::String JUCECB::getProgramName (int index)
{
    return {};
}

void JUCECB::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void JUCECB::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampler.setCurrentPlaybackSampleRate(sampleRate);
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
#endif

void JUCECB::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (!hasLoadedFile) {
        buffer.clear();
        return;
    }

    juce::ScopedNoDenormals noDenormals;
    
    // Process MIDI messages to update note state and pitch
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            isNotePlaying = true;
            currentSamplePosition = 0;  // Reset position when note starts
            currentMidiNote = msg.getNoteNumber();
            // Calculate playback rate based on MIDI note
            playbackRate = std::pow(2.0, (currentMidiNote - midiRootNote) / 12.0);
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == currentMidiNote) {
            isNotePlaying = false;
        }
    }
    
    // Clear the output buffer
    buffer.clear();
    
    // Only process audio if a note is playing and we haven't reached the end
    if (isNotePlaying && currentSamplePosition < originalBuffer.getNumSamples()) {
        // Create temporary buffer for sampler output
        AudioBuffer<float> samplerBuffer(buffer.getNumChannels(), buffer.getNumSamples());
        samplerBuffer.clear();
        
        // Get the wet/dry mix
        float wetMix = parameters.getRawParameterValue("wetdry")->load();
        float dryMix = 1.0f - wetMix;
        
        // Mix in dry signal if needed
        if (dryMix > 0.0f) {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                float* channelData = buffer.getWritePointer(channel);
                const float* originalData = originalBuffer.getReadPointer(channel);
                
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                    // Calculate the position in the original buffer with pitch adjustment
                    double readPosition = currentSamplePosition + (sample * playbackRate);
                    
                    // Check if we've reached the end of the buffer
                    if (readPosition >= originalBuffer.getNumSamples()) {
                        // Stop playing if we've reached the end
                        if (sample == 0) {
                            isNotePlaying = false;
                        }
                        break;
                    }
                    
                    // Linear interpolation between samples
                    int pos1 = static_cast<int>(readPosition);
                    int pos2 = pos1 + 1;
                    float fraction = static_cast<float>(readPosition - pos1);
                    
                    float sample1 = originalData[pos1];
                    float sample2 = pos2 < originalBuffer.getNumSamples() ? originalData[pos2] : sample1;
                    
                    channelData[sample] = (sample1 + (sample2 - sample1) * fraction) * dryMix;
                }
            }
        }
        
        // Process sampler (wet/encrypted signal)
        if (wetMix > 0.0f) {
            sampler.renderNextBlock(samplerBuffer, midiMessages, 0, buffer.getNumSamples());
            
            // Mix in the wet signal
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                float* outputData = buffer.getWritePointer(channel);
                const float* wetData = samplerBuffer.getReadPointer(channel);
                
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                    outputData[sample] += wetData[sample] * wetMix;
                }
            }
        }
        
        // Update position based on playback rate
        currentSamplePosition += buffer.getNumSamples() * playbackRate;
    }
}

//==============================================================================
bool JUCECB::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JUCECB::createEditor()
{
    return new JUCECBEditor (*this);
}

//==============================================================================
void JUCECB::getStateInformation (juce::MemoryBlock& destData)
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
    // Generate a fixed key from the string
    std::vector<uint8_t> aesKey(32, 0); // 256-bit key
    for (size_t i = 0; i < static_cast<size_t>(key.length()) && i < 32; ++i) {
        aesKey[i] = static_cast<uint8_t>(key[i]);
    }
    
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    
    // Process each channel
    for (int channel = 0; channel < numChannels; ++channel) {
        float* data = buffer.getWritePointer(channel);
        
        // Convert float samples to 16-bit integers
        std::vector<int16_t> intSamples(numSamples);
        for (int i = 0; i < numSamples; ++i) {
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
        for (int i = 0; i < numSamples; ++i) {
            data[i] = static_cast<float>(encryptedSamples[i]) / 32767.0f;
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
            originalBuffer.setSize(reader->numChannels, reader->lengthInSamples);
            reader->read(&originalBuffer, 0, reader->lengthInSamples, 0, true, true);
            
            // Create a copy for encryption
            AudioBuffer<float> encryptedBuffer;
            encryptedBuffer.setSize(reader->numChannels, reader->lengthInSamples);
            encryptedBuffer.copyFrom(0, 0, originalBuffer, 0, 0, originalBuffer.getNumSamples());
            if (originalBuffer.getNumChannels() > 1) {
                encryptedBuffer.copyFrom(1, 0, originalBuffer, 1, 0, originalBuffer.getNumSamples());
            }
            
            // Encrypt the copy
            encryptAudioECB(encryptedBuffer, encryptionKey);
            
            // Save encrypted version to file (necessary for sampler)
            File outputFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_encrypted.wav");
            
            WavAudioFormat wavFormat;
            std::unique_ptr<FileOutputStream> fileStream(outputFile.createOutputStream());
            
            if (fileStream != nullptr)
            {
                std::unique_ptr<AudioFormatWriter> writer(wavFormat.createWriterFor(
                    fileStream.release(),
                    reader->sampleRate,
                    encryptedBuffer.getNumChannels(),
                    16,
                    {},
                    0
                ));
                
                if (writer != nullptr)
                {
                    writer->writeFromAudioSampleBuffer(encryptedBuffer, 0, encryptedBuffer.getNumSamples());
                    writer.reset();
                    
                    // Load encrypted file into sampler
                    formatReader.reset(formatManager.createReaderFor(outputFile));
                    
                    if (formatReader != nullptr)
                    {
                        sampler.clearSounds();
                        
                        BigInteger range;
                        range.setRange(0, 128, true);
                        
                        sampler.addSound(new SamplerSound(
                            "EncryptedSample",
                            *formatReader,
                            range,
                            60,    // root note
                            0.01,  // attack time
                            0.01,  // release time
                            10.0   // maximum sample length
                        ));
                        
                        hasLoadedFile = true;
                        currentSamplePosition = 0;
                        DBG("File loaded successfully");
                    }
                }
            }
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
        sampler.noteOff(1, currentMidiNote, 1.0f, true);
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
        sampler.noteOn(1, currentMidiNote, 1.0f);
        noteIsPlaying = true;
    }
}

void JUCECB::reloadWithNewKey()
{
    // Stop any playing notes
    stopNote();
    
    // Create a new encrypted version with the new key
    AudioBuffer<float> encryptedBuffer;
    encryptedBuffer.setSize(originalBuffer.getNumChannels(), originalBuffer.getNumSamples());
    encryptedBuffer.copyFrom(0, 0, originalBuffer, 0, 0, originalBuffer.getNumSamples());
    if (originalBuffer.getNumChannels() > 1) {
        encryptedBuffer.copyFrom(1, 0, originalBuffer, 1, 0, originalBuffer.getNumSamples());
    }
    
    // Encrypt with new key
    encryptAudioECB(encryptedBuffer, encryptionKey);
    
    // Save to temporary file
    File tempFile = File::getSpecialLocation(File::tempDirectory)
                        .getNonexistentChildFile("temp_encrypted", ".wav");
    
    WavAudioFormat wavFormat;
    std::unique_ptr<FileOutputStream> fileStream(tempFile.createOutputStream());
    
    if (fileStream != nullptr) {
        std::unique_ptr<AudioFormatWriter> writer(wavFormat.createWriterFor(
            fileStream.release(),
            getSampleRate(),  // Use AudioProcessor's getSampleRate()
            encryptedBuffer.getNumChannels(),
            16,
            {},
            0
        ));
        
        if (writer != nullptr) {
            writer->writeFromAudioSampleBuffer(encryptedBuffer, 0, encryptedBuffer.getNumSamples());
            writer.reset();
            
            // Load new encrypted file into sampler
            formatReader.reset(formatManager.createReaderFor(tempFile));
            
            if (formatReader != nullptr) {
                sampler.clearSounds();
                
                BigInteger range;
                range.setRange(0, 128, true);
                
                sampler.addSound(new SamplerSound(
                    "EncryptedSample",
                    *formatReader,
                    range,
                    60,    // root note
                    0.01,  // attack time
                    0.01,  // release time
                    10.0   // maximum sample length
                ));
                
                currentSamplePosition = 0;
                DBG("Reloaded with new key: " + encryptionKey);
            }
        }
    }
    
    // Clean up temporary file
    tempFile.deleteFile();
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JUCECB();
}
