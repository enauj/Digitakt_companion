#include "AudioDSP.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include <cstring>

static void require(bool condition, const char* description)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}

struct Synth
{
    void* handle = synth_create();
    Synth() { require(handle != nullptr, "synth allocation"); }
    ~Synth() { synth_destroy(handle); }
    void note(int n, int velocity = 100) { synth_handle_midi(handle, 0x98, (uint8_t)n, (uint8_t)velocity); }
};

static double energy(void* s, int frames, double rate)
{
    std::vector<float> l((size_t)frames), r((size_t)frames);
    synth_render(s, l.data(), r.data(), (uint32_t)frames, rate);
    double e = 0;
    for (int i = 0; i < frames; ++i)
    {
        require(std::isfinite(l[(size_t)i]) && std::isfinite(r[(size_t)i]), "finite stereo output");
        require(std::abs(l[(size_t)i]) <= .751f && std::abs(r[(size_t)i]) <= .751f, "bounded output");
        e += (double)l[(size_t)i] * l[(size_t)i];
    }
    return e;
}

static void clickReport(bool verify)
{
    for(double rate:{44100.,48000.,96000.})for(int engine=0;engine<3;++engine)
    {
        double retrigger=0,steal=0;
        for(int trial=0;trial<32;++trial)
        {
            Synth s;synth_set_engine(s.handle,engine);s.note(48);int frames=1500+trial*139;std::vector<float> l((size_t)frames),r((size_t)frames);synth_render(s.handle,l.data(),r.data(),(uint32_t)frames,rate);float before=l.back(),after=0,right=0;s.note(48);synth_render(s.handle,&after,&right,1,rate);retrigger=std::max(retrigger,(double)std::abs(after-before));
            synth_all_sounds_off(s.handle);for(int n:{48,55,60,64})s.note(n);synth_render(s.handle,l.data(),r.data(),(uint32_t)frames,rate);before=l.back();s.note(67);synth_render(s.handle,&after,&right,1,rate);steal=std::max(steal,(double)std::abs(after-before));
        }
        std::printf("Click boundary engine=%d rate=%.0f retrigger=%.9f steal=%.9f\n",engine,rate,retrigger,steal);
        if(verify)require(retrigger<.000001&&steal<.000001,"voice reset/steal transition preserves its previous output sample");
    }
}
static void cleanTone(void* s)
{
    synth_set_parameter(s,0,0);synth_set_parameter(s,1,0);synth_set_parameter(s,3,0);synth_set_parameter(s,4,0);synth_set_parameter(s,5,0);synth_set_parameter(s,8,1);synth_set_parameter(s,9,0);synth_set_parameter(s,11,.5f);synth_set_parameter(s,16,0);synth_set_parameter(s,18,1);synth_set_parameter(s,20,.5f);synth_set_parameter(s,22,0);synth_set_parameter(s,23,0);
}
static double frequency(const std::vector<float>& audio,int from,int count,double rate)
{
    double first=-1,last=-1;int crossings=0;for(int i=from+1;i<from+count;++i)if(audio[(size_t)i-1]<=0&&audio[(size_t)i]>0){double at=i-1-audio[(size_t)i-1]/(audio[(size_t)i]-audio[(size_t)i-1]);if(first<0)first=at;last=at;++crossings;}require(crossings>=3,"frequency measurement has enough cycles");return (crossings-1)*rate/(last-first);
}
int main(int argc,char** argv)
{
    if(argc==2&&std::strcmp(argv[1],"--click-report")==0){clickReport(false);return 0;}
    clickReport(true);
    for(double rate:{44100.,48000.,96000.})
    {
        Synth s,reference;for(auto p:{s.handle,reference.handle}){cleanTone(p);synth_set_glide(p,1,std::sqrt(.05f));synth_handle_midi(p,0x98,57,100);energy(p,(int)(rate*.25),rate);}
        s.note(69);int count=(int)(rate*.22);std::vector<float> l((size_t)count),r((size_t)count);synth_render(s.handle,l.data(),r.data(),(uint32_t)count,rate);float continuation=0,right=0;synth_render(reference.handle,&continuation,&right,1,rate);require(std::abs(l[0]-continuation)<.000001f,"legato preserves oscillator/filter/envelope continuity on the first sample");
        double early=frequency(l,0,(int)(rate*.025),rate),late=frequency(l,(int)(rate*.12),(int)(rate*.08),rate);std::printf("Glide rate=%.0f early=%.3f Hz late=%.3f Hz\n",rate,early,late);require(early>220&&early<290&&std::abs(late-440)<1,"100 ms legato glide passes intermediate pitches and reaches target");
        synth_handle_midi(s.handle,0x88,69,0);synth_render(s.handle,l.data(),r.data(),(uint32_t)count,rate);require(std::abs(frequency(l,(int)(rate*.12),(int)(rate*.08),rate)-220)<1,"releasing latest note glides back to the older held note");
        synth_handle_midi(s.handle,0x88,57,0);energy(s.handle,(int)(rate*5),rate);require(energy(s.handle,128,rate)==0,"legato releases after all notes end");s.note(69);synth_render(s.handle,l.data(),r.data(),(uint32_t)count,rate);require(std::abs(frequency(l,(int)(rate*.025),(int)(rate*.08),rate)-440)<1,"separate notes start at their own pitch without an unwanted glide");
        synth_all_sounds_off(s.handle);synth_set_glide(s.handle,1,0);s.note(57);energy(s.handle,(int)(rate*.1),rate);s.note(69);synth_render(s.handle,l.data(),r.data(),(uint32_t)count,rate);require(std::abs(frequency(l,0,(int)(rate*.08),rate)-440)<1,"zero glide time makes an immediate legato pitch change");
        synth_all_sounds_off(s.handle);synth_set_glide(s.handle,0,0);s.note(60);energy(s.handle,2048,rate);synth_handle_midi(s.handle,0xb8,64,127);synth_handle_midi(s.handle,0x88,60,0);require(energy(s.handle,(int)rate,rate)>.01,"sustain pedal holds a released key");synth_handle_midi(s.handle,0xb8,64,0);energy(s.handle,(int)(rate*5),rate);require(energy(s.handle,128,rate)==0,"pedal release ends latched voices");
        for(int engine=0;engine<3;++engine){synth_all_sounds_off(s.handle);synth_set_engine(s.handle,engine);for(int n=0;n<300;++n){s.note(45+n%24,20+n%107);energy(s.handle,17,rate);}synth_all_sounds_off(s.handle);require(energy(s.handle,128,rate)==0,"panic clears residual correction after rapid voice stealing");}
    }
    for (auto rate : {44100.0, 48000.0, 96000.0})
        for (int engine = 0; engine < 3; ++engine)
        {
            Synth s;
            synth_set_engine(s.handle, engine);
            require(energy(s.handle, 1024, rate) == 0.0, "silence before note");
            s.note(60);
            require(energy(s.handle, 8192, rate) > .0001, "each engine produces sound at each sample rate");
            synth_handle_midi(s.handle, 0x88, 60, 0);
            energy(s.handle, (int)(rate * 5), rate);
            require(energy(s.handle, 256, rate) == 0.0, "note-off eventually becomes silent");
            s.note(64, 0);
            require(energy(s.handle, 256, rate) == 0.0, "zero-velocity note is note-off");
        }
    {
        Synth s;
        synth_handle_midi(s.handle, 0x90, 60, 100);
        require(energy(s.handle, 1024, 48000) == 0.0, "reject other MIDI channels");
        for (int engine = 0; engine < 3; ++engine)
        {
            synth_set_engine(s.handle, engine);
            for (int page = 0; page < 3; ++page)
            {
                synth_set_page(s.handle, page);
                for (int slot = 0; slot < 8; ++slot)
                {
                    int expected = page == 0 ? (engine == 0 ? slot : (engine == 1 ? 24 : 32) + slot) : page * 8 + slot;
                    require(synth_handle_midi(s.handle, 0xb8, (uint8_t)(20 + slot), 127) == expected, "CC maps to engine and page");
                    require(synth_get_parameter(s.handle, expected) == 1.f, "CC reaches DSP parameter");
                }
            }
        }
        synth_set_cc(s.handle, 0, 40);
        synth_set_page(s.handle, 1);
        require(synth_handle_midi(s.handle, 0xb8, 40, 0) == 8, "custom CC mapping");
        synth_set_parameter(s.handle, 8, std::numeric_limits<float>::quiet_NaN());
        require(synth_get_parameter(s.handle, 8) == 0.f, "reject NaN parameter");
        synth_set_parameter(s.handle, 8, 9.f);
        require(synth_get_parameter(s.handle, 8) == 1.f, "clamp parameter range");
    }
    {
        Synth s;
        s.note(60);
        energy(s.handle, 256, 48000);
        synth_all_sounds_off(s.handle);
        require(energy(s.handle, 2048, 48000) == 0.0, "panic immediately silences voices");
        s.note(60);
        require(energy(s.handle, 2048, 48000) > .0001, "notes work after panic");
    }
    {
        // A fifth held note steals one voice. The stolen note must not return
        // and reset the other voices every subsequent block.
        Synth s;
        for (int n = 60; n < 65; ++n) { s.note(n); energy(s.handle, 128, 48000); }
        synth_set_parameter(s.handle, 16, 1.f); // A new attack needs two seconds.
        double e = 0;
        for (int b = 0; b < 100; ++b) e += energy(s.handle, 128, 48000);
        require(e > 0.1, "held polyphony survives voice stealing without repeated restarts");
        synth_handle_midi(s.handle, 0xb8, 123, 0);
        energy(s.handle, 240000, 48000);
        require(energy(s.handle, 256, 48000) == 0.0, "all notes off releases held polyphony");
    }
    {
        // Zero velocity sensitivity would mask the release gain bug.
        // With full sensitivity, soft notes remain soft during their tails.
        Synth soft, loud;
        for (auto* s : {soft.handle, loud.handle}) { synth_set_parameter(s, 22, 1.f); synth_set_parameter(s, 23, 0.f); }
        soft.note(60, 10); loud.note(60, 100);
        energy(soft.handle, 8192, 48000); energy(loud.handle, 8192, 48000);
        synth_handle_midi(soft.handle, 0x88, 60, 0); synth_handle_midi(loud.handle, 0x88, 60, 0);
        require(energy(soft.handle, 2048, 48000) < energy(loud.handle, 2048, 48000) * .05, "release preserves captured note velocity");
    }
    std::puts("PASS: three engines, three sample rates, MIDI pages/channels, release, polyphony and panic");
}
