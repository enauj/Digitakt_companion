#include "AudioDSP.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <math.h>

#define TAU 6.2831853071795864769
#define VOICES 4
#define PARAMS 40
#define OSC_PARAMS 8
#define METAL_OFFSET 24
#define FM_OFFSET 32

typedef struct {
    double phase1, phase2, phase3, phase4, feedback, sweepTime;
    double lp, bp;
    float amp, filt, velocity, targetVelocity;
    double pitch, pitchStep;
    int glideRemaining;
    int ampStage, filtStage;
    uint8_t note, active;
    unsigned generation;
} Voice;

typedef struct {
    _Atomic float p[PARAMS];
    _Atomic int cc[8], channel, page, engine, mono, sustain;
    _Atomic float glide;
    _Atomic unsigned gate[128], latched[128], gen[128], counter;
    Voice voice[VOICES];
    double previousSum, correction, rampLeft, rampLength;
    int switchPending, lastMono, monoHeld;
    unsigned consumed[128]; // Render-thread note generations; stolen notes stay stolen.
} Synth;

static float clip(float x) { return fminf(1.f,fmaxf(0.f,x)); }
static double blep(double t, double d) {
    if (t < d) { t/=d; return t+t-t*t-1.; }
    if (t > 1.-d) { t=(t-1.)/d; return t*t+t+t+1.; }
    return 0.;
}
static double shape(double phase, double increment, float morph) {
    double sine=sin(TAU*phase);
    double tri=2.*fabs(2.*phase-1.)-1.;
    double saw=2.*phase-1.-blep(phase,increment);
    double square=(phase<.5?1.:-1.)+blep(phase,increment)-blep(fmod(phase+.5,1.),increment);
    double pos=clip(morph)*3.;
    if (pos<1.) return sine+(tri-sine)*pos;
    if (pos<2.) return tri+(saw-tri)*(pos-1.);
    return saw+(square-saw)*(pos-2.);
}
static double envelope(float *value, int *stage, int gate, double a, double d, double sustain, double r, double sr) {
    if (gate && (*stage==0 || *stage==4)) *stage=1;
    if (!gate && *stage!=0 && *stage!=4) *stage=4;
    if (*stage==1) { *value+=(float)(1./(sr*a)); if (*value>=1.f) {*value=1.f;*stage=2;} }
    if (*stage==2) { *value-=(float)((1.-sustain)/(sr*d)); if (*value<=sustain) {*value=(float)sustain;*stage=3;} }
    if (*stage==3) *value=(float)sustain;
    if (*stage==4) { *value-=(float)(1./(sr*r)); if (*value<=0.f){*value=0.f;*stage=0;} }
    return *value;
}
void *synth_create(void) {
    Synth *s=calloc(1,sizeof(Synth)); if(!s) return NULL;
    float init[PARAMS]={
        .18f,.68f,.5f,.5f,.5f,.5f,0.f,.0f, // osc: shape1 shape2 tune2 mix fm fold detune sync
        .62f,.18f,.0f,.5f,.05f,.3f,.0f,.4f, // filter: cutoff res morph envAmt A D S R
        .01f,.25f,.8f,.35f,.65f,.5f,.0f,.5f, // amp: A D S R level pan velocity drive
        .5f,0.f,0.f,.5f,.5f,.4f,.15f,.5f, // metallic: tune model fold sweep metal feedback color transient
        .0f,.5f,.5f,.5f,.6f,.5f,.3f,.35f // FM: algo ratios/index feedback decay/color/ratio shift/brightness
    };
    for(int i=0;i<PARAMS;i++) atomic_init(&s->p[i],init[i]);
    for(int i=0;i<8;i++) atomic_init(&s->cc[i],20+i);
    atomic_init(&s->channel,9); atomic_init(&s->page,0); atomic_init(&s->engine,0); atomic_init(&s->counter,0); atomic_init(&s->mono,0); atomic_init(&s->sustain,0); atomic_init(&s->glide,0);
    for(int n=0;n<128;n++){atomic_init(&s->gate[n],0);atomic_init(&s->latched[n],0);atomic_init(&s->gen[n],0);}
    return s;
}
void synth_destroy(void *h){free(h);}
void synth_set_parameter(void *h,int i,float v){if(h&&i>=0&&i<PARAMS&&isfinite(v))atomic_store(&((Synth*)h)->p[i],clip(v));}
float synth_get_parameter(void *h,int i){return h&&i>=0&&i<PARAMS?atomic_load(&((Synth*)h)->p[i]):0.f;}
void synth_all_sounds_off(void *h){
    if(!h)return;
    Synth*s=h;
    for(int n=0;n<128;n++){atomic_store(&s->gate[n],0);atomic_store(&s->latched[n],0);s->consumed[n]=atomic_load(&s->gen[n]);}
    for(int v=0;v<VOICES;v++)s->voice[v]=(Voice){0};
    atomic_store(&s->sustain,0);s->previousSum=s->correction=s->rampLeft=s->rampLength=0;s->switchPending=s->monoHeld=0;
}
void synth_set_cc(void *h,int i,int cc){if(h&&i>=0&&i<8&&cc>=0&&cc<120)atomic_store(&((Synth*)h)->cc[i],cc);}
void synth_set_channel(void *h,int ch){if(h&&ch>=1&&ch<=16)atomic_store(&((Synth*)h)->channel,ch);}
void synth_set_engine(void *h,int engine){if(h&&engine>=0&&engine<3)atomic_store(&((Synth*)h)->engine,engine);}
void synth_set_glide(void *h,int mono,float time){if(!h||!isfinite(time))return;atomic_store(&((Synth*)h)->mono,mono!=0);atomic_store(&((Synth*)h)->glide,clip(time));}
static unsigned held_velocity(Synth*s,int n){unsigned g=atomic_load(&s->gate[n]);return g?g:atomic_load(&s->latched[n]);}
void synth_set_page(void *h,int page){if(h&&page>=0&&page<3)atomic_store(&((Synth*)h)->page,page);}
int synth_handle_midi(void *h,uint8_t status,uint8_t d1,uint8_t d2){
    if(!h||d1>127||d2>127||status<0x80||status>=0xf0)return -1;
    Synth*s=h; if((status&15)+1!=atomic_load(&s->channel))return -1;
    int type=status&0xf0;
    if(type==0x90||type==0x80){
        unsigned vel=(type==0x80||!d2)?0:(unsigned)d2+1;
        unsigned before=atomic_exchange(&s->gate[d1],vel);
        if(vel){atomic_store(&s->latched[d1],0);atomic_store(&s->gen[d1],atomic_fetch_add(&s->counter,1)+1);}
        else if(atomic_load(&s->sustain)&&before)atomic_store(&s->latched[d1],before);
    }else if(type==0xb0){
        if(d1==64){atomic_store(&s->sustain,d2>=64);if(d2<64)for(int n=0;n<128;n++)atomic_store(&s->latched[n],0);}
        if(d1==120||d1==123){atomic_store(&s->sustain,0);for(int n=0;n<128;n++){atomic_store(&s->gate[n],0);atomic_store(&s->latched[n],0);}}
        int page=atomic_load(&s->page);
        for(int i=0;i<8;i++)if(d1==atomic_load(&s->cc[i])){
            int param=page==0?(atomic_load(&s->engine)==0?i:(atomic_load(&s->engine)==1?METAL_OFFSET+i:FM_OFFSET+i)):page*8+i;
            synth_set_parameter(h,param,d2/127.f);
            return param;
        }
    }
    return -1;
}
void synth_render(void*h,float*l,float*r,uint32_t frames,double sr){
    if(!h||!l||!r||sr<=0)return;
    Synth*s=h;float p[PARAMS];for(int i=0;i<PARAMS;i++)p[i]=atomic_load(&s->p[i]);
    int engine=atomic_load(&s->engine),mono=engine==0&&atomic_load(&s->mono);
    if(mono!=s->lastMono){for(int v=0;v<VOICES;v++){if(s->voice[v].active)s->switchPending=1;s->voice[v]=(Voice){0};}for(int n=0;n<128;n++)s->consumed[n]=0;s->monoHeld=0;s->lastMono=mono;}
    if(mono)
    {
        int latest=-1;unsigned order=0;for(int n=0;n<128;n++){unsigned g=atomic_load(&s->gen[n]);s->consumed[n]=g;if(held_velocity(s,n)&&g>=order){latest=n;order=g;}}
        Voice*q=&s->voice[0];
        if(latest>=0)
        {
            unsigned vel=held_velocity(s,latest);
            if(!s->monoHeld||!q->active)
            {
                if(q->active)s->switchPending=1;*q=(Voice){0};q->active=1;q->note=(uint8_t)latest;q->generation=order;q->pitch=latest;q->velocity=q->targetVelocity=(float)(vel-1)/127.f;
            }
            else if(q->note!=latest||q->generation!=order)
            {
                q->note=(uint8_t)latest;q->generation=order;q->targetVelocity=(float)(vel-1)/127.f;
                double time=atomic_load(&s->glide);q->glideRemaining=(int)round(2.*time*time*sr);q->pitchStep=q->glideRemaining>0?(latest-q->pitch)/q->glideRemaining:0;if(q->glideRemaining==0)q->pitch=latest;
            }
            s->monoHeld=1;
        }
        else s->monoHeld=0;
    }
    else for(int n=0;n<128;n++)
    {
        unsigned g=atomic_load(&s->gen[n]);if(!g||g==s->consumed[n])continue;s->consumed[n]=g;
        unsigned velocity=held_velocity(s,n);if(!velocity)continue;
        int slot=-1;for(int v=0;v<VOICES;v++)if(s->voice[v].active&&s->voice[v].note==n){slot=v;break;}
        if(slot<0)for(int v=0;v<VOICES;v++)if(!s->voice[v].active){slot=v;break;}
        if(slot<0){slot=0;for(int v=1;v<VOICES;v++)if(s->voice[v].generation<s->voice[slot].generation)slot=v;}
        if(s->voice[slot].active)s->switchPending=1;
        s->voice[slot]=(Voice){0};s->voice[slot].note=(uint8_t)n;s->voice[slot].pitch=n;s->voice[slot].active=1;s->voice[slot].generation=g;s->voice[slot].velocity=s->voice[slot].targetVelocity=(float)(velocity-1)/127.f;
    }
    double velocitySlew=1.-exp(-1./(.003*sr));
    double filterCutoff=35.*pow(2.,9.1*p[8]);
    double fEnvA=.002+2.*p[12]*p[12],fEnvD=.008+3.*p[13]*p[13],fEnvR=.008+3.*p[15]*p[15];
    double ampA=.002+2.*p[16]*p[16],ampD=.008+3.*p[17]*p[17],ampR=.008+4.*p[19]*p[19];
    double oscRatio=pow(2.,round((p[2]-.5)*48.)/12.);
    double fineRatio=pow(2.,(p[6]-.5)*.12);
    double damping=.08+(1.-p[9])*1.65;
    // Jury stability bound for this explicit SVF: f*f + 2*f*damping < 4.
    // The old fixed .95 cap is unstable at low resonance/high cutoff.
    double stableF=fmin(.95,.98*(sqrt(damping*damping+4.)-damping));
    for(uint32_t frame=0;frame<frames;frame++){
        double sum=0.;
        for(int v=0;v<VOICES;v++){
            Voice*q=&s->voice[v];if(!q->active)continue;
            unsigned vel=held_velocity(s,q->note);
            q->velocity+=(float)((q->targetVelocity-q->velocity)*velocitySlew);
            if(q->glideRemaining>0){q->pitch+=q->pitchStep;if(--q->glideRemaining==0)q->pitch=q->note;}
            double amp=envelope(&q->amp,&q->ampStage,vel!=0,ampA,ampD,p[18],ampR,sr);
            double fenv=envelope(&q->filt,&q->filtStage,vel!=0,fEnvA,fEnvD,p[14],fEnvR,sr);
            if(!vel&&q->ampStage==0){q->active=0;continue;}
            double freq=440.*pow(2.,((mono?q->pitch:(double)q->note)-69.)/12.);
            double d2=fmin(.45,freq*oscRatio*fineRatio/sr), d1=fmin(.45,freq/sr);
            double signal=0.;
            if(engine==0){
                double b=shape(q->phase2,d2,p[1]);
                double fm=2.*p[4]*p[4];
                double a=shape(q->phase1,d1,p[0]);
                double offset=sin(TAU*q->phase1+fm*b);
                a=a*(1.-p[4])+offset*p[4];
                signal=a*(1.-p[3])+b*p[3];
                double fold=1.+7.*p[5];
                signal=sin(signal*fold)*(.8+.2/fold);
                q->phase1+=d1;if(q->phase1>=1.)q->phase1-=1.;
                q->phase2+=d2;if(q->phase2>=1.){q->phase2-=1.;if(p[7]>.5)q->phase1=0.;}
            } else if(engine==1){
                // Independent metallic-percussion voice: NOT upstream DigiSophie DSP.
                // Controls: tune, model, fold, sweep, metal, feedback, color, transient.
                double tune=pow(2.,(p[24]-.5)*4.);
                double model=fmin(3.,floor(p[25]*4.));
                double color=p[30], metal=p[28], fb=p[29];
                double ratios[4]={1.44+color*3.7,1.0+color*1.4,2.31+color*5.4,0.73+color*8.};
                double age=q->sweepTime;
                double sweep=pow(2.,(p[27]-.5)*7.*exp(-age*20.));
                double base=fmin(.45,freq*tune*sweep/sr);
                double mod=sin(TAU*q->phase2)*metal*9.+q->feedback*fb*2.;
                double main=sin(TAU*q->phase1+mod);
                double ring=sin(TAU*q->phase3*ratios[(int)model]);
                double attackNoise=sin((q->phase4*12345.+age*87.)*TAU)*exp(-age*(30.+p[31]*110.));
                if(model==0.)signal=main*.65+main*ring*.35;
                else if(model==1.)signal=main*.85+ring*.15;
                else if(model==2.)signal=main*ring*.8+main*.2;
                else signal=main*.48+ring*.3+attackNoise*.22;
                q->feedback=.8*q->feedback+.2*signal;
                double fold=1.+p[26]*11.;signal=sin(signal*fold)/sqrt(fold);
                q->phase1=fmod(q->phase1+base,1.);
                q->phase2=fmod(q->phase2+fmin(.45,base*(1.3+color*2.7)),1.);
                q->phase3=fmod(q->phase3+base,1.);
                q->phase4=fmod(q->phase4+base*.371,1.);
                q->sweepTime+=1./sr;
            } else {
                // Independent four-operator FM voice: NOT upstream DT-FM DSP.
                // Eight source macros drive a four-op cascade/parallel topology.
                int algorithm=(int)fmin(7.,floor(p[32]*8.));
                double ratioA=pow(2.,round((p[33]-.5)*8.));
                double ratioB=pow(2.,round((p[34]-.5)*8.));
                double shift=pow(2.,(p[38]-.5)*3.);
                double depth=p[35]*10.;
                double decay=exp(-q->sweepTime/(.025+3.*p[36]*p[36]));
                double op4=sin(TAU*q->phase4+q->feedback*p[37]*3.);
                double op3=sin(TAU*q->phase3+op4*depth*.7);
                double op2=sin(TAU*q->phase2+op3*depth);
                double op1=sin(TAU*q->phase1+op2*depth);
                if(algorithm<2)signal=op1;
                else if(algorithm<4)signal=.65*op1+.35*op3;
                else if(algorithm<6)signal=.6*op1+.4*op2;
                else signal=(op1+op2+op3+op4)*.25;
                signal=signal*(.5+.5*decay);
                q->feedback=.8*q->feedback+.2*op4;
                q->phase1=fmod(q->phase1+fmin(.45,freq/sr),1.);
                q->phase2=fmod(q->phase2+fmin(.45,freq*ratioA/sr),1.);
                q->phase3=fmod(q->phase3+fmin(.45,freq*ratioB/sr),1.);
                q->phase4=fmod(q->phase4+fmin(.45,freq*shift*(1.+p[39]*3.)/sr),1.);
                q->sweepTime+=1./sr;
            }
            double cutoff=filterCutoff*pow(2.,(fenv-.5)*7.*(p[11]*2.-1.));
            cutoff=fmax(25.,fmin(sr*.35,cutoff));
            double f=fmin(stableF,2.*sin(3.141592653589793*cutoff/sr));
            q->lp+=f*q->bp;
            double hp=signal-q->lp-damping*q->bp;
            q->bp+=f*hp;
            if(!isfinite(q->lp)||!isfinite(q->bp)){q->lp=0.;q->bp=0.;}
            double morph=p[10]*2.;
            double filtered=morph<1.?q->lp+(q->bp-q->lp)*morph:q->bp+(hp-q->bp)*(morph-1.);
            double vscale=(1.-p[22])+p[22]*q->velocity;
            sum+=filtered*amp*vscale;
        }
        // Switch-and-ramp: cancel a reset/steal discontinuity at its exact
        // sample, then remove the correction over 4 ms. No extra note latency.
        if(s->switchPending){s->correction=s->previousSum-sum;s->rampLeft=s->rampLength=fmax(1.,round(sr*.004));s->switchPending=0;}
        if(s->rampLeft>0){sum+=s->correction*s->rampLeft/s->rampLength;s->rampLeft-=1.;}
        s->previousSum=sum;
        float output=tanhf((float)(sum*.28*p[20]*(1.+p[23]*8.)))*.75f;
        l[frame]=output*fminf(1.f,2.f*(1.f-p[21]));r[frame]=output*fminf(1.f,2.f*p[21]);
    }
}
