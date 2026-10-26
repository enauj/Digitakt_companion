#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

// Digitakt I MIDI implementation, OS 1.51, appendix B.
// No state restoration or device selection emits CCs; only explicit edits do.
class HardwareMidi
{
public:
    HardwareMidi();
    static const char* label(int page, int slot);
    int value(int page, int slot) const;
    int minimum(int p,int s) const {return juce::jmax(0,-offsets[(size_t)(p*8+s)].load());}
    int maximum(int p,int s) const {return 127-juce::jmax(0,offsets[(size_t)(p*8+s)].load());}
    float normalised(int p,int s) const {return (value(p,s)-minimum(p,s))/(float)(maximum(p,s)-minimum(p,s));}
    int offset(int p,int s) const {return offsets[(size_t)(p*8+s)].load();}
    void setOffset(int p,int s,int value);
    bool isKnown(int page, int slot) const;
    void edit(int page, int slot, float normalised);
    void editLevel(int track,float normalised);
    std::array<std::atomic<int>,8> levels;
    std::array<std::atomic<bool>,8> levelKnown;
    juce::MidiBuffer takeMessages();
    void appendMessages(juce::MidiBuffer&,int sample=0);
    juce::var save() const;
    void restore(const juce::var&);
    void discardPending() { pending.store(0);pendingLevels.store(0); }
    std::atomic<bool> active {false}, outputReady {false};
    std::atomic<int> page {0}, fxChannel {16},inputChannel {15};
    std::array<std::atomic<int>,8> trackChannels;
private:
    std::array<std::atomic<int>,32> values;
    std::array<std::atomic<int>,32> offsets;
    std::array<std::atomic<bool>,32> known;
    std::atomic<uint32_t> pending {0};
    std::atomic<uint32_t> pendingLevels {0};
};
