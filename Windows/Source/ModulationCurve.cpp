#include "ModulationCurve.h"
#include "Parameters.h"
#include "MidiMap.h"
#include <cmath>
#include <vector>

ModulationCurve::ModulationCurve()
{
    for(int t=0;t<8;++t){destination[(size_t)t].store(-1);targetTrack[(size_t)t].store(t%4);amount[(size_t)t].store(0);}
    for(auto& value:queuedVertices)value.store(0);for(auto& value:lfoOutput)value.store(0);
    resetShape();audioShape=displayShape=shape;
}
void ModulationCurve::resetShape()
{
    const juce::ScopedLock lock(mutex);shape.count=5;
    shape.points[0]={0,0,0,0};shape.points[1]={.2f,1,0,1};shape.points[2]={.55f,.85f,0,2};shape.points[3]={.78f,.25f,0,3};shape.points[4]={1,0,0,4};pointCount.store(5);queuedMask.store(0);originalPoints=juce::var();
    for(auto& lfo:lfos)lfo.target.store(-1);selectedPoint.store(1);restart();
}
ModulationCurve::Shape ModulationCurve::snapshot() const {const juce::ScopedLock lock(mutex);return shape;}
ModulationCurve::Shape ModulationCurve::effectiveSnapshot() const
{
    bool active=false;for(auto& lfo:lfos)active|=lfo.enabled.load()&&lfo.target.load()>=0&&lfo.amount.load()!=0;
    if(!active||!enabled.load())return snapshot();const juce::ScopedLock lock(displayMutex);return displayShape;
}
void ModulationCurve::setVertexUnlocked(int id,int dimension,float v)
{
    for(int i=0;i<shape.count;++i)if(shape.points[(size_t)i].id==id)
    {
        auto& p=shape.points[(size_t)i];if(dimension==0)p.x=i==0?0:i==shape.count-1?1:juce::jlimit(shape.points[(size_t)i-1].x+.005f,shape.points[(size_t)i+1].x-.005f,v);
        if(dimension==1)p.y=v;if(dimension==2)p.bend=2*v-1;return;
    }
}
void ModulationCurve::setVertexControl(int id,int dimension,float v)
{
    if(!juce::isPositiveAndBelow(id,8)||!juce::isPositiveAndBelow(dimension,3)||!std::isfinite(v))return;const juce::ScopedLock lock(mutex);setVertexUnlocked(id,dimension,juce::jlimit(0.f,1.f,v));
}
void ModulationCurve::queueVertexControl(int id,int dimension,float v)
{
    if(!juce::isPositiveAndBelow(id,8)||!juce::isPositiveAndBelow(dimension,3)||!std::isfinite(v))return;int index=id*3+dimension;queuedVertices[(size_t)index].store(juce::jlimit(0.f,1.f,v));queuedMask.fetch_or(uint32_t(1)<<index);
}
int ModulationCurve::controlCC(int p,int slot) const
{
    if(!juce::isPositiveAndBelow(slot,8))return -1;
    if(p==0&&slot>=1&&slot<=3){auto s=snapshot();int id=s.points[(size_t)juce::jlimit(0,s.count-1,selectedPoint.load())].id;return companion::midi::vertexCC(id,slot==1?2:slot-2);}
    if(p==6){static const int cc[]={16,96,102,97,110,98,-1,-1};return cc[slot];}
    return companion::midi::modCC(p,slot);
}
void ModulationCurve::movePoint(int index,float x,float y)
{
    if(!std::isfinite(x)||!std::isfinite(y))return;const juce::ScopedLock lock(mutex);
    if(!juce::isPositiveAndBelow(index,shape.count))return;
    auto& p=shape.points[(size_t)index];p.x=index==0?0:index==shape.count-1?1:juce::jlimit(shape.points[(size_t)index-1].x+.005f,shape.points[(size_t)index+1].x-.005f,x);p.y=juce::jlimit(0.f,1.f,y);
}
int ModulationCurve::addPoint(float x,float y)
{
    if(!std::isfinite(x)||!std::isfinite(y))return -1;const juce::ScopedLock lock(mutex);
    if(shape.count>=maximumPoints)return -1;
    int id=0;for(;id<maximumPoints;++id){bool used=false;for(int k=0;k<shape.count;++k)used|=shape.points[(size_t)k].id==id;if(!used)break;}
    for(int i=1;i<shape.count;++i)if(x>shape.points[(size_t)i-1].x+.01f&&x<shape.points[(size_t)i].x-.01f)
    {
        for(int j=shape.count;j>i;--j)shape.points[(size_t)j]=shape.points[(size_t)j-1];
        shape.points[(size_t)i]={x,juce::jlimit(0.f,1.f,y),0,id};++shape.count;pointCount.store(shape.count);selectedPoint.store(i);return i;
    }
    return -1;
}
void ModulationCurve::removePoint(int index)
{
    const juce::ScopedLock lock(mutex);if(index<=0||index>=shape.count-1||shape.count<=2)return;
    int id=shape.points[(size_t)index].id;for(auto& lfo:lfos)if(lfo.target.load()/3==id&&lfo.target.load()>=0)lfo.target.store(-1);
    for(int i=index;i<shape.count-1;++i)shape.points[(size_t)i]=shape.points[(size_t)i+1];--shape.count;pointCount.store(shape.count);selectedPoint.store(juce::jmin(index,shape.count-1));
}
void ModulationCurve::bendPoint(int index,float value)
{
    if(!std::isfinite(value))return;const juce::ScopedLock lock(mutex);if(juce::isPositiveAndBelow(index,shape.count))shape.points[(size_t)index].bend=juce::jlimit(-1.f,1.f,value);
}
float ModulationCurve::sample(const Shape& s,float p)
{
    if(s.count<2)return 0;if(p<=0)return s.points[0].y;
    for(int i=1;i<s.count;++i)if(p<=s.points[(size_t)i].x)
    {
        auto a=s.points[(size_t)i-1],b=s.points[(size_t)i];float f=juce::jlimit(0.f,1.f,(p-a.x)/juce::jmax(.00001f,b.x-a.x));
        if(a.bend>0)f=std::pow(f,1+5*a.bend);else if(a.bend<0)f=1-std::pow(1-f,1-5*a.bend);
        return a.y+(b.y-a.y)*f;
    }
    return s.points[(size_t)s.count-1].y;
}
double ModulationCurve::multiplierValue(float v)
{
    static const double values[]={1./16,1./8,1./6,1./4,1./3,1./2,1,2,3,4,5,6,8,12,16};return values[juce::jlimit(0,14,juce::roundToInt(v*14))];
}
double ModulationCurve::cycleBeats() const {return beatLength(speed.load())/multiplierValue(multiplier.load());}
int ModulationCurve::midiValue() const
{
    float value=output.load();if(bipolar.load())value=(value+1)*.5f;return juce::jlimit(0,127,juce::roundToInt(midiMin.load()+juce::jlimit(0.f,1.f,value)*(midiMax.load()-midiMin.load())));
}
float ModulationCurve::advance(int frames,double rate,double beats,bool running,bool available)
{
    {const juce::CriticalSection::ScopedTryLockType lock(mutex);if(lock.isLocked()){uint32_t pending=queuedMask.exchange(0);for(int i=0;i<24;++i)if(pending&(uint32_t(1)<<i))setVertexUnlocked(i/3,i%3,queuedVertices[(size_t)i].load());audioShape=shape;}}
    if(restartPending.exchange(false)){audioPhase=0;syncOrigin=beats;lfoPhase={};smoothedLfo={};}
    if(beats<syncOrigin)syncOrigin=beats;
    Shape effective=audioShape;
    bool active=enabled.load()&&!(sync.load()&&(!available||!running));
    for(int k=0;k<2;++k)
    {
        auto& lfo=lfos[(size_t)k];float value=0;
        if(active&&lfo.enabled.load()&&(!lfo.sync.load()||(available&&running)))
        {
            if(lfo.sync.load())lfoPhase[(size_t)k]=juce::jmax(0.,beats-syncOrigin)/lfoCycleBeats(k);
            else lfoPhase[(size_t)k]+=frames/juce::jmax(1.,rate)*(.02*std::pow(1000.,lfo.speed.load())*multiplierValue(lfo.multiplier.load()));
            double phase=lfoPhase[(size_t)k]+lfo.phaseOffset.load();phase-=std::floor(phase);double raw=0;
            switch(lfo.wave.load()){case 0:raw=std::sin(juce::MathConstants<double>::twoPi*phase);break;case 1:raw=1-4*std::abs(phase-.5);break;case 2:raw=2*phase-1;break;default:raw=phase<.5?1:-1;break;}
            double smooth=.1*lfo.smooth.load()*lfo.smooth.load();double alpha=smooth<=0?1:1-std::exp(-frames/(juce::jmax(1.,rate)*smooth));smoothedLfo[(size_t)k]+=(raw-smoothedLfo[(size_t)k])*alpha;value=(float)smoothedLfo[(size_t)k]*lfo.amount.load();
        }
        else smoothedLfo[(size_t)k]=0;
        lfoOutput[(size_t)k].store(value);int target=lfo.target.load();if(target<0)continue;
        for(int i=0;i<effective.count;++i)if(effective.points[(size_t)i].id==target/3){auto& p=effective.points[(size_t)i];switch(target%3){case 0:if(i>0&&i<effective.count-1)p.x+=value;break;case 1:p.y+=value;break;case 2:p.bend+=2*value;break;}break;}
    }
    for(int i=0;i<effective.count;++i){auto& p=effective.points[(size_t)i];p.x=juce::jlimit(0.f,1.f,p.x);p.y=juce::jlimit(0.f,1.f,p.y);p.bend=juce::jlimit(-1.f,1.f,p.bend);}
    for(int i=1;i<effective.count-1;++i)effective.points[(size_t)i].x=juce::jmax(effective.points[(size_t)i].x,effective.points[(size_t)i-1].x+.005f);
    for(int i=effective.count-2;i>0;--i)effective.points[(size_t)i].x=juce::jmin(effective.points[(size_t)i].x,effective.points[(size_t)i+1].x-.005f);
    {const juce::CriticalSection::ScopedTryLockType lock(displayMutex);if(lock.isLocked())displayShape=effective;}
    if(!active){output.store(0);return 0;}
    if(sync.load()){if(beats<syncOrigin)syncOrigin=beats;audioPhase=juce::jmax(0.,beats-syncOrigin)/cycleBeats();}
    else audioPhase+=(double)frames/juce::jmax(1.,rate)*rateHz();
    double p=loop.load()?audioPhase-std::floor(audioPhase):juce::jmin(1.,audioPhase);
    phase.store((float)p);float v=sample(effective,(float)p);v=bipolar.load()?2*v-1:v;v*=depth.load();output.store(v);return v;
}
float ModulationCurve::offsetFor(int route) const{return juce::isPositiveAndBelow(route,8)?output.load()*amount[(size_t)route].load():0;}
float ModulationCurve::controlValue(int page,int slot) const
{
    if(page<0||page>7||!juce::isPositiveAndBelow(slot,8))return 0;
    if(page==2||page==3){int r=(page-2)*4+slot%4;return slot<4?(destination[(size_t)r].load()+1)/48.f:companion::midi::amountPosition(amount[(size_t)r].load());}
    if(page==6){if(slot>=6)return 0;return slot%2==0?(slot==0?speed.load():lfos[(size_t)(slot/2-1)].speed.load()):(slot==1?multiplier.load():lfos[(size_t)(slot/2-1)].multiplier.load());}
    if(page==7){switch(slot){case 0:return midiEnabled.load()?1.f:0.f;case 1:return midiCC.load()/119.f;case 2:return (midiChannel.load()-1)/15.f;case 3:return midiMin.load()/127.f;case 4:return midiMax.load()/127.f;default:return 0;}}
    if(page==4||page==5){auto& l=lfos[(size_t)page-4];switch(slot){case 0:return l.speed.load();case 1:return companion::midi::amountPosition(l.amount.load());case 2:return l.wave.load()/3.f;case 3:return (l.target.load()+1)/24.f;case 4:return l.phaseOffset.load();case 5:return l.sync.load()?1.f:0.f;case 6:return l.smooth.load();default:return l.enabled.load()?1.f:0.f;}}
    auto s=snapshot();int index=juce::jlimit(0,s.count-1,selectedPoint.load());
    if(page==0){switch(slot){case 0:return speed.load();case 1:return (s.points[(size_t)index].bend+1)*.5f;case 2:return s.points[(size_t)index].x;case 3:return s.points[(size_t)index].y;default:return 0;}}
    switch(slot){case 0:return depth.load();case 1:return loop.load()?1.f:0.f;case 2:return sync.load()?1.f:0.f;case 3:return bipolar.load()?1.f:0.f;default:return 0;}
}
void ModulationCurve::setControl(int p,int slot,float v)
{
    if(!std::isfinite(v)||!juce::isPositiveAndBelow(slot,8)||p<0||p>7)return;v=juce::jlimit(0.f,1.f,v);
    if(p==2||p==3){int r=(p-2)*4+slot%4;if(slot<4)destination[(size_t)r].store(juce::roundToInt(v*48)-1);else amount[(size_t)r].store(companion::midi::signedAmount(v));return;}
    if(p==6){if(slot<6){if(slot%2==0){if(slot==0)speed.store(v);else lfos[(size_t)(slot/2-1)].speed.store(v);}else {if(slot==1)multiplier.store(v);else lfos[(size_t)(slot/2-1)].multiplier.store(v);}}return;}
    if(p==7){switch(slot){case 0:midiEnabled.store(v>=.5f);break;case 1:midiCC.store(juce::roundToInt(v*119));break;case 2:midiChannel.store(1+juce::roundToInt(v*15));break;case 3:midiMin.store(juce::roundToInt(v*127));break;case 4:midiMax.store(juce::roundToInt(v*127));break;}return;}
    if(p==4||p==5){auto& l=lfos[(size_t)p-4];switch(slot){case 0:l.speed.store(v);break;case 1:l.amount.store(companion::midi::signedAmount(v));break;case 2:l.wave.store(juce::roundToInt(v*3));break;case 3:l.target.store(juce::roundToInt(v*24)-1);break;case 4:l.phaseOffset.store(v);break;case 5:l.sync.store(v>=.5f);break;case 6:l.smooth.store(v);break;case 7:l.enabled.store(v>=.5f);break;}return;}
    if(p==0)
    {
        if(slot==0)speed.store(v);if(slot==1)bendPoint(selectedPoint.load(),2*v-1);
        if(slot==2||slot==3){auto s=snapshot();int i=juce::jlimit(0,s.count-1,selectedPoint.load());movePoint(i,slot==2?v:s.points[(size_t)i].x,slot==3?v:s.points[(size_t)i].y);}
        return;
    }
    if(slot==0)depth.store(v);else if(slot==1)loop.store(v>=.5f);else if(slot==2){bool previous=sync.exchange(v>=.5f);if(previous!=(v>=.5f))restart();}
    else if(slot==3)bipolar.store(v>=.5f);
}
juce::String ModulationCurve::controlName(int p,int s) const
{
    if(p<0||p>7||!juce::isPositiveAndBelow(s,8))return {};
    if(p==2||p==3){int r=(p-2)*4+s%4;return "R"+juce::String(r+1)+(s<4?">T"+juce::String(targetTrack[(size_t)r].load()+1):" AMOUNT");}
    if(p==6){static const char* labels[]={"CURVE RATE","CURVE MULT","LFO 1 RATE","LFO 1 MULT","LFO 2 RATE","LFO 2 MULT","",""};return labels[s];}
    if(p==7){static const char* labels[]={"CC OUT","CC NUMBER","OUT CH","MIN VALUE","MAX VALUE","","",""};return labels[s];}
    if(p==4||p==5){static const char* labels[]={"SPEED","AMOUNT","WAVE","DEST","PHASE","SYNC","SMOOTH","ACTIVE"};return labels[s];}
    static const char* curve[]={"SPEED","CURVE","POINT X","POINT Y"};static const char* setup[]={"DEPTH","LOOP","SYNC","BIPOLAR","","","",""};return p==0?curve[juce::jlimit(0,3,s)]:setup[juce::jlimit(0,7,s)];
}
juce::String ModulationCurve::controlText(int p,int s) const
{
    if(p<0||p>7||!juce::isPositiveAndBelow(s,8))return {};
    if(p==2||p==3){int r=(p-2)*4+s%4;if(s>=4){float a=amount[(size_t)r].load()*100;return (a>0?"+":"")+juce::String(a,1)+"%";}int d=destination[(size_t)r].load();return d<0?"OFF":d<40?juce::String(companion::names[(size_t)d]):"FX"+juce::String(1+(d-40)/4)+" P"+juce::String(1+(d-40)%4);}
    if(p==6){if(s>=6)return {};if(s%2!=0){float m=s==1?multiplier.load():lfos[(size_t)(s/2-1)].multiplier.load();double v=multiplierValue(m);return v>=1?"X"+juce::String(v,0):"/"+juce::String(1/v,0);}if(s==0)return controlText(0,0);return controlText(4+s/2-1,0);}
    if(p==7){switch(s){case 0:return midiEnabled.load()?"ON":"OFF";case 1:return juce::String(midiCC.load());case 2:return juce::String(midiChannel.load());case 3:return juce::String(midiMin.load());case 4:return juce::String(midiMax.load());default:return {};}}
    if(p==4||p==5){auto& l=lfos[(size_t)p-4];switch(s){case 0:return l.sync.load()?juce::String(beatLength(l.speed.load()),2)+" BT":juce::String(.02*std::pow(1000.,l.speed.load()),2)+" HZ";case 1:{float a=l.amount.load()*100;return (a>0?"+":"")+juce::String(a,1)+"%";}case 2:{static const char* waves[]={"SINE","TRIANGLE","SAW","SQUARE"};return waves[l.wave.load()];}case 3:{int d=l.target.load();return d<0?"OFF":"V"+juce::String(d/3+1)+(d%3==0?" X":d%3==1?" Y":" BEND");}case 4:return juce::String(l.phaseOffset.load()*360,1)+" DEG";case 5:return l.sync.load()?"ON":"OFF";case 6:return juce::String(100*l.smooth.load()*l.smooth.load(),1)+" MS";default:return l.enabled.load()?"ON":"OFF";}}
    if(p==0&&s==0)return sync.load()?juce::String(beatLength(speed.load()),2)+" BT":juce::String(.05*std::pow(240.,speed.load()),2)+" HZ";
    if(p==1&&s>=1&&s<=3)return controlValue(p,s)>.5f?"ON":"OFF";
    if(p==0&&s==1)return juce::String(2*controlValue(p,s)-1,2);
    return juce::String(controlValue(p,s)*100,1)+"%";
}
juce::var ModulationCurve::save() const
{
    auto* o=new juce::DynamicObject();o->setProperty("rateVersion",1);o->setProperty("multiplier",multiplier.load());o->setProperty("midiEnabled",midiEnabled.load());o->setProperty("midiCC",midiCC.load());o->setProperty("midiChannelOut",midiChannel.load());o->setProperty("midiMin",midiMin.load());o->setProperty("midiMax",midiMax.load());o->setProperty("routeVersion",2);o->setProperty("vertexVersion",1);o->setProperty("midiChannel",channel.load());o->setProperty("speed",speed.load());o->setProperty("depth",depth.load());o->setProperty("enabled",enabled.load());o->setProperty("loop",loop.load());o->setProperty("sync",sync.load());o->setProperty("bipolar",bipolar.load());
    juce::Array<juce::var> points,routes;auto s=snapshot();for(int i=0;i<s.count;++i){auto* point=new juce::DynamicObject();point->setProperty("id",s.points[(size_t)i].id);point->setProperty("x",s.points[(size_t)i].x);point->setProperty("y",s.points[(size_t)i].y);point->setProperty("bend",s.points[(size_t)i].bend);points.add(juce::var(point));}
    for(int t=0;t<8;++t){auto* route=new juce::DynamicObject();route->setProperty("track",targetTrack[(size_t)t].load());route->setProperty("destination",destination[(size_t)t].load());route->setProperty("amount",amount[(size_t)t].load());routes.add(juce::var(route));}
    juce::Array<juce::var> lfoState;for(auto& l:lfos){auto* f=new juce::DynamicObject();f->setProperty("speed",l.speed.load());f->setProperty("multiplier",l.multiplier.load());f->setProperty("amount",l.amount.load());f->setProperty("wave",l.wave.load());f->setProperty("target",l.target.load());f->setProperty("phase",l.phaseOffset.load());f->setProperty("sync",l.sync.load());f->setProperty("smooth",l.smooth.load());f->setProperty("enabled",l.enabled.load());lfoState.add(juce::var(f));}
    o->setProperty("lfos",lfoState);{const juce::ScopedLock lock(mutex);if(originalPoints.isArray())o->setProperty("originalPoints",originalPoints.clone());}
    o->setProperty("points",points);o->setProperty("routes",routes);return juce::var(o);
}
bool ModulationCurve::restore(const juce::var& state)
{
    if(!state.isObject())return false;auto points=state.getProperty("points",{}),routes=state.getProperty("routes",{}),lfoState=state.getProperty("lfos",{});Shape next;bool modern=state.hasProperty("routeVersion"),identified=state.hasProperty("vertexVersion");
    if(modern&&(!state["routeVersion"].isInt()||(int)state["routeVersion"]!=2))return false;
    if(identified&&(!state["vertexVersion"].isInt()||(int)state["vertexVersion"]!=1||!lfoState.isArray()||lfoState.size()!=2))return false;
    if(!points.isArray()||points.size()<2||points.size()>(identified?maximumPoints:16)||!routes.isArray()||routes.size()!=(modern?8:4))return false;
    auto valid=[](juce::var v,double lo,double hi){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v)&&(double)v>=lo&&(double)v<=hi;};
    if(!valid(state["speed"],0,1)||!valid(state["depth"],0,1))return false;
    if(modern&&(!state["midiChannel"].isInt()||!valid(state["midiChannel"],1,16)))return false;
    for(const char* flag:{"enabled","loop","sync","bipolar"})if(!state[flag].isBool())return false;
    bool timed=state.hasProperty("rateVersion");if(timed&&(!state["rateVersion"].isInt()||(int)state["rateVersion"]!=1||!valid(state["multiplier"],0,1)||!state["midiEnabled"].isBool()||!state["midiCC"].isInt()||!valid(state["midiCC"],0,119)||!state["midiChannelOut"].isInt()||!valid(state["midiChannelOut"],1,16)||!state["midiMin"].isInt()||!valid(state["midiMin"],0,127)||!state["midiMax"].isInt()||!valid(state["midiMax"],0,127)))return false;
    if(timed&&identified)for(auto f:*lfoState.getArray())if(!valid(f["multiplier"],0,1))return false;
    std::vector<Point> parsed;uint32_t ids=0;
    for(int i=0;i<points.size();++i){auto p=points[i];if(!valid(p["x"],0,1)||!valid(p["y"],0,1)||!valid(p["bend"],-1,1))return false;int id=identified?(int)p["id"]:-1;if(identified&&(!p["id"].isInt()||id<0||id>=8||(ids&(1u<<id))))return false;if(identified)ids|=1u<<id;parsed.push_back({(float)(double)p["x"],(float)(double)p["y"],(float)(double)p["bend"],id});if(i>0&&parsed[(size_t)i].x-parsed[(size_t)i-1].x<.0049f)return false;}
    if(parsed.front().x!=0||parsed.back().x!=1)return false;
    juce::var backup=state.getProperty("originalPoints",{});if(!backup.isVoid()&&(!backup.isArray()||backup.size()>16))return false;
    if(parsed.size()>8)backup=points.clone();
    while(parsed.size()>8){size_t remove=1;double smallest=1e9;for(size_t i=1;i+1<parsed.size();++i){auto a=parsed[i-1],b=parsed[i],c=parsed[i+1];double y=a.y+(c.y-a.y)*(b.x-a.x)/(c.x-a.x);double error=(std::abs(b.y-y)+.1*std::abs(a.bend-b.bend))*(c.x-a.x);if(error<smallest){smallest=error;remove=i;}}parsed.erase(parsed.begin()+(int)remove);}
    next.count=(int)parsed.size();for(int i=0;i<next.count;++i){next.points[(size_t)i]=parsed[(size_t)i];if(!identified)next.points[(size_t)i].id=i;}
    for(int t=0;t<routes.size();++t)if(!routes[t]["destination"].isInt()||!valid(routes[t]["destination"],-1,47)||!valid(routes[t]["amount"],modern?-1:0,1)||(modern&&(!routes[t]["track"].isInt()||!valid(routes[t]["track"],0,3))))return false;
    if(identified)for(auto f:*lfoState.getArray())if(!f.isObject()||!valid(f["speed"],0,1)||!valid(f["amount"],-1,1)||!f["wave"].isInt()||!valid(f["wave"],0,3)||!f["target"].isInt()||!valid(f["target"],-1,23)||!valid(f["phase"],0,1)||!valid(f["smooth"],0,1)||!f["sync"].isBool()||!f["enabled"].isBool())return false;
    {const juce::ScopedLock lock(mutex);shape=next;originalPoints=backup.clone();queuedMask.store(0);pointCount.store(next.count);}
    auto migrateSpeed=[](float v,bool synced){static const double old[]={.25,.5,1,2,4,8,16,32};return synced?(float)((old[juce::roundToInt(v*7)]/.25-1)/127):v;};
    multiplier.store(timed?(float)(double)state["multiplier"]:6.f/14);midiEnabled.store(timed&&(bool)state["midiEnabled"]);midiCC.store(timed?(int)state["midiCC"]:20);midiChannel.store(timed?(int)state["midiChannelOut"]:12);midiMin.store(timed?(int)state["midiMin"]:0);midiMax.store(timed?(int)state["midiMax"]:127);
    speed.store(timed?(float)(double)state["speed"]:migrateSpeed((float)(double)state["speed"],(bool)state["sync"]));depth.store((float)(double)state["depth"]);enabled.store((bool)state["enabled"]);loop.store((bool)state["loop"]);sync.store((bool)state["sync"]);bipolar.store((bool)state["bipolar"]);if(modern)channel.store((int)state["midiChannel"]);selectedPoint.store(juce::jmin(1,next.count-1));
    for(int t=0;t<8;++t){destination[(size_t)t].store(t<routes.size()?(int)routes[t]["destination"]:-1);targetTrack[(size_t)t].store(modern?(int)routes[t]["track"]:t%4);amount[(size_t)t].store(t<routes.size()?(float)(double)routes[t]["amount"]:0);}
    for(int k=0;k<2;++k){auto& l=lfos[(size_t)k];auto f=identified?lfoState[k]:juce::var();l.speed.store(identified?(timed?(float)(double)f["speed"]:migrateSpeed((float)(double)f["speed"],(bool)f["sync"])):.5f);l.multiplier.store(timed&&identified?(float)(double)f["multiplier"]:6.f/14);l.amount.store(identified?(float)(double)f["amount"]:0);l.target.store(identified?(int)f["target"]:-1);l.wave.store(identified?(int)f["wave"]:0);l.phaseOffset.store(identified?(float)(double)f["phase"]:0);l.sync.store(identified?(bool)f["sync"]:false);l.smooth.store(identified?(float)(double)f["smooth"]:0);l.enabled.store(identified?(bool)f["enabled"]:true);}restart();return true;
}
