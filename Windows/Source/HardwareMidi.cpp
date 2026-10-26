#include "HardwareMidi.h"
#include <cmath>

HardwareMidi::HardwareMidi()
{
    for(int i=0;i<32;++i){values[(size_t)i].store(0);offsets[(size_t)i].store(i==0?1:0);known[(size_t)i].store(false);}
    for(int i=0;i<8;++i){trackChannels[(size_t)i].store(i+1);levels[(size_t)i].store(100);levelKnown[(size_t)i].store(false);}
}
const char* HardwareMidi::label(int p,int slot)
{
    static const char* labels[4][8]={
        {"TIME","PINGPONG","WIDTH","FEEDBACK","HPF","LPF","REV SEND","LEVEL"},
        {"PREDELAY","DECAY","SHELF FRQ","SHELF GAIN","HPF","LPF","PRE/POST","LEVEL"},
        {"T1 DELAY","T2 DELAY","T3 DELAY","T4 DELAY","T5 DELAY","T6 DELAY","T7 DELAY","T8 DELAY"},
        {"T1 REVERB","T2 REVERB","T3 REVERB","T4 REVERB","T5 REVERB","T6 REVERB","T7 REVERB","T8 REVERB"}};
    return labels[juce::jlimit(0,3,p)][juce::jlimit(0,7,slot)];
}
int HardwareMidi::value(int p,int s) const { return values[(size_t)(juce::jlimit(0,3,p)*8+juce::jlimit(0,7,s))].load(); }
bool HardwareMidi::isKnown(int p,int s) const { return known[(size_t)(juce::jlimit(0,3,p)*8+juce::jlimit(0,7,s))].load(); }
void HardwareMidi::edit(int p,int s,float v)
{
    if(!juce::isPositiveAndBelow(p,4)||!juce::isPositiveAndBelow(s,8)||!std::isfinite(v))return;
    int index=p*8+s,newValue=juce::jlimit(minimum(p,s),maximum(p,s),minimum(p,s)+juce::roundToInt(juce::jlimit(0.f,1.f,v)*(maximum(p,s)-minimum(p,s))));
    if(known[(size_t)index].load()&&values[(size_t)index].load()==newValue)return;
    values[(size_t)index].store(newValue);known[(size_t)index].store(true);pending.fetch_or(uint32_t(1)<<index);
}
void HardwareMidi::editLevel(int t,float v)
{
    if(!juce::isPositiveAndBelow(t,8)||!std::isfinite(v))return;int value=juce::roundToInt(juce::jlimit(0.f,1.f,v)*127);if(levelKnown[(size_t)t].load()&&levels[(size_t)t].load()==value)return;levels[(size_t)t].store(value);levelKnown[(size_t)t].store(true);pendingLevels.fetch_or(1u<<t);
}
void HardwareMidi::setOffset(int p,int s,int v)
{
    if(!juce::isPositiveAndBelow(p,4)||!juce::isPositiveAndBelow(s,8))return;int i=p*8+s;int wire=values[(size_t)i].load()+offsets[(size_t)i].load();offsets[(size_t)i].store(juce::jlimit(-2,2,v));values[(size_t)i].store(juce::jlimit(minimum(p,s),maximum(p,s),wire-offsets[(size_t)i].load()));discardPending();
}
juce::MidiBuffer HardwareMidi::takeMessages()
{
    juce::MidiBuffer result;appendMessages(result);return result;
}
void HardwareMidi::appendMessages(juce::MidiBuffer& result,int sample)
{
    auto changedLevels=pendingLevels.exchange(0);for(int t=0;t<8;++t)if(changedLevels&(1u<<t)){int ch=trackChannels[(size_t)t].load();if(ch>=1&&ch<=16)result.addEvent(juce::MidiMessage::controllerEvent(ch,95,levels[(size_t)t].load()),sample);}
    uint32_t changed=pending.exchange(0);
    for(int i=0;i<32;++i)if(changed&(uint32_t(1)<<i))
    {
        int p=i/8,s=i%8,channel=p<2?fxChannel.load():trackChannels[(size_t)s].load();
        int cc=p==0?85+s:p==1?24+s:p==2?82:83;
        if(channel>=1&&channel<=16)result.addEvent(juce::MidiMessage::controllerEvent(channel,cc,juce::jlimit(0,127,values[(size_t)i].load()+offsets[(size_t)i].load())),sample);
    }
}
juce::var HardwareMidi::save() const
{
    auto* o=new juce::DynamicObject();o->setProperty("calibrationVersion",1);o->setProperty("fxChannel",fxChannel.load());o->setProperty("inputChannel",inputChannel.load());
    juce::Array<juce::var> channels,v,k,calibration;
    for(auto& ch:trackChannels)channels.add(ch.load());
    for(int i=0;i<32;++i){v.add(values[(size_t)i].load());k.add(known[(size_t)i].load());calibration.add(offsets[(size_t)i].load());}
    juce::Array<juce::var> l,lk;for(int i=0;i<8;++i){l.add(levels[(size_t)i].load());lk.add(levelKnown[(size_t)i].load());}o->setProperty("levels",l);o->setProperty("levelKnown",lk);
    o->setProperty("offsets",calibration);o->setProperty("trackChannels",channels);o->setProperty("values",v);o->setProperty("known",k);return juce::var(o);
}
void HardwareMidi::restore(const juce::var& state)
{
    fxChannel.store(juce::jlimit(0,16,(int)state.getProperty("fxChannel",16)));
    inputChannel.store(juce::jlimit(1,16,(int)state.getProperty("inputChannel",15)));
    auto channels=state.getProperty("trackChannels",{}),v=state.getProperty("values",{}),k=state.getProperty("known",{});
    for(int i=0;i<8;++i)trackChannels[(size_t)i].store(channels.isArray()&&channels.size()==8?juce::jlimit(0,16,(int)channels[i]):i+1);
    auto calibration=state.getProperty("offsets",{});bool modern=state.hasProperty("calibrationVersion");
    for(int i=0;i<32;++i){int d=modern&&calibration.isArray()&&calibration.size()==32?juce::jlimit(-2,2,(int)calibration[i]):i==0?1:0;offsets[(size_t)i].store(d);int value=v.isArray()&&v.size()==32?(int)v[i]-(modern?0:d):0;values[(size_t)i].store(juce::jlimit(minimum(i/8,i%8),maximum(i/8,i%8),value));known[(size_t)i].store(k.isArray()&&k.size()==32?(bool)k[i]:false);}
    auto l=state["levels"],lk=state["levelKnown"];for(int i=0;i<8;++i){levels[(size_t)i].store(l.isArray()&&l.size()==8?juce::jlimit(0,127,(int)l[i]):100);levelKnown[(size_t)i].store(lk.isArray()&&lk.size()==8&&(bool)lk[i]);}
    discardPending();active.store(false);
}
