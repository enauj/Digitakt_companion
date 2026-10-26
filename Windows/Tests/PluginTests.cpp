#include "Processor.h"
#include "PixelEditor.h"
#include "HostedEditorWindow.h"
#include "ParameterDisplay.h"
#include "TransportSettings.h"
#include "RoutingSettings.h"
#include <cstdio>
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

static void pump(int milliseconds=120)
{
    auto until=juce::Time::getMillisecondCounter()+milliseconds;
    do{MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}juce::Thread::sleep(1);}while(juce::Time::getMillisecondCounter()<until);
}

static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static double render(CompanionProcessor& p,int channel=1)
{
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(channel,60,(juce::uint8)100),0);
    double energy=0;
    for(int i=0;i<12;++i){p.processBlock(audio,midi);midi.clear();for(int c=0;c<2;++c)for(int s=0;s<512;++s){auto v=audio.getSample(c,s);require(std::isfinite(v),"finite plugin audio");energy+=(double)v*v;}}
    p.panic.store(true);midi.clear();p.processBlock(audio,midi);return energy;
}
int main(int argc,char** argv)
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc==3&&juce::String(argv[1])=="--gui")
    {
        auto stage=[](const char* s){std::puts(s);std::fflush(stdout);};
        stage("Discovering native plugin");PluginRack rack;rack.prepare(48000,512);auto plugins=rack.discover(juce::File(juce::String::fromUTF8(argv[2])));require(!plugins.empty(),"native smoke plugin discovery");int slot=plugins[0].instrument?0:1;
        stage("Loading native plugin");require(rack.load(slot,plugins[0]).isEmpty(),"native smoke plugin load");
        juce::DocumentWindow owner("Companion native GUI check",juce::Colours::black,0);owner.setBounds(100,100,720,450);owner.setVisible(true);
        stage("Creating floating editor");{HostedEditorWindow floating(rack.instance(slot),&owner);stage("Floating editor created");std::printf("Editor %d x %d; client %d x %d; display scale %.2f\n",floating.pluginEditor()->getWidth(),floating.pluginEditor()->getHeight(),floating.getContentComponent()->getWidth(),floating.getContentComponent()->getHeight(),floating.getPeer()->getPlatformScaleFactor());std::fflush(stdout);for(int i=0;i<900&&floating.isVisible();++i)pump(100);stage("Closing floating editor");}
        stage("PASS: native GUI smoke check");return 0;
    }
    require(argc==5,"CLAP, VST3, effect and preview directory arguments");
    {
        ModulationCurve curve;auto original=curve.snapshot();int originalId=original.points[1].id;int inserted=curve.addPoint(.1f,.4f);require(inserted==1,"vertex can be inserted ahead of an existing point");int newId=curve.snapshot().points[1].id;curve.selectedPoint.store(0);curve.queueVertexControl(originalId,0,.3f);curve.queueVertexControl(originalId,1,.25f);curve.queueVertexControl(originalId,2,1);curve.advance(512,48000,0,false,false);auto points=curve.snapshot();bool reached=false,otherUntouched=false;
        for(int i=0;i<points.count;++i){auto p=points.points[(size_t)i];if(p.id==originalId)reached=p.x==.3f&&p.y==.25f&&p.bend==1;if(p.id==newId)otherUntouched=p.y==.4f;}require(reached&&otherUntouched&&curve.selectedPoint.load()==0,"fixed vertex CC edits stable identity without depending on selection or insertion order");
        require(curve.addPoint(.45f,.3f)>=0&&curve.addPoint(.7f,.6f)>=0&&curve.snapshot().count==8&&curve.addPoint(.9f,.2f)<0,"curve capacity is exactly eight vertices");points=curve.snapshot();unsigned ids=0;for(int i=0;i<8;++i)ids|=1u<<points.points[(size_t)i].id;require(ids==255,"all eight vertex IDs are unique");
        curve.lfos[0].target.store(originalId*3);curve.lfos[0].wave.store(3);curve.lfos[0].amount.store(1);curve.lfos[1].target.store(points.points[3].id*3);curve.lfos[1].wave.store(3);curve.lfos[1].amount.store(-1);curve.advance(512,48000,0,false,false);auto effective=curve.effectiveSnapshot();require(effective.points[0].x==0&&effective.points[7].x==1,"LFOs keep endpoint anchors");for(int i=1;i<8;++i)require(effective.points[(size_t)i].x-effective.points[(size_t)i-1].x>=.0049f,"simultaneous opposing X LFOs cannot cross vertices");require(curve.snapshot().points[2].x==points.points[2].x,"LFO geometry does not overwrite base vertices");
        curve.removePoint(2);require(curve.lfos[0].target.load()==-1,"deleting a vertex disconnects LFOs assigned to its identity");
        ModulationCurve bend;for(auto& l: bend.lfos){l.target.store(5);l.wave.store(3);l.amount.store(.25f);}bend.advance(512,48000,0,false,false);require(bend.effectiveSnapshot().points[1].bend==1&&bend.snapshot().points[1].bend==0,"both LFOs can add to the same vertex bend without changing its base");bend.lfos[0].enabled.store(false);bend.advance(512,48000,0,false,false);require(bend.effectiveSnapshot().points[1].bend==.5f,"each vertex LFO can be disabled independently");bend.lfos[1].amount.store(1);bend.lfos[1].smooth.store(1);bend.restart();bend.advance(480,48000,0,false,false);require(bend.lfoOutput[1].load()>0&&bend.lfoOutput[1].load()<.2f,"LFO smoothing limits a square-wave transition using its time constant");
        ModulationCurve synced;synced.lfos[0].target.store(4);synced.lfos[0].amount.store(.5f);synced.lfos[0].sync.store(true);synced.lfos[0].speed.store(15.f/127);synced.advance(512,48000,8,true,true);synced.advance(512,48000,9,true,true);require(std::abs(synced.lfoOutput[0].load()-.5f)<.00001f,"vertex LFO sync follows beats independently of the curve's free-run mode");synced.advance(512,48000,10,false,true);require(synced.lfoOutput[0].load()==0,"synced LFO restores base geometry when transport stops");
        auto legacy=original.count>0?synced.save():juce::var();legacy.getDynamicObject()->removeProperty("vertexVersion");legacy.getDynamicObject()->removeProperty("lfos");juce::Array<juce::var> dense;for(int i=0;i<16;++i){auto* p=new juce::DynamicObject();p->setProperty("x",i/15.f);p->setProperty("y",(i%3)*.4f);p->setProperty("bend",0);dense.add(juce::var(p));}legacy.getDynamicObject()->setProperty("points",dense);ModulationCurve migrated;require(migrated.restore(legacy)&&migrated.snapshot().count==8&&migrated.save()["originalPoints"].size()==16,"older dense curves reduce to eight and preserve all original points for recovery");auto roundtrip=migrated.save();ModulationCurve recovered;require(recovered.restore(roundtrip)&&recovered.save()["originalPoints"].size()==16,"original dense-point backup survives re-save");
        auto invalid=roundtrip.clone();invalid["points"][1].getDynamicObject()->setProperty("id",(int)invalid["points"][0]["id"]);require(!recovered.restore(invalid),"duplicate vertex IDs rejected");
        std::array<bool,120> used {};for(int p=0;p<8;++p)for(int k=0;k<8;++k){int cc=companion::midi::modCC(p,k);if(cc>=0){require(!used[(size_t)cc],"all six modulation pages have distinct static CCs");used[(size_t)cc]=true;}}for(int id=0;id<8;++id)for(int d=0;d<3;++d){int cc=companion::midi::vertexCC(id,d);require(!used[(size_t)cc]&&cc!=32&&cc!=64&&cc<120,"24 fixed vertex CCs avoid every other modulation control");used[(size_t)cc]=true;}
        CompanionProcessor p;p.prepareToPlay(48000,512);p.selectTrack(7);p.modulation.selectedPoint.store(4);juce::AudioBuffer<float> a(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(13,73,23),0);midi.addEvent(juce::MidiMessage::controllerEvent(14,73,87),0);p.processBlock(a,midi);require(std::abs(p.modulation.snapshot().points[1].y-23.f/127)<.00001&&std::abs(p.modulation2.snapshot().points[1].y-87.f/127)<.00001&&p.modulation.selectedPoint.load()==4,"both curves receive their own per-vertex CCs off-page and off-selection");
        midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(13,106,32),0);midi.addEvent(juce::MidiMessage::controllerEvent(14,111,0),0);p.processBlock(a,midi);require(std::abs(p.modulation.lfos[0].phaseOffset.load()-32.f/127)<.00001&&p.modulation2.lfos[1].amount.load()==-1&&p.modulation.lfos[1].amount.load()==0,"LFO CC banks route to the correct source and curve independently of the visible page");
        p.modulation2.lfos[1].target.store(4);p.modulation2.lfos[1].amount.store(-.75f);p.modulation2.lfos[1].wave.store(2);p.modulation2.lfos[1].smooth.store(.5f);p.selectTrack(5);p.setPage(5);auto project=p.state(false);CompanionProcessor restored;require(restored.restore(project)&&restored.page.load()==5&&restored.modulation2.lfos[1].target.load()==4&&restored.modulation2.lfos[1].amount.load()==-.75f&&restored.modulation2.lfos[1].wave.load()==2,"both LFO configurations and six-page navigation round trip");
        std::puts("PASS: eight stable vertex identities, fixed CCs, independent LFOs, constrained geometry, dense-curve migration and state");
    }
    {
        CompanionProcessor dry,animated;for(auto* p:{&dry,&animated}){p->prepareToPlay(48000,512);p->sound[20]->setValueNotifyingHost(0);auto shape=p->modulation.snapshot();for(int i=0;i<shape.count;++i)p->modulation.movePoint(i,shape.points[(size_t)i].x,0);p->modulation.depth.store(1);p->modulation.bipolar.store(false);p->modulation.destination[0].store(20);p->modulation.amount[0].store(1);}
        animated.modulation.lfos[0].target.store(1);animated.modulation.lfos[0].wave.store(3);animated.modulation.lfos[0].speed.store(0);animated.modulation.lfos[0].amount.store(1);require(render(dry)==0&&render(animated)>.01,"vertex LFO changes curve output and actual synth audio");require(animated.sound[20]->get()==0&&animated.modulation.snapshot().points[0].y==0,"vertex LFO preserves synth and curve base values");animated.modulation.lfos[0].enabled.store(false);require(render(animated)==0,"disabling vertex LFO restores base curve audio");
        animated.selectTrack(0);animated.setPage(3);require(animated.isVoicePage()&&animated.controlCC(0)==70&&animated.controlCC(1)==71,"DAW simple synth exposes voice/glide page and fixed CCs");animated.setControl(0,1);animated.setControl(1,std::sqrt(.05f));require(animated.mono[0]->get()&&animated.controlText(1)=="100.0 MS","glide time and mono legato controls use real units");require(animated.getParameters()[196]==animated.mono[0]&&animated.getParameters().size()==205,"new voice parameters append without shifting existing automation ordinals");auto state=animated.state(false);CompanionProcessor restored;require(restored.restore(state)&&restored.mono[0]->get()&&std::abs(restored.glide[0]->get()-std::sqrt(.05f))<.00001f&&!restored.mono[1]->get(),"independent voice/glide settings save per sound track");state.getDynamicObject()->setProperty("layoutVersion",4);require(restored.restore(state)&&!restored.mono[0]->get()&&restored.glide[0]->get()==0,"older projects default to poly with glide off");
        auto directory=juce::File(juce::String::fromUTF8(argv[4]));directory.createDirectory();animated.selectTrack(4);animated.modulation.lfos[0].enabled.store(true);animated.modulation.lfos[0].target.store(4);animated.modulation.lfos[0].amount.store(.35f);animated.modulation.lfos[1].target.store(8);animated.modulation.lfos[1].amount.store(-.5f);animated.modulation.advance(512,48000,0,false,false);auto editor=std::make_unique<PixelEditor>(animated);editor->showPerformance();auto snapshot=[&](const juce::String& name){auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream out(directory.getChildFile(name));out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);};
        for(int page=0;page<8;++page){animated.setPage(page);snapshot("vertex-"+juce::String(page)+".png");}animated.selectTrack(0);animated.setPage(3);snapshot("synth-voice.png");
        std::puts("PASS: audible vertex modulation, base preservation, glide controls/automation/state and rendered six-page editors");
    }
    {
        std::array<bool,120> used {};for(int i=0;i<40;++i){int code=companion::midi::nativeCC(i);require(code<120&&!used[(size_t)code]&&code!=32&&code!=64,"native parameter CC addresses are unique and avoid bank/sustain controllers");used[(size_t)code]=true;require(companion::midi::nativeParameter(code)==i,"fixed CC map reverses to exact native parameter");}
        for(int code=72;code<=95;++code)require(!used[(size_t)code],"FX, macro and mix banks do not collide with native parameters");
        std::array<bool,120> curveCC {};for(int page=0;page<4;++page)for(int k=0;k<8;++k){int c=companion::midi::modCC(page,k);if(c<0)continue;require(!curveCC[(size_t)c],"all modulation pages have distinct knob CCs");curveCC[(size_t)c]=true;}
        CompanionProcessor groove,reference;groove.prepareToPlay(48000,512);reference.prepareToPlay(48000,512);reference.setPage(2);juce::AudioBuffer<float> a(2,512),b(2,512);
        for(int block=0;block<24;++block)
        {
            groove.selectTrack(block%8);groove.setPage(block%4);juce::MidiBuffer input;if(block==0)input.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);
            input.addEvent(juce::MidiMessage::controllerEvent(1,48,30+block*3),0);input.addEvent(juce::MidiMessage::controllerEvent(1,16,block*5),0);input.addEvent(juce::MidiMessage::controllerEvent(1,56,block*2),0);auto copy=input;groove.processBlock(a,input);reference.processBlock(b,copy);
            for(int c=0;c<2;++c)for(int s=0;s<512;++s)require(std::abs(a.getSample(c,s)-b.getSample(c,s))<.000001f,"the same parameter-lock groove sounds identical on every selected track/page");
            require(std::abs(groove.sound[8]->get()-(30+block*3)/127.f)<.00001f,"filter CC continues updating cutoff on unrelated pages");
        }
        auto old=groove.state(false);old.getDynamicObject()->setProperty("layoutVersion",3);for(const char* key:{"modulation","modulation2"}){auto mod=old[key];mod.getDynamicObject()->removeProperty("routeVersion");mod.getDynamicObject()->removeProperty("midiChannel");mod["routes"].getArray()->removeRange(4,4);for(auto& r:*mod["routes"].getArray())r.getDynamicObject()->setProperty("amount",.35);}
        CompanionProcessor migrated;require(migrated.restore(old)&&migrated.modulation.targetTrack[1].load()==1&&migrated.modulation.destination[4].load()==-1&&std::abs(migrated.modulation.amount[0].load()-.35f)<.00001f&&migrated.modulation2.channel.load()==14,"0.5 curves migrate positive amounts and track targets without changing their sound");
        std::puts("PASS: unique fixed CC maps, page-independent audible parameter locks and four-route migration");
    }
    {
        for(float position:{0.f,.5f,64.f/127.f,1.f})
        {
            CompanionProcessor p,reference;p.prepareToPlay(48000,512);reference.prepareToPlay(48000,512);p.selectTrack(4);p.setPage(2);p.setControl(4,position);float amount=p.modulation.amount[0].load();require(amount==(position==0?-1.f:position==1?1.f:0.f),"modulation amount has negative/zero/positive endpoints and exact MIDI centre");
            auto shape=p.modulation.snapshot();for(int i=0;i<shape.count;++i)p.modulation.movePoint(i,shape.points[(size_t)i].x,1);p.modulation.destination[0].store(20);p.modulation.depth.store(.25f);p.modulation.bipolar.store(false);p.sound[20]->setValueNotifyingHost(.5f);reference.sound[20]->setValueNotifyingHost(.5f+.25f*amount);double actual=render(p),expected=render(reference);require(std::abs(actual-expected)<.001,"negative and positive route amounts invert/scale real DSP modulation");
        }
        CompanionProcessor p,reference;p.prepareToPlay(48000,512);reference.prepareToPlay(48000,512);p.sound[20]->setValueNotifyingHost(0);reference.sound[20]->setValueNotifyingHost(.32f);auto shape=p.modulation.snapshot();for(int i=0;i<shape.count;++i)p.modulation.movePoint(i,shape.points[(size_t)i].x,1);p.modulation.depth.store(.04f);p.modulation.bipolar.store(false);
        for(int r=0;r<8;++r){p.modulation.targetTrack[(size_t)r].store(0);p.modulation.destination[(size_t)r].store(20);p.modulation.amount[(size_t)r].store(1);}require(std::abs(render(p)-render(reference))<.001,"all eight freely assignable routes can target one sound track and sum correctly");
        p.selectTrack(4);p.setPage(3);p.modulation.targetTrack[7].store(3);p.modulation.destination[7].store(47);p.modulation.amount[7].store(-.9f);auto state=p.state(false);CompanionProcessor restored;require(restored.restore(state)&&restored.page.load()==3&&restored.modulation.targetTrack[7].load()==3&&restored.modulation.destination[7].load()==47&&restored.modulation.amount[7].load()==-.9f,"second assignment page saves arbitrary track/FX/negative amount");
        auto malformed=state.clone();malformed["modulation"]["routes"][7].getDynamicObject()->setProperty("track",4);require(!restored.restore(malformed),"invalid free-route target is rejected");
        std::puts("PASS: bipolar amounts, exact centre, eight routes per curve, same-track sums and second assignment page state");
    }
    {
        TransportClock master;master.source.store(TransportClock::companion);master.setTempo(120);master.requestStart();int pulses=0,starts=0;juce::MidiBuffer out;
        for(int block=0;block<96;++block){master.advance(500,48000,out);for(auto e:out){if(e.getMessage().isMidiClock())++pulses;if(e.getMessage().isMidiStart())++starts;require(e.samplePosition>=0&&e.samplePosition<500,"clock events have valid sample positions");}}
        require(pulses==48&&starts==1&&master.running.load()&&std::abs(master.beat.load()-2)<.00001,"120 BPM master generates exactly 24 PPQN with start and sample-time beats");master.requestStop();master.advance(512,48000,out);bool stopped=false;for(auto e:out)stopped|=e.getMessage().isMidiStop();require(stopped&&!master.running.load(),"master stop is emitted once");master.requestContinue();master.advance(512,48000,out);require(master.running.load(),"master continue resumes transport");
        auto saved=master.save();TransportClock restored;require(restored.restore(saved)&&!restored.running.load(),"clock source/tempo restore without automatically starting hardware");restored.advance(512,48000,out);for(auto e:out)require(e.getMessage().isMidiClock(),"restoring master sends no start/continue command");
        saved.getDynamicObject()->setProperty("source",0);require(master.restore(saved),"switching from running master via project restore succeeds");master.advance(512,48000,out);bool restoreStop=false;for(auto e:out)restoreStop|=e.getMessage().isMidiStop();require(restoreStop,"restoring an external-clock project stops the previous running master");
        TransportClock slave;slave.advance(1000,48000,out);slave.input(juce::MidiMessage::midiStart(),0,48000);
        for(int pulse=0;pulse<180;++pulse){double stamp=slave.samplePosition();slave.advance(1000,48000,out);slave.input(juce::MidiMessage::midiClock(),stamp+(pulse%2?20:-20),48000);require(out.isEmpty(),"slave never echoes incoming clock or transport");}
        require(std::abs(slave.bpm.load()-120)<.02&&std::abs(slave.displayBpm.load()-120)<.2,"full-beat tempo estimation and held display reject alternating pulse jitter");
        CompanionProcessor p;p.hostingEnabled=true;p.prepareToPlay(48000,512);p.transport.source.store(TransportClock::companion);p.transport.requestStart();juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);midi.addEvent(juce::MidiMessage::midiStop(),16);p.processBlock(audio,midi);require(p.playing.load(),"master ignores incoming transport feedback");for(auto e:midi)require(e.getMessage().isMidiStart()||e.getMessage().isMidiClock(),"standalone output contains generated transport only, with no raw note echo");
        std::puts("PASS: clock master sample timing, PPQN/start/stop/continue, no input echo, state and jitter-resistant slave tempo");
    }
    {
        CompanionProcessor p;p.hostingEnabled=true;auto layout=p.getBusesLayout();layout.inputBuses.getReference(0)=juce::AudioChannelSet::stereo();require(p.setBusesLayout(layout),"standalone accepts stereo return input");p.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
        auto fill=[&]{for(int i=0;i<512;++i){audio.setSample(0,i,.25f);audio.setSample(1,i,-.5f);}};fill();p.processBlock(audio,midi);require(audio.getMagnitude(0,512)==0,"hardware return monitoring defaults off");
        p.monitorEnabled.store(true);p.monitorGain.store(.5f);fill();p.processBlock(audio,midi);require(audio.getSample(0,100)==.125f&&audio.getSample(1,100)==-.25f,"stereo return preserves channels and applies gain into the shared output");
        p.alignmentMs.store(-10);p.monitorGain.store(1);p.reset();audio.clear();audio.setSample(0,0,.75f);audio.setSample(1,0,.75f);midi.clear();p.processBlock(audio,midi);for(int i=0;i<512;++i)require(std::abs(audio.getSample(0,i)-(i==480?.75f:0))<.000001f,"negative alignment delays hardware return by the requested sample count");
        auto state=p.state(false);CompanionProcessor restored;require(restored.restore(state)&&restored.monitorEnabled.load()&&restored.monitorGain.load()==1&&restored.alignmentMs.load()==-10,"return routing settings round trip");
        CompanionProcessor delayed,normal;delayed.hostingEnabled=normal.hostingEnabled=true;delayed.alignmentMs.store(10);delayed.prepareToPlay(48000,512);normal.prepareToPlay(48000,512);juce::AudioBuffer<float> a(2,512),b(2,512);juce::MidiBuffer input;input.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);auto copy=input;delayed.processBlock(a,input);normal.processBlock(b,copy);for(int i=0;i<512;++i)require(std::abs(a.getSample(0,i)-(i<480?0:b.getSample(0,i-480)))<.000001f,"positive alignment delays Companion synths by the requested sample count");
        std::puts("PASS: optional stereo return, default-off monitoring/gain, signed timing alignment and saved routing state");
        juce::File directory(juce::String::fromUTF8(argv[4]));directory.createDirectory();auto snapshot=[&](juce::Component& component,const juce::String& name){auto image=component.createComponentSnapshot(component.getLocalBounds());juce::FileOutputStream file(directory.getChildFile(name));require(file.openedOk(),"settings snapshot opens");file.setPosition(0);file.truncate();juce::PNGImageFormat().writeImageToStream(image,file);};
        p.transport.source.store(TransportClock::companion);TransportSettings tempo(p);snapshot(tempo,"transport-master.png");juce::AudioDeviceManager noDevice;RoutingSettings routing(noDevice,p);snapshot(routing,"routing-latency.png");
    }
    {
        CompanionProcessor p;p.prepareToPlay(48000,512);
        for(int engine=0;engine<3;++engine){p.selectTrack(engine);auto e=render(p,engine+1);std::printf("Engine %d energy %.9f (selected %d, level %.3f)\n",engine,e,p.engine(),p.sound[20]->get());require(e>.0001,"independent DAW channel renders each engine");}
        p.engineParameter->setValueNotifyingHost(.5f);require(p.engine(0)==1,"legacy normalized engine automation retains metallic at 0.5");p.engineParameter->setValueNotifyingHost(1.f);require(p.engine(0)==2,"legacy normalized engine automation retains FM at 1.0");p.setTrackEngine(0,0);require(p.sound[0]->paramID=="dual.shape1"&&p.trackSound[1][0]->paramID=="track2.dual.shape1","stable track 1 IDs and distinct track 2 automation IDs");
        p.selectTrack(0);p.setPage(1);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,48,64),0);p.processBlock(audio,midi);
        require(std::abs(p.sound[8]->get() - 64.f/127.f)<.00001f,"MIDI CC updates automatable cutoff");
        p.mix[3]->setValueNotifyingHost(.25f);require(std::abs(p.mixValues()[3]-.25f)<.00001f,"DAW output automation retained");
        juce::MemoryBlock state;p.getStateInformation(state);CompanionProcessor restored;restored.setStateInformation(state.getData(),(int)state.getSize());
        require(std::abs(restored.sound[8]->get()-p.sound[8]->get())<.000001f&&restored.engine()==0,"DAW processor state restores engine and parameters");
        auto editor=std::make_unique<PixelEditor>(p);juce::File directory(juce::String::fromUTF8(argv[4]));directory.createDirectory();
        auto snapshot=[&](const juce::String& name){auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream file(directory.getChildFile(name));require(file.openedOk(),"preview file opens");file.setPosition(0);file.truncate();juce::PNGImageFormat().writeImageToStream(image,file);};
        snapshot("tracks.png");p.selectTrack(0);editor->showPerformance();
        for(int page=0;page<3;++page){p.setPage(page);snapshot(juce::String(page)+"-synth.png");}
    }
    {
        CompanionProcessor ensemble;ensemble.setTrackEngine(3,2);ensemble.prepareToPlay(48000,512);
        std::array<std::unique_ptr<CompanionProcessor>,4> references;
        std::array<juce::AudioBuffer<float>,4> separate;
        for(int t=0;t<4;++t){references[(size_t)t]=std::make_unique<CompanionProcessor>();references[(size_t)t]->setTrackEngine(3,2);references[(size_t)t]->prepareToPlay(48000,512);separate[(size_t)t].setSize(2,512);}
        juce::AudioBuffer<float> together(2,512);juce::MidiBuffer all;
        for(int t=0;t<4;++t)all.addEvent(juce::MidiMessage::noteOn(t+1,60+t*3,(juce::uint8)92),t*91);
        double energy=0;
        for(int block=0;block<8;++block)
        {
            ensemble.selectTrack(block);ensemble.processBlock(together,all);
            for(int t=0;t<4;++t){juce::MidiBuffer one;if(block==0)one.addEvent(juce::MidiMessage::noteOn(t+1,60+t*3,(juce::uint8)92),t*91);references[(size_t)t]->processBlock(separate[(size_t)t],one);}
            for(int c=0;c<2;++c)for(int s=0;s<512;++s){float sum=0;for(auto& a:separate)sum+=a.getSample(c,s);require(std::abs(together.getSample(c,s)-sum)<.00001f,"four simultaneous tracks equal independent audio sum, including MIDI positions and UI changes");energy+=sum*sum;}
            all.clear();
        }
        require(energy>.01,"four-track ensemble audible");for(auto& peak:ensemble.trackPeaks)require(peak.load()>.0001f,"all four tracks keep sounding while other pages are selected");
        all.addEvent(juce::MidiMessage::allSoundOff(1),0);ensemble.processBlock(together,all);require(ensemble.trackPeaks[0].load()==0&&ensemble.trackPeaks[1].load()>.0001f&&ensemble.trackPeaks[2].load()>.0001f&&ensemble.trackPeaks[3].load()>.0001f,"track-local all sound off leaves other tracks playing");
        ensemble.setTrackChannel(3,12);all.clear();all.addEvent(juce::MidiMessage::noteOn(4,72,(juce::uint8)100),0);ensemble.processBlock(together,all);require(ensemble.trackPeaks[3].load()==0,"old MIDI channel no longer triggers track");all.clear();all.addEvent(juce::MidiMessage::noteOn(12,72,(juce::uint8)100),127);ensemble.processBlock(together,all);require(ensemble.trackPeaks[3].load()>.0001f,"custom MIDI channel triggers the fourth track");
        for(int t=0;t<4;++t){ensemble.setTrackEngine(t,2);ensemble.selectTrack(t);ensemble.setPage(1);ensemble.setControl(0,.15f+t*.18f);}
        ensemble.selectTrack(7);auto saved=ensemble.state(false);CompanionProcessor restored;require(restored.restore(saved),"four-track project restores");require(restored.track()==7&&restored.soundTrack()==3,"reserved selection and last sound track restore independently");
        for(int t=0;t<4;++t)require(restored.engine(t)==2&&std::abs(restored.trackSound[(size_t)t][8]->get()-(.15f+t*.18f))<.00001f,"same engine on four tracks retains independent patches");
        auto malformed=saved.clone();malformed["tracks"][1].getDynamicObject()->setProperty("parameters",juce::Array<juce::var>{.5});float before=restored.trackSound[0][8]->get();require(!restored.restore(malformed)&&restored.trackSound[0][8]->get()==before,"malformed multitrack project rejected without partial changes");
        auto legacy=saved.clone();for(const char* key:{"layoutVersion","tracks","trackPages","selectedTrack","lastSoundTrack","modulation"})legacy.getDynamicObject()->removeProperty(key);legacy.getDynamicObject()->setProperty("engine",2);legacy.getDynamicObject()->setProperty("channel",9);CompanionProcessor migrated;migrated.hostingEnabled=true;require(migrated.restore(legacy),"previous single-source project migrates");for(int t=0;t<4;++t)require(migrated.channel(t)==9+(t+2)%4&&std::abs(migrated.trackSound[(size_t)t][8]->get()-ensemble.trackSound[0][8]->get())<.00001f,"legacy shared patch migrates while the old selected source keeps its receive channel");
        CompanionProcessor oldDaw;require(oldDaw.restore(legacy)&&oldDaw.engine(0)==2&&oldDaw.track()==0&&oldDaw.channel(0)==1,"legacy DAW source keeps sounding on MIDI channel 1");
        std::puts("PASS: simultaneous four-track audio, sample-position routing, selection continuity, custom channels, independent engine patches and legacy migration");
    }
    {
        CompanionProcessor p;p.prepareToPlay(48000,512);p.selectTrack(0);p.setTrackEngine(0,2);p.setPage(0);
        for(int algorithm=0;algorithm<8;++algorithm){p.setControl(0,algorithm/7.f);require(p.controlText(0)==juce::String(algorithm+1)+" / 8"&&std::abs(p.controlValue(0)-algorithm/7.f)<.00001f,"FM knob visits exactly eight algorithms with matching display/position");require((int)std::floor(p.trackSound[0][32]->get()*8)==algorithm,"FM knob agrees with DSP algorithm selection");}
        p.setControl(1,0);require(p.controlText(1)=="0.0625 X","FM ratio display matches DSP at lower bound");p.setControl(1,1);require(p.controlText(1)=="16 X","FM ratio display matches DSP upper bound");
        p.setTrackEngine(0,1);for(int model=0;model<4;++model){p.setControl(1,model/3.f);require(p.controlText(1)=="MODEL "+juce::String(model+1),"metallic knob has exactly four models");}
        p.setTrackEngine(0,0);p.setControl(2,0);require(p.controlText(2)=="-24 ST","oscillator semitone lower bound");p.setControl(2,1);require(p.controlText(2)=="24 ST","oscillator semitone upper bound");p.setControl(7,.49f);require(p.controlText(7)=="OFF","sync control snaps to off");p.setControl(7,.51f);require(p.controlText(7)=="ON","sync control snaps to on");
        p.setPage(1);p.setControl(0,0);require(p.controlText(0)=="35.0 HZ","base cutoff display follows DSP frequency mapping");p.setControl(4,1);require(p.controlText(4)=="2.00 S","filter attack display follows DSP time mapping");
        require(p.controlCC(0)==48,"cutoff has a fixed page-specific CC");
        p.selectTrack(4);p.setPage(0);p.setControl(0,.2f);p.currentModulation().movePoint(1,.3f,.7f);
        p.selectTrack(5);p.setControl(0,.8f);p.currentModulation().movePoint(1,.4f,.2f);p.setPage(2);p.setControl(4,1);p.currentModulation().destination[0].store(47);
        require(std::abs(p.modulation.speed.load()-.2f)<.00001f&&std::abs(p.modulation2.speed.load()-.8f)<.00001f&&p.modulation.snapshot().points[1].y!=p.modulation2.snapshot().points[1].y,"track 6 edits a separate curve and settings");
        p.selectTrack(7);juce::AudioBuffer<float> a(2,512);juce::MidiBuffer cc;cc.addEvent(juce::MidiMessage::controllerEvent(14,16,100),0);p.processBlock(a,cc);require(std::abs(p.modulation2.speed.load()-100.f/127)<.00001f&&p.modulation.speed.load()==.2f&&p.hardware.takeMessages().isEmpty()&&!p.hardware.active.load(),"track 6 MIDI controls the second curve without hardware output or changing curve 1 while another track is selected");p.selectTrack(5);p.setPage(0);p.setControl(0,.8f);p.setPage(2);
        auto project=p.state(false);CompanionProcessor loaded;require(loaded.restore(project)&&loaded.track()==5&&loaded.modulation2.destination[0].load()==47&&loaded.modulation2.amount[0].load()==1&&loaded.modulation2.snapshot().points[1].y==.2f,"both curves, FX routes, editor pages and selected track round trip");
        auto bad=project.clone();bad["modulation2"]["points"][1].getDynamicObject()->setProperty("x",0);float before=loaded.sound[8]->get();require(!loaded.restore(bad)&&loaded.sound[8]->get()==before,"invalid second curve rejected before patch mutation");
        auto old=project.clone();old.getDynamicObject()->setProperty("layoutVersion",2);old.getDynamicObject()->removeProperty("modulation2");old.getDynamicObject()->setProperty("selectedTrack",5);old["trackPages"].getArray()->set(5,3);old["modulation"]["routes"][0].getDynamicObject()->setProperty("destination",8);
        require(loaded.restore(old)&&loaded.track()==6&&loaded.page.load()==3&&loaded.modulation2.destination[0].load()==-1&&loaded.modulation.destination[0].load()==8,"0.4 FX selection/page migrates to track 7 and second curve initializes unassigned");
        // Two unipolar curves add before clamping, and continue while track 7 is visible.
        CompanionProcessor dual;dual.prepareToPlay(48000,512);dual.sound[20]->setValueNotifyingHost(0);dual.selectTrack(6);
        for(auto* mod:{&dual.modulation,&dual.modulation2}){auto shape=mod->snapshot();for(int i=0;i<shape.count;++i)mod->movePoint(i,shape.points[(size_t)i].x,1);mod->enabled.store(true);mod->bipolar.store(false);mod->depth.store(.2f);mod->amount[0].store(1);mod->destination[0].store(20);}
        CompanionProcessor reference;reference.prepareToPlay(48000,512);reference.sound[20]->setValueNotifyingHost(.4f);double expected=render(reference),actual=render(dual);require(expected>.01&&std::abs(actual-expected)<.00001,"independent modulation curves combine on the same native destination without base drift");require(dual.sound[20]->get()==0,"combined modulation preserves base value");
        std::puts("PASS: real function units, stepped controls, two independent curves, summed native routes and old/new layout migration");
    }
    {
        ModulationCurve curve;auto initial=curve.snapshot();require(initial.count==5&&std::abs(ModulationCurve::sample(initial,.2f)-1.f)<.00001f,"MSEG interpolates default vertices");
        curve.bendPoint(0,1);require(ModulationCurve::sample(curve.snapshot(),.1f)<.05f,"outgoing segment curvature changes interpolation");curve.resetShape();int added=curve.addPoint(.4f,.3f);require(added==2&&curve.snapshot().count==6,"MSEG point insertion sorted");curve.movePoint(added,.99f,.7f);auto shape=curve.snapshot();require(shape.points[2].x<shape.points[3].x,"point drag cannot reverse order");curve.movePoint(0,.5f,.1f);curve.movePoint(shape.count-1,.5f,.2f);shape=curve.snapshot();require(shape.points[0].x==0&&shape.points[(size_t)shape.count-1].x==1,"MSEG endpoints stay anchored");curve.removePoint(0);require(curve.snapshot().count==6,"endpoint cannot be deleted");curve.removePoint(2);require(curve.snapshot().count==5,"interior vertex deletion");
        curve.resetShape();curve.enabled.store(true);curve.speed.store(0);curve.depth.store(1);curve.bipolar.store(false);curve.restart();curve.advance(480000,48000,0,false,false);require(std::abs(curve.phase.load()-.5f)<.00001f,"free-running MSEG uses sample time");
        curve.loop.store(false);curve.restart();curve.advance(1920000,48000,0,false,false);require(curve.phase.load()==1,"one-shot holds final point");curve.sync.store(true);curve.loop.store(true);curve.speed.store(15.f/127.f);curve.restart();curve.advance(512,48000,8,true,true);curve.advance(512,48000,10,true,true);require(std::abs(curve.phase.load()-.5f)<.00001f,"MSEG sync follows beats and cycle length");curve.advance(512,48000,11,false,true);require(curve.output.load()==0&&std::abs(curve.phase.load()-.5f)<.00001f,"stopped sync transport restores base modulation offset");
        curve.destination[0].store(8);curve.amount[0].store(.72f);auto saved=curve.save();ModulationCurve restored;require(restored.restore(saved)&&restored.destination[0].load()==8&&std::abs(restored.amount[0].load()-.72f)<.00001f,"MSEG curve, settings and routes round trip");auto bad=saved.clone();bad["points"][1].getDynamicObject()->setProperty("x",0);require(!restored.restore(bad),"invalid MSEG vertex ordering rejected");
        CompanionProcessor dry,modulated;dry.prepareToPlay(48000,512);modulated.prepareToPlay(48000,512);auto s=modulated.modulation.snapshot();for(int i=0;i<s.count;++i)modulated.modulation.movePoint(i,s.points[(size_t)i].x,0);modulated.modulation.destination[0].store(20);modulated.modulation.depth.store(1);modulated.modulation.amount[0].store(1);modulated.modulation.bipolar.store(true);modulated.modulation.enabled.store(true);
        auto dryEnergy=render(dry);auto mutedEnergy=render(modulated);require(dryEnergy>.01&&mutedEnergy<dryEnergy*.0001,"curve modulation reaches the actual track DSP level");require(std::abs(modulated.trackSound[0][20]->get()-dry.trackSound[0][20]->get())<.000001f,"modulation leaves base parameter and automation value untouched");modulated.modulation.enabled.store(false);require(render(modulated)>.01,"disabling MSEG restores normal sound without rewriting patch");
        for(int target=0;target<4;++target)
        {
            CompanionProcessor routed;routed.prepareToPlay(48000,512);routed.selectTrack(4);auto flat=routed.modulation.snapshot();for(int i=0;i<flat.count;++i)routed.modulation.movePoint(i,flat.points[(size_t)i].x,0);routed.modulation.destination[(size_t)target].store(20);routed.modulation.amount[(size_t)target].store(1);routed.modulation.depth.store(1);routed.modulation.enabled.store(true);
            require(render(routed,target+1)<.000001,"MSEG destination reaches each of tracks 1-4 while modulation editor selected");require(render(routed,(target+1)%4+1)>.01,"MSEG route leaves other tracks audible");
        }
        auto editor=std::make_unique<PixelEditor>(modulated);juce::File directory(juce::String::fromUTF8(argv[4]));modulated.selectTrack(4);editor->showPerformance();modulated.modulation.resetShape();modulated.modulation.destination[0].store(8);modulated.modulation.destination[1].store(24);modulated.modulation.destination[2].store(35);modulated.modulation.destination[3].store(20);
        for(int p=0;p<4;++p){modulated.setPage(p);auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream out(directory.getChildFile("modulation-"+juce::String(p)+".png"));out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);}
        auto project=modulated.state(false);CompanionProcessor loaded;require(loaded.restore(project)&&loaded.modulation.destination[2].load()==35&&loaded.track()==4,"MSEG is saved in DAW project state");
        std::puts("PASS: editable curved MSEG, vertex constraints, sample-time playback, MIDI/DAW tempo sync, one-shot, live DSP routing, base preservation and project state");
    }
    for(int format=1;format<=2;++format)
    {
        CompanionProcessor host;host.hostingEnabled=true;host.prepareToPlay(48000,512);
        auto found=host.rack.discover(juce::File(juce::String::fromUTF8(argv[format])));require(!found.empty(),"exported format discovered");
        host.selectTrack(3);auto error=host.loadPlugin(0,found[0]);if(error.isNotEmpty())std::fprintf(stderr,"load: %s\n",error.toRawUTF8());require(error.isEmpty(),"exported format loads in standalone rack");
        host.selectTrack(3);require(render(host,4)>.0001,"exported format renders through hosted instrument slot");
        host.setPage(1);host.assignMacro(0,0,0);host.setControl(0,.333f);
        require(std::abs(host.controlValue(0)-.333f)<.005f,"host macro controls plugin parameter");
        auto state=host.state();CompanionProcessor restored;restored.hostingEnabled=true;restored.prepareToPlay(48000,512);require(restored.restore(state),"host project restores");
        require(restored.rack.name(0)==host.rack.name(0),"plugin identity restores");
        restored.setPage(1);require(std::abs(restored.controlValue(0)-.333f)<.005f,"plugin state and stable macro ID restore");
        require(render(restored,4)>.0001,"restored hosted plugin renders");
        {
            juce::DocumentWindow owner("Test owner",juce::Colours::black,0);owner.setBounds(100,100,720,450);owner.setVisible(true);
            HostedEditorWindow floating(host.rack.instance(0),&owner);pump();
            require(!floating.isFullScreen()&&floating.getContentComponent()->getWidth()==floating.pluginEditor()->getWidth(),"exported plugin opens fitted floating editor");
            require(GetWindow((HWND)floating.getPeer()->getNativeHandle(),GW_OWNER)==(HWND)owner.getPeer()->getNativeHandle(),"floating editor owned by Companion window");
            require(floating.pluginEditor()->getWidth()==960&&floating.pluginEditor()->getHeight()==600,"exported native editor size preserved");
        }
        require(host.rack.instance(0)->getActiveEditor()==nullptr,"closing exported editor clears active editor");
        {HostedEditorWindow reopened(host.rack.instance(0),nullptr);pump();require(reopened.pluginEditor()!=nullptr,"exported editor reopens after close");}
        auto unavailable=host.state();auto rackState=unavailable["tracks"][3]["rack"];
        auto* array=rackState.getArray();(*array)[0].getDynamicObject()->setProperty("path","C:\\missing-dtx-test-plugin.clap");
        if(format==1)
        {
            CompanionProcessor missing;missing.hostingEnabled=true;require(missing.restore(unavailable),"missing plugin keeps project readable");
            require(missing.lastError.isNotEmpty(),"missing plugin reports an error");
            auto retained=missing.state()["tracks"][3]["rack"];require((*retained.getArray())[0].getProperty("path",{}).toString()=="C:\\missing-dtx-test-plugin.clap","missing plugin identity/state survives re-save");
        }
        auto editor=std::make_unique<PixelEditor>(host);editor->showPerformance();host.setPage(0);
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream out(juce::File(juce::String::fromUTF8(argv[4])).getChildFile(format==1?"clap-chain.png":"vst3-chain.png"));out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);
        host.releaseResources();restored.releaseResources();
        std::printf("PASS: %s instrument discovery, MIDI render, parameter mapping, state restore\n",format==1?"CLAP":"VST3");
    }
    {
        HardwareMidi hardware;require(hardware.takeMessages().isEmpty(),"hardware starts without transmitting");
        for(int p=0;p<4;++p)for(int s=0;s<8;++s)hardware.edit(p,s,(p*8+s)/127.f);
        auto messages=hardware.takeMessages();require(messages.getNumEvents()==32,"all FX and track-send controls emit once");
        int i=0;for(auto e:messages){auto m=e.getMessage();int p=i/8,s=i%8;require(m.getChannel()==(p<2?16:s+1),"Digitakt FX and per-track channels");require(m.getControllerNumber()==(p==0?85+s:p==1?24+s:p==2?82:83),"Digitakt I delay/reverb/send CC map");require(m.getControllerValue()==i+(i==0?1:0),"hardware exact MIDI value including delay calibration");++i;}
        require(hardware.takeMessages().isEmpty(),"drained hardware values are not resent");
        hardware.fxChannel.store(12);hardware.edit(0,0,.2f);hardware.edit(0,0,.5f);messages=hardware.takeMessages();require(messages.getNumEvents()==1,"hardware rapid edits coalesce");
        auto m=(*messages.begin()).getMessage();require(m.getChannel()==12&&m.getControllerValue()==64,"custom FX channel and latest value");
        hardware.trackChannels[0].store(4);hardware.edit(2,0,1.f);messages=hardware.takeMessages();require((*messages.begin()).getMessage().getChannel()==4,"custom track send channel");
        hardware.trackChannels[1].store(0);hardware.edit(2,1,1.f);require(hardware.takeMessages().isEmpty(),"disabled hardware track channel");
        auto saved=hardware.save();HardwareMidi restored;restored.restore(saved);require(restored.takeMessages().isEmpty()&&restored.value(0,0)==63&&restored.isKnown(0,0)&&restored.fxChannel.load()==12,"restoring hardware settings never transmits");
        std::puts("PASS: Digitakt I 32 controls, exact CC/channels, coalescing, disabled channels and silent restore");
    }
    {
        HardwareMidi h;h.edit(0,0,41.f/126.f);auto wire=h.takeMessages();require(h.value(0,0)==41&&wire.getNumEvents()==1&&(*wire.begin()).getMessage().getControllerValue()==42,"reported delay off-by-one corrected on the wire while displaying the requested value");h.setOffset(0,0,0);require(h.takeMessages().isEmpty()&&h.value(0,0)==42,"calibration change preserves last wire value without transmitting");h.setOffset(0,0,1);h.edit(0,0,1);wire=h.takeMessages();require(h.value(0,0)==126&&(*wire.begin()).getMessage().getControllerValue()==127,"calibrated endpoint stays representable in a MIDI byte");
        auto state=h.save();HardwareMidi restored;restored.restore(state);require(restored.offset(0,0)==1&&restored.value(0,0)==126&&restored.takeMessages().isEmpty(),"hardware calibration/value state restores without output");
        std::puts("PASS: delay display calibration, exact wire values, endpoints and silent calibration restore");
    }
    {
        CompanionProcessor p;p.hostingEnabled=true;p.prepareToPlay(48000,512);auto effects=p.rack.discover(juce::File(juce::String::fromUTF8(argv[3])));
        p.selectTrack(0);require(p.loadPlugin(1,effects[0]).isEmpty(),"synth independent FX1 loads");p.setPage(0);juce::AudioBuffer<float> fxAudio(2,512);juce::MidiBuffer fxMidi;fxMidi.addEvent(juce::MidiMessage::controllerEvent(1,72,83),0);p.processBlock(fxAudio,fxMidi);p.setPage(3);require(std::abs(p.controlValue(0)-83.f/127)<.00001f,"fixed insert CC reaches FX parameter while the oscillator page is visible");p.setControl(0,.65f);p.setFxWet(1,.25f);p.setFxBypass(1,true);
        p.selectTrack(1);require(!p.currentRack().instance(1)&&p.mixValues()[1]==1.f&&p.mixValues()[5]==0.f,"metallic has independent empty FX/mix");require(p.loadPlugin(2,effects[0]).isEmpty(),"metallic independent FX2 loads");
        p.selectTrack(2);require(!p.currentRack().instance(1)&&!p.currentRack().instance(2),"FM has its own inserts");
        require(!p.rack.instance(1)&&!p.rack.instance(2),"plugin host insert chain stays separate");
        auto state=p.state();CompanionProcessor restored;restored.hostingEnabled=true;restored.prepareToPlay(48000,512);require(restored.restore(state),"per-source FX state restores");
        restored.selectTrack(0);restored.setPage(3);require(std::abs(restored.controlValue(0)-.65f)<.0001f&&restored.mixValues()[1]==.25f&&restored.mixValues()[5]==1.f,"synth insert parameter/wet/bypass restored");
        restored.selectTrack(1);require(!restored.currentRack().instance(1)&&restored.currentRack().instance(2)!=nullptr,"metallic insert placement restored");
        {
            auto plugin=std::dynamic_pointer_cast<ClapProcessor>(p.synthRacks[0].instance(1));require(plugin!=nullptr,"CLAP fixture processor");
            juce::DocumentWindow owner("Test owner",juce::Colours::black,0);owner.setBounds(100,100,720,450);owner.setVisible(true);
            HostedEditorWindow floating(plugin,&owner);pump();
            auto* ptr=static_cast<const HWND*>(plugin->nativePlugin()->get_extension(plugin->nativePlugin(),"org.dtx.test.gui-window"));require(ptr&&IsWindow(*ptr),"native CLAP GUI created");
            auto parent=GetParent(*ptr);auto root=(HWND)floating.getPeer()->getNativeHandle();
            require(parent&&parent!=root&&GetParent(parent)==root,"CLAP editor gets dedicated native child container");
            double scale=floating.getPeer()->getPlatformScaleFactor();
            require(floating.getContentComponent()->getWidth()==juce::roundToInt(320/scale)&&floating.getContentComponent()->getHeight()==juce::roundToInt(200/scale),"small plugin fits floating client with no black surround");
            RECT client{},container{};GetClientRect(*ptr,&client);GetClientRect(parent,&container);require(std::abs(client.right-container.right)<=1&&std::abs(client.bottom-container.bottom)<=1,"native GUI and container fit in physical pixels at display DPI");
            plugin->nativeGui()->set_size(plugin->nativePlugin(),480,280);plugin->requestedWidth.store(480);plugin->requestedHeight.store(280);pump();require(floating.getContentComponent()->getWidth()==juce::roundToInt(480/scale)&&floating.getContentComponent()->getHeight()==juce::roundToInt(280/scale),"plugin resize request fits floating window");
        }
        auto editor=std::make_unique<PixelEditor>(p);p.selectTrack(0);p.setPage(3);editor->showPerformance();
        auto snapshot=[&](const juce::String& name){auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream out(juce::File(juce::String::fromUTF8(argv[4])).getChildFile(name));out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);};
        snapshot("synth-inserts.png");editor->showHome();snapshot("tracks-standalone.png");
        for(int page=0;page<4;++page){editor->showHardwarePage(page);snapshot("digitakt-"+juce::String(page)+".png");}
        p.selectTrack(3);p.selectTrack(7);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(15,40,77),117);p.processBlock(audio,midi);auto messages=midi;require(messages.getNumEvents()==1&&(*messages.begin()).samplePosition==117&&(*messages.begin()).getMessage().getControllerNumber()==82&&(*messages.begin()).getMessage().getControllerValue()==77,"track 7 hardware page encoder CC routes even when host source selected");
        std::puts("PASS: per-synth FX isolation/state, owned floating editors, native CLAP parenting/sizing and hardware pages");
    }
    {
        CompanionProcessor p;p.hostingEnabled=true;p.prepareToPlay(48000,512);auto effects=p.rack.discover(juce::File(juce::String::fromUTF8(argv[3])));
        for(int t=0;t<4;++t)for(int slot=1;slot<=2;++slot)require(p.loadPlugin(slot,effects[0],t).isEmpty(),"FX modulation fixture on every track/insert");
        p.selectTrack(0);p.setPage(3);require(p.controlText(0)=="0.500"&&p.modulationTargetName(0,40)=="FX1 GAIN","FX values use plugin function range and destination includes slot/name");
        // Each curve reaches either insert's displayed parameter on every sound track.
        for(int curve=0;curve<2;++curve)for(int target=0;target<4;++target)for(int slot=1;slot<=2;++slot)
        {
            p.modulation.enabled.store(false);p.modulation2.enabled.store(false);p.selectTrack(4+curve);auto& mod=p.currentModulation();auto shape=mod.snapshot();for(int i=0;i<shape.count;++i)mod.movePoint(i,shape.points[(size_t)i].x,0);
            for(int t=0;t<4;++t)mod.destination[(size_t)t].store(-1);mod.destination[(size_t)target].store(40+(slot-1)*4);mod.amount[(size_t)target].store(1);mod.depth.store(1);mod.bipolar.store(true);mod.enabled.store(true);
            require(render(p,target+1)<.000001,"either curve modulates FX1/FX2 gain on any sound track");require(render(p,(target+1)%4+1)>.001,"FX modulation preserves other sound tracks");
            require(std::abs(p.rackForTrack(target).instance(slot)->getParameters()[0]->getValue()-.25f)<.000001,"FX base knob restored after each audio buffer");
            auto saved=p.state();CompanionProcessor restored;restored.hostingEnabled=true;restored.prepareToPlay(48000,512);require(restored.restore(saved)&&restored.currentModulation().destination[(size_t)target].load()==40+(slot-1)*4,"FX modulation destinations and plugin base states restore");
            restored.modulation.enabled.store(false);restored.modulation2.enabled.store(false);require(render(restored,target+1)>.001,"saved plugin base survives enabled modulation and resumes after disable");
        }
        p.selectTrack(5);p.setPage(2);auto editor=std::make_unique<PixelEditor>(p);editor->showPerformance();auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::FileOutputStream out(juce::File(juce::String::fromUTF8(argv[4])).getChildFile("modulation-2-fx.png"));out.setPosition(0);out.truncate();juce::PNGImageFormat().writeImageToStream(image,out);
        std::puts("PASS: both MSEGs modulate either insert on all four tracks, audible isolation, base restoration and project round trip");
    }
    {
        PluginRack rack;rack.prepare(48000,512);auto effects=rack.discover(juce::File(juce::String::fromUTF8(argv[3])));require(effects.size()==1,"CLAP effect discovery");
        require(rack.load(1,effects[0]).isEmpty(),"CLAP effect loads in FX slot");
        juce::AudioBuffer<float> buffer(2,512);juce::MidiBuffer midi;std::array<float,8> mix{2.f/3.f,1,1,1,.5f,0,0,8.f/15.f};
        auto process=[&]{for(int c=0;c<2;++c)for(int i=0;i<512;++i)buffer.setSample(c,i,1.f);rack.process(buffer,midi,false,mix,nullptr);};
        process();require(std::abs(buffer.getSample(0,400)-.5f)<.0001f,"serial FX processes audio");
        mix[1]=0;process();require(std::abs(buffer.getSample(0,400)-1.f)<.0001f,"FX dry blend");
        mix[1]=1;rack.control(1,0,.75f);process();require(std::abs(buffer.getSample(0,400)-1.5f)<.0001f,"CLAP parameter events reach effect");
        auto state=rack.save();PluginRack restored;restored.prepare(48000,512);require(restored.restore(state).isEmpty(),"FX state restores");
        require(std::abs(restored.instance(1)->getParameters()[0]->getValue()-.75f)<.0001f,"effect parameter state retained");
        require(rack.load(2,effects[0]).isEmpty(),"second FX slot loads");process();require(std::abs(buffer.getSample(0,400)-.75f)<.0001f,"two serial effect slots");
        std::puts("PASS: CLAP serial effects, wet/dry mix, parameter events, two FX slots, state restore");
    }
    std::puts("PASS: synth automation/state and pixel UI snapshots");
}





