#pragma once
#include "Processor.h"

class TransportSettings final : public juce::Component,private juce::Timer
{
public:
    explicit TransportSettings(CompanionProcessor& p):processor(p)
    {
        sourceLabel.setText("Clock / transport source",juce::dontSendNotification);addAndMakeVisible(sourceLabel);
        source.addItem("Digitakt (receive MIDI clock / transport)",1);source.addItem("Companion (send clock / transport)",2);source.setSelectedId(p.transport.source.load()+1,juce::dontSendNotification);addAndMakeVisible(source);
        source.onChange=[this]{processor.transport.source.store(source.getSelectedId()-1);refresh();};
        tempoLabel.setText("Companion tempo (BPM)",juce::dontSendNotification);addAndMakeVisible(tempoLabel);tempo.setRange(20,300,.1);tempo.setSliderStyle(juce::Slider::LinearHorizontal);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,28);tempo.setValue(p.transport.tempo.load());tempo.onValueChange=[this]{processor.transport.setTempo(tempo.getValue());};addAndMakeVisible(tempo);
        start.setButtonText("START / RESTART");resume.setButtonText("CONTINUE");stop.setButtonText("STOP");start.onClick=[this]{processor.transport.requestStart();};resume.onClick=[this]{processor.transport.requestContinue();};stop.onClick=[this]{processor.transport.requestStop();};for(auto* b:{&start,&resume,&stop})addAndMakeVisible(b);
        help.setText(p.hostingEnabled?"Select the Digitakt MIDI output in Utility.\nCompanion master: enable CLOCK RECEIVE and TRANSPORT RECEIVE on the DT. Disable DT CLOCK SEND to avoid two masters.\nDigitakt master: use CLOCK SEND / TRANSPORT SEND on the DT.\nClock generation uses the audio timeline and the MIDI output scheduler. It does not remove device audio latency.":"The DAW controls tempo and transport for this plugin.\nUse the DAW's BPM and play/stop controls.\nStandalone Companion can instead act as MIDI clock master.",juce::dontSendNotification);addAndMakeVisible(help);addAndMakeVisible(status);
        source.setEnabled(p.hostingEnabled);setSize(650,395);refresh();startTimer(250);
    }
    void resized() override
    {
        sourceLabel.setBounds(20,15,610,25);source.setBounds(20,45,610,30);tempoLabel.setBounds(20,90,200,28);tempo.setBounds(220,90,410,28);
        start.setBounds(20,140,195,34);resume.setBounds(227,140,195,34);stop.setBounds(435,140,195,34);status.setBounds(20,190,610,30);help.setBounds(20,230,610,150);
    }
private:
    void timerCallback() override {refresh();}
    void refresh()
    {
        bool master=processor.hostingEnabled&&processor.transport.source.load()==TransportClock::companion;tempo.setEnabled(master);for(auto* b:{&start,&resume,&stop})b->setEnabled(master);
        status.setText(juce::String(processor.hostingEnabled?processor.transport.displayBpm.load():processor.bpm.load(),1)+" BPM / "+(processor.playing.load()?"PLAYING":"STOPPED")+(master?(processor.hardware.outputReady.load()?" / MIDI OUTPUT READY":" / CHOOSE MIDI OUTPUT IN UTILITY"):""),juce::dontSendNotification);
    }
    CompanionProcessor& processor;juce::Label sourceLabel,tempoLabel,help,status;juce::ComboBox source;juce::Slider tempo;juce::TextButton start,resume,stop;
};
