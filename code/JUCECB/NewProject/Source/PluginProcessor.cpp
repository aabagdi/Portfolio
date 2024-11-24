/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JUCECB::JUCECB()
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

void JUCECB::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

  
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool JUCECB::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* JUCECB::createEditor()
{
    return new JUCECBEditor (*this);
}

//==============================================================================
void JUCECB::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void JUCECB::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void JUCECB::encryptAudioECB(AudioBuffer<float>& buffer, const String& key) {
    // Generate key from string
    std::vector<uint8_t> aesKey(32, 0);
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
            // Convert -1.0 to 1.0 range to -32768 to 32767
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
        
        // Convert encrypted bytes directly back to int16_t samples
        std::vector<int16_t> encryptedSamples(numSamples);
        memcpy(encryptedSamples.data(), encryptedBytes.data(),
               std::min(encryptedBytes.size(), encryptedSamples.size() * sizeof(int16_t)));
        
        // Store encrypted samples back in buffer
        for (int i = 0; i < numSamples; ++i) {
            // Store directly without normalization or clamping
            data[i] = static_cast<float>(encryptedSamples[i]) / 32767.0f;
        }
    }
}

std::vector<uint8_t> JUCECB::encryptBlockECB(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> encrypted(data.size() + AES_BLOCK_SIZE); // Add extra space for padding
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return encrypted;
    }
    
    // Disable padding - we handle it manually
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

void JUCECB::loadFile() {
    FileChooser chooser { "Please load a .wav file" };
    
    if (chooser.browseForFileToOpen()) {
        auto file = chooser.getResult();
        
        // Create reader for original file
        std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));
        
        if (reader != nullptr) {
            // Create buffer with correct size from actual file
            AudioBuffer<float> buffer(reader->numChannels, static_cast<int>(reader->lengthInSamples));
            
            // Read the entire file into the buffer
            reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
            
            // Encrypt the buffer
            encryptAudioECB(buffer, "ThisIsALongerEncryptionKey1234567890");
            
            // Create output file
            File outputFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_encrypted.wav");
            
            // Set up WAV format writer
            WavAudioFormat wavFormat;
            
            // Create output stream
            std::unique_ptr<FileOutputStream> fileStream(outputFile.createOutputStream());
            
            if (fileStream != nullptr) {
                std::unique_ptr<AudioFormatWriter> writer(wavFormat.createWriterFor(
                    fileStream.release(),         // FileOutputStream
                    reader->sampleRate,           // sample rate from original
                    buffer.getNumChannels(),      // number of channels
                    16,                           // bits per sample (fixed for our encryption)
                    {},                           // metadata
                    0                             // quality (ignored for WAV)
                ));
                
                if (writer != nullptr) {
                    // Write the encrypted buffer to file using blocks
                    const int blockSize = 4096;
                    const int numBlocks = (buffer.getNumSamples() + blockSize - 1) / blockSize;
                    
                    for (int block = 0; block < numBlocks; ++block) {
                        const int startSample = block * blockSize;
                        const int numSamples = jmin(blockSize,
                                                  buffer.getNumSamples() - startSample);
                        
                        // Write block by block
                        writer->writeFromAudioSampleBuffer(buffer,
                                                         startSample,
                                                         numSamples);
                    }
                    
                    // Ensure all data is written and file is properly closed
                    writer.reset();
                    
                    // Now load the encrypted file into the sampler
                    formatReader = formatManager.createReaderFor(outputFile);
                    
                    if (formatReader != nullptr) {
                        // Clear any existing sounds
                        sampler.clearSounds();
                        
                        BigInteger range;
                        range.setRange(0, 128, true);
                        
                        // Add the encrypted sound with modified parameters
                        SamplerSound* sound = new SamplerSound(
                            "EncryptedSample",
                            *formatReader,
                            range,
                            60,    // root note
                            0.0,   // attack time
                            0.0,   // release time
                            10.0   // maximum sample length
                        );
                        
                        sampler.addSound(sound);
                        
                        DBG("Encrypted file loaded into sampler successfully");
                    } else {
                        DBG("Failed to load encrypted file into sampler");
                    }
                } else {
                    DBG("Failed to create writer");
                }
            } else {
                DBG("Failed to create output stream");
            }
        } else {
            DBG("Failed to read input file");
        }
    }
}

AudioBuffer<float> JUCECB::getAudioBufferFromFile(juce::File file)
{
    AudioBuffer<float> audioBuffer;
    
    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader != nullptr) {
        audioBuffer.setSize(reader->numChannels, reader->lengthInSamples);
        reader->read(&audioBuffer, 0, reader->lengthInSamples, 0, true, true);
    } else {
        // If reading fails, create an empty buffer
        audioBuffer.setSize(2, 0);  // default to stereo
        DBG("Failed to read audio file: " + file.getFullPathName());
    }
    
    return audioBuffer;
}



//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JUCECB();
}
