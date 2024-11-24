/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class JUCECBEditor : public juce::AudioProcessorEditor
{
public:
    JUCECBEditor (JUCECB&);
    ~JUCECBEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCECB& audioProcessor;
    
    TextButton loadButton;
    Slider wetDrySlider;
    Label wetDryLabel;
    TextEditor keyInput;
    Label keyLabel;
    
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> wetDryAttachment;

    void loadButtonClicked();
    void keyInputChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JUCECBEditor)
};
