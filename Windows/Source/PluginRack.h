#pragma once
#include "ClapHost.h"

class PluginRack
{
public:
    PluginRack();
    ~PluginRack();
    std::vector<HostedDescriptor> discover(const juce::File&);
    juce::String load(int slot, const HostedDescriptor&, const juce::MemoryBlock* saved = nullptr);
    void remove(int slot);
    void prepare(double, int);
    void release();
    void panic();
    void process(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool hostedSource, const std::array<float, 8>& mix, juce::AudioPlayHead*, const std::array<float,8>& fxOffsets = {});
    std::shared_ptr<juce::AudioProcessor> instance(int slot) const;
    juce::String name(int slot) const;
    juce::String parameterID(int slot, int index) const;
    void control(int slot, int index, float value, bool realtime = false);
    int parameterIndex(int slot, const juce::String& id) const;
    int latency(bool hostedSource, const std::array<float, 8>& mix) const;
    juce::var save() const;
    juce::String restore(const juce::var&);
private:
    struct Slot
    {
        std::shared_ptr<juce::AudioProcessor> plugin;
        HostedDescriptor descriptor;
        juce::MemoryBlock missingState;
        juce::AudioBuffer<float> dry, delay;
        int delayPosition = 0, delaySamples = 0;
        void prepare(double, int);
    };
    mutable juce::CriticalSection mutex;
    std::array<Slot, 3> slots;
    juce::AudioPluginFormatManager formats;
    double sampleRate = 48000;
    int blockSize = 512;
};
