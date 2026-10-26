#pragma once
#include "Parameters.h"

namespace companion::midi
{
// Addresses identify functions, never the selected UI page. Avoid bank select,
// sustain, NRPN and channel-mode CCs. Each sound track has its own MIDI channel.
inline int nativeCC(int parameter)
{
    if(parameter<8)return 16+parameter;
    if(parameter<16)return 48+parameter-8;
    if(parameter<24)return 56+parameter-16;
    if(parameter<32)return 24+parameter-24;
    return 40+parameter-32;
}
inline int nativeParameter(int cc)
{
    for(int i=0;i<40;++i)if(nativeCC(i)==cc)return i;return -1;
}
inline int soundCC(int engine,int page,int knob)
{
    if(engine==3)return page==1?80+knob:page==2?88+knob:-1;
    return page==3?72+knob:nativeCC(parameterIndex(engine,page,knob));
}
inline int modCC(int page,int knob)
{
    return page==0?(knob==0?16:-1):page==1?(knob<4?24+knob:-1):page==2?40+knob:page==3?48+knob:page==4?102+knob:page==5?110+knob:page==6?(knob==1?96:knob==3?97:knob==5?98:-1):page==7?(knob<5?33+knob:-1):-1;
}
inline int vertexCC(int vertex,int dimension) {return (dimension==0?56:dimension==1?72:88)+vertex;}
inline int hardwareCC(int page,int knob) {return (page<2?16+page*8:40+(page-2)*8)+knob;}
inline float signedAmount(float normalized)
{
    // MIDI 64 and a dragged centre of 0.5 are both an exact zero.
    if(normalized>=.5f&&normalized<=64.f/127.f)return 0;
    return normalized<.5f?normalized*2-1:(normalized-64.f/127.f)/(1-64.f/127.f);
}
inline float amountPosition(float amount) {return amount<=0?(amount+1)*.5f:64.f/127.f+amount*(1-64.f/127.f);}
}
