#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

struct RecorderDevice
{
    juce::String name,backend;
    juce::StringArray inputs,outputs;
    juce::BigInteger enabledInputs,enabledOutputs;
    bool connected=false;
    juce::String key() const {return backend+"|"+name;}
    int activeInput(int physical) const
    {
        if(!connected||physical<0||physical>=inputs.size()||!enabledInputs[physical])return -1;
        int index=0;for(int i=0;i<physical;++i)if(enabledInputs[i])++index;return index;
    }
    int physicalInput(int active) const {for(int i=0,n=0;i<inputs.size();++i)if(enabledInputs[i]){if(n++==active)return i;}return -1;}
    static RecorderDevice from(juce::AudioIODevice& device)
    {
        RecorderDevice d;d.name=device.getName();d.backend=device.getTypeName();d.inputs=device.getInputChannelNames();d.outputs=device.getOutputChannelNames();d.enabledInputs=device.getActiveInputChannels();d.enabledOutputs=device.getActiveOutputChannels();d.connected=true;return d;
    }
    juce::var save() const
    {
        auto* o=new juce::DynamicObject();o->setProperty("name",name);o->setProperty("backend",backend);o->setProperty("inputs",juce::var(inputs));o->setProperty("outputs",juce::var(outputs));o->setProperty("enabledInputs",enabledInputs.toString(16));o->setProperty("enabledOutputs",enabledOutputs.toString(16));return juce::var(o);
    }
};

struct RecorderLane
{
    juce::String name="Input",deviceKey,leftName,rightName;
    int left=-1,right=-1;
    bool stereo=false,armed=true,legacyOrdinal=false;
    void bind(int side,int physical,const RecorderDevice& device)
    {
        if(deviceKey!=device.key()){left=right=-1;leftName.clear();rightName.clear();}
        deviceKey=device.key();legacyOrdinal=false;
        auto text=juce::isPositiveAndBelow(physical,device.inputs.size())?device.inputs[physical]:juce::String();
        if(side==0){left=physical;leftName=text;}else{right=physical;rightName=text;}
    }
    int physical(int side,const RecorderDevice& device) const
    {
        int index=side==0?left:right;auto text=side==0?leftName:rightName;
        if(legacyOrdinal)return device.physicalInput(index);
        if(deviceKey!=device.key()||index<0)return -1;
        if(juce::isPositiveAndBelow(index,device.inputs.size())&&(text.isEmpty()||device.inputs[index]==text))return index;
        int found=-1;for(int i=0;i<device.inputs.size();++i)if(device.inputs[i]==text){if(found>=0)return -1;found=i;}return found;
    }
    juce::var save() const
    {
        auto* o=new juce::DynamicObject();o->setProperty("name",name);o->setProperty("device",deviceKey);o->setProperty("leftName",leftName);o->setProperty("rightName",rightName);o->setProperty("left",left);o->setProperty("right",right);o->setProperty("stereo",stereo);o->setProperty("armed",armed);o->setProperty("legacyOrdinal",legacyOrdinal);return juce::var(o);
    }
};

class RecorderRouting
{
public:
    static constexpr int maximumLanes=64;
    struct Plan {std::vector<RecorderLane> lanes;bool recordMix=false;};
    Plan snapshot() const {const juce::ScopedLock lock(mutex);return plan;}
    void replace(Plan value) {const juce::ScopedLock lock(mutex);plan=std::move(value);}
    void edit(int i,const RecorderLane& lane){const juce::ScopedLock lock(mutex);if(juce::isPositiveAndBelow(i,(int)plan.lanes.size()))plan.lanes[(size_t)i]=lane;}
    void add(){const juce::ScopedLock lock(mutex);if(plan.lanes.size()<maximumLanes){RecorderLane l;l.name="Input-"+juce::String((int)plan.lanes.size()+1);plan.lanes.push_back(l);}}
    void remove(int i){const juce::ScopedLock lock(mutex);if(juce::isPositiveAndBelow(i,(int)plan.lanes.size()))plan.lanes.erase(plan.lanes.begin()+i);}
    void setRecordMix(bool enabled){const juce::ScopedLock lock(mutex);plan.recordMix=enabled;}
    void assignAll(const RecorderDevice& device,bool stereo)
    {
        auto next=snapshot();next.lanes.clear();for(int i=0;i<juce::jmin(64,device.inputs.size());i+=stereo?2:1){RecorderLane lane;lane.name=device.inputs[i].substring(0,64);lane.bind(0,i,device);lane.stereo=stereo&&i+1<device.inputs.size();if(lane.stereo)lane.bind(1,i+1,device);next.lanes.push_back(lane);}replace(std::move(next));
    }
    void bindLegacy(const RecorderDevice& device)
    {
        if(!device.connected)return;auto next=snapshot();bool changed=false;for(auto& lane:next.lanes)if(lane.legacyOrdinal){int left=lane.physical(0,device),right=lane.physical(1,device);if(left<0||(lane.stereo&&right<0))continue;lane.bind(0,left,device);if(lane.stereo)lane.bind(1,right,device);changed=true;}if(changed)replace(std::move(next));
    }
    void migrate(const juce::var& inputs)
    {
        Plan next;if(inputs.isArray()&&inputs.size()==10){for(int i=0;i<9;++i){RecorderLane l;l.name=i<8?"Digitakt-"+juce::String(i+1):"Digitakt-Main";l.left=(int)inputs[i<8?i:8];l.right=i==8?(int)inputs[9]:-1;l.stereo=i==8;l.legacyOrdinal=true;next.lanes.push_back(l);}}replace(std::move(next));
    }
    juce::var save() const
    {
        auto next=snapshot();auto* o=new juce::DynamicObject();o->setProperty("version",1);o->setProperty("recordMix",next.recordMix);juce::Array<juce::var> lanes;for(auto& lane:next.lanes)lanes.add(lane.save());o->setProperty("lanes",lanes);return juce::var(o);
    }
    bool restore(const juce::var& state)
    {
        auto array=state["lanes"];if(!state.isObject()||!state["version"].isInt()||(int)state["version"]!=1||!state["recordMix"].isBool()||!array.isArray()||array.size()>maximumLanes)return false;
        Plan next;next.recordMix=(bool)state["recordMix"];for(auto v:*array.getArray()){if(!v.isObject()||!v["name"].isString()||v["name"].toString().length()>64||!v["device"].isString()||!v["leftName"].isString()||!v["rightName"].isString()||!v["stereo"].isBool()||!v["armed"].isBool()||!v["legacyOrdinal"].isBool())return false;for(const char* key:{"left","right"})if(!v[key].isInt()||(int)v[key]<-1||(int)v[key]>63)return false;RecorderLane l;l.name=v["name"].toString();l.deviceKey=v["device"].toString();l.leftName=v["leftName"].toString();l.rightName=v["rightName"].toString();l.left=(int)v["left"];l.right=(int)v["right"];l.stereo=(bool)v["stereo"];l.armed=(bool)v["armed"];l.legacyOrdinal=(bool)v["legacyOrdinal"];next.lanes.push_back(l);}replace(std::move(next));return true;
    }
private:
    mutable juce::CriticalSection mutex;
    Plan plan;
};
