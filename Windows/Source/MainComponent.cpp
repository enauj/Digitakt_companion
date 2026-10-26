#include "MainComponent.h"
#include "TransportSettings.h"
#include "RoutingSettings.h"
#include "RecorderSettings.h"

namespace
{
class HardwareSettings final : public juce::Component, private juce::Timer
{
public:
    HardwareSettings(HardwareMidi& model,juce::String current,std::function<void(juce::String)> selected)
        : hardware(model),currentOutput(std::move(current)),selectOutput(std::move(selected))
    {
        help.setText("DIGITAKT I\nEnable MIDI CONFIG > PORT CONFIG > RECEIVE CC/NRPN on the Digitakt.\nMatch FX CONTROL CH and audio TRACK 1-8 channels under MIDI CONFIG > CHANNELS.\nOnly edited controls are transmitted. Each page has fixed incoming CCs. -- means the value has not been set here.",juce::dontSendNotification);addAndMakeVisible(help);
        outputLabel.setText("DT MIDI output (FX + clock)",juce::dontSendNotification);addAndMakeVisible(outputLabel);
        addAndMakeVisible(output);refreshPorts();
        output.onChange=[this]{int i=output.getSelectedId()-2;currentOutput=juce::isPositiveAndBelow(i,ports.size())?ports[i].identifier:juce::String();selectOutput(currentOutput);};
        for(int i=0;i<9;++i)
        {
            labels[(size_t)i].setText(i==0?"FX CONTROL CH":"Track "+juce::String(i)+" channel",juce::dontSendNotification);addAndMakeVisible(labels[(size_t)i]);
            auto& box=channels[(size_t)i];box.addItem("OFF",1);for(int c=1;c<=16;++c)box.addItem(juce::String(c),c+1);
            box.setSelectedId((i==0?hardware.fxChannel.load():hardware.trackChannels[(size_t)(i-1)].load())+1,juce::dontSendNotification);addAndMakeVisible(box);
            box.onChange=[this,i]{hardware.discardPending();int ch=channels[(size_t)i].getSelectedId()-1;if(i==0)hardware.fxChannel.store(ch);else hardware.trackChannels[(size_t)(i-1)].store(ch);};
        }
        inputLabel.setText("Companion FX input CH",juce::dontSendNotification);addAndMakeVisible(inputLabel);for(int c=1;c<=16;++c)inputChannel.addItem(juce::String(c),c);inputChannel.setSelectedId(hardware.inputChannel.load(),juce::dontSendNotification);inputChannel.onChange=[this]{hardware.inputChannel.store(inputChannel.getSelectedId());};addAndMakeVisible(inputChannel);
        calibrationLabel.setText("Display-to-MIDI correction",juce::dontSendNotification);addAndMakeVisible(calibrationLabel);for(int p=0;p<4;++p)for(int k=0;k<8;++k)calibrationTarget.addItem(juce::String(p==0?"DELAY ":p==1?"REVERB ":"SEND ")+HardwareMidi::label(p,k),1+p*8+k);calibrationTarget.setSelectedId(1);addAndMakeVisible(calibrationTarget);
        correction.setRange(-2,2,1);correction.setSliderStyle(juce::Slider::IncDecButtons);correction.setTextBoxStyle(juce::Slider::TextBoxLeft,false,45,28);correction.setValue(hardware.offset(0,0));addAndMakeVisible(correction);calibrationTarget.onChange=[this]{int i=calibrationTarget.getSelectedId()-1;correction.setValue(hardware.offset(i/8,i%8),juce::dontSendNotification);};correction.onValueChange=[this]{int i=calibrationTarget.getSelectedId()-1;hardware.setOffset(i/8,i%8,(int)correction.getValue());};
        status.setText("FX pages: DELAY / REVERB\nTrack send pages: DLY SEND / REV SEND\nDelay TIME defaults to +1 wire correction for the reported display offset. Other controls default to 0. Companion does not query current hardware values. CC payloads are 0-127; correction can reduce the available display range.",juce::dontSendNotification);addAndMakeVisible(status);
        setSize(760,640);startTimer(2000);
    }
    void resized() override
    {
        help.setBounds(24,15,710,105);outputLabel.setBounds(24,135,180,30);output.setBounds(210,135,520,30);
        labels[0].setBounds(24,193,170,30);channels[0].setBounds(195,193,110,30);inputLabel.setBounds(380,193,195,30);inputChannel.setBounds(600,193,100,30);
        for(int i=1;i<9;++i){int x=24+((i-1)%2)*365,y=251+((i-1)/2)*56;labels[(size_t)i].setBounds(x,y,165,30);channels[(size_t)i].setBounds(x+175,y,150,30);}
        calibrationLabel.setBounds(24,485,180,30);calibrationTarget.setBounds(210,485,320,30);correction.setBounds(550,485,150,30);status.setBounds(24,535,710,90);
    }
private:
    void refreshPorts()
    {
        ports=juce::MidiOutput::getAvailableDevices();output.clear(juce::dontSendNotification);output.addItem("None",1);int chosen=1;
        for(int i=0;i<ports.size();++i){output.addItem(ports[i].name,i+2);if(ports[i].identifier==currentOutput)chosen=i+2;}
        if(chosen==1&&currentOutput.isNotEmpty()){output.addItem("Saved output disconnected",ports.size()+2);chosen=ports.size()+2;}
        output.setSelectedId(chosen,juce::dontSendNotification);
    }
    void timerCallback() override {refreshPorts();}
    HardwareMidi& hardware;
    juce::String currentOutput;
    std::function<void(juce::String)> selectOutput;
    juce::Array<juce::MidiDeviceInfo> ports;
    juce::ComboBox output,inputChannel,calibrationTarget;
    juce::Slider correction;
    std::array<juce::ComboBox,9> channels;
    std::array<juce::Label,9> labels;
    juce::Label help,outputLabel,status,inputLabel,calibrationLabel;
};
class AudioSettings final : public juce::Component, private juce::Timer
{
public:
    AudioSettings(juce::AudioDeviceManager& manager, CompanionProcessor& p, juce::String current, std::function<void(juce::String)> selected)
        : deviceSelector(manager,0,64,0,2,false,false,true,false),processor(p),currentMidi(std::move(current)),selectMidi(std::move(selected))
    {
        deviceViewport.setViewedComponent(&deviceSelector,false);addAndMakeVisible(deviceViewport);midiLabel.setText("MIDI input",juce::dontSendNotification);addAndMakeVisible(midiLabel);addAndMakeVisible(midi);refreshMidi();
        midi.onChange=[this]{int i=midi.getSelectedId()-2;currentMidi=juce::isPositiveAndBelow(i,ports.size())?ports[i].identifier:juce::String();selectMidi(currentMidi);};
        controlTrack=p.soundTrack();channelLabel.setText("T"+juce::String(controlTrack+1)+" MIDI channel",juce::dontSendNotification);addAndMakeVisible(channelLabel);channel.setRange(1,16,1);channel.setSliderStyle(juce::Slider::IncDecButtons);channel.setTextBoxStyle(juce::Slider::TextBoxLeft,false,50,25);channel.setValue(p.channel(controlTrack));channel.onValueChange=[this]{processor.setTrackChannel(controlTrack,(int)channel.getValue());};addAndMakeVisible(channel);
        panic.setButtonText("PANIC / ALL SOUND OFF");panic.onClick=[this]{processor.reset();};addAndMakeVisible(panic);
        help.setText("FIXED CC ADDRESSES - changing pages does not change automation.\nSynth OSC 16-23 / Metal OSC 24-31 / FM OSC 40-47\nFilter 48-55 / Amp 56-63 / Track FX 72-79\nPlugin macros 80-87 / Mix 88-95\nCurve 1 defaults to CH13; Curve 2 to CH14; hardware FX input to CH15.\nRight click tracks 1-7 to change their input channels.\nEnable audio input channels above, then use Routing / latency to monitor the Digitakt return.\nTempo / transport has the Companion master and Digitakt slave options.",juce::dontSendNotification);addAndMakeVisible(help);setSize(760,640);startTimer(2000);
    }
    void resized() override
    {
        deviceViewport.setBounds(16,12,728,330);deviceSelector.setBounds(0,0,710,330);midiLabel.setBounds(24,354,110,28);midi.setBounds(140,354,590,28);channelLabel.setBounds(24,398,140,28);channel.setBounds(175,398,170,28);panic.setBounds(400,398,330,28);help.setBounds(24,445,706,180);
    }
private:
    void refreshMidi(){ports=juce::MidiInput::getAvailableDevices();midi.clear(juce::dontSendNotification);midi.addItem("None",1);int chosen=1;for(int i=0;i<ports.size();++i){midi.addItem(ports[i].name,i+2);if(ports[i].identifier==currentMidi)chosen=i+2;}midi.setSelectedId(chosen,juce::dontSendNotification);}
    void timerCallback() override {refreshMidi();}
    juce::AudioDeviceSelectorComponent deviceSelector;juce::Viewport deviceViewport;CompanionProcessor& processor;juce::String currentMidi;std::function<void(juce::String)> selectMidi;juce::Array<juce::MidiDeviceInfo> ports;juce::ComboBox midi;juce::Slider channel;int controlTrack=0;juce::Label midiLabel,channelLabel,help;juce::TextButton panic;
};

}
MainComponent::MainComponent()
{
    processor.hostingEnabled=true;
    for(int t=0;t<4;++t)processor.setTrackChannel(t,9+t);
    auto state=juce::JSON::parse(sessionFile());processor.restore(state);
    auto audio=juce::parseXML(state.getProperty("audioDevice",{}).toString());
    auto error=devices.initialise(2,2,audio.get(),true);
    player.onDeviceStart=[this](juce::AudioIODevice* device){processor.setRecorderDevice(RecorderDevice::from(*device));};
    player.onDeviceStop=[this]{auto device=processor.recorderDevice();device.connected=false;processor.setRecorderDevice(device);};
    player.setProcessor(&processor);devices.addAudioCallback(&player);
    selectMidi(state.getProperty("midiInput",{}).toString());
    selectMidiOutput(state.getProperty("midiOutput",{}).toString());
    editor=std::make_unique<PixelEditor>(processor);editor->onUtility=[this]{showSettings();};editor->onRecorderSettings=[this]{showSettings(true);};
    addAndMakeVisible(*editor);setSize(960,600);
    startTimerHz(30);
    if(error.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Audio device",error+"\nOpen UTILITY to choose an output.");
}
MainComponent::~MainComponent()
{
    stopTimer();
    if(settings)delete settings.getComponent();editor->closeHostedEditors();
    auto state=processor.state();state.getDynamicObject()->setProperty("midiInput",midiIdentifier);
    state.getDynamicObject()->setProperty("midiOutput",outputIdentifier);
    if(auto xml=devices.createStateXml())state.getDynamicObject()->setProperty("audioDevice",xml->toString());
    auto file=sessionFile();if(file.getParentDirectory().createDirectory().wasOk())file.replaceWithText(juce::JSON::toString(state));
    editor.reset();if(midiIdentifier.isNotEmpty())devices.removeMidiInputDeviceCallback(midiIdentifier,&player);
    devices.removeAudioCallback(&player);player.setMidiOutput(nullptr);if(midiOutput&&processor.transport.source.load()==TransportClock::companion&&processor.playing.load())midiOutput->sendMessageNow(juce::MidiMessage::midiStop());player.setProcessor(nullptr);devices.closeAudioDevice();
}
juce::File MainComponent::sessionFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DigitaktCompanion").getChildFile("windows-session.json");
}
void MainComponent::resized(){if(editor)editor->setBounds(getLocalBounds());}
void MainComponent::selectMidi(const juce::String& id)
{
    if(midiIdentifier.isNotEmpty())devices.removeMidiInputDeviceCallback(midiIdentifier,&player);
    for(auto& port:juce::MidiInput::getAvailableDevices())devices.setMidiInputDeviceEnabled(port.identifier,false);
    processor.reset();midiIdentifier=id;
    if(id.isNotEmpty()){devices.setMidiInputDeviceEnabled(id,true);devices.addMidiInputDeviceCallback(id,&player);}
}
void MainComponent::showSettings(bool recorder)
{
    if(settings){if(recorder)if(auto* tabs=dynamic_cast<juce::TabbedComponent*>(settings->getContentComponent()))tabs->setCurrentTabIndex(4);settings->toFront(true);return;}
    auto* tabs=new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);
    tabs->addTab("Audio / MIDI",juce::Colour(0xff11120b),new AudioSettings(devices,processor,midiIdentifier,[this](juce::String id){selectMidi(id);}),true);
    tabs->addTab("Routing / latency",juce::Colour(0xff11120b),new RoutingSettings(devices,processor),true);
    tabs->addTab("Tempo / transport",juce::Colour(0xff11120b),new TransportSettings(processor),true);
    tabs->addTab("Digitakt I FX / track sends",juce::Colour(0xff11120b),new HardwareSettings(processor.hardware,outputIdentifier,[this](juce::String id){selectMidiOutput(id);}),true);
    tabs->addTab("Recorder inputs",juce::Colour(0xff11120b),new RecorderSettings(processor,&devices),true);
    tabs->setSize(760,675);tabs->setCurrentTabIndex(recorder?4:processor.hardware.active.load()?3:0);
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(tabs);
    options.dialogTitle="DTX Companion - Audio / MIDI / CC";options.dialogBackgroundColour=juce::Colour(0xff11120b);options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.componentToCentreAround=this;settings=options.launchAsync();
}
void MainComponent::selectMidiOutput(const juce::String& id)
{
    processor.hardware.discardPending();player.setMidiOutput(nullptr);if(midiOutput&&processor.transport.source.load()==TransportClock::companion&&processor.playing.load())midiOutput->sendMessageNow(juce::MidiMessage::midiStop());midiOutput.reset();outputIdentifier=id;
    if(id.isNotEmpty())midiOutput=juce::MidiOutput::openDevice(id);
    if(midiOutput){midiOutput->startBackgroundThread();player.setMidiOutput(midiOutput.get());}
    processor.hardware.outputReady.store(midiOutput!=nullptr);
}
void MainComponent::timerCallback()
{
    if(!devices.getCurrentAudioDevice()||!devices.getCurrentAudioDevice()->isPlaying()){auto messages=processor.hardware.takeMessages();if(midiOutput)for(const auto event:messages)midiOutput->sendMessageNow(event.getMessage());}
    if(++reconnectTicks<60)return;reconnectTicks=0;
    if(midiIdentifier.isNotEmpty())
    {
        bool found=false;for(const auto& port:juce::MidiInput::getAvailableDevices())if(port.identifier==midiIdentifier)found=true;
        if(!found&&!midiDisconnected)processor.reset();
        if(found&&!devices.isMidiInputDeviceEnabled(midiIdentifier)){auto id=midiIdentifier;selectMidi(id);}
        midiDisconnected=!found;
    }
    if(outputIdentifier.isNotEmpty())
    {
        bool found=false;for(const auto& port:juce::MidiOutput::getAvailableDevices())if(port.identifier==outputIdentifier)found=true;
        if(!found){player.setMidiOutput(nullptr);midiOutput.reset();processor.hardware.outputReady.store(false);processor.hardware.discardPending();}
        else if(!midiOutput){auto id=outputIdentifier;selectMidiOutput(id);}
    }
}
