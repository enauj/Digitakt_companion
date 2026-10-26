#pragma once
#include "PixelEditor.h"

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void resized() override;
private:
    void showSettings(bool recorder=false);
    void timerCallback() override;
    void selectMidi(const juce::String&);
    void selectMidiOutput(const juce::String&);
    juce::File sessionFile() const;
    juce::AudioDeviceManager devices;
    class DevicePlayer final : public juce::AudioProcessorPlayer
    {
    public:
        std::function<void(juce::AudioIODevice*)> onDeviceStart;
        std::function<void()> onDeviceStop;
        void audioDeviceAboutToStart(juce::AudioIODevice* device) override {if(onDeviceStart)onDeviceStart(device);juce::AudioProcessorPlayer::audioDeviceAboutToStart(device);}
        void audioDeviceStopped() override {juce::AudioProcessorPlayer::audioDeviceStopped();if(onDeviceStop)onDeviceStop();}
    } player;
    CompanionProcessor processor;
    std::unique_ptr<PixelEditor> editor;
    juce::String midiIdentifier;
    juce::String outputIdentifier;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    int reconnectTicks=0;
    bool midiDisconnected = false;
    juce::Component::SafePointer<juce::DialogWindow> settings;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
