#pragma once
#include "Processor.h"

class RoutingSettings final:public juce::Component,private juce::Timer
{
public:
    RoutingSettings(juce::AudioDeviceManager& d,CompanionProcessor& p):devices(d),processor(p)
    {
        monitor.setButtonText("Monitor assigned device return inputs");monitor.setToggleState(p.monitorEnabled.load(),juce::dontSendNotification);monitor.onClick=[this]{processor.monitorEnabled.store(monitor.getToggleState());};addAndMakeVisible(monitor);
        gainLabel.setText("Return level",juce::dontSendNotification);addAndMakeVisible(gainLabel);gain.setRange(0,200,.1);gain.setTextValueSuffix(" %");gain.setValue(p.monitorGain.load()*100);gain.onValueChange=[this]{processor.monitorGain.store((float)gain.getValue()/100);};addAndMakeVisible(gain);
        alignmentLabel.setText("Relative timing adjustment",juce::dontSendNotification);addAndMakeVisible(alignmentLabel);alignment.setRange(-250,250,.1);alignment.setTextValueSuffix(" ms");alignment.setValue(p.alignmentMs.load());alignment.onValueChange=[this]{processor.alignmentMs.store((float)alignment.getValue());};addAndMakeVisible(alignment);
        help.setText("Select the Digitakt input and output in Audio / MIDI and enable the input channels. Monitoring defaults OFF for new projects.\n\nFor a USB return setup, select USB AUDIO/MIDI on the Digitakt, INT TO MAIN = OFF, USB TO MAIN = 0 dB, then listen to its main/headphone output. Use a recording source that excludes the USB playback return to avoid a feedback loop.\n\nThis combines the Digitakt stereo mix and Companion tracks in one output. For separate hardware stems, select an audio driver, then assign any exposed channels in Recorder inputs, including external inputs.\n\nPositive timing adjustment delays Companion synths. Negative delays the monitored Digitakt return. Compare a sharp click from each and adjust by ear or measurement. Driver buffer size and input/output latency still matter; no automatic timing correction is assumed.",juce::dontSendNotification);addAndMakeVisible(help);addAndMakeVisible(status);setSize(760,640);startTimer(250);refresh();
    }
    void resized() override{monitor.setBounds(24,20,710,32);gainLabel.setBounds(24,75,225,30);gain.setBounds(275,75,455,30);alignmentLabel.setBounds(24,130,225,30);alignment.setBounds(275,130,455,30);status.setBounds(24,185,710,85);help.setBounds(24,285,710,335);}
private:
    void refresh()
    {
        juce::String text="No audio device selected";if(auto* d=devices.getCurrentAudioDevice()){double r=d->getCurrentSampleRate();text="Buffer: "+juce::String(d->getCurrentBufferSizeSamples())+" samples / input latency: "+juce::String(d->getInputLatencyInSamples()*1000/juce::jmax(1.,r),2)+" ms / output latency: "+juce::String(d->getOutputLatencyInSamples()*1000/juce::jmax(1.,r),2)+" ms\nActive input channels: "+juce::String(d->getActiveInputChannels().countNumberOfSetBits())+" / return peak: "+juce::String(juce::Decibels::gainToDecibels(processor.monitorPeak.load(),-100.f),1)+" dBFS";}status.setText(text,juce::dontSendNotification);
    }
    void timerCallback() override {refresh();}
    juce::AudioDeviceManager& devices;CompanionProcessor& processor;juce::ToggleButton monitor;juce::Slider gain,alignment;juce::Label gainLabel,alignmentLabel,status,help;
};

