#include "PluginProcessor.h"
#include "PluginEditor.h"

JUCECBEditor::JUCECBEditor (JUCECB& p)
: AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set up load button
    loadButton.setButtonText("Load .wav file");
    loadButton.onClick = [this] { loadButtonClicked(); };
    addAndMakeVisible(loadButton);
    
    // Set up wet/dry slider
    wetDrySlider.setSliderStyle(Slider::LinearHorizontal);
    wetDrySlider.setRange(0.0f, 1.0f, 0.01f);
    wetDrySlider.setTextBoxStyle(Slider::TextBoxRight, false, 70, 20);
    wetDrySlider.setColour(Slider::textBoxTextColourId, Colours::white);
    wetDrySlider.setColour(Slider::trackColourId, Colours::lightblue);
    addAndMakeVisible(wetDrySlider);
    
    wetDryLabel.setText("Dry/Wet", dontSendNotification);
    wetDryLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(wetDryLabel);
    
    // Connect slider to parameter
    wetDryAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
                                                                              audioProcessor.parameters, "wetdry", wetDrySlider));
    
    // Set up key input
    keyInput.setMultiLine(false);
    keyInput.setReturnKeyStartsNewLine(false);
    keyInput.setScrollbarsShown(false);
    keyInput.setText(audioProcessor.getCurrentKey());
    keyInput.onTextChange = [this] { keyInputChanged(); };
    keyInput.setColour(TextEditor::textColourId, Colours::white);
    keyInput.setColour(TextEditor::backgroundColourId, Colours::darkgrey);
    addAndMakeVisible(keyInput);
    
    keyLabel.setText("Encryption Key", dontSendNotification);
    keyLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(keyLabel);
    
    // Set up gain slider
    gainSlider.setSliderStyle(Slider::LinearHorizontal);
    gainSlider.setRange(-48.0f, 12.0f, 0.1f);
    gainSlider.setTextBoxStyle(Slider::TextBoxRight, false, 70, 20);
    gainSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainSlider.setColour(Slider::trackColourId, Colours::lightblue);
    addAndMakeVisible(gainSlider);
    
    gainLabel.setText("Gain (dB)", dontSendNotification);
    gainLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(gainLabel);
    
    // Connect gain slider to parameter
    gainAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(audioProcessor.parameters, "gain", gainSlider));
    
    // Adjust window size to accommodate new control
    setSize(400, 240);  // Made window slightly taller
}

JUCECBEditor::~JUCECBEditor()
{
}

void JUCECBEditor::paint(juce::Graphics& g)
{
    g.fillAll(Colours::darkgrey);
    
    g.setColour(Colours::white);
    g.setFont(15.0f);
    g.drawText("WAV ECB Encryptor", getLocalBounds().removeFromTop(30),
               Justification::centred, true);
}

void JUCECBEditor::resized()
{
    // Define layout constants
    const int padding = 20;
    const int buttonHeight = 30;
    const int sliderHeight = 25;
    const int labelWidth = 100;
    
    auto area = getLocalBounds().reduced(padding);
    
    // Title space
    area.removeFromTop(30);
    
    // Load button
    loadButton.setBounds(area.removeFromTop(buttonHeight).reduced(50, 0));
    area.removeFromTop(10); // spacing
    
    // Wet/Dry slider
    auto sliderArea = area.removeFromTop(sliderHeight);
    wetDryLabel.setBounds(sliderArea.removeFromLeft(labelWidth));
    wetDrySlider.setBounds(sliderArea);
    area.removeFromTop(10); // spacing
    
    // Gain slider
    auto gainArea = area.removeFromTop(sliderHeight);
    gainLabel.setBounds(gainArea.removeFromLeft(labelWidth));
    gainSlider.setBounds(gainArea);
    area.removeFromTop(10); // spacing
    
    // Key input
    auto keyArea = area.removeFromTop(buttonHeight);
    keyLabel.setBounds(keyArea.removeFromLeft(labelWidth));
    keyInput.setBounds(keyArea);
}

void JUCECBEditor::loadButtonClicked()
{
    audioProcessor.loadFile();
}

void JUCECBEditor::keyInputChanged()
{
    String newKey = keyInput.getText();
    if (newKey.isNotEmpty()) {
        audioProcessor.setEncryptionKey(newKey);
        DBG("Key changed to: " + newKey);
    } else {
        // If empty, set a default key
        keyInput.setText("DefaultKey123", dontSendNotification);
        audioProcessor.setEncryptionKey("DefaultKey123");
        DBG("Key reset to default");
    }
}
