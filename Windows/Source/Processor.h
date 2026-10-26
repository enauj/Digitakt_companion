#pragma once
#include "PluginRack.h"
#include "PatchState.h"
#include "HardwareMidi.h"
#include "ModulationCurve.h"
#include "MidiMap.h"
#include "TransportClock.h"
#include "SessionRecorder.h"

class CompanionProcessor final : public juce::AudioProcessor
{
public:
    CompanionProcessor();
    ~CompanionProcessor() override;
    const juce::String getName() const override { return "DTX Companion"; }
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void reset() override { keyboard.reset();panic.store(true);modulation.restart();modulation2.restart(); }
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& l) const override {return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()&&(l.getMainInputChannelSet().isDisabled()||(hostingEnabled&&l.getMainInputChannelSet().size()<=64));}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return true;}
    double getTailLengthSeconds() const override {return 5.;}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Current project";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::var state(bool includeRack=true) const;
    bool restore(const juce::var&,bool notifyHost=true);
    int track() const {return selectedTrack.load();}
    int soundTrack() const {return lastSoundTrack.load();}
    int engine(int t=-1) const {auto n=(size_t)(t<0?soundTrack():juce::jlimit(0,3,t));return hostingEnabled&&pluginSources[n].load()?3:engines[n]->getIndex();}
    int channel(int t=-1) const {return 1+juce::roundToInt(channels[(size_t)(t<0?soundTrack():juce::jlimit(0,3,t))]->get()*15.f);}
    bool isPluginTrack(int t=-1) const {return hostingEnabled&&engine(t)==3;}
    juce::String trackName(int) const;
    void selectTrack(int);
    void setTrackEngine(int,int);
    void setTrackChannel(int,int);
    void setChannel(int c) {setTrackChannel(soundTrack(),c);}
    void setPage(int);
    int pageCount(int track) const;
    bool isVoicePage(int t,int p) const {return t>=0&&t<4&&!isPluginTrack(t)&&engine(t)==0&&p==(hostingEnabled?4:3);}
    bool isVoicePage() const {return isVoicePage(track(),page.load());}
    void setCC(int,int);
    std::array<float,8> mixValues(int t=-1) const;
    PluginRack& rackForTrack(int t) {return synthRacks[(size_t)juce::jlimit(0,3,t)];}
    const PluginRack& rackForTrack(int t) const {return synthRacks[(size_t)juce::jlimit(0,3,t)];}
    PluginRack& currentRack() {return rackForTrack(soundTrack());}
    const PluginRack& currentRack() const {return rackForTrack(soundTrack());}
    void setFxBypass(int,bool);
    void setFxWet(int,float);
    juce::String controlName(int) const;
    juce::String controlText(int) const;
    int controlCC(int slot) const;
    int controlChannel() const;
    void setControlChannel(int track,int channel);
    juce::String modulationTargetName(int track,int destination) const;
    bool isModTrack() const {return track()==4||track()==5;}
    ModulationCurve& currentModulation() {return track()==5?modulation2:modulation;}
    const ModulationCurve& currentModulation() const {return track()==5?modulation2:modulation;}
    float controlValue(int) const;
    bool controlEnabled(int) const;
    void setControl(int,float,bool realtime=false);
    void beginGesture(int);
    void endGesture(int);
    void assignMacro(int macro,int rackSlot,int index);
    void resetMacros(int t=-1);
    juce::AudioProcessorParameter* controlParameter(int) const;
    juce::String loadPlugin(int,const HostedDescriptor&,int targetTrack=-1);
    void removePlugin(int,int targetTrack=-1);
    void updateLatency();
    std::array<std::array<juce::AudioParameterFloat*,40>,4> trackSound;
    std::array<juce::AudioParameterChoice*,4> engines;
    std::array<juce::AudioParameterFloat*,4> channels;
    std::array<juce::AudioParameterBool*,4> mono;
    std::array<juce::AudioParameterFloat*,4> glide;
    std::array<std::array<juce::AudioParameterFloat*,7>,4> trackMix;
    std::array<PluginRack,4> synthRacks;
    PluginRack& rack {synthRacks[3]}; // Legacy project/import API.
    std::array<juce::AudioParameterFloat*,40> sound; // Track 1 legacy automation IDs.
    std::array<juce::AudioParameterFloat*,8> mix;
    juce::AudioParameterChoice* engineParameter=nullptr;
    std::array<std::array<std::atomic<int>,8>,4> macroSlot,macroIndex;
    std::array<std::array<juce::String,8>,4> macroIds;
    std::array<std::atomic<int>,8> cc; // Legacy schema field; MidiMap.h owns current fixed addresses.
    std::array<std::atomic<int>,8> trackPages;
    std::array<std::atomic<float>,4> trackPeaks;
    HardwareMidi hardware;
    ModulationCurve modulation,modulation2;
    TransportClock transport;
    SessionRecorder recorder;
    RecorderRouting recorderRouting;
    std::array<std::atomic<int>,2> monitorInputs;
    void setRecorderDevice(const RecorderDevice&);
    RecorderDevice recorderDevice() const {const juce::ScopedLock lock(deviceMutex);return deviceInfo;}
    std::atomic<int> mixerChannel {16},availableInputs {0};
    juce::AudioParameterFloat* masterLevel=nullptr;
    juce::Result startRecording(const juce::File&);
    double currentSampleRate() const {return rate;}
    std::atomic<bool> monitorEnabled {false};
    std::atomic<float> monitorGain {1},alignmentMs {0},monitorPeak {0};
    juce::MidiKeyboardState keyboard;
    std::atomic<int> page {0};
    std::atomic<bool> panic {false};
    std::atomic<float> peak {0};
    std::atomic<double> bpm {120},beats {0};
    std::atomic<bool> playing {false},clockActive {false};
    bool hostingEnabled=false;
    juce::String lastError;
private:
    struct LocalTransport final : juce::AudioPlayHead
    {
        CompanionProcessor& owner;
        explicit LocalTransport(CompanionProcessor& p):owner(p){}
        juce::Optional<PositionInfo> getPosition() const override {PositionInfo i;i.setBpm(owner.bpm.load());i.setPpqPosition(owner.beats.load());i.setIsPlaying(owner.playing.load());i.setTimeSignature(TimeSignature{4,4});return i;}
    } localTransport {*this};
    static constexpr int maximumDelay=192000;
    std::array<void*,4> synths {};
    std::array<juce::AudioBuffer<float>,4> audio,alignment;
    juce::AudioBuffer<float> inputAudio,inputDelay;
    juce::AudioBuffer<float> deviceAudio;
    mutable juce::CriticalSection deviceMutex;
    RecorderDevice deviceInfo;
    std::array<std::atomic<int>,64> physicalToActive;
    std::atomic<bool> legacyMonitorInputs {false};
    std::array<juce::MidiBuffer,2> curveRecord;
    juce::MidiBuffer curveOutput;
    std::array<int,2> lastCurveValue {{-1,-1}},lastCurveCC {{-1,-1}},lastCurveChannel {{-1,-1}};
    std::array<bool,2> lastCurveEnabled {};
    int untilCurveCC=0;
    bool wasRecording=false;
    std::atomic<bool> audioPrepared {false};
    juce::LinearSmoothedValue<float> masterSmooth;
    juce::MidiBuffer clockOutput;
    int inputDelayPosition=0;
    std::array<juce::MidiBuffer,4> routed;
    std::array<int,4> lastEngines {{-1,-1,-1,-1}},delayPosition {};
    std::array<std::atomic<bool>,4> trackPanic;
    std::array<std::atomic<bool>,4> pluginSources;
    std::array<std::atomic<int>,4> trackLatency;
    std::atomic<int> selectedTrack {0},lastSoundTrack {0},maximumLatency {0};
    double rate=48000;
    int64_t sampleCounter=0;
    juce::AudioProcessorParameter* parameterFor(int t,int p,int slot) const;
    void setTrackControl(int t,int p,int slot,float,bool realtime);
};
