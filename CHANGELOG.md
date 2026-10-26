# Digitakt Companion — Windows changelog

## Preview 0.9

- Device-driven recorder inventory lists all detected input/output channel names and enabled states. No Digitakt-specific input layout is required.
- Up to64 named, armed mono/stereo device lanes, arbitrary L/R choices, All mono / All pairs allocation and explicit Enable assigned inputs. Includes external-input channels when exposed by the driver.
- Four Companion stems and two modulation CC lanes retained; optional final Companion mix captures post-master app output. Generic lane files and full source/device manifests.
- Saved device/channel bindings resist active-channel compaction, follow unique channel names on reorder and prevent silent reassignment to another device. Preview0.8 routes migrate from enabled-input ordinals.
- Independent device return monitor inputs; driver inventory updates at audio-device lifecycle changes. DT MIX/FX hardware mappings remain Digitakt I-specific.

## Preview 0.8

- Common RATE page for curve/LFO1/LFO2 base rates and independent /16–x16 speed multipliers; synchronized quarter-beat steps include 3, 5 and 6 beats. Old synchronized projects retain their durations.
- Removed redundant SETUP vertex selector and CC28.
- Curve MIDI pages with live output enable, CC/channel and min/max mapping. Exported VST3/CLAP now advertise MIDI output.
- Track 8 COMP MIX / DT MIX / REC: four Companion levels, DT return and smoothed master; eight hardware levels using CC95; recorder with up to 13 WAV stems and two tempo-aware modulation CC lanes.
- Explicit multi-input assignments and up to 64 device inputs; missing hardware sources are reported and omitted. Background disk streaming, shared sample count, incomplete-take reporting and safe device-change finalization.

## Preview 0.7

- Eight vertices per curve with stable IDs and individual X/Y/bend CCs independent of selection. Inserting/deleting other points preserves existing addresses; deleted-point LFO targets disconnect.
- Two independent vertex LFOs per curve, with sine/triangle/saw/square, free or tempo-synced rate, signed amount, phase, smoothing and bypass; added LFO1/2 pages and CCs.
- Base/effective curve display and saved vertex/LFO state. Older curves above eight points simplify with the full original point array retained for recovery.
- Simple Synth VOICE page with Poly/Mono Legato and 0–2 s glide; last-note priority, held-note return and standard sustain-pedal handling. New automation parameters append after previous ordinals.
- Investigated native retrigger clicks; added 4 ms switch-and-ramp correction to active voice reset/steal. Boundary-jump regressions pass at 44.1/48/96 kHz.
- Corrected the explicit SVF's stability bound at high cutoff/low resonance, found during glide tests. Added DSP_RETRIGGER_NOTES.md with sources, metrics and limits.
- Updated MIDI reference; selected-point CC17/18/19 aliases retire in favor of stable vertex addresses.

## Preview 0.6

- Replaced page-following MIDI controls with fixed addresses; sound parameter locks remain consistent on every UI page. Old CC assignments must be updated to the new reference.
- Dedicated input channels for each curve and hardware FX controls; all knob pages display their CC/channel addresses.
- Each curve now has eight freely targeted routes over ASSIGN 1/2. Amounts range from -100% through exact centre zero to +100%; old positive amounts migrate unchanged.
- Curve page uses Speed/Curve/X/Y; Depth moves to Setup with Loop/Sync/Bipolar/Vertex. Run knob removed; modulation bypass uses a footer button or dedicated CC.
- Adjustable per-control hardware display correction, with delay TIME defaulting to +1 based on the reported one-step discrepancy. No correction is assumed for other FX controls.
- Standalone stereo return monitoring/gain, driver latency display and signed manual relative timing alignment.
- Companion master BPM/start/stop/continue, sample-clock 24 PPQN with scheduled MIDI output, no raw input echo, and full-beat incoming BPM averaging/display deadband.
- Audio-active hardware MIDI locks retain their sample offsets; GUI edits are sent at the next audio block.
- Common Files and per-user VST3/CLAP browse shortcuts plus last-folder memory.
- Saved curve/routing/transport/calibration state and migration from prior Windows previews. Hardware synchronization and display mapping still require physical checks.

## Preview 0.5

- Track selection uses two rows of four tiles.
- Track 6 is a second independent curve editor; Digitakt FX moves to track 7 and track 8 stays empty.
- Both curves can modulate the eight insert parameters exposed on each sound track's FX page; offsets combine while base values and saved plugin states are retained.
- Function-aware knob values and steps: eight FM algorithms, four metallic models, pitches, ratios, switches, real times/frequencies and plugin-provided text.
- Live incoming CC and MIDI-channel labels below internal-synth knobs; added MIDI_CC_REFERENCE.md.
- Earlier eight-track projects migrate the FX selection/page; both curves save independently.
- Recorder, vertex LFO and animated interface ideas captured in FUTURE_IDEAS.md without implementation.

## Preview 0.4

- Eight tracks: four independent sound tracks, modulation on track 5, Digitakt effects on track 6 and two reserved tracks.
- Assign Synth, Metallic or FM to any sound track, with separate voices, patches, channels and inserts; optional standalone plugin sources remain available.
- Editable curved modulation envelope with up to 16 points, loop/one-shot, free playback, tempo sync, four destinations and depths.
- Modulation changes effective DSP values without overwriting base parameters or DAW automation.
- Per-track project state and migration from earlier Windows previews; track 1 retains previous automation IDs.
- Windows-only project cleanup: removed Swift application sources and Xcode project configuration; retained native C DSP required by Windows.

## Preview 0.3

- Owned floating VST3/CLAP editors with fitted client sizes, native CLAP child parenting and display-scale conversion.
- Independent two-slot insert chains for built-in sources, saved wet mix and bypass.
- Digitakt I delay/reverb and eight-track delay/reverb send pages, configurable MIDI output and channels.
- Hardware settings restore without transmitting; only control edits send CC messages.

## Preview 0.2

- Pixel interface with one eight-control page visible at a time.
- Standalone VST3/CLAP instrument hosting, serial effects and parameter macros.
- Companion VST3 and CLAP instrument exports, plugin/project state and integration tests.

## Preview 0.1

- Native Windows standalone, C synthesis engines, audio/MIDI setup, test note and patch state.
