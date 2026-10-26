#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace companion
{
// Normalized automation/state stays compatible; these conversions mirror AudioDSP.c.
inline int discreteSteps(int i) { return i==7?2:i==25?4:i==32?8:i==33||i==34?9:i==2?49:0; }
inline int discreteIndex(int i,float v)
{
    if(i==7)return v>.5f?1:0;
    if(i==25||i==32)return juce::jmin(discreteSteps(i)-1,(int)std::floor(v*discreteSteps(i)));
    if(i==2)return (int)std::round((v-.5)*48)+24;
    if(i==33||i==34)return (int)std::round((v-.5)*8)+4;
    return juce::roundToInt(v*(discreteSteps(i)-1));
}
inline float knobPosition(int i,float v) { return discreteSteps(i)?discreteIndex(i,v)/(float)(discreteSteps(i)-1):v; }
inline float knobToParameter(int i,float v)
{
    int steps=discreteSteps(i);if(!steps)return v;int index=juce::roundToInt(v*(steps-1));
    return i==25||i==32?(index+.5f)/steps:index/(float)(steps-1);
}
inline juce::String percentage(double v) { return juce::String(v*100.,1)+"%"; }
inline juce::String timeText(double seconds) { return seconds<1?juce::String(seconds*1000.,1)+" MS":juce::String(seconds,2)+" S"; }
inline juce::String parameterText(int i,float v)
{
    v=juce::jlimit(0.f,1.f,v);
    if(i==0||i==1){const char* waves[]={"SIN","TRI","SAW","SQR"};float p=v*3;int a=juce::jmin(3,(int)p);return a==3||p-a<.005f?juce::String(waves[a]):juce::String(waves[a])+">"+waves[a+1]+" "+juce::String(juce::roundToInt((p-a)*100))+"%";}
    switch(i)
    {
        case 2:return juce::String((int)std::round((v-.5)*48))+" ST";
        case 5:return juce::String(1+7*v,2)+" X";
        case 6:return juce::String((v-.5f)*144,1)+" CT";
        case 7:return v>.5f?"ON":"OFF";
        case 8:{double hz=35*std::pow(2.,9.1*v);return hz>=1000?juce::String(hz/1000,2)+" KHZ":juce::String(hz,1)+" HZ";}
        case 10:{float p=v*2;return p<1?"LP>BP "+percentage(p):"BP>HP "+percentage(p-1);}
        case 11:return juce::String((v*2-1)*7,2)+" OCT";
        case 12:case 16:return timeText(.002+2*v*v);
        case 13:case 15:case 17:return timeText(.008+3*v*v);
        case 19:return timeText(.008+4*v*v);
        case 21:return std::abs(v-.5f)<.004f?"CENTER":juce::String(juce::roundToInt(std::abs(2*v-1)*100))+(v<.5f?" L":" R");
        case 23:return juce::String(1+8*v,2)+" X";
        case 24:return juce::String((v-.5f)*48,1)+" ST";
        case 25:return "MODEL "+juce::String(discreteIndex(i,v)+1);
        case 26:return juce::String(1+11*v,2)+" X";
        case 27:return juce::String((v-.5f)*7,2)+" OCT";
        case 31:return timeText(1./(30+110*v));
        case 32:return juce::String(discreteIndex(i,v)+1)+" / 8";
        case 33:case 34:return juce::String(std::pow(2.,std::round((v-.5)*8)),4).trimCharactersAtEnd("0").trimCharactersAtEnd(".")+" X";
        case 35:return juce::String(v*10,2)+" RAD";
        case 36:return timeText(.025+3*v*v);
        case 37:return juce::String(v*3,2)+" RAD";
        case 38:return juce::String(std::pow(2.,(v-.5)*3),3)+" X";
        case 39:return juce::String(1+3*v,2)+" X";
        default:return percentage(v); // Dimensionless amounts/mixes, not invented MIDI units.
    }
}
}
