#pragma once
#include <array>

namespace companion
{
inline constexpr int parameterCount = 40;
inline constexpr std::array<const char*, 40> names {
    "OSC 1 SHAPE", "OSC 2 SHAPE", "OSC 2 TUNE", "OSC MIX", "FM / XMOD", "WAVEFOLD", "FINE DETUNE", "HARD SYNC",
    "CUTOFF", "RESONANCE", "LP / BP / HP", "ENV AMOUNT", "FILTER ATTACK", "FILTER DECAY", "FILTER SUSTAIN", "FILTER RELEASE",
    "AMP ATTACK", "AMP DECAY", "AMP SUSTAIN", "AMP RELEASE", "LEVEL", "PAN", "VELOCITY", "DRIVE",
    "TUNE", "MODEL", "FOLD", "SWEEP", "METAL", "FEEDBACK", "COLOR", "TRANSIENT",
    "ALGORITHM", "RATIO A", "RATIO B", "FM INDEX", "DECAY", "FEEDBACK", "RATIO SHIFT", "BRIGHTNESS"
};
inline constexpr std::array<const char*, 3> engineNames { "DUAL OSC", "METALLIC", "FOUR-OP FM" };
inline int parameterIndex(int engine, int page, int slot)
{
    return page == 0 ? (engine == 0 ? slot : (engine == 1 ? 24 : 32) + slot) : page * 8 + slot;
}
}
