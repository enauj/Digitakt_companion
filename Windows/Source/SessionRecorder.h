#pragma once
#include "RecorderRouting.h"
#include <array>
#include <atomic>

// All stems share one FIFO and sample counter. Only the worker writes files.
class SessionRecorder final : private juce::TimeSliceClient
{
public:
    static constexpr int capacity=262144;
    SessionRecorder();
    ~SessionRecorder() override;
    juce::Result start(const juce::File& parent,double rate,const RecorderRouting::Plan&,const RecorderDevice&);
    void stop();
    void capture(const juce::AudioBuffer<float>& device,int inputCount,const std::array<juce::AudioBuffer<float>,4>& companion,const juce::AudioBuffer<float>& mix,int frames,const std::array<juce::MidiBuffer,2>& curves,double bpm);
    bool isRecording() const {return active.load();}
    bool hasError() const {return failed.load();}
    int64_t samples() const {return captured.load();}
    juce::File folder() const {const juce::ScopedLock lock(control);return take;}
    double seconds() const {return captured.load()/captureRate.load();}
    bool pendingFinalization() const {return pending.load();}
private:
    struct MidiEvent {double tick=0;int curve=0,channel=1,cc=0,value=0;double tempo=0;};
    int useTimeSlice() override;
    void drainMidi();
    void finish();
    juce::TimeSliceThread worker {"Companion recorder disk writer"};
    mutable juce::CriticalSection control;
    juce::CriticalSection captureLock;
    juce::AbstractFifo audioFifo {capacity},midiFifo {8192};
    juce::AudioBuffer<float> ring;
    std::unique_ptr<MidiEvent[]> midiEvents {std::make_unique<MidiEvent[]>(8192)};
    struct Stream {std::unique_ptr<juce::AudioFormatWriter> writer;int offset=0,kind=0,left=-1,right=-1,channels=1;};
    std::vector<Stream> streams;
    juce::Array<juce::var> sourceManifest;
    RecorderDevice recordedDevice;
    std::atomic<double> captureRate {48000};
    std::array<juce::MidiMessageSequence,3> sequences;
    std::atomic<bool> active {false},failed {false},pending {false};
    std::atomic<int64_t> captured {0};
    double sampleRate=48000,ticks=0,lastTempo=0;
    juce::File take;
    bool pushMidi(const MidiEvent&);
};
