#pragma once
#include "Processor.h"

class RecorderSettings final : public juce::Component,private juce::Timer
{
public:
    RecorderSettings(CompanionProcessor& p,juce::AudioDeviceManager* d=nullptr):processor(p),devices(d)
    {
        addAndMakeVisible(deviceLabel);for(auto* text:{&inputList,&outputList}){text->setMultiLine(true);text->setReadOnly(true);text->setScrollbarsShown(true);text->setFont(juce::Font(juce::FontOptions(12)));addAndMakeVisible(text);}
        add.setButtonText("Add lane");mono.setButtonText("All mono");pairs.setButtonText("All pairs");clear.setButtonText("Clear lanes");enable.setButtonText("Enable assigned inputs");mix.setButtonText("Record Companion mix");
        for(auto* button:{&add,&mono,&pairs,&clear,&enable})addAndMakeVisible(button);addAndMakeVisible(mix);
        add.onClick=[this]{if(!processor.recorder.isRecording())processor.recorderRouting.add();};mono.onClick=[this]{if(!processor.recorder.isRecording())processor.recorderRouting.assignAll(processor.recorderDevice(),false);};pairs.onClick=[this]{if(!processor.recorder.isRecording())processor.recorderRouting.assignAll(processor.recorderDevice(),true);};clear.onClick=[this]{if(processor.recorder.isRecording())return;auto plan=processor.recorderRouting.snapshot();plan.lanes.clear();processor.recorderRouting.replace(std::move(plan));};mix.onClick=[this]{if(!processor.recorder.isRecording())processor.recorderRouting.setRecordMix(mix.getToggleState());};
        enable.onClick=[this]{enableAssigned();};
        headings.setText("ARM   FILE / LANE NAME             FORMAT       INPUT / LEFT                         RIGHT (STEREO)",juce::dontSendNotification);addAndMakeVisible(headings);
        viewport.setViewedComponent(&rows,false);addAndMakeVisible(viewport);addAndMakeVisible(status);
        for(int i=0;i<2;++i){monitorLabels[(size_t)i].setText(i==0?"Monitor left":"Monitor right",juce::dontSendNotification);addAndMakeVisible(monitorLabels[(size_t)i]);addAndMakeVisible(monitor[(size_t)i]);monitor[(size_t)i].onChange=[this,i]{if(!processor.recorder.isRecording())processor.monitorInputs[(size_t)i].store(monitor[(size_t)i].getSelectedId()-2);};}
        setSize(760,640);refresh();startTimer(500);
    }
    void resized() override
    {
        deviceLabel.setBounds(18,8,724,30);inputList.setBounds(18,40,352,110);outputList.setBounds(388,40,352,110);add.setBounds(18,162,76,28);mono.setBounds(100,162,82,28);pairs.setBounds(188,162,82,28);clear.setBounds(276,162,86,28);enable.setBounds(368,162,166,28);mix.setBounds(540,162,205,28);
        headings.setBounds(18,198,724,26);viewport.setBounds(18,227,724,335);rows.setSize(706,juce::jmax(335,(int)laneRows.size()*38));for(size_t i=0;i<laneRows.size();++i)laneRows[i]->setBounds(0,(int)i*38,706,36);
        for(int i=0;i<2;++i){int x=18+i*370;monitorLabels[(size_t)i].setBounds(x,574,95,28);monitor[(size_t)i].setBounds(x+98,574,254,28);}status.setBounds(18,608,724,28);
    }
private:
    struct LaneRow final : juce::Component
    {
        RecorderSettings& owner;int index;juce::ToggleButton armed;juce::TextEditor name;juce::ComboBox format,left,right;juce::TextButton remove {"X"};juce::String inventory;
        LaneRow(RecorderSettings& o,int i):owner(o),index(i)
        {
            for(juce::Component* c:std::array<juce::Component*,6>{{&armed,&name,&format,&left,&right,&remove}})addAndMakeVisible(c);format.addItem("Mono",1);format.addItem("Stereo",2);
            auto edit=[this](auto function){if(owner.processor.recorder.isRecording())return;auto plan=owner.processor.recorderRouting.snapshot();if(index>=(int)plan.lanes.size())return;auto lane=plan.lanes[(size_t)index];function(lane);owner.processor.recorderRouting.edit(index,lane);};
            armed.onClick=[this,edit]{edit([this](auto& lane){lane.armed=armed.getToggleState();});};name.onTextChange=[this,edit]{edit([this](auto& lane){lane.name=name.getText().substring(0,64);});};format.onChange=[this,edit]{edit([this](auto& lane){lane.stereo=format.getSelectedId()==2;});};
            left.onChange=[this,edit]{if(left.getSelectedId()<1000)edit([this](auto& lane){lane.bind(0,left.getSelectedId()-2,owner.processor.recorderDevice());});};right.onChange=[this,edit]{if(right.getSelectedId()<1000)edit([this](auto& lane){lane.bind(1,right.getSelectedId()-2,owner.processor.recorderDevice());});};remove.onClick=[this]{if(!owner.processor.recorder.isRecording())owner.processor.recorderRouting.remove(index);};
        }
        void resized() override {armed.setBounds(0,4,24,27);name.setBounds(26,4,151,27);format.setBounds(183,4,73,27);left.setBounds(262,4,202,27);right.setBounds(470,4,202,27);remove.setBounds(678,4,26,27);}
        void sync(const RecorderLane& lane,const RecorderDevice& device)
        {
            armed.setToggleState(lane.armed,juce::dontSendNotification);if(!name.hasKeyboardFocus(true))name.setText(lane.name,false);format.setSelectedId(lane.stereo?2:1,juce::dontSendNotification);
            auto key=device.key()+device.inputs.joinIntoString("|")+device.enabledInputs.toString(16);bool changed=inventory!=key;
            for(int side=0;side<2;++side){auto& box=side==0?left:right;int physical=lane.physical(side,device);int saved=side==0?lane.left:lane.right;int selected=physical>=0?physical+2:saved>=0?1000:1;if(changed||box.getNumItems()==0||box.getSelectedId()!=selected){box.clear(juce::dontSendNotification);box.addItem("None",1);for(int i=0;i<juce::jmin(64,device.inputs.size());++i)box.addItem(juce::String(i+1)+": "+device.inputs[i]+(device.enabledInputs[i]?"":" [disabled]"),i+2);if(selected==1000)box.addItem("Saved: "+(side==0?lane.leftName:lane.rightName)+" [unavailable]",1000);box.setSelectedId(selected,juce::dontSendNotification);}}inventory=key;right.setEnabled(lane.stereo);
        }
    };
    void enableAssigned()
    {
        if(!devices||processor.recorder.isRecording())return;auto device=processor.recorderDevice();auto setup=devices->getAudioDeviceSetup();for(const auto& lane:processor.recorderRouting.snapshot().lanes)if(lane.armed)for(int side=0;side<(lane.stereo?2:1);++side){int input=lane.physical(side,device);if(input>=0)setup.inputChannels.setBit(input);}for(auto& input:processor.monitorInputs)if(input.load()>=0)setup.inputChannels.setBit(input.load());setup.useDefaultInputChannels=false;deviceError=devices->setAudioDeviceSetup(setup,true);refresh();
    }
    void refresh()
    {
        for(auto& row:laneRows)if(row->left.isPopupActive()||row->right.isPopupActive()||row->format.isPopupActive())return;for(auto& box:monitor)if(box.isPopupActive())return;
        auto device=processor.recorderDevice();auto plan=processor.recorderRouting.snapshot();bool recording=processor.recorder.isRecording();deviceLabel.setText((device.name.isEmpty()?"No audio device":device.backend+" / "+device.name)+" — "+juce::String(device.inputs.size())+" inputs / "+juce::String(device.outputs.size())+" outputs",juce::dontSendNotification);
        juce::String inputText="DEVICE INPUTS\n",outputText="DEVICE OUTPUTS (playback destinations)\n";for(int i=0;i<device.inputs.size();++i)inputText+=juce::String(i+1)+": "+device.inputs[i]+(device.enabledInputs[i]?" [on]":" [disabled]")+"\n";for(int i=0;i<device.outputs.size();++i)outputText+=juce::String(i+1)+": "+device.outputs[i]+(device.enabledOutputs[i]?" [on]":" [disabled]")+"\n";if(inputList.getText()!=inputText)inputList.setText(inputText,false);if(outputList.getText()!=outputText)outputList.setText(outputText,false);
        if(laneRows.size()!=plan.lanes.size()){laneRows.clear();for(size_t i=0;i<plan.lanes.size();++i){auto row=std::make_unique<LaneRow>(*this,(int)i);rows.addAndMakeVisible(*row);laneRows.push_back(std::move(row));}resized();}for(size_t i=0;i<laneRows.size();++i)laneRows[i]->sync(plan.lanes[i],device);rows.setEnabled(!recording&&processor.hostingEnabled);
        for(auto* button:{&add,&mono,&pairs,&clear,&enable})button->setEnabled(!recording&&processor.hostingEnabled&&(button!=&enable||devices!=nullptr));add.setEnabled(!recording&&processor.hostingEnabled&&plan.lanes.size()<64);mix.setToggleState(plan.recordMix,juce::dontSendNotification);mix.setEnabled(!recording);
        auto key=device.key()+device.inputs.joinIntoString("|")+device.enabledInputs.toString(16);for(int side=0;side<2;++side){auto& box=monitor[(size_t)side];int selected=processor.monitorInputs[(size_t)side].load();if(inventory!=key||box.getNumItems()==0||box.getSelectedId()!=selected+2){box.clear(juce::dontSendNotification);box.addItem("None",1);for(int i=0;i<juce::jmin(64,device.inputs.size());++i)box.addItem(juce::String(i+1)+": "+device.inputs[i]+(device.enabledInputs[i]?"":" [disabled]"),i+2);if(selected>=device.inputs.size())box.addItem("Saved input [unavailable]",selected+2);box.setSelectedId(selected+2,juce::dontSendNotification);}box.setEnabled(processor.hostingEnabled&&!recording);}inventory=key;
        status.setText(deviceError.isNotEmpty()?deviceError:recording?"Recording — assignments frozen until STOP.":"Up to 64 named device lanes + Companion 1–4 + optional mix + two MIDI CC lanes.",juce::dontSendNotification);
    }
    void timerCallback() override {refresh();}
    CompanionProcessor& processor;juce::AudioDeviceManager* devices;juce::String inventory,deviceError;
    juce::Label deviceLabel,headings,status;juce::TextEditor inputList,outputList;juce::TextButton add,mono,pairs,clear,enable;juce::ToggleButton mix;juce::Viewport viewport;juce::Component rows;std::vector<std::unique_ptr<LaneRow>> laneRows;std::array<juce::Label,2> monitorLabels;std::array<juce::ComboBox,2> monitor;
};
