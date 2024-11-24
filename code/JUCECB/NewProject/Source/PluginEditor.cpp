/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JUCECBEditor::JUCECBEditor (JUCECB& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    loadButton.onClick = [&]() { audioProcessor.loadFile(); };
    addAndMakeVisible(loadButton);
    
    setSize (400, 300);
}

JUCECBEditor::~JUCECBEditor()
{
}

//==============================================================================
void JUCECBEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void JUCECBEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    loadButton.setBounds(getWidth() / 2 - 50, getHeight() / 2 - 50, 100, 100);
}
