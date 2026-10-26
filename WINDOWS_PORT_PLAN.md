# Windows implementation status and roadmap

## Implemented in preview 0.9

- Native Windows x64 standalone, VST3 instrument and CLAP instrument.
- Four independent sound tracks with assignable Synth/Metallic/FM engines,
  separate patches, voices, MIDI channels and insert chains.
- Pixel interface with two rows of four track tiles and one page visible at a time.
- Tracks 5–6 independent modulation curve editors with vertices, curvature, free/tempo sync,
  loop/one-shot, eight free native/insert routes and signed amounts per curve.
- Track 7 Digitakt I delay/reverb and eight hardware-track sends.
- Up to eight stable vertex IDs with individual X/Y/bend CCs; two vertex LFOs
  per curve with constrained effective geometry and preserved base points.
- Simple-synth Mono Legato/glide and standard sustain; native retrigger smoothing
  and a corrected high-cutoff filter stability bound.
- Track8 device-based mixer/recorder: detected input/output inventories, up to64 named mono/stereo device lanes including external inputs, four Companion stems, optional output mix and two CC lanes; stable physical channel bindings, background disk streaming and source manifest.
- Quarter-beat cycle lengths and independent /16–x16 speed multipliers on one RATE page; configurable live curve CC outputs.
- Fixed CC addresses independent of the visible page, separate curve channels,
  displayed CC/channel labels and two assignment pages per curve.
- Standalone clock-master/slave option, BPM/start/continue/stop controls and
  averaged incoming tempo with display hysteresis.
- Optional stereo return monitoring, driver-latency display and manual timing
  alignment; configurable per-control Digitakt display-to-wire correction.
- Standard Common Files and per-user VST3/CLAP browse shortcuts.
- Standalone VST3/CLAP instruments/inserts, parameter macros, saved plugin state
  and fitted owned floating editors.
- Per-track DAW automation and transport, project/session state, earlier Windows
  preview migration and missing-plugin state retention.

Build and package with `Windows/build.ps1`. Usage is in `Windows/README.md`;
completed checks and limitations are in `Windows/VALIDATION.md`.

## Project structure

The native C engine in `Audio/` is linked into the JUCE processor. Windows app,
plugin, UI, modulation, MIDI and hosting code live in `Windows/Source/`.
The standalone owns hardware device connections; DAW exports receive audio/MIDI
and tempo from their host. Dependencies remain pinned under `Windows/third_party/`.

## Next milestones

1. Physical Digitakt I MIDI, parameter-lock and ASIO validation; listening and
   sustained CPU/latency checks with four simultaneous tracks and inserts.
2. Broader third-party plugin/DAW testing, mixed-DPI editor movement and dynamic
   parameter-layout/latency handling.
3. Modulation extensions: note-triggered envelopes, arbitrary hosted source
   destinations and dedicated DAW automation for curve controls.
4. Pattern banks, sequencing, snapshots and broader mixer/routing features.
5. Optional crash isolation and broader routing/sidechain/multi-output support.

## Current boundaries

Metallic/FM are independent approximations. Modulation is evaluated per audio
buffer; track latency alignment is bounded to 192,000 samples. Plugin hosting
runs in process and supports initial mono/stereo main buses and parameter lists.
Nested hosting and hardware MIDI devices are standalone-only features.

Additional user ideas are in `FUTURE_IDEAS.md` (status marked for implemented ideas).
