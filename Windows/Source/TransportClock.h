#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// Audio-clock transport. Standalone sends only generated clock/transport, never
// echoes its MIDI input. A MidiOutput background thread schedules sample offsets.
class TransportClock
{
public:
    enum Source {digitakt=0,companion=1};
    std::atomic<int> source {digitakt};
    std::atomic<double> tempo {120};
    std::atomic<bool> running {false},available {false};
    std::atomic<double> bpm {120},beat {0},displayBpm {120};
    void requestStart() {command.store(1);}
    void requestContinue() {command.store(2);}
    void requestStop() {command.store(3);}
    void setTempo(double value) {if(std::isfinite(value))tempo.store(juce::jlimit(20.,300.,value));}
    void reset()
    {
        if(running.exchange(false)&&source.load()==companion)stopPending.store(true);available.store(false);beat.store(0);command.store(0);
        resetPending.store(true);
    }
    bool input(const juce::MidiMessage& m,double sample,double rate)
    {
        if(source.load()!=digitakt)return false;
        if(m.isMidiStart()){currentBeat=0;beat.store(0);running.store(true);return true;}
        if(m.isMidiContinue())running.store(true);if(m.isMidiStop())running.store(false);
        if(!m.isMidiClock())return false;
        available.store(true);
        if(lastPulse>=0&&(sample-lastPulse>rate||sample<=lastPulse)){historyCount=historyHead=0;}
        lastPulse=sample;history[(size_t)historyHead]=sample;historyHead=(historyHead+1)%25;historyCount=juce::jmin(25,historyCount+1);
        if(historyCount==25)
        {
            double seconds=(sample-history[(size_t)historyHead])/rate; // 24 complete pulse intervals.
            if(seconds>.18&&seconds<3.2){double measured=juce::jlimit(20.,300.,60./seconds);double previous=bpm.load();bpm.store(std::abs(measured-previous)>15?measured:previous*.9+measured*.1);}
        }
        if(running.load())currentBeat+=1./24.;beat.store(currentBeat);return false;
    }
    bool advance(int frames,double rate,juce::MidiBuffer& output)
    {
        output.clear();bool restart=false;int mode=source.load();
        if(stopPending.exchange(false))output.addEvent(juce::MidiMessage::midiStop(),0);
        if(resetPending.exchange(false)){nextPulse=elapsed;lastPulse=-1;historyCount=historyHead=0;lastSource=mode;displayFrames=0;currentBeat=0;}
        int action=command.exchange(0);
        if(lastSource!=mode)
        {
            if(lastSource==companion&&running.load())output.addEvent(juce::MidiMessage::midiStop(),0);
            running.store(false);historyCount=historyHead=0;lastPulse=-1;nextPulse=elapsed;lastSource=mode;
        }
        if(mode==companion)
        {
            bpm.store(tempo.load());available.store(true);
            if(action==1){currentBeat=0;nextPulse=elapsed;running.store(true);restart=true;output.addEvent(juce::MidiMessage::midiStart(),0);}
            if(action==2){running.store(true);nextPulse=elapsed;output.addEvent(juce::MidiMessage::midiContinue(),0);}
            if(action==3){running.store(false);output.addEvent(juce::MidiMessage::midiStop(),0);}
            double interval=rate*60./(bpm.load()*24.);
            if(nextPulse<elapsed)nextPulse=elapsed;
            while(nextPulse<elapsed+frames){output.addEvent(juce::MidiMessage::midiClock(),juce::jlimit(0,frames-1,(int)std::llround(nextPulse-elapsed)));nextPulse+=interval;}
            if(running.load())currentBeat+=frames/rate*bpm.load()/60.;beat.store(currentBeat);
        }
        else available.store(lastPulse>=0&&elapsed+frames-lastPulse<rate);
        elapsed+=frames;displayFrames+=frames;
        if(displayFrames>=rate*.25){displayFrames=0;double rounded=std::round(bpm.load()*10)/10;if(std::abs(displayBpm.load()-rounded)>=.199||mode==companion)displayBpm.store(rounded);}
        return restart;
    }
    double samplePosition() const {return elapsed;}
    juce::var save() const {auto* o=new juce::DynamicObject();o->setProperty("source",source.load());o->setProperty("tempo",tempo.load());return juce::var(o);}
    bool restore(const juce::var& state)
    {
        if(!state.isObject()||!state["source"].isInt()||(int)state["source"]<0||(int)state["source"]>1||!(state["tempo"].isDouble()||state["tempo"].isInt())||!std::isfinite((double)state["tempo"])||(double)state["tempo"]<20||(double)state["tempo"]>300)return false;
        if(source.load()==companion&&running.load())stopPending.store(true);source.store((int)state["source"]);tempo.store((double)state["tempo"]);reset();return true;
    }
private:
    std::atomic<int> command {0};
    std::atomic<bool> resetPending {true};
    std::atomic<bool> stopPending {false};
    std::array<double,25> history {};
    int historyCount=0,historyHead=0,lastSource=-1;
    double elapsed=0,nextPulse=0,lastPulse=-1,currentBeat=0,displayFrames=0;
};
