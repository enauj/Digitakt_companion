#include "PixelEditor.h"
#include "PixelFont.h"
#include "HostedEditorWindow.h"
#include "TransportSettings.h"
#include "RecorderSettings.h"

namespace
{
const juce::Colour yellow(0xffffdf18),dim(0xff7b6a0b);
juce::File commonPlugins(const juce::String& format){return juce::File(juce::SystemStats::getEnvironmentVariable("CommonProgramFiles","C:/Program Files/Common Files")).getChildFile(format);}
juce::File userPlugins(const juce::String& format){return juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName())).getChildFile("Programs/Common").getChildFile(format);}
const char* synthPages[]={"OSC","FILT","AMP","FX","VOICE"};
const char* hostPages[]={"CHAIN","PARAM","MIX"};
const char* hardwarePages[]={"DELAY","REVERB","DLY SEND","REV SEND"};
const char* modPages[]={"CURVE","SETUP","ASSIGN 1","ASSIGN 2","LFO 1","LFO 2","RATE","MIDI"};
const char* mixerPages[]={"COMP MIX","DT MIX","REC"};
const char* modTabs[]={"CURVE","SETUP","ASGN1","ASGN2","LFO1","LFO2","RATE","MIDI"};
const char* compactLabels[5][8]={
    {"SHAPE 1","SHAPE 2","TUNE 2","OSC MIX","FM AMT","FOLD","DETUNE","SYNC"},
    {"TUNE","MODEL","FOLD","SWEEP","METAL","FEEDBACK","COLOR","TRANSIENT"},
    {"ALGO","RATIO A","RATIO B","FM INDEX","DECAY","FEEDBACK","SHIFT","BRIGHT"},
    {"CUTOFF","RESON","LP-BP-HP","ENV AMT","ATTACK","DECAY","SUSTAIN","RELEASE"},
    {"ATTACK","DECAY","SUSTAIN","RELEASE","LEVEL","PAN","VELO","DRIVE"}};
class MidiSettings final:public juce::Component
{
public:
    explicit MidiSettings(CompanionProcessor& p):processor(p)
    {
        message.setText("FIXED CCs: parameter locks keep their meaning on every page.\nSynth OSC 16-23 / Metal OSC 24-31 / FM OSC 40-47\nFilter 48-55 / Amp 56-63 / FX 72-79\nCurve editors have separate channels and CC banks.\nRight click tracks 1-8 to change their input channels.",juce::dontSendNotification);addAndMakeVisible(message);
        for(int t=0;t<8;++t){labels[(size_t)t].setText("Track "+juce::String(t+1)+" MIDI CH",juce::dontSendNotification);addAndMakeVisible(labels[(size_t)t]);auto& box=values[(size_t)t];box.setSliderStyle(juce::Slider::IncDecButtons);box.setRange(1,16,1);box.setTextBoxStyle(juce::Slider::TextBoxLeft,false,50,28);int ch=t<4?p.channel(t):t==4?p.modulation.channel.load():t==5?p.modulation2.channel.load():t==6?p.hardware.inputChannel.load():p.mixerChannel.load();box.setValue(ch);box.onValueChange=[this,t]{processor.setControlChannel(t,(int)values[(size_t)t].getValue());};addAndMakeVisible(box);}setSize(620,380);
    }
    void resized() override{message.setBounds(20,15,580,125);for(int t=0;t<8;++t){int x=20+(t%2)*300,y=160+(t/2)*48;labels[(size_t)t].setBounds(x,y,130,28);values[(size_t)t].setBounds(x+135,y,140,28);}}
private:CompanionProcessor& processor;juce::Label message;std::array<juce::Label,8> labels;std::array<juce::Slider,8> values;
};
void knob(juce::Graphics& g,int cx,int cy,float value,bool enabled,const juce::String& name,const juce::String& text,int radius=23)
{
    g.setColour(enabled?yellow:dim);pixels::circle(g,cx,cy,radius);double angle=-2.35619449+4.71238898*value;pixels::line(g,cx,cy,cx+juce::roundToInt(std::sin(angle)*(radius-2)),cy-juce::roundToInt(std::cos(angle)*(radius-2)));
    pixels::centred(g,name,cx,cy+radius+10,name.length()<=9?2:1,18);pixels::centred(g,text,cx,cy+radius+26,text.length()<=9?2:1,18);
}
}
PixelEditor::PixelEditor(CompanionProcessor& p):AudioProcessorEditor(p),processor(p)
{
    setOpaque(true);setWantsKeyboardFocus(true);setResizable(true,false);setResizeLimits(720,450,1920,1200);if(auto* c=getConstrainer())c->setFixedAspectRatio(1.6);setSize(960,600);startTimerHz(30);
}
PixelEditor::~PixelEditor(){stopTimer();if(noteHeld)processor.keyboard.noteOff(noteChannel,60,0.f);closeHostedEditors();if(utilityWindow)delete utilityWindow.getComponent();if(transportWindow)delete transportWindow.getComponent();}
void PixelEditor::closeHostedEditors(){pluginWindows.clear();}
void PixelEditor::resized(){float s=juce::jmin(getWidth()/480.f,getHeight()/300.f);viewport={(getWidth()-480*s)*.5f,(getHeight()-300*s)*.5f,480*s,300*s};}
juce::Point<int> PixelEditor::logical(juce::Point<float> p) const{return {juce::roundToInt((p.x-viewport.getX())*480/viewport.getWidth()),juce::roundToInt((p.y-viewport.getY())*300/viewport.getHeight())};}
int PixelEditor::tabCount() const{return home?4:processor.pageCount(processor.track());}
juce::PopupMenu::Options PixelEditor::menuOptions(){return juce::PopupMenu::Options().withTargetComponent(this).withParentComponent(this).withMousePosition();}
void PixelEditor::paint(juce::Graphics& g){{juce::Graphics c(screen);drawScreen(c);}g.fillAll(juce::Colours::black);g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.drawImage(screen,viewport);}
void PixelEditor::drawAddress(juce::Graphics& g,int slot,int x,int y)
{
    int code=processor.controlCC(slot);if(code<0)return;g.setColour(dim);pixels::centred(g,juce::String::charToString((juce::juce_wchar)('A'+slot))+" CC"+juce::String(code)+" CH"+juce::String(processor.controlChannel()),x,y,1);g.setColour(yellow);
}
void PixelEditor::drawCurve(juce::Graphics& g)
{
    auto shape=processor.currentModulation().snapshot();auto effective=processor.currentModulation().effectiveSnapshot();g.setColour(dim);g.drawRect(25,49,431,104);
    for(int x=0;x<=8;++x)g.drawVerticalLine(25+x*430/8,50,152);for(int y=0;y<=4;++y)g.drawHorizontalLine(50+y*101/4,25,455);
    int vertex=shape.points[(size_t)juce::jlimit(0,shape.count-1,processor.currentModulation().selectedPoint.load())].id;pixels::centred(g,"V"+juce::String(vertex+1)+" / X CC"+juce::String(companion::midi::vertexCC(vertex,0))+" / Y CC"+juce::String(companion::midi::vertexCC(vertex,1))+" / BEND CC"+juce::String(companion::midi::vertexCC(vertex,2))+" / CH"+juce::String(processor.controlChannel()),240,40,1);
    g.setColour(dim);int lastY=151-juce::roundToInt(ModulationCurve::sample(shape,0)*100);
    for(int x=1;x<=430;++x){int y=151-juce::roundToInt(ModulationCurve::sample(shape,x/430.f)*100);pixels::line(g,24+x,lastY,25+x,y);lastY=y;}
    g.setColour(yellow);lastY=151-juce::roundToInt(ModulationCurve::sample(effective,0)*100);for(int x=1;x<=430;++x){int y=151-juce::roundToInt(ModulationCurve::sample(effective,x/430.f)*100);pixels::line(g,24+x,lastY,25+x,y);lastY=y;}
    for(int i=0;i<effective.count;++i){auto p=effective.points[(size_t)i];g.fillRect(25+juce::roundToInt(p.x*430)-1,151-juce::roundToInt(p.y*100)-1,3,3);}
    for(int i=0;i<shape.count;++i){auto p=shape.points[(size_t)i];int x=25+juce::roundToInt(p.x*430),y=151-juce::roundToInt(p.y*100);g.setColour(i==processor.currentModulation().selectedPoint.load()?juce::Colours::white:yellow);g.fillRect(x-3,y-3,7,7);g.setColour(juce::Colours::black);g.fillRect(x-1,y-1,3,3);g.setColour(yellow);pixels::text(g,juce::String(p.id+1),x<430?x+6:x-12,y>66?y-11:y+8,1);}
    g.setColour(yellow);if(processor.currentModulation().enabled.load())g.drawVerticalLine(25+juce::roundToInt(processor.currentModulation().phase.load()*430),50,152);
    g.setColour(dim);pixels::centred(g,"MAX 8 / DRAG / DOUBLE CLICK ADD / RIGHT CLICK REMOVE",240,160,1);g.setColour(yellow);
    for(int i=0;i<4;++i){knob(g,65+i*117,187,processor.currentModulation().controlValue(0,i),true,processor.currentModulation().controlName(0,i),processor.currentModulation().controlText(0,i),17);drawAddress(g,i,65+i*117,247);}
    g.setColour(dim);pixels::centred(g,(processor.currentModulation().enabled.load()?"MOD ON":"MOD OFF")+juce::String(" / CLICK HERE TO TOGGLE / R: RESTART"),240,259,1);g.setColour(yellow);
}
void PixelEditor::drawScreen(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);g.setColour(yellow);g.drawRect(5,5,470,290,1);int track=processor.track(),page=processor.page.load();bool plugin=track<4&&processor.isPluginTrack();
    if(home)
    {
        g.drawRect(7,7,293,31,1);g.drawRect(300,7,78,31,1);g.drawRect(378,7,95,31,1);pixels::text(g,"DTX COMPANION",16,16,2);pixels::centred(g,juce::String(processor.hostingEnabled?processor.transport.displayBpm.load():processor.bpm.load(),1),339,16,2);
        if(processor.playing.load()){for(int y=0;y<13;++y)g.fillRect(389,16+y,juce::jmax(1,7-std::abs(6-y)),1);}else{g.fillRect(388,17,4,11);g.fillRect(395,17,4,11);}int beat=(int)std::floor(juce::jmax(0.,processor.beats.load()));pixels::text(g,juce::String(beat/4+1)+":"+juce::String(beat%4+1),411,16,2);
        for(int t=0;t<8;++t)
        {
            auto box=trackBounds(t);int x=box.getX(),y=box.getY();bool selected=track==t;
            if(selected)g.fillRect(box);else g.drawRect(box,1);
            auto ink=selected?juce::Colours::black:yellow;g.setColour(ink);g.drawRect(x+8,y+7,24,24,1);pixels::centred(g,juce::String(t+1),x+20,y+12,2);
            pixels::centred(g,processor.trackName(t),x+52,y+40,1,16);
            if(t<4){pixels::text(g,"...",x+79,y+16,1);pixels::centred(g,"MIDI CH "+juce::String(processor.channel(t)),x+52,y+56,1);for(int s=0;s<8;++s){g.setColour(processor.trackPeaks[(size_t)t].load()>s*.085f?ink:selected?yellow:dim);g.fillRect(x+13+s*10,y+76,8,5);}}
            else{pixels::centred(g,t<6?"MSEG / ASSIGN":t==6?"DIGITAKT I":"MIX / RECORD",x+52,y+56,1);if(t<6){auto& mod=t==4?processor.modulation:processor.modulation2;pixels::centred(g,mod.enabled.load()?"ACTIVE":"BYPASSED",x+52,y+76,1);}}
            g.setColour(yellow);
        }
        g.setColour(dim);pixels::centred(g,"RIGHT CLICK: TRACK SETUP / CLICK BPM: TRANSPORT",240,260,1);g.setColour(yellow);
    }
    else
    {
        int tabs=tabCount();page=juce::jlimit(0,tabs-1,page);g.fillRect(7,7,178,29);g.drawRect(185,7,222,29,1);g.drawRect(407,7,66,29,1);
        auto title=juce::String(track+1)+":"+(processor.isModTrack()?juce::String("MOD ")+juce::String(track-3):track==6?juce::String("DT FX"):track==7?juce::String("MIX / REC"):plugin?juce::String("HOST"):processor.engine()==2?juce::String("FM"):processor.engine()==1?juce::String("METAL"):juce::String("SYNTH"));
        g.setColour(juce::Colours::black);pixels::text(g,title,15,15,2);g.setColour(yellow);auto pageName=processor.isModTrack()?modPages[page]:track==6?hardwarePages[page]:track==7?mixerPages[page]:plugin?hostPages[page]:processor.isVoicePage()?"VOICE":synthPages[page];pixels::centred(g,pageName,296,15,2);pixels::centred(g,juce::String(page+1)+"/"+juce::String(tabs),441,15,2);
        if(track==7&&page==2)
        {
            const char* buttons[]={processor.recorder.isRecording()?"STOP / SAVE":"RECORD","FOLDER","INPUTS"};for(int i=0;i<3;++i){g.drawRect(15+i*155,59,143,32);pixels::centred(g,buttons[i],86+i*155,70,1);}
            pixels::centred(g,processor.recorder.isRecording()?"RECORDING":processor.recorder.hasError()?"RECORDING ERROR / CHECK TAKE":"READY",240,113,2);
            double elapsed=processor.recorder.seconds();pixels::centred(g,juce::String(elapsed,1)+" SEC / 24 BIT WAV + MIDI",240,143,1);
            auto plan=processor.recorderRouting.snapshot();auto device=processor.recorderDevice();int ready=0;for(const auto& lane:plan.lanes)if(lane.armed&&device.activeInput(lane.physical(0,device))>=0&&(!lane.stereo||device.activeInput(lane.physical(1,device))>=0))++ready;
            pixels::centred(g,"DEVICE LANES: "+juce::String(ready)+"/"+juce::String((int)plan.lanes.size())+" / MIX: "+juce::String(plan.recordMix?"YES":"NO"),240,169,1);
            pixels::centred(g,"COMPANION 1-4 / CURVE CC LANES 1+2",240,190,1);g.setColour(dim);pixels::centred(g,recordingFolder.getFullPathName().toUpperCase(),240,216,1,70);pixels::centred(g,"SEPARATE FILES / SAME START / MISSING INPUTS OMITTED",240,242,1);g.setColour(yellow);
        }
        else if(processor.isModTrack()&&page==0)drawCurve(g);
        else if(plugin&&page==0)
        {
            const char* roles[]={"MIDI","SOURCE","FX 1","FX 2"};for(int i=0;i<4;++i){int x=18+i*116;g.drawRect(x,99,96,106,1);pixels::centred(g,juce::String(i+1),x+48,111,2);pixels::centred(g,roles[i],x+48,139,1);pixels::centred(g,i==0?"CH "+juce::String(processor.channel()):processor.currentRack().name(i-1).toUpperCase(),x+48,160,1,14);pixels::centred(g,i==0?"SETUP":"CLICK TO EDIT",x+48,186,1,14);if(i<3)pixels::text(g,">",x+101,147,1);}g.setColour(dim);pixels::centred(g,"SOURCE > FX 1 > FX 2",240,237,1);g.setColour(yellow);
        }
        else
        {
            if(track<4&&!plugin&&page==3&&!processor.isVoicePage())for(int s=1;s<=2;++s){int y=39+(s-1)*112;g.drawRect(8,y,464,14);pixels::text(g,("FX"+juce::String(s)+": "+processor.currentRack().name(s).toUpperCase()+" / CLICK TO EDIT").substring(0,74),12,y+3,1);}
            for(int i=0;i<8;++i)
            {
                if(processor.isVoicePage()&&i>=2)continue;if(processor.isModTrack()&&((page==1&&i>=4)||(page==6&&i>=6)||(page==7&&i>=5)))continue;if(track==7&&page==0&&i>=6)continue;
                float value=track==6?processor.hardware.normalised(page,i):processor.controlValue(i);bool enabled=track==6?processor.hostingEnabled:processor.controlEnabled(i);auto name=track==6?juce::String(HardwareMidi::label(page,i)):processor.controlName(i);
                if(track<4&&!plugin&&page<3)name=compactLabels[page==0?processor.engine():page+2][i];juce::String text=enabled?(track==6?juce::String(processor.hardware.value(page,i)):processor.controlText(i)):"--";
                if(track==6&&!processor.hardware.isKnown(page,i))text="--";
                knob(g,65+(i%4)*117,78+(i/4)*112,value,enabled,name,text);
                drawAddress(g,i,65+(i%4)*117,144+(i/4)*112);
            }
            if(processor.isVoicePage()){g.setColour(dim);pixels::centred(g,"MONO LEGATO: OVERLAP NOTES TO GLIDE",240,188,1);pixels::centred(g,"LAST NOTE PRIORITY / HELD NOTES RETURN ON RELEASE",240,211,1);pixels::centred(g,"POLY IS THE DEFAULT / GLIDE TIME 0-2 SECONDS",240,234,1);g.setColour(yellow);}
            if(track==6){g.setColour(dim);pixels::centred(g,!processor.hostingEnabled?"HARDWARE MIDI: STANDALONE APP":processor.hardware.outputReady.load()?"OUTPUT READY / CLICK FOR SETUP":"CLICK HERE TO SELECT MIDI OUTPUT",240,42,1);g.setColour(yellow);}
            if(track==7){g.setColour(dim);pixels::centred(g,page==0?"DEVICE RETURN MONITOR: "+juce::String(processor.monitorEnabled.load()?"ON":"OFF")+" / UTILITY TO CHANGE":"LEVELS SEND CC95 ON THE DT TRACK CHANNELS",240,42,1);g.setColour(yellow);}
            if(processor.isModTrack()&&page==6){g.setColour(dim);pixels::centred(g,"MULTIPLIER = SPEED X",347,180,1);pixels::centred(g,"SYNC: BEATS / MULT",347,200,1);pixels::centred(g,"RIGHT CLICK RATE: SYNC",347,220,1);pixels::centred(g,"CLOCK "+juce::String(processor.bpm.load(),1)+" BPM",347,240,1);g.setColour(yellow);}
            if(processor.isModTrack()&&page==7){g.setColour(dim);pixels::centred(g,"50 HZ / VALUE CHANGES",325,186,1);pixels::centred(g,"RECORDER SAVES BOTH LANES",325,207,1);pixels::centred(g,"LIVE OUTPUT IS OPTIONAL",325,228,1);g.setColour(yellow);}
            if(processor.isModTrack()&&page==1){g.setColour(dim);pixels::centred(g,"CURVE: BEND / X / Y",240,185,1);pixels::centred(g,"ASSIGN 1 + 2: EIGHT FREE ROUTES",240,207,1);pixels::centred(g,"RATE PAGE: CURVE + TWO LFO MULTIPLIERS",240,229,1);g.setColour(yellow);}
        }
    }
    if(toastTicks>0){g.setColour(juce::Colours::black);g.fillRect(12,252,456,18);g.setColour(yellow);pixels::centred(g,toast,240,258,1,72);}
    const char* homeTabs[]={"TRK","LOAD","SAVE","UTILITY"};int tabs=tabCount();for(int i=0;i<tabs;++i){int x=7+i*466/tabs,w=7+(i+1)*466/tabs-x;bool selected=home?i==0:i==juce::jmin(tabs-1,page);g.setColour(yellow);if(selected)g.fillRect(x,271,w,22);else g.drawRect(x,271,w,22,1);g.setColour(selected?juce::Colours::black:yellow);pixels::centred(g,home?homeTabs[i]:processor.isModTrack()?modTabs[i]:track==6?hardwarePages[i]:track==7?mixerPages[i]:plugin?hostPages[i]:processor.isVoicePage(processor.track(),i)?"VOICE":synthPages[i],x+w/2,275,tabs>6?1:2);g.setColour(yellow);}
}
int PixelEditor::pointAt(juce::Point<int> p) const
{
    auto s=processor.currentModulation().snapshot();for(int i=0;i<s.count;++i){int x=25+juce::roundToInt(s.points[(size_t)i].x*430),y=151-juce::roundToInt(s.points[(size_t)i].y*100);if(p.getDistanceSquaredFrom({x,y})<=81)return i;}return -1;
}
int PixelEditor::knobAt(juce::Point<int> p) const
{
    if(home||(processor.track()==7&&processor.page.load()==2))return -1;
    if(processor.isModTrack()&&processor.page.load()==0)return p.y>=171&&p.y<=254?juce::jlimit(0,3,(p.x-7)*4/466):-1;
    if((processor.track()<4&&processor.isPluginTrack()&&processor.page.load()==0)||p.y<54||p.y>253)return -1;
    return juce::jlimit(0,3,(p.x-7)*4/466)+(p.y>=155?4:0);
}
void PixelEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();auto p=logical(e.position);int track=processor.track(),page=processor.page.load();
    if(p.y>=271&&p.y<=294){int tab=juce::jlimit(0,tabCount()-1,(p.x-7)*tabCount()/466);if(home){if(tab==1)choosePatch(false);if(tab==2)choosePatch(true);if(tab==3)utility();}else processor.setPage(tab);repaint();return;}
    if(home&&p.y<38&&p.x>=300){transportSettings();return;}
    if(!home&&p.y<38&&p.x<185){if(e.mods.isPopupMenu()&&track<8)trackMenu(track);else showHome();return;}
    if(home)
    {
        for(int t=0;t<8;++t)if(trackBounds(t).contains(p)){if(t<8&&(e.mods.isPopupMenu()||(t<4&&p.x>=trackBounds(t).getRight()-30&&p.y<trackBounds(t).getY()+34))){trackMenu(t);return;}if(noteHeld){processor.keyboard.noteOff(noteChannel,60,0.f);noteHeld=false;}processor.selectTrack(t);showPerformance();return;}return;
    }
    if(track==7&&page==2){if(p.y>=59&&p.y<=91){int button=juce::jlimit(0,2,(p.x-15)/155);if(button==0){if(processor.recorder.isRecording()||processor.recorder.pendingFinalization())processor.recorder.stop();else{auto result=processor.startRecording(recordingFolder);if(result.failed()){toast=result.getErrorMessage().toUpperCase();toastTicks=120;}}}else if(button==1)chooseRecordingFolder();else recorderSettings();repaint();}return;}
    if(track==6&&p.y>=38&&p.y<53){utility();return;}
    if(processor.isModTrack()&&page==0&&p.y>=255&&p.y<270){auto& mod=processor.currentModulation();mod.enabled.store(!mod.enabled.load());repaint();return;}
    if(processor.isModTrack()&&page==0&&p.y>=43&&p.y<=157)
    {
        int point=pointAt(p);if(point>=0){processor.currentModulation().selectedPoint.store(point);if(e.mods.isPopupMenu())processor.currentModulation().removePoint(point);else dragPoint=point;}repaint();return;
    }
    if(track<4&&!processor.isPluginTrack()&&page==3&&!processor.isVoicePage()){if(p.y>=39&&p.y<54){slotMenu(1);return;}if(p.y>=151&&p.y<166){slotMenu(2);return;}}
    if(track<4&&processor.isPluginTrack()&&page==0){if(p.y>=99&&p.y<=205){int node=juce::jlimit(0,3,(p.x-18)/116);if(node==0)trackMenu(track);else slotMenu(node-1);}return;}
    int slot=knobAt(p);if(slot<0)return;
    if(processor.isModTrack()&&page==6&&slot<6&&slot%2==0&&e.mods.isPopupMenu()){auto& mod=processor.currentModulation();int targetPage=slot==0?1:3+slot/2,targetSlot=slot==0?2:5;mod.setControl(targetPage,targetSlot,mod.controlValue(targetPage,targetSlot)>.5f?0.f:1.f);repaint();return;}
    if(processor.isModTrack()&&(page==2||page==3)&&slot<4){modAssignMenu((page-2)*4+slot);return;}
    if(processor.isModTrack()&&(page==4||page==5)&&slot==3){lfoAssignMenu(page-4);return;}
    if(processor.isVoicePage()&&slot==0){processor.setControl(0,processor.controlValue(0)>.5f?0.f:1.f);repaint();return;}
    if(processor.isModTrack()&&((page==1&&slot>=1&&slot<=3)||((page==4||page==5)&&(slot==5||slot==7))||(page==7&&slot==0))){processor.setControl(slot,processor.controlValue(slot)>.5f?0.f:1.f);repaint();return;}
    if(track<4&&e.mods.isPopupMenu()&&processor.isPluginTrack()&&page==1){assignMenu(slot);return;}
    if(track<4&&page==3&&processor.hostingEnabled&&(e.mods.isPopupMenu()||!processor.controlEnabled(slot))){slotMenu(1+slot/4);return;}
    if(track==6&&!processor.hostingEnabled)return;
    if(track==6||processor.controlEnabled(slot)){dragSlot=slot;dragOrigin=p;dragValue=track==6?processor.hardware.normalised(page,slot):processor.controlValue(slot);if(track<4)processor.beginGesture(slot);}
}
void PixelEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto p=logical(e.position);if(dragPoint>=0){processor.currentModulation().movePoint(dragPoint,(p.x-25)/430.f,(151-p.y)/100.f);repaint();return;}if(dragSlot<0)return;float v=dragValue+(dragOrigin.y-p.y)/(e.mods.isShiftDown()?900.f:130.f);if(processor.track()==6)processor.hardware.edit(processor.page.load(),dragSlot,v);else processor.setControl(dragSlot,v);repaint();
}
void PixelEditor::mouseUp(const juce::MouseEvent&){if(dragSlot>=0&&processor.track()<4)processor.endGesture(dragSlot);dragSlot=-1;dragPoint=-1;}
void PixelEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    auto p=logical(e.position);if(!home&&processor.isModTrack()&&processor.page.load()==0&&p.y>=49&&p.y<=152){if(pointAt(p)<0&&processor.currentModulation().addPoint((p.x-25)/430.f,(151-p.y)/100.f)<0){toast=processor.currentModulation().snapshot().count>=8?"MAXIMUM 8 VERTICES":"POINT TOO CLOSE TO NEIGHBOUR";toastTicks=60;}repaint();return;}
    int slot=knobAt(p);if(processor.isModTrack()&&((slot>=4&&(processor.page.load()==2||processor.page.load()==3))||(slot==1&&(processor.page.load()==4||processor.page.load()==5)))){processor.setControl(slot,.5f);repaint();return;}
    if(slot>=0&&processor.track()<4)if(auto* param=processor.controlParameter(slot)){param->beginChangeGesture();if(!processor.isPluginTrack()&&processor.page.load()<3)param->setValueNotifyingHost(param->getDefaultValue());else processor.setControl(slot,param->getDefaultValue());param->endChangeGesture();}
}
bool PixelEditor::keyPressed(const juce::KeyPress& key)
{
    if(key.getTextCharacter()=='t'||key.getTextCharacter()=='T'){transportSettings();return true;}
    if(key==juce::KeyPress::escapeKey){showHome();return true;}if(key.getTextCharacter()=='r'||key.getTextCharacter()=='R'){processor.currentModulation().restart();return true;}
    if(key.getKeyCode()==juce::KeyPress::spaceKey){if(!noteHeld){noteChannel=processor.channel();processor.keyboard.noteOn(noteChannel,60,.65f);noteHeld=true;}return true;}if(key.getTextCharacter()=='!'){processor.reset();return true;}return false;
}
bool PixelEditor::keyStateChanged(bool){if(noteHeld&&!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)){processor.keyboard.noteOff(noteChannel,60,0.f);noteHeld=false;return true;}return false;}
void PixelEditor::timerCallback(){if(!processor.recorder.isRecording()&&processor.recorder.pendingFinalization())processor.recorder.stop();if(toastTicks>0)--toastTicks;repaint();}
void PixelEditor::trackMenu(int t)
{
    juce::PopupMenu m;m.addSectionHeader("TRACK "+juce::String(t+1));
    if(t<4){m.addItem(1,"Synth",true,processor.engine(t)==0);m.addItem(2,"Metallic",true,processor.engine(t)==1);m.addItem(3,"Four-op FM",true,processor.engine(t)==2);if(processor.hostingEnabled)m.addItem(4,"VST3 / CLAP instrument",true,processor.engine(t)==3);}
    int ch=t<4?processor.channel(t):t==4?processor.modulation.channel.load():t==5?processor.modulation2.channel.load():t==6?processor.hardware.inputChannel.load():processor.mixerChannel.load();
    juce::PopupMenu channels;for(int c=1;c<=16;++c)channels.addItem(100+c,juce::String(c),true,ch==c);m.addSubMenu("MIDI input channel",channels);
    m.showMenuAsync(menuOptions(),[safe=juce::Component::SafePointer<PixelEditor>(this),t](int action){if(!safe)return;if(t<4&&action>=1&&action<=4)safe->processor.setTrackEngine(t,action-1);if(action>=101&&action<=116)safe->processor.setControlChannel(t,action-100);safe->repaint();});
}
void PixelEditor::modAssignMenu(int route)
{
    auto& mod=processor.currentModulation();juce::PopupMenu m;m.addSectionHeader("ROUTE "+juce::String(route+1)+" / TARGET TRACK AND PARAMETER");m.addItem(1,"Off",true,mod.destination[(size_t)route].load()<0);
    for(int t=0;t<4;++t)
    {
        juce::PopupMenu trackMenu;int engine=processor.engine(t);
        if(engine<3)for(int page=0;page<3;++page){juce::PopupMenu group;for(int i=0;i<8;++i){int index=companion::parameterIndex(engine,page,i);group.addItem(100+t*100+index,companion::names[(size_t)index],true,mod.targetTrack[(size_t)route].load()==t&&mod.destination[(size_t)route].load()==index);}trackMenu.addSubMenu(synthPages[page],group);}
        for(int slot=1;slot<=2;++slot)
        {
            juce::PopupMenu group;auto plugin=processor.rackForTrack(t).instance(slot);
            for(int i=0;i<4;++i){int index=40+(slot-1)*4+i;auto* param=plugin&&i<plugin->getParameters().size()?plugin->getParameters()[i]:nullptr;group.addItem(100+t*100+index,param?param->getName(60):"P"+juce::String(i+1)+" (empty)",param&&param->isAutomatable(),mod.targetTrack[(size_t)route].load()==t&&mod.destination[(size_t)route].load()==index);}
            trackMenu.addSubMenu("FX "+juce::String(slot)+": "+processor.rackForTrack(t).name(slot),group,processor.hostingEnabled&&plugin!=nullptr);
        }
        m.addSubMenu("T"+juce::String(t+1)+" "+processor.trackName(t),trackMenu);
    }
    m.showMenuAsync(menuOptions(),[safe=juce::Component::SafePointer<PixelEditor>(this),route,modTrack=processor.track()](int id){if(!safe)return;auto& mod=modTrack==5?safe->processor.modulation2:safe->processor.modulation;if(id==1)mod.destination[(size_t)route].store(-1);if(id>=100){int encoded=id-100,t=encoded/100,d=encoded%100;if(t<4&&d<48){mod.targetTrack[(size_t)route].store(t);mod.destination[(size_t)route].store(d);}}safe->repaint();});
}
void PixelEditor::lfoAssignMenu(int lfo)
{
    auto& mod=processor.currentModulation();auto shape=mod.snapshot();juce::PopupMenu menu;menu.addSectionHeader("LFO "+juce::String(lfo+1)+" / VERTEX PARAMETER");menu.addItem(1,"Off",true,mod.lfos[(size_t)lfo].target.load()<0);
    for(int i=0;i<shape.count;++i){int id=shape.points[(size_t)i].id;juce::PopupMenu vertex;const char* labels[]={"X (time)","Y (level)","Bend / Z (curve)"};for(int d=0;d<3;++d)vertex.addItem(10+id*3+d,labels[d],d!=0||(i>0&&i<shape.count-1),mod.lfos[(size_t)lfo].target.load()==id*3+d);menu.addSubMenu("VERTEX "+juce::String(id+1),vertex);}
    menu.showMenuAsync(menuOptions(),[safe=juce::Component::SafePointer<PixelEditor>(this),lfo,track=processor.track()](int action){if(!safe)return;auto& mod=track==5?safe->processor.modulation2:safe->processor.modulation;if(action==1)mod.lfos[(size_t)lfo].target.store(-1);if(action>=10&&action<34)mod.lfos[(size_t)lfo].target.store(action-10);safe->repaint();});
}
void PixelEditor::transportSettings()
{
    if(transportWindow){transportWindow->toFront(true);return;}
    juce::DialogWindow::LaunchOptions o;o.content.setOwned(new TransportSettings(processor));o.dialogTitle="Companion tempo / transport";o.dialogBackgroundColour=juce::Colours::black;o.useNativeTitleBar=true;o.escapeKeyTriggersCloseButton=true;o.componentToCentreAround=this;transportWindow=o.launchAsync();
}
void PixelEditor::recorderSettings()
{
    if(onRecorderSettings){onRecorderSettings();return;}if(utilityWindow){utilityWindow->toFront(true);return;}juce::DialogWindow::LaunchOptions o;o.content.setOwned(new RecorderSettings(processor));o.dialogTitle="Recorder inputs";o.dialogBackgroundColour=juce::Colours::black;o.useNativeTitleBar=true;o.escapeKeyTriggersCloseButton=true;o.componentToCentreAround=this;utilityWindow=o.launchAsync();
}
void PixelEditor::chooseRecordingFolder()
{
    if(chooser||processor.recorder.isRecording())return;chooser=std::make_unique<juce::FileChooser>("Choose parent folder for recording takes",recordingFolder,"",false);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<PixelEditor>(this)](const juce::FileChooser& f){if(!safe)return;if(f.getResult().isDirectory())safe->recordingFolder=f.getResult();safe->chooser.reset();safe->repaint();});
}
void PixelEditor::utility()
{
    if(onUtility){onUtility();return;}
    if(utilityWindow){utilityWindow->toFront(true);return;}
    juce::DialogWindow::LaunchOptions o;o.content.setOwned(new MidiSettings(processor));o.dialogTitle="Companion MIDI mappings";o.dialogBackgroundColour=juce::Colours::black;o.useNativeTitleBar=true;o.escapeKeyTriggersCloseButton=true;o.componentToCentreAround=this;utilityWindow=o.launchAsync();
}
void PixelEditor::choosePatch(bool save)
{
    if(chooser)return;auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("Patches");
    if(!folder.isDirectory())folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    chooser=std::make_unique<juce::FileChooser>(save?"Save Companion project":"Load Companion project",folder.getChildFile(save?"Companion Patch.json":"Init.json"),"*.json",false);
    int flags=juce::FileBrowserComponent::canSelectFiles|(save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode);
    chooser->launchAsync(flags,[safe=juce::Component::SafePointer<PixelEditor>(this),save](const juce::FileChooser& f){
        if(!safe)return;auto file=f.getResult();if(file!=juce::File())
        {
            if(save){bool ok=file.withFileExtension("json").replaceWithText(juce::JSON::toString(safe->processor.state()));safe->toast=ok?"PROJECT SAVED":"SAVE FAILED";}
            else {safe->closeHostedEditors();bool ok=safe->processor.restore(juce::JSON::parse(file));safe->toast=ok?"PROJECT LOADED":"INVALID PROJECT";safe->showHome();if(safe->processor.lastError.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Some plugins could not be restored",safe->processor.lastError);}
            safe->toastTicks=90;
        }safe->chooser.reset();
    });
}
void PixelEditor::slotMenu(int slot)
{
    juce::PopupMenu menu;menu.addItem(1,"Load VST3 / CLAP (browse)...");
    menu.addItem(20,"Load from Common Files / VST3...",commonPlugins("VST3").isDirectory());menu.addItem(21,"Load from Common Files / CLAP...",commonPlugins("CLAP").isDirectory());
    menu.addItem(22,"Load from user VST3 folder...",userPlugins("VST3").isDirectory());menu.addItem(23,"Load from user CLAP folder...",userPlugins("CLAP").isDirectory());
    menu.addItem(24,"Load bundled Companion plugin...");menu.addSeparator();bool loaded=processor.currentRack().instance(slot)!=nullptr;
    menu.addItem(2,"Open plugin editor",loaded);menu.addItem(3,"Remove plugin",loaded);
    if(slot>0)
    {
        menu.addItem(4,"Bypass",loaded,processor.mixValues()[(size_t)(slot+4)]>.5f);
        juce::PopupMenu wet;for(int i=0;i<=4;++i)wet.addItem(10+i,juce::String(i*25)+"%",loaded,std::abs(processor.mixValues()[(size_t)slot]-i*.25f)<.001f);
        menu.addSubMenu("Wet mix",wet,loaded);
    }
    menu.showMenuAsync(menuOptions(),[safe=juce::Component::SafePointer<PixelEditor>(this),slot](int action){
        if(!safe)return;if(action==1)safe->choosePlugin(slot);
        if(action>=20&&action<=23)safe->choosePlugin(slot,action<22?commonPlugins(action%2?"CLAP":"VST3"):userPlugins(action%2?"CLAP":"VST3"));
        if(action==24)safe->choosePlugin(slot,juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("Plugins"));if(action==2)safe->openEditor(slot);if(action==3){safe->closeHostedEditors();safe->processor.removePlugin(slot);}if(action==4)safe->processor.setFxBypass(slot,safe->processor.mixValues()[(size_t)(slot+4)]<.5f);
        if(action>=10&&action<=14)safe->processor.setFxWet(slot,(action-10)*.25f);
    });
}
void PixelEditor::choosePlugin(int slot,juce::File folder)
{
    if(chooser)return;
    if(folder==juce::File())folder=lastPluginFolder.isDirectory()?lastPluginFolder:commonPlugins("VST3").isDirectory()?commonPlugins("VST3"):commonPlugins("CLAP").isDirectory()?commonPlugins("CLAP"):juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    chooser=std::make_unique<juce::FileChooser>("Choose a Windows x64 VST3 or CLAP plugin",folder,"*.vst3;*.clap",false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<PixelEditor>(this),slot,target=processor.soundTrack()](const juce::FileChooser& f){if(!safe)return;auto file=f.getResult();safe->chooser.reset();if(file!=juce::File()){safe->lastPluginFolder=file.getParentDirectory();safe->loadFile(slot,file,target);}});
}
void PixelEditor::loadFile(int slot,const juce::File& file,int targetTrack)
{
    if(targetTrack<0)targetTrack=processor.soundTrack();
    try
    {
        auto found=processor.rack.discover(file);if(found.empty()){toast="NO COMPATIBLE PLUGINS FOUND";toastTicks=120;return;}
        auto load=[safe=juce::Component::SafePointer<PixelEditor>(this),slot,found,targetTrack](int index){
            if(!safe||!juce::isPositiveAndBelow(index,(int)found.size()))return;safe->closeHostedEditors();auto error=safe->processor.loadPlugin(slot,found[(size_t)index],targetTrack);safe->toast=error.isEmpty()?"LOADED "+found[(size_t)index].name.toUpperCase():"LOAD FAILED";safe->toastTicks=120;
            if(error.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Plugin could not be loaded",error);
        };
        if(found.size()==1)load(0);else{juce::PopupMenu m;for(int i=0;i<(int)found.size();++i)m.addItem(i+1,found[(size_t)i].name);m.showMenuAsync(menuOptions(),[load](int id){if(id>0)load(id-1);});}
    }
    catch(const std::exception& error){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Plugin scan failed",error.what());}
}
void PixelEditor::openEditor(int slot)
{
    closeHostedEditors();
    if(auto p=processor.currentRack().instance(slot))pluginWindows.push_back(std::make_unique<HostedEditorWindow>(p,this));
}
void PixelEditor::assignMenu(int macro)
{
    juce::PopupMenu root;root.addItem(1,"Unassign");
    for(int slot=0;slot<3;++slot)if(auto p=processor.currentRack().instance(slot))
    {
        juce::PopupMenu sub;for(int i=0;i<juce::jmin(512,p->getParameters().size());++i)if(p->getParameters()[i]->isAutomatable())sub.addItem(100+slot*10000+i,p->getParameters()[i]->getName(80));root.addSubMenu(p->getName(),sub);
    }
    root.showMenuAsync(menuOptions(),[safe=juce::Component::SafePointer<PixelEditor>(this),macro](int id){if(!safe)return;if(id==1)safe->processor.assignMacro(macro,0,-1);else if(id>=100){int encoded=id-100;safe->processor.assignMacro(macro,encoded/10000,encoded%10000);}});
}
bool PixelEditor::isInterestedInFileDrag(const juce::StringArray& files){if(!processor.hostingEnabled||processor.track()>=4)return false;for(auto& f:files)if(juce::File(f).hasFileExtension("clap;vst3"))return true;return false;}
void PixelEditor::filesDropped(const juce::StringArray& files,int x,int y)
{
    if(processor.track()>=4)return;auto p=logical({(float)x,(float)y});int slot=!processor.isPluginTrack()?1+(p.y>=151?1:0):processor.page.load()==0?juce::jlimit(0,2,(p.x-18)/116-1):0;
    for(auto& file:files)if(juce::File(file).hasFileExtension("clap;vst3")){loadFile(slot,juce::File(file));break;}
}

