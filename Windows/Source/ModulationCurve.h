#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

class ModulationCurve
{
public:
    static constexpr int maximumPoints=8;
    struct Point { float x=0, y=0, bend=0; int id=-1; };
    struct Shape { std::array<Point,maximumPoints> points {}; int count=0; };
    struct Lfo
    {
        std::atomic<float> speed {.5f},amount {0},phaseOffset {0},smooth {0},multiplier {6.f/14};
        std::atomic<int> target {-1},wave {0}; // target = stable vertex ID * 3 + X/Y/bend.
        std::atomic<bool> sync {false},enabled {true};
    };
    ModulationCurve();
    Shape snapshot() const;
    Shape effectiveSnapshot() const;
    void setVertexControl(int id,int dimension,float normalized);
    void queueVertexControl(int id,int dimension,float normalized);
    int controlCC(int page,int slot) const;
    void movePoint(int, float x, float y);
    int addPoint(float x,float y);
    void removePoint(int);
    void bendPoint(int,float);
    void resetShape();
    static float sample(const Shape&,float phase);
    float advance(int frames,double sampleRate,double beats,bool transportRunning,bool clockAvailable);
    float offsetFor(int route) const;
    float controlValue(int page,int slot) const;
    void setControl(int page,int slot,float);
    juce::String controlName(int page,int slot) const;
    juce::String controlText(int page,int slot) const;
    juce::var save() const;
    bool restore(const juce::var&);
    void restart() { restartPending.store(true); }
    static double multiplierValue(float);
    static double beatLength(float speed) {return .25*(1+juce::roundToInt(speed*127));}
    double rateHz() const { return .05*std::pow(240.,speed.load())*multiplierValue(multiplier.load()); }
    double cycleBeats() const;
    double lfoCycleBeats(int k) const {return beatLength(lfos[(size_t)k].speed.load())/multiplierValue(lfos[(size_t)k].multiplier.load());}
    int midiValue() const;
    std::atomic<float> multiplier {6.f/14};
    std::atomic<bool> midiEnabled {false};
    std::atomic<int> midiCC {20},midiChannel {12},midiMin {0},midiMax {127};
    std::atomic<float> speed {.58f},depth {.5f},phase {0},output {0};
    std::atomic<bool> enabled {true},loop {true},sync {false},bipolar {true};
    std::atomic<int> channel {13};
    std::atomic<int> selectedPoint {1};
    std::array<std::atomic<int>,8> destination,targetTrack;
    std::array<std::atomic<float>,8> amount;
    std::array<Lfo,2> lfos;
    std::array<std::atomic<float>,2> lfoOutput;
private:
    mutable juce::CriticalSection mutex;
    mutable juce::CriticalSection displayMutex;
    Shape shape,audioShape,displayShape;
    juce::var originalPoints;
    std::array<std::atomic<float>,24> queuedVertices;
    std::atomic<uint32_t> queuedMask {0};
    std::atomic<int> pointCount {5};
    void setVertexUnlocked(int id,int dimension,float normalized);
    std::atomic<bool> restartPending {true};
    double audioPhase=0,syncOrigin=0;
    std::array<double,2> lfoPhase {},smoothedLfo {};
};
