#include "SessionRecorder.h"
#include <cmath>

SessionRecorder::SessionRecorder(){worker.startThread();}
SessionRecorder::~SessionRecorder(){stop();worker.stopThread(5000);}
juce::Result SessionRecorder::start(const juce::File& parent,double rate,const RecorderRouting::Plan& plan,const RecorderDevice& device)
{
    stop();const juce::ScopedLock lock(control);if(!std::isfinite(rate)||rate<8000||rate>384000)return juce::Result::fail("Audio device is not ready");
    if(plan.lanes.size()>RecorderRouting::maximumLanes)return juce::Result::fail("Too many recorder lanes");
    auto result=parent.createDirectory();if(result.failed())return result;
    take=parent.getNonexistentChildFile("Take-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"),"",false);result=take.createDirectory();if(result.failed())return result;
    sampleRate=rate;captureRate.store(rate);recordedDevice=device;streams.clear();sourceManifest.clear();int channelCount=0;
    juce::WavAudioFormat format;
    auto add=[&](juce::String label,juce::String filename,int kind,int left,int right,bool stereo,bool available,const RecorderLane* lane)->bool
    {
        auto* info=new juce::DynamicObject();info->setProperty("name",label);info->setProperty("kind",kind==0?"device-input":kind==1?"companion-track":"companion-mix");info->setProperty("available",available);info->setProperty("channels",stereo?2:1);info->setProperty("file",available?filename:juce::String());if(lane){info->setProperty("assignment",lane->save());info->setProperty("physicalLeft",lane->physical(0,device)+1);info->setProperty("physicalRight",stereo?lane->physical(1,device)+1:0);info->setProperty("activeLeft",left+1);info->setProperty("activeRight",stereo?right+1:0);info->setProperty("armed",lane->armed);}sourceManifest.add(juce::var(info));
        if(!available)return true;auto output=take.getChildFile(filename).createOutputStream();if(!output)return false;auto* writer=format.createWriterFor(output.get(),rate,stereo?2:1,24,{},0);if(!writer)return false;output.release();Stream stream;stream.writer.reset(writer);stream.offset=channelCount;stream.kind=kind;stream.left=left;stream.right=right;stream.channels=stereo?2:1;channelCount+=stream.channels;streams.push_back(std::move(stream));return true;
    };
    bool ok=true;for(size_t i=0;i<plan.lanes.size();++i){const auto& lane=plan.lanes[i];int left=device.activeInput(lane.physical(0,device)),right=lane.stereo?device.activeInput(lane.physical(1,device)):-1;bool available=lane.armed&&left>=0&&(!lane.stereo||right>=0);auto name=juce::File::createLegalFileName(lane.name).trim().substring(0,64);if(name.isEmpty())name="Input";auto filename=juce::String((int)i+1).paddedLeft('0',2)+"-"+name+".wav";ok=add(lane.name,filename,0,left,right,lane.stereo,available,&lane)&&ok;}
    for(int i=0;i<4;++i)ok=add("Companion "+juce::String(i+1),"Companion-"+juce::String(i+1)+".wav",1,i,-1,true,true,nullptr)&&ok;
    if(plan.recordMix)ok=add("Companion mix","Companion-Mix.wav",2,0,1,true,true,nullptr)&&ok;
    if(!ok){streams.clear();failed.store(true);return juce::Result::fail("Cannot create all recording files");}
    ring.setSize(channelCount,capacity);ring.clear();audioFifo.reset();midiFifo.reset();for(auto& sequence:sequences)sequence.clear();ticks=lastTempo=0;captured.store(0);failed.store(false);pending.store(true);
    worker.addTimeSliceClient(this);active.store(true);return juce::Result::ok();
}
bool SessionRecorder::pushMidi(const MidiEvent& e)
{
    int a,n,b,m;midiFifo.prepareToWrite(1,a,n,b,m);if(n+m!=1)return false;midiEvents[(size_t)(n?a:b)]=e;midiFifo.finishedWrite(1);return true;
}
void SessionRecorder::capture(const juce::AudioBuffer<float>& device,int inputCount,const std::array<juce::AudioBuffer<float>,4>& companion,const juce::AudioBuffer<float>& mix,int frames,const std::array<juce::MidiBuffer,2>& curves,double bpm)
{
    if(!active.load())return;const juce::CriticalSection::ScopedTryLockType lock(captureLock);if(!lock.isLocked()||!active.load())return;
    if(frames<=0||frames>audioFifo.getFreeSpace()){failed.store(true);active.store(false);return;}
    int a,n,b,m;audioFifo.prepareToWrite(frames,a,n,b,m);
    for(const auto& stream:streams)for(int c=0;c<stream.channels;++c)
    {
        const juce::AudioBuffer<float>* source=stream.kind==0?&device:stream.kind==1?&companion[(size_t)stream.left]:&mix;int channel=stream.kind==0?(c==0?stream.left:stream.right):c;
        bool valid=channel>=0&&channel<source->getNumChannels()&&source->getNumSamples()>=frames&&(stream.kind!=0||channel<inputCount);if(valid){const auto* input=source->getReadPointer(channel);ring.copyFrom(stream.offset+c,a,input,n);if(m)ring.copyFrom(stream.offset+c,b,input+n,m);}else{ring.clear(stream.offset+c,a,n);if(m)ring.clear(stream.offset+c,b,m);failed.store(true);active.store(false);}
    }
    bpm=std::isfinite(bpm)?juce::jlimit(20.,300.,bpm):120.;bool ok=true;
    if(std::abs(bpm-lastTempo)>.01){ok=pushMidi({ticks,0,1,0,0,bpm});lastTempo=bpm;}
    for(int lane=0;lane<2;++lane)for(auto event:curves[(size_t)lane]){auto msg=event.getMessage();if(msg.isController())ok=pushMidi({ticks+juce::jlimit(0,frames-1,event.samplePosition)/sampleRate*bpm*16.,lane+1,msg.getChannel(),msg.getControllerNumber(),msg.getControllerValue(),0})&&ok;}
    ticks+=frames/sampleRate*bpm*16.;captured.fetch_add(frames);audioFifo.finishedWrite(frames);if(!ok){failed.store(true);active.store(false);}
}
void SessionRecorder::drainMidi()
{
    int a,n,b,m;midiFifo.prepareToRead(midiFifo.getNumReady(),a,n,b,m);
    auto consume=[&](int start,int count){for(int i=0;i<count;++i){const auto& e=midiEvents[(size_t)(start+i)];auto message=e.tempo>0?juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000./e.tempo)):juce::MidiMessage::controllerEvent(e.channel,e.cc,e.value);message.setTimeStamp(e.tick);sequences[(size_t)e.curve].addEvent(message);}};
    consume(a,n);consume(b,m);midiFifo.finishedRead(n+m);
}
int SessionRecorder::useTimeSlice()
{
    drainMidi();int a,n,b,m;audioFifo.prepareToRead(juce::jmin(4096,audioFifo.getNumReady()),a,n,b,m);
    auto write=[&](int start,int count){if(count<=0)return;for(auto& stream:streams){const float* data[2]={ring.getReadPointer(stream.offset,start),ring.getReadPointer(stream.offset+stream.channels-1,start)};if(!stream.writer->writeFromFloatArrays(data,stream.channels,count)){failed.store(true);active.store(false);}}};
    write(a,n);write(b,m);audioFifo.finishedRead(n+m);return audioFifo.getNumReady()>0?0:2;
}
void SessionRecorder::stop()
{
    active.store(false);const juce::ScopedLock lock(control);{const juce::ScopedLock wait(captureLock);}worker.removeTimeSliceClient(this);if(!pending.exchange(false))return;
    while(audioFifo.getNumReady()>0)useTimeSlice();drainMidi();finish();
}
void SessionRecorder::finish()
{
    streams.clear();
    juce::MidiFile midi;midi.setTicksPerQuarterNote(960);for(int i=0;i<3;++i){auto name=juce::MidiMessage::textMetaEvent(3,i==0?"Tempo":i==1?"Modulation Curve 1":"Modulation Curve 2");sequences[(size_t)i].addEvent(name);auto end=juce::MidiMessage::endOfTrack();end.setTimeStamp(ticks);sequences[(size_t)i].addEvent(end);midi.addTrack(sequences[(size_t)i]);}
    auto output=take.getChildFile("Modulation.mid").createOutputStream();if(!output||!midi.writeTo(*output))failed.store(true);output.reset();
    auto* manifest=new juce::DynamicObject();manifest->setProperty("sampleRate",sampleRate);manifest->setProperty("samples",(juce::int64)captured.load());manifest->setProperty("complete",!failed.load());manifest->setProperty("format","24-bit PCM WAV / two CC lanes in 960 PPQ MIDI");
    manifest->setProperty("device",recordedDevice.save());manifest->setProperty("lanes",sourceManifest);
    manifest->setProperty("companionStems","Post effects, track level and alignment; before master. Optional Companion mix is post master. Device lanes capture assigned input streams directly.");
    if(!take.getChildFile("Recording.json").replaceWithText(juce::JSON::toString(juce::var(manifest))))failed.store(true);
}
