#include "Processor.h"
#include "PixelEditor.h"
#include "AudioDSP.h"
#include "ParameterDisplay.h"

namespace
{
const char* parameterIds[]={"dual.shape1","dual.shape2","dual.tune2","dual.mix","dual.xmod","dual.fold","dual.fine","dual.sync","filter.cutoff","filter.resonance","filter.morph","filter.env","filter.attack","filter.decay","filter.sustain","filter.release","amp.attack","amp.decay","amp.sustain","amp.release","amp.level","amp.pan","amp.velocity","amp.drive","metal.tune","metal.model","metal.fold","metal.sweep","metal.metal","metal.feedback","metal.color","metal.transient","fm.algorithm","fm.ratioA","fm.ratioB","fm.index","fm.decay","fm.feedback","fm.shift","fm.brightness"};
const char* mixNames[]={"IN LEVEL","FX1 WET","FX2 WET","OUTPUT","PAN","FX1 BYPASS","FX2 BYPASS","MIDI CH"};
const char* mixIds[]={"rack.input","rack.fx1wet","rack.fx2wet","rack.output","rack.pan","rack.fx1bypass","rack.fx2bypass"};
bool number(const juce::var& v,double low,double high){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v)&&(double)v>=low&&(double)v<=high;}
}
CompanionProcessor::CompanionProcessor():AudioProcessor(BusesProperties().withInput("Digitakt return",juce::AudioChannelSet::stereo(),false).withOutput("Output",juce::AudioChannelSet::stereo(),true))
{
    for(int t=0;t<4;++t)
    {
        auto n=(size_t)t;synths[n]=synth_create();if(!synths[n]){for(auto s:synths)if(s)synth_destroy(s);throw std::bad_alloc();}
        auto prefix=t==0?juce::String():"track"+juce::String(t+1)+".";
        auto name="T"+juce::String(t+1)+" ";
        for(int i=0;i<40;++i){auto* p=new juce::AudioParameterFloat(juce::ParameterID(prefix+parameterIds[i],1),name+companion::names[(size_t)i],juce::NormalisableRange<float>(0,1),synth_get_parameter(synths[n],i),juce::AudioParameterFloatAttributes().withStringFromValueFunction([i](float v,int){return companion::parameterText(i,v);}));trackSound[n][(size_t)i]=p;addParameter(p);}
        engines[n]=new juce::AudioParameterChoice(juce::ParameterID(prefix+"engine",1),name+"Engine",{"Synth","Metallic","Four-op FM"},t<3?t:0);addParameter(engines[n]);
        float defaults[]={2.f/3.f,1,1,1,.5f,0,0};
        for(int i=0;i<7;++i){auto* p=new juce::AudioParameterFloat(juce::ParameterID(prefix+mixIds[i],1),name+mixNames[i],juce::NormalisableRange<float>(0,1),defaults[i]);trackMix[n][(size_t)i]=p;addParameter(p);}
        channels[n]=new juce::AudioParameterFloat(juce::ParameterID(prefix+"midi.channel",1),name+"MIDI channel",juce::NormalisableRange<float>(0,1),t/15.f);addParameter(channels[n]);
        trackPanic[n].store(false);pluginSources[n].store(false);trackPeaks[n].store(0);trackLatency[n].store(0);routed[n].ensureSize(65536);
        for(int i=0;i<8;++i){macroSlot[n][(size_t)i].store(0);macroIndex[n][(size_t)i].store(i);}
    }
    for(int i=0;i<8;++i){cc[(size_t)i].store(20+i);trackPages[(size_t)i].store(0);}
    // Append new automation parameters after all previous IDs/ordinals.
    for(int t=0;t<4;++t){auto prefix=t==0?juce::String():"track"+juce::String(t+1)+".";auto name="T"+juce::String(t+1)+" ";mono[(size_t)t]=new juce::AudioParameterBool(juce::ParameterID(prefix+"dual.mono",1),name+"MONO LEGATO",false);addParameter(mono[(size_t)t]);glide[(size_t)t]=new juce::AudioParameterFloat(juce::ParameterID(prefix+"dual.glide",1),name+"GLIDE TIME",juce::NormalisableRange<float>(0,1),0,juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v,int){return v<=0?juce::String("OFF"):companion::timeText(2*v*v);}));addParameter(glide[(size_t)t]);}
    masterLevel=new juce::AudioParameterFloat(juce::ParameterID("mixer.master",1),"Mixer master",juce::NormalisableRange<float>(0,1),1);addParameter(masterLevel);
    for(int i=0;i<2;++i)monitorInputs[(size_t)i].store(i);for(int i=0;i<64;++i)physicalToActive[(size_t)i].store(i);
    modulation2.channel.store(14);modulation2.midiCC.store(21);clockOutput.ensureSize(4096);curveOutput.ensureSize(4096);for(auto& m:curveRecord)m.ensureSize(4096);
    sound=trackSound[0];engineParameter=engines[0];for(int i=0;i<7;++i)mix[(size_t)i]=trackMix[0][(size_t)i];mix[7]=channels[0];
}
CompanionProcessor::~CompanionProcessor(){recorder.stop();for(auto s:synths)synth_destroy(s);}
void CompanionProcessor::prepareToPlay(double r,int frames)
{
    recorder.stop();rate=r;availableInputs.store(getTotalNumInputChannels());deviceAudio.setSize(juce::jmax(2,getTotalNumInputChannels()),juce::jmax(32,frames));masterSmooth.reset(r,.01);masterSmooth.setCurrentAndTargetValue(masterLevel->get());untilCurveCC=0;lastCurveValue={{-1,-1}};wasRecording=false;sampleCounter=0;transport.reset();inputAudio.setSize(2,juce::jmax(32,frames));inputDelay.setSize(2,maximumDelay+1);inputDelay.clear();inputDelayPosition=0;
    for(int t=0;t<4;++t){auto n=(size_t)t;synth_all_sounds_off(synths[n]);lastEngines[n]=-1;audio[n].setSize(2,juce::jmax(32,frames));alignment[n].setSize(2,maximumDelay+1);alignment[n].clear();delayPosition[n]=0;if(hostingEnabled)synthRacks[n].prepare(r,frames);}
    modulation.restart();modulation2.restart();updateLatency();audioPrepared.store(true);
}
void CompanionProcessor::releaseResources(){audioPrepared.store(false);recorder.stop();if(hostingEnabled)for(auto& r:synthRacks)r.release();}
juce::String CompanionProcessor::trackName(int t) const
{
    if(t==4)return "MOD CURVE 1";if(t==5)return "MOD CURVE 2";if(t==6)return "DT FX CTRL";if(t==7)return "MIX / REC";
    static const char* names[]={"SYNTH","METALLIC","FOUR-OP FM","PLUGIN HOST"};return names[engine(t)];
}
void CompanionProcessor::selectTrack(int t)
{
    if(!juce::isPositiveAndBelow(t,8))return;selectedTrack.store(t);if(t<4)lastSoundTrack.store(t);page.store(trackPages[(size_t)t].load());hardware.active.store(t==6);if(t==6)hardware.page.store(page.load());
}
int CompanionProcessor::pageCount(int t) const {if(t==7)return 3;if(t==4||t==5)return 8;if(t==6)return 4;if(isPluginTrack(t))return 3;return engine(t)==0?(hostingEnabled?5:4):(hostingEnabled?4:3);}
void CompanionProcessor::setPage(int p){p=juce::jlimit(0,pageCount(track())-1,p);page.store(p);trackPages[(size_t)track()].store(p);if(track()==6)hardware.page.store(p);}
void CompanionProcessor::setTrackEngine(int t,int e)
{
    if(!juce::isPositiveAndBelow(t,4)||e<0||e>(hostingEnabled?3:2))return;
    if(engine(t)==e)return;
    int old=engines[(size_t)t]->getIndex();
    for(auto* mod:{&modulation,&modulation2})for(int r=0;r<8;++r){int dest=mod->destination[(size_t)r].load();if(mod->targetTrack[(size_t)r].load()==t&&old<3&&e<3){int from=companion::parameterIndex(old,0,0);if(dest>=from&&dest<from+8)mod->destination[(size_t)r].store(companion::parameterIndex(e,0,dest-from));}}
    pluginSources[(size_t)t].store(e==3);if(e<3)engines[(size_t)t]->setValueNotifyingHost(engines[(size_t)t]->convertTo0to1(e));trackPages[(size_t)t].store(0);if(track()==t)page.store(0);trackPanic[(size_t)t].store(true);updateLatency();
}
void CompanionProcessor::setTrackChannel(int t,int c){if(!juce::isPositiveAndBelow(t,4))return;channels[(size_t)t]->setValueNotifyingHost((juce::jlimit(1,16,c)-1)/15.f);trackPanic[(size_t)t].store(true);}
void CompanionProcessor::setCC(int slot,int v)
{
    if(!juce::isPositiveAndBelow(slot,8))return;v=juce::jlimit(0,119,v);int old=cc[(size_t)slot].load();for(int i=0;i<8;++i)if(i!=slot&&cc[(size_t)i].load()==v)cc[(size_t)i].store(old);cc[(size_t)slot].store(v);
}
std::array<float,8> CompanionProcessor::mixValues(int t) const
{
    t=t<0?soundTrack():juce::jlimit(0,3,t);std::array<float,8> result;for(int i=0;i<7;++i)result[(size_t)i]=trackMix[(size_t)t][(size_t)i]->get();result[7]=channels[(size_t)t]->get();return result;
}
void CompanionProcessor::setFxBypass(int s,bool b){if(s<1||s>2)return;trackMix[(size_t)soundTrack()][(size_t)(s+4)]->setValueNotifyingHost(b?1.f:0.f);updateLatency();}
void CompanionProcessor::setFxWet(int s,float v){if(s<1||s>2||!std::isfinite(v))return;trackMix[(size_t)soundTrack()][(size_t)s]->setValueNotifyingHost(juce::jlimit(0.f,1.f,v));}
juce::AudioProcessorParameter* CompanionProcessor::parameterFor(int t,int p,int s) const
{
    if(!juce::isPositiveAndBelow(t,4)||!juce::isPositiveAndBelow(s,8))return nullptr;
    if(isVoicePage(t,p))return s==0?static_cast<juce::AudioProcessorParameter*>(mono[(size_t)t]):s==1?glide[(size_t)t]:nullptr;
    if(!isPluginTrack(t)&&p<3)return trackSound[(size_t)t][(size_t)companion::parameterIndex(engine(t),p,s)];
    if(isPluginTrack(t)&&p==2)return s==7?channels[(size_t)t]:trackMix[(size_t)t][(size_t)s];
    if(isPluginTrack(t)&&p==0)return nullptr;
    int rs=p==3?1+s/4:macroSlot[(size_t)t][(size_t)s].load(),index=p==3?s%4:macroIndex[(size_t)t][(size_t)s].load();auto plugin=rackForTrack(t).instance(rs);
    return plugin&&juce::isPositiveAndBelow(index,plugin->getParameters().size())?plugin->getParameters()[index]:nullptr;
}
juce::AudioProcessorParameter* CompanionProcessor::controlParameter(int s) const{if(track()==7&&page.load()==0)return s<4?trackMix[(size_t)s][3]:s==5?masterLevel:nullptr;return track()<4?parameterFor(track(),page.load(),s):nullptr;}
juce::String CompanionProcessor::controlName(int s) const
{
    if(track()==7){if(page.load()==0)return s<4?"COMP "+juce::String(s+1):s==4?"DEV RETURN":s==5?"MASTER":"";if(page.load()==1)return "DT "+juce::String(s+1);return {};}
    if(isModTrack())return currentModulation().controlName(page.load(),s);
    if(isVoicePage())return s==0?"MODE":s==1?"GLIDE":"";
    auto* p=controlParameter(s);if(!p)return page.load()==3?(currentRack().instance(1+s/4)?"NO PARAM":s<4?"FX1 EMPTY":"FX2 EMPTY"):"UNASSIGNED";
    if(isPluginTrack()&&page.load()==2)return mixNames[s];return p->getName(32).toUpperCase().replace("T"+juce::String(soundTrack()+1)+" ","").replace("FILTER ","FLT ");
}
int CompanionProcessor::controlChannel() const {return track()<4?channel(track()):isModTrack()?currentModulation().channel.load():track()==7?mixerChannel.load():hardware.inputChannel.load();}
void CompanionProcessor::setControlChannel(int t,int ch){ch=juce::jlimit(1,16,ch);if(t<4)setTrackChannel(t,ch);else if(t==4)modulation.channel.store(ch);else if(t==5)modulation2.channel.store(ch);else if(t==6)hardware.inputChannel.store(ch);else if(t==7)mixerChannel.store(ch);}
int CompanionProcessor::controlCC(int s) const {return track()==7?(page.load()==0?(s<6?16+s:-1):page.load()==1?24+s:-1):isVoicePage()?(s<2?70+s:-1):track()<4?companion::midi::soundCC(engine(),page.load(),s):isModTrack()?currentModulation().controlCC(page.load(),s):track()==6?companion::midi::hardwareCC(page.load(),s):-1;}
juce::String CompanionProcessor::modulationTargetName(int t,int d) const
{
    if(d<0)return "OFF";if(d<40)return companion::names[(size_t)d];if(d>=48)return "INVALID";
    int s=d-40;auto plugin=rackForTrack(t).instance(1+s/4);auto prefix="FX"+juce::String(1+s/4)+" ";return plugin&&s%4<plugin->getParameters().size()?prefix+plugin->getParameters()[s%4]->getName(40).toUpperCase():prefix+"EMPTY";
}
juce::String CompanionProcessor::controlText(int s) const
{
    if(track()==7){if(page.load()==0)return s<4?companion::percentage(trackMix[(size_t)s][3]->get()):s==4?companion::percentage(monitorGain.load()):s==5?companion::percentage(masterLevel->get()):"";if(page.load()==1)return hardware.levelKnown[(size_t)s].load()?juce::String(hardware.levels[(size_t)s].load()):"--";return {};}
    if(isModTrack()){if((page.load()==2||page.load()==3)&&s<4){int r=(page.load()-2)*4+s;return modulationTargetName(currentModulation().targetTrack[(size_t)r].load(),currentModulation().destination[(size_t)r].load());}return currentModulation().controlText(page.load(),s);}
    auto* p=controlParameter(s);if(!p)return "--";if(isVoicePage()&&s==0)return mono[(size_t)track()]->get()?"MONO LEGATO":"POLY";
    if(!isPluginTrack()&&page.load()<3)return companion::parameterText(companion::parameterIndex(engine(),page.load(),s),p->getValue());
    if(isPluginTrack()&&page.load()==2){if(s==7)return juce::String(channel());if(s==5||s==6)return p->getValue()>.5f?"ON":"OFF";if(s==4){float pan=p->getValue();return std::abs(pan-.5f)<.004f?"CENTER":juce::String(juce::roundToInt(std::abs(2*pan-1)*100))+(pan<.5f?" L":" R");}return companion::percentage(p->getValue()*(s==0?1.5f:1.f));}
    auto text=p->getText(p->getValue(),48);auto label=p->getLabel();return label.isEmpty()||text.endsWithIgnoreCase(label)?text:text+" "+label;
}
float CompanionProcessor::controlValue(int s) const{if(track()==7){if(page.load()==0)return s<4?trackMix[(size_t)s][3]->get():s==4?monitorGain.load()*.5f:s==5?masterLevel->get():0;if(page.load()==1)return hardware.levels[(size_t)s].load()/127.f;return 0;}if(isModTrack())return currentModulation().controlValue(page.load(),s);auto* p=controlParameter(s);if(!p)return 0;return track()<4&&!isPluginTrack()&&page.load()<3?companion::knobPosition(companion::parameterIndex(engine(),page.load(),s),p->getValue()):p->getValue();}
bool CompanionProcessor::controlEnabled(int s) const{return track()==7?(page.load()==0?s<6:page.load()==1?hostingEnabled:false):isModTrack()?(page.load()==0?s<4:page.load()==1?s<4:page.load()==6?s<6:page.load()==7?s<5:true):controlParameter(s)&&controlParameter(s)->isAutomatable();}
void CompanionProcessor::setTrackControl(int t,int p,int s,float v,bool realtime)
{
    if(!std::isfinite(v)||!juce::isPositiveAndBelow(s,8))return;v=juce::jlimit(0.f,1.f,v);
    if(isVoicePage(t,p)){if(s==0)mono[(size_t)t]->setValueNotifyingHost(v>=.5f?1.f:0.f);if(s==1)glide[(size_t)t]->setValueNotifyingHost(v);return;}
    if(isPluginTrack(t)&&p==0)return;
    if(!isPluginTrack(t)&&p<3){int index=companion::parameterIndex(engine(t),p,s);trackSound[(size_t)t][(size_t)index]->setValueNotifyingHost(companion::knobToParameter(index,v));}
    else if(isPluginTrack(t)&&p==2){if(s==7)setTrackChannel(t,1+juce::roundToInt(v*15));else trackMix[(size_t)t][(size_t)s]->setValueNotifyingHost(v);}
    else if(hostingEnabled)rackForTrack(t).control(p==3?1+s/4:macroSlot[(size_t)t][(size_t)s].load(),p==3?s%4:macroIndex[(size_t)t][(size_t)s].load(),v,realtime);
}
void CompanionProcessor::setControl(int s,float v,bool realtime){if(!std::isfinite(v)||!juce::isPositiveAndBelow(s,8))return;v=juce::jlimit(0.f,1.f,v);if(track()==7){if(page.load()==0){if(s<4)trackMix[(size_t)s][3]->setValueNotifyingHost(v);else if(s==4)monitorGain.store(2*v);else if(s==5)masterLevel->setValueNotifyingHost(v);}else if(page.load()==1&&hostingEnabled)hardware.editLevel(s,v);return;}if(isModTrack())currentModulation().setControl(page.load(),s,v);else if(track()<4)setTrackControl(track(),page.load(),s,v,realtime);}
void CompanionProcessor::beginGesture(int s){if(auto* p=controlParameter(s))p->beginChangeGesture();}
void CompanionProcessor::endGesture(int s){if(auto* p=controlParameter(s))p->endChangeGesture();}
void CompanionProcessor::assignMacro(int m,int s,int i)
{
    if(!juce::isPositiveAndBelow(m,8))return;auto t=(size_t)soundTrack();macroSlot[t][(size_t)m].store(s);macroIndex[t][(size_t)m].store(i);macroIds[t][(size_t)m]=currentRack().parameterID(s,i);
}
void CompanionProcessor::resetMacros(int t){if(t<0)t=soundTrack();for(int i=0;i<8;++i){macroSlot[(size_t)t][(size_t)i].store(0);macroIndex[(size_t)t][(size_t)i].store(i);macroIds[(size_t)t][(size_t)i]=rackForTrack(t).parameterID(0,i);}}
juce::String CompanionProcessor::loadPlugin(int slot,const HostedDescriptor& d,int t)
{
    if(!hostingEnabled)return "Plugin hosting is available in the standalone app";if(t<0)t=soundTrack();if(!juce::isPositiveAndBelow(t,4))return "Choose sound track 1-4";
    auto error=rackForTrack(t).load(slot,d);if(error.isEmpty()&&slot==0){resetMacros(t);setTrackEngine(t,3);}updateLatency();return error;
}
void CompanionProcessor::removePlugin(int s,int t){if(t<0)t=soundTrack();rackForTrack(t).remove(s);updateLatency();}
void CompanionProcessor::updateLatency()
{
    int maximum=0;for(int t=0;t<4;++t){int latency=hostingEnabled?juce::jlimit(0,maximumDelay,rackForTrack(t).latency(isPluginTrack(t),mixValues(t))):0;trackLatency[(size_t)t].store(latency);maximum=juce::jmax(maximum,latency);}maximumLatency.store(maximum);setLatencySamples(maximum);
}
void CompanionProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;int count=buffer.getNumSamples();if(count<=0||buffer.getNumChannels()<2||count>audio[0].getNumSamples()){buffer.clear();if(hostingEnabled)midi.clear();return;}
    int inputs=hostingEnabled?juce::jmin(getTotalNumInputChannels(),buffer.getNumChannels()):0;deviceAudio.clear();for(int c=0;c<juce::jmin(inputs,deviceAudio.getNumChannels());++c){deviceAudio.copyFrom(c,0,buffer,c,0,count);auto* data=deviceAudio.getWritePointer(c);for(int i=0;i<count;++i)if(!std::isfinite(data[i]))data[i]=0;}
    inputAudio.clear();if(hostingEnabled)for(int c=0;c<2;++c){int physical=monitorInputs[(size_t)c].load();int source=juce::isPositiveAndBelow(physical,64)?physicalToActive[(size_t)physical].load():-1;if(source>=0&&source<inputs)inputAudio.copyFrom(c,0,deviceAudio,source,0,count);}buffer.clear();curveOutput.clear();for(auto& m:curveRecord)m.clear();
    double blockStamp=transport.samplePosition();if(hostingEnabled&&transport.advance(count,rate,clockOutput)){modulation.restart();modulation2.restart();}
    if(hostingEnabled)hardware.appendMessages(clockOutput);
    if(!hostingEnabled)if(auto* head=getPlayHead())if(auto position=head->getPosition()){if(auto tempo=position->getBpm())bpm.store(*tempo);if(auto beat=position->getPpqPosition())beats.store(*beat);playing.store(position->getIsPlaying());clockActive.store(true);}
    keyboard.processNextMidiBuffer(midi,0,count,true);bool allPanic=panic.exchange(false);
    if(allPanic){inputDelay.clear();inputDelayPosition=0;}
    for(int t=0;t<4;++t)
    {
        auto n=(size_t)t;routed[n].clear();audio[n].clear();int e=engine(t);
        if(allPanic||trackPanic[n].exchange(false)||e!=lastEngines[n])
        {
            synth_all_sounds_off(synths[n]);if(hostingEnabled)synthRacks[n].panic();alignment[n].clear();delayPosition[n]=0;
            routed[n].addEvent(juce::MidiMessage::allSoundOff(1),0);lastEngines[n]=e;
        }
    }
    for(auto event:midi)
    {
        auto message=event.getMessage();int at=juce::jlimit(0,count-1,event.samplePosition);
        if(hostingEnabled&&transport.input(message,blockStamp+at,rate)){modulation.restart();modulation2.restart();}
        if(message.getChannel()==0)continue;
        if(message.isController())
        {
            int code=message.getControllerNumber();float value=message.getControllerValue()/127.f;
            for(auto* mod:{&modulation,&modulation2})if(message.getChannel()==mod->channel.load())
            {
                for(int p=0;p<8;++p)for(int k=0;k<8;++k)if(companion::midi::modCC(p,k)==code)mod->setControl(p,k,value);
                for(int id=0;id<8;++id)for(int d=0;d<3;++d)if(companion::midi::vertexCC(id,d)==code)mod->queueVertexControl(id,d,value);
                if(code>=80&&code<88)mod->targetTrack[(size_t)code-80].store(juce::jmin(3,message.getControllerValue()/32));
                if(code==29)mod->enabled.store(message.getControllerValue()>=64);if(code==30&&message.getControllerValue()>=64)mod->restart();
            }
            if(message.getChannel()==mixerChannel.load()){if(code>=16&&code<=19)trackMix[(size_t)(code-16)][3]->setValueNotifyingHost(value);else if(code==20)monitorGain.store(2*value);else if(code==21)masterLevel->setValueNotifyingHost(value);else if(hostingEnabled&&code>=24&&code<=31){hardware.editLevel(code-24,value);hardware.appendMessages(clockOutput,at);}}
            if(hostingEnabled&&message.getChannel()==hardware.inputChannel.load())for(int p=0;p<4;++p)for(int k=0;k<8;++k)if(companion::midi::hardwareCC(p,k)==code){hardware.edit(p,k,value);hardware.appendMessages(clockOutput,at);}
        }
        for(int t=0;t<4;++t)if(message.getChannel()==channel(t))
        {
            if(message.isController())
            {
                int code=message.getControllerNumber(),index=companion::midi::nativeParameter(code);float value=message.getControllerValue()/127.f;
                if(index>=0){trackSound[(size_t)t][(size_t)index]->setValueNotifyingHost(companion::knobToParameter(index,value));continue;}
                if(code==70){mono[(size_t)t]->setValueNotifyingHost(value>=.5f?1.f:0.f);continue;}if(code==71){glide[(size_t)t]->setValueNotifyingHost(value);continue;}
                if(code>=72&&code<=79){if(hostingEnabled)setTrackControl(t,3,code-72,value,true);continue;}
                if(code>=80&&code<=87){if(hostingEnabled)rackForTrack(t).control(macroSlot[(size_t)t][(size_t)code-80].load(),macroIndex[(size_t)t][(size_t)code-80].load(),value,true);continue;}
                if(code>=88&&code<=95){int k=code-88;if(k==7)setTrackChannel(t,1+juce::roundToInt(value*15));else trackMix[(size_t)t][(size_t)k]->setValueNotifyingHost(k>=5?(value>=.5f?1.f:0.f):value);continue;}
            }
            auto routedMessage=message;routedMessage.setChannel(1);routed[(size_t)t].addEvent(routedMessage,at);
        }
    }
    if(hostingEnabled){bpm.store(transport.bpm.load());beats.store(transport.beat.load());playing.store(transport.running.load());clockActive.store(transport.available.load());}
    modulation.advance(count,rate,beats.load(),playing.load(),clockActive.load());modulation2.advance(count,rate,beats.load(),playing.load(),clockActive.load());int latency=maximumLatency.load();
    bool recording=recorder.isRecording();if(recording&&(!wasRecording||recorder.samples()==0))untilCurveCC=0;wasRecording=recording;
    int interval=juce::jmax(1,juce::roundToInt(rate/50));
    for(int at=untilCurveCC;at<count;at+=interval){for(int k=0;k<2;++k){auto& mod=k==0?modulation:modulation2;int value=mod.midiValue(),cc=mod.midiCC.load(),ch=mod.midiChannel.load();auto message=juce::MidiMessage::controllerEvent(ch,cc,value);curveRecord[(size_t)k].addEvent(message,at);bool changed=value!=lastCurveValue[(size_t)k]||cc!=lastCurveCC[(size_t)k]||ch!=lastCurveChannel[(size_t)k]||mod.midiEnabled.load()!=lastCurveEnabled[(size_t)k];if(mod.midiEnabled.load()&&changed)curveOutput.addEvent(message,at);lastCurveValue[(size_t)k]=value;lastCurveCC[(size_t)k]=cc;lastCurveChannel[(size_t)k]=ch;lastCurveEnabled[(size_t)k]=mod.midiEnabled.load();}}
    untilCurveCC=(untilCurveCC-count)%interval;if(untilCurveCC<0)untilCurveCC+=interval;
    float overall=0;
    for(int t=0;t<4;++t)
    {
        auto n=(size_t)t;auto& signal=audio[n];auto* left=signal.getWritePointer(0);auto* right=signal.getWritePointer(1);auto* synth=synths[n];
        std::array<float,48> offsets {};for(auto* mod:{&modulation,&modulation2})for(int r=0;r<8;++r){int d=mod->destination[(size_t)r].load();if(mod->targetTrack[(size_t)r].load()==t&&juce::isPositiveAndBelow(d,48))offsets[(size_t)d]+=mod->offsetFor(r);}
        for(int i=0;i<40;++i)synth_set_parameter(synth,i,juce::jlimit(0.f,1.f,trackSound[n][(size_t)i]->get()+offsets[(size_t)i]));
        synth_set_channel(synth,1);synth_set_engine(synth,juce::jmin(2,engine(t)));synth_set_glide(synth,mono[n]->get(),glide[n]->get());
        if(!isPluginTrack(t))
        {
            int cursor=0;for(auto event:routed[n]){int at=juce::jlimit(0,count-1,event.samplePosition);if(at>cursor)synth_render(synth,left+cursor,right+cursor,(uint32_t)(at-cursor),rate);cursor=at;auto m=event.getMessage();if(m.isAllSoundOff())synth_all_sounds_off(synth);else if(event.numBytes>=3)synth_handle_midi(synth,(uint8_t)event.data[0],event.data[1],event.data[2]);}
            if(count>cursor)synth_render(synth,left+cursor,right+cursor,(uint32_t)(count-cursor),rate);
        }
        juce::AudioBuffer<float> view(signal.getArrayOfWritePointers(),2,count);
        std::array<float,8> fxOffsets;std::copy(offsets.begin()+40,offsets.end(),fxOffsets.begin());
        if(hostingEnabled)synthRacks[n].process(view,routed[n],isPluginTrack(t),mixValues(t),&localTransport,fxOffsets);else view.applyGain(mixValues(t)[3]);
        float maximum=0;int delay=juce::jlimit(0,maximumDelay,latency-trackLatency[n].load()+(hostingEnabled?juce::roundToInt(juce::jmax(0.f,alignmentMs.load())*rate/1000):0));auto& ring=alignment[n];
        for(int i=0;i<count;++i)
        {
            int read=(delayPosition[n]+maximumDelay+1-delay)%(maximumDelay+1);
            for(int c=0;c<2;++c){float value=view.getSample(c,i);if(!std::isfinite(value))value=0;ring.setSample(c,delayPosition[n],value);value=ring.getSample(c,read);maximum=juce::jmax(maximum,std::abs(value));view.setSample(c,i,value);buffer.addSample(c,i,value);}
            delayPosition[n]=(delayPosition[n]+1)%(maximumDelay+1);
        }
        trackPeaks[n].store(maximum);
    }
    if(hostingEnabled)
    {
        int delay=juce::jlimit(0,maximumDelay,latency+juce::roundToInt(juce::jmax(0.f,-alignmentMs.load())*rate/1000));float inputPeak=0;
        for(int i=0;i<count;++i){int read=(inputDelayPosition+maximumDelay+1-delay)%(maximumDelay+1);for(int c=0;c<2;++c){float v=inputAudio.getSample(c,i);inputDelay.setSample(c,inputDelayPosition,std::isfinite(v)?v:0);float out=inputDelay.getSample(c,read)*monitorGain.load();inputPeak=juce::jmax(inputPeak,std::abs(out));if(monitorEnabled.load())buffer.addSample(c,i,out);}inputDelayPosition=(inputDelayPosition+1)%(maximumDelay+1);}monitorPeak.store(inputPeak);
        clockOutput.addEvents(curveOutput,0,count,0);midi.swapWith(clockOutput); // No raw input echo to the Digitakt.
    }
    else midi.swapWith(curveOutput);
    masterSmooth.setTargetValue(masterLevel->get());for(int i=0;i<count;++i){float gain=masterSmooth.getNextValue();for(int c=0;c<2;++c)buffer.getWritePointer(c)[i]*=gain;}
    for(int c=0;c<2;++c)for(int i=0;i<count;++i){auto& value=buffer.getWritePointer(c)[i];if(!std::isfinite(value))value=0;overall=juce::jmax(overall,std::abs(value));}
    if(recording)recorder.capture(deviceAudio,inputs,audio,buffer,count,curveRecord,bpm.load());
    peak.store(overall);sampleCounter+=count;
}
juce::Result CompanionProcessor::startRecording(const juce::File& folder)
{
    if(!audioPrepared.load())return juce::Result::fail("Start the audio device before recording");
    auto device=recorderDevice();if(!hostingEnabled)device.connected=false;recorderRouting.bindLegacy(device);return recorder.start(folder,rate,recorderRouting.snapshot(),device);
}
void CompanionProcessor::setRecorderDevice(const RecorderDevice& device)
{
    recorder.stop();{const juce::ScopedLock lock(deviceMutex);deviceInfo=device;}
    for(int i=0;i<64;++i)physicalToActive[(size_t)i].store(device.activeInput(i));
    if(device.connected){recorderRouting.bindLegacy(device);if(legacyMonitorInputs.exchange(false))for(auto& input:monitorInputs)input.store(device.physicalInput(input.load()));}
}
juce::var CompanionProcessor::state(bool includeRack) const
{
    companion::PatchState legacy;legacy.engine=juce::jmin(2,engine(0));legacy.page=juce::jmin(2,page.load());legacy.channel=channel(0);for(int i=0;i<40;++i)legacy.parameters[(size_t)i]=sound[(size_t)i]->get();for(int i=0;i<8;++i)legacy.cc[(size_t)i]=cc[(size_t)i].load();
    auto json=legacy.toJson();auto* o=json.getDynamicObject();o->setProperty("layoutVersion",7);o->setProperty("selectedTrack",track());o->setProperty("lastSoundTrack",soundTrack());o->setProperty("modulation",modulation.save());o->setProperty("modulation2",modulation2.save());o->setProperty("hardware",hardware.save());o->setProperty("transportClock",transport.save());
    auto* monitor=new juce::DynamicObject();monitor->setProperty("enabled",monitorEnabled.load());monitor->setProperty("gain",monitorGain.load());monitor->setProperty("alignmentMs",alignmentMs.load());o->setProperty("monitor",juce::var(monitor));
    auto* mixer=new juce::DynamicObject();mixer->setProperty("channel",mixerChannel.load());mixer->setProperty("master",masterLevel->get());juce::Array<juce::var> inputs;for(auto& input:monitorInputs)inputs.add(input.load());mixer->setProperty("monitorInputs",inputs);o->setProperty("mixer",juce::var(mixer));o->setProperty("recorderRouting",recorderRouting.save());
    juce::Array<juce::var> tracks,pages;
    for(int t=0;t<4;++t)
    {
        auto* tr=new juce::DynamicObject();tr->setProperty("engine",engine(t));tr->setProperty("nativeEngine",engines[(size_t)t]->getIndex());tr->setProperty("channel",channel(t));juce::Array<juce::var> parameters,values,mappings;
        for(auto p:trackSound[(size_t)t])parameters.add(p->get());for(auto p:trackMix[(size_t)t])values.add(p->get());
        tr->setProperty("parameters",parameters);tr->setProperty("mix",values);auto* voice=new juce::DynamicObject();voice->setProperty("mono",mono[(size_t)t]->get());voice->setProperty("glide",glide[(size_t)t]->get());tr->setProperty("voice",juce::var(voice));
        if(hostingEnabled&&includeRack)
        {
            tr->setProperty("rack",synthRacks[(size_t)t].save());
            for(int i=0;i<8;++i){auto* m=new juce::DynamicObject();int s=macroSlot[(size_t)t][(size_t)i].load();auto id=synthRacks[(size_t)t].parameterID(s,macroIndex[(size_t)t][(size_t)i].load());m->setProperty("slot",s);m->setProperty("parameterId",id.isEmpty()?macroIds[(size_t)t][(size_t)i]:id);mappings.add(juce::var(m));}tr->setProperty("macros",mappings);
        }
        tracks.add(juce::var(tr));
    }
    for(auto& p:trackPages)pages.add(p.load());o->setProperty("tracks",tracks);o->setProperty("trackPages",pages);return json;
}
bool CompanionProcessor::restore(const juce::var& json,bool notifyHost)
{
    auto legacy=companion::PatchState::fromJson(json);if(!legacy)return false;auto tracks=json.getProperty("tracks",{});bool modern=json.hasProperty("layoutVersion");
    if(modern)
    {
        if(!json["layoutVersion"].isInt()||((int)json["layoutVersion"]<2||(int)json["layoutVersion"]>7)||!tracks.isArray()||tracks.size()!=4)return false;
        if(!json["selectedTrack"].isInt()||!number(json["selectedTrack"],0,7)||!json["lastSoundTrack"].isInt()||!number(json["lastSoundTrack"],0,3))return false;
        for(int t=0;t<4;++t){auto tr=tracks[t],params=tr["parameters"],values=tr["mix"];if(!tr["engine"].isInt()||!number(tr["engine"],0,3)||!tr["channel"].isInt()||!number(tr["channel"],1,16)||!params.isArray()||params.size()!=40||!values.isArray()||values.size()!=7)return false;for(auto v:*params.getArray())if(!number(v,0,1))return false;for(auto v:*values.getArray())if(!number(v,0,1))return false;}
        if((int)json["layoutVersion"]>=5)for(int t=0;t<4;++t){auto v=tracks[t]["voice"];if(!v.isObject()||!v["mono"].isBool()||!number(v["glide"],0,1))return false;}
        if((int)json["layoutVersion"]>=6){auto m=json["mixer"];bool current=(int)json["layoutVersion"]>=7;auto inputs=m[current?"monitorInputs":"inputs"];if(!m.isObject()||!m["channel"].isInt()||!number(m["channel"],1,16)||!number(m["master"],0,1)||!inputs.isArray()||inputs.size()!=(current?2:10))return false;for(auto v:*inputs.getArray())if(!v.isInt()||!number(v,-1,63))return false;if(current){RecorderRouting validator;if(!validator.restore(json["recorderRouting"]))return false;}}
        ModulationCurve validator;if(!validator.restore(json["modulation"]))return false;if((int)json["layoutVersion"]>=3&&!validator.restore(json["modulation2"]))return false;
        if((int)json["layoutVersion"]>=4){TransportClock clock;if(!clock.restore(json["transportClock"]))return false;auto m=json["monitor"];if(!m.isObject()||!m["enabled"].isBool()||!number(m["gain"],0,2)||!number(m["alignmentMs"],-250,250))return false;}
    }
    auto set=[notifyHost](juce::AudioProcessorParameter* p,float value){if(notifyHost)p->setValueNotifyingHost(value);else p->setValue(value);};
    recorder.stop();panic.store(true);keyboard.reset();lastError.clear();for(int i=0;i<8;++i)cc[(size_t)i].store(legacy->cc[(size_t)i]);
    auto oldMix=json.getProperty("mix",{}),oldRacks=json.getProperty("synthRacks",{});int defaults[]={0,1,2,0};int oldSelected=(bool)json.getProperty("hostSelected",false)?3:legacy->engine;
    for(int t=0;t<4;++t)
    {
        auto n=(size_t)t;auto tr=modern?tracks[t]:juce::var();int e=modern?(int)tr["engine"]:!hostingEnabled&&t==0?legacy->engine:t==3&&(bool)json.getProperty("hostSelected",false)?3:defaults[t];
        int native=e<3?e:juce::jlimit(0,2,(int)tr.getProperty("nativeEngine",0));pluginSources[n].store(hostingEnabled&&e==3);
        set(engines[n],engines[n]->convertTo0to1(native));set(channels[n],(modern?(int)tr["channel"]-1:hostingEnabled?(legacy->channel-1+(t-oldSelected+4)%4)%16:t)/15.f);
        for(int i=0;i<40;++i)set(trackSound[n][(size_t)i],modern?(float)(double)tr["parameters"][i]:legacy->parameters[(size_t)i]);
        set(mono[n],modern&&(int)json["layoutVersion"]>=5&&(bool)tr["voice"]["mono"]?1.f:0.f);set(glide[n],modern&&(int)json["layoutVersion"]>=5?(float)(double)tr["voice"]["glide"]:0);
        auto oldSynthMix=json.getProperty("synthMix",{});auto oldValues=t<3&&oldSynthMix.isArray()&&oldSynthMix.size()==3?oldSynthMix[t]:oldMix;
        for(int i=0;i<7;++i){auto* p=trackMix[n][(size_t)i];float v=modern?(float)(double)tr["mix"][i]:oldValues.isArray()&&oldValues.size()>=7&&number(oldValues[i],0,1)?(float)(double)oldValues[i]:static_cast<juce::AudioProcessorParameter*>(p)->getDefaultValue();set(p,v);}
        if(hostingEnabled)
        {
            for(int s=0;s<3;++s)synthRacks[n].remove(s);juce::var rs;
            if(modern)rs=tr["rack"];else if(t<3&&oldRacks.isArray()&&oldRacks.size()==3)rs=oldRacks[t];else {rs=json.getProperty("rack",{}).clone();if(t<3&&rs.isArray()&&rs.size()==3)rs.getArray()->set(0,juce::var());}
            lastError+=synthRacks[n].restore(rs);resetMacros(t);auto mappings=modern?tr["macros"]:json.getProperty("macros",{});
            if(mappings.isArray()&&mappings.size()==8)for(int i=0;i<8;++i){int slot=juce::jlimit(0,2,(int)mappings[i].getProperty("slot",0));auto id=mappings[i].getProperty("parameterId",{}).toString();macroSlot[n][(size_t)i].store(slot);macroIndex[n][(size_t)i].store(synthRacks[n].parameterIndex(slot,id));macroIds[n][(size_t)i]=id;}
        }
    }
    bool previousLayout=modern&&(int)json["layoutVersion"]==2;
    auto pages=json.getProperty("trackPages",{});for(int i=0;i<8;++i){int source=previousLayout&&i==6?5:i;int maxPage=pageCount(i)-1;trackPages[(size_t)i].store(modern&&pages.isArray()&&pages.size()==8&&!(previousLayout&&i==5)?juce::jlimit(0,maxPage,(int)pages[source]):0);}
    modulation.channel.store(13);modulation2.channel.store(14);
    if(modern)modulation.restore(json["modulation"]);else {ModulationCurve init;modulation.restore(init.save());}
    if(modern&&!previousLayout)modulation2.restore(json["modulation2"]);else {ModulationCurve init;modulation2.restore(init.save());}
    if(modern&&(int)json["layoutVersion"]>=4){transport.restore(json["transportClock"]);auto m=json["monitor"];monitorEnabled.store((bool)m["enabled"]);monitorGain.store((float)(double)m["gain"]);alignmentMs.store((float)(double)m["alignmentMs"]);}else{transport.source.store(TransportClock::digitakt);transport.setTempo(120);transport.reset();monitorEnabled.store(false);monitorGain.store(1);alignmentMs.store(0);}
    if(modern&&(int)json["layoutVersion"]>=6){auto m=json["mixer"];bool current=(int)json["layoutVersion"]>=7;mixerChannel.store((int)m["channel"]);set(masterLevel,(float)(double)m["master"]);for(int i=0;i<2;++i)monitorInputs[(size_t)i].store((int)m[current?"monitorInputs":"inputs"][current?i:8+i]);if(current)recorderRouting.restore(json["recorderRouting"]);else recorderRouting.migrate(m["inputs"]);legacyMonitorInputs.store(!current);}else{mixerChannel.store(16);set(masterLevel,1);for(int i=0;i<2;++i)monitorInputs[(size_t)i].store(i);recorderRouting.replace({});legacyMonitorInputs.store(false);if(!json["modulation2"].hasProperty("rateVersion"))modulation2.midiCC.store(21);}
    auto knownDevice=recorderDevice();recorderRouting.bindLegacy(knownDevice);if(knownDevice.connected&&legacyMonitorInputs.exchange(false))for(auto& input:monitorInputs)input.store(knownDevice.physicalInput(input.load()));
    int selected=modern?(int)json["selectedTrack"]:!hostingEnabled?0:(bool)json.getProperty("hostSelected",false)?3:legacy->engine;
    if(previousLayout)selected=selected==5?6:selected>=6?7:selected;
    hardware.restore(json.getProperty("hardware",{}));lastSoundTrack.store(modern?(int)json["lastSoundTrack"]:!hostingEnabled?0:(bool)json.getProperty("hostSelected",false)?3:legacy->engine);selectTrack(selected);updateLatency();return true;
}
void CompanionProcessor::getStateInformation(juce::MemoryBlock& data){auto s=juce::JSON::toString(state(hostingEnabled));data.replaceAll(s.toRawUTF8(),(size_t)s.getNumBytesAsUTF8());}
void CompanionProcessor::setStateInformation(const void* data,int bytes){restore(juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data),bytes)),false);}
juce::AudioProcessorEditor* CompanionProcessor::createEditor(){return new PixelEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new CompanionProcessor();}

