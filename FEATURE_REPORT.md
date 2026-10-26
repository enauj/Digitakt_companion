# Digitakt Companion — Windows feature report

Version 0.9.0. Targets: Windows x64 standalone, VST3 instrument and CLAP instrument.

## Current features

| Area | Implementation |
| --- | --- |
| Tracks | Four independent sound tracks with separate voices, patches, engine assignments, MIDI channels and inserts; modulation on tracks 5–6, hardware FX on track 7, track 8 mixer and recorder |
| Engines | Synth, Metallic and Four-op FM assignable to each sound track; four-voice polyphony plus optional Synth mono-legato glide; 4 ms retrigger correction and SVF stability bound |
| Sound controls | Independent oscillator, filter, amp and per-engine parameter banks; actual function units/choices and incoming CC/channel labels |
| UI | Yellow/black 480 x 300 pixel canvas, original bitmap font, two rows of four home tiles, one page visible at a time |
| Modulation | Two independent editable curves with up to 8 stable CC-addressed vertices and outgoing curvature, free/tempo sync, loop/one-shot, eight free native/insert routes per curve, signed amounts with exact neutral centre; two vertex LFOs per curve with free/synced rates, wave/phase/smoothing; preserves base parameter values; quarter-beat sync and /16–x16 multipliers; common RATE page |
| MIDI | Per-track receive channels, fixed page-independent CCs for every function, separate curve channels, optional live modulation CC outputs and recorded CC lanes; incoming or generated 24-PPQN clock/start/continue/stop, steady BPM display and panic |
| Stereo return | Optional mono/stereo input monitoring, return gain and signed relative timing adjustment; reported device latencies; no automatic hardware calibration |
| Standalone hosting | Per-track Windows VST3/CLAP instrument and two serial inserts, macros, wet mix, bypass, saved plugin state, standard Common Files/user folder shortcuts and owned floating editors |
| Digitakt I controls | Delay/reverb parameters plus all eight hardware audio tracks' delay/reverb sends, configurable output/channels and per-control display-to-wire correction |
| Recorder / mixer | Up to64 named mono/stereo device lanes, four Companion stems and optional app mix; detected input/output inventory, external-input capture, stable physical assignments, two tempo-aware CC lanes and full source manifest; track/master/return and Digitakt I hardware faders |
| DAW builds | Four native sound tracks, modulation, per-track automation, host transport and full project state |
| State | Complete project/session save and restore, earlier Windows preview migration and missing-plugin state retention |

## Architecture

`Audio/AudioDSP.c` provides the native C synthesis library. The JUCE processor in
`Windows/Source/Processor.cpp` owns four DSP instances, MIDI routing, stereo mixing,
insert chains and modulation. The standalone adds audio/MIDI device management.
VST3 and CLAP exports share the processor and pixel editor, with device access
and nested third-party hosting reserved for standalone use.

## Validation and remaining work

See `Windows/VALIDATION.md` for actual tests and verification boundaries. The DSP,
legacy patch-state and plugin integration suites cover simultaneous tracks,
modulation, project migration, exported formats and floating editor lifecycle.

Metallic/FM are independent approximations. Physical Digitakt/ASIO operation,
listening quality, broad DAW/plugin compatibility and moving editors between
mixed-DPI displays, physical clock/audio alignment and firmware-specific control display mapping still need real-world checks. Modulation runs once per audio
buffer and targets native synth parameters and the eight insert knobs. Pattern banks, sequencing,
note-triggered envelopes and broader mixer/routing features remain future work.

Recorder implementation status and the remaining visual animation concepts are in
`FUTURE_IDEAS.md`. Recording setup is in `Windows/RECORDING.md`.
