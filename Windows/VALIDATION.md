# Windows preview 0.9 — validation

Validated on 26 October 2026 using Windows x64 Release, Visual Studio Build Tools
2026/MSVC19.50, Windows SDK10.0.26100 and CMake4.2.2. JUCE8.0.16-security and the
CLAP adapter/SDK remain pinned to the revisions in THIRD_PARTY.md.

## Automated checks

Release result: **4/4 suites passed**. The final package is copied from the
tested executable, CLAP file and complete VST3 bundle.

`ctest --test-dir build -C Release --output-on-failure` runs:

1. DSPAudioAndMidi: native C engines at44.1/48/96kHz, finite stereo audio, notes,
   envelopes, voices/stealing, velocity, MIDI channels, panic and validation.
2. PatchStateRoundTripAndValidation: factory/legacy patch schemas and typed
   parameter/range/CC validation.
3. PluginFormatsHostingStateAndPixelUI: actual exported VST3/CLAP load/render/
   state, four simultaneous independent tracks, per-track FX, native editor
   lifecycle/parenting/size, MIDI hardware maps, and the following regressions.

Preview0.6 tests cover:

- Unique fixed CC banks across native parameters, inserts, macros, mix and
  modulation pages. A parameter-lock groove renders identically while all
  tracks/pages are visited versus a fixed-page reference; filter/source/amp
  controls continue targeting the correct parameters off-page.
- Independent curve input channels; CCs continue addressing each curve when its
  editor is not selected. Exact negative/neutral/positive modulation endpoints,
  including MIDI64 and dragged centre, affect real DSP audio as expected.
- All eight routes can target the same track, summing before clamping; assignment
  page2 preserves arbitrary target track, insert parameter and negative amount.
  Invalid route targets/curves are rejected before partial patch application.
- Earlier four-route curves migrate positive amounts and original per-track
  destinations, with new routes unassigned at neutral zero. Earlier FX selection
  still migrates to track7.
- Both MSEGs reach either insert on all four tracks. Deterministic CLAP gain
  fixtures prove audio change, other-track isolation, base restoration and
  saved plugin state while modulated.
- Master clock emits exactly24 PPQN, Start/Stop/Continue and sample-positioned
  pulses with correct sample-time beat advancement. Incoming master-feedback
  commands are ignored; raw notes/CCs/clock are not echoed to the output.
- Source/BPM restore without automatic Start; changing a running master to an
  external-clock project emits Stop. Full-beat tempo averaging and held display
  reject alternating pulse-arrival jitter. No physical MIDI timing is assumed.
- Optional stereo input monitoring defaults off, preserves channels and gain,
  mixes to the shared output, and saves routing state. Negative return alignment
  and positive synth alignment each produce the requested sample delay.
- Hardware input CCs retain their sample positions at output. Exact delay
  correction (display41 -> wire42), MIDI-byte endpoints, per-control calibration,
  and state changes without unsolicited output. Hardware conversion is tested
  against the user-reported mapping, not an attached Digitakt.
- Existing engine choice counts/units, four-track audio sum, note routing,
  channel changes, DAW parameter IDs, missing-plugin state, wet/dry/serial FX,
  exported plugin editor sizing/ownership and no-CC-on-restore regressions.

Preview 0.7 additionally verifies:

- Eight-vertex capacity, stable IDs across insertion/removal, direct CCs independent
  of selected point/page/curve, uniqueness of all 24 vertex addresses and both LFO
  banks, and typed ID/LFO validation. Dense legacy curves retain a full original
  point backup while reducing to eight; the backup survives re-save.
- Both LFOs target X/Y/bend, can share a destination, and can be disabled separately.
  Opposing X offsets preserve order/anchors; sync follows transport and smoothing
  limits a square-wave edge. Base points and synth parameters are not overwritten.
- A vertex LFO changes the actual curve output and synth audio; disabling it
  restores the base result. Independent LFO state and six-page selection restore.
- Mono/glide page CCs, real time display, per-track settings and defaults for old
  patches. New parameters append at ordinal 196+, preserving previous IDs/order.
- 32 retrigger ages and voice steals for every engine at 44.1/48/96 kHz: first-sample
  boundary jumps fall below 1e-6 (printed as zero). See DSP_RETRIGGER_NOTES.md for
  measured pre-fix values and the scope of that metric.
- A 100 ms legato glide passes intermediate frequencies (~236.9 Hz early) and
  reaches 440 Hz; oscillator/envelope continuity, held-note return, separate-note
  pitch, zero glide, sustain pedal and panic under rapid stealing are checked at
  all three rates. The clean high-cutoff test also exercises the SVF stability fix.

## Visual review

Integration tests render the pixel editor to `previews/`: track grid, synth
pages, all eight modulation pages, stable vertex labels/effective geometry, the Synth
VOICE page, the second curve assignment page, inserts, host
chains, and hardware FX pages.
These images are reviewed for real value text, fixed CC/channel labels, curve
Speed/Curve/X/Y controls, Setup controls and two assignment tabs. Native utility
panels also generate tempo/master and routing/latency snapshots without opening
an audio device or sending MIDI.

Live desktop clicking is omitted per the user's preference to finish with
automated checks. Prior0.3/0.4 interactive evidence is historical: Surge XT CLAP
and Valhalla Supermassive VST3 editors fitted/scrolled correctly at200% scaling;
track selection, engine changes and curve dragging worked then. Current automated
editor tests still cover close/reopen, ownership, native child parenting and DPI
size conversion. Mixed-DPI monitor movement remains unchecked.

## Preview 0.8 recorder, rates and mixer checks

`RecorderRatesMixerAndMidi` verifies exact 3/5/6-beat periods, independent
curve/LFO speed multipliers, free-Hz multiplication, saved rates and migration
from the old eight sync choices without changing their duration. Fixed new CCs
control both curves while other tracks are selected. The unused SETUP vertex
selector and CC28 do nothing.

The suite verifies live MIDI output addressing, bipolar centre64, unchanged-value
coalescing and disabling output; mixer track levels, hardware CC95/channels,
off-page controls and state restoration without transmitting hardware values.
The master parameter is appended at ordinal204; previous automation IDs and
ordinals are retained (205 parameters total).

The original0.8 recorder checked13 stems. The current0.9 suite uses a42-input
simulated interface and reads26 stereo WAVs:21 freely assigned device pairs,
four Companion stems and optional post-master mix. It verifies51,200-sample
lengths, all42 channel values including external L/R, audible pre-master stems,
post-master silence after the gain ramp and two tempo-aware CC lanes. MIDI
endpoints still match audio through a120-to90BPM change. Missing hardware,
explicit FIFO overflow, recording before audio preparation and back-to-back
short takes remain covered.

Additional0.9 checks use sparse physical inputs0/4/7/11, non-adjacent stereo and
mono lanes. Disabling an earlier channel retains the selected physical audio.
Routes persist by device/channel identity, follow unique channel names when
reordered, and refuse another device at the same numeric indices. Invalid routes
are rejected before state application;0.8 enabled-input ordinal assignments
migrate using the active inventory. A64-lane plan round trips. Manifest and GUI
snapshots include complete input/output inventories, generic lane assignments,
mono/stereo selectors, external input choices and recording readiness. No actual
audio device is opened by these automated tests.

A first integration run overflowed the test process stack because the recorder's
8192-event queue was stored inline in each processor. It now uses preallocated
heap storage, and the complete integration suite passes. The recording audio
FIFO is also allocated before capture; file I/O stays on the disk worker.

## Hardware and compatibility boundaries

No physical Digitakt loopback, display readback, ASIO device, audio listening or
clock-output jitter measurement was performed. The optional stereo return and
relative adjustment support a routing experiment; they do not automatically
remove round-trip latency or guarantee beat alignment. The capture stream must
exclude playback when monitoring to avoid a feedback loop. Capture supports individually assigned Overbridge ASIO inputs; physical
Digitakt multichannel capture was not exercised and no Overbridge protocol
implementation is included. Delay calibration is
configurable because the reported one-step conversion is not verified on every
parameter or firmware. Hardware values are last edited values, not queried state.

Modulation updates once per audio buffer. Plugin hosting remains in process,
uses initial mono/stereo main buses/parameter lists and bounded192,000-sample
alignment. Broad third-party plugin/DAW compatibility, dynamic layout/latency
changes, sustained performance and native listening quality remain future checks.
Metallic/FM engines are independent approximations. Click corrections were
validated numerically; perceptual listening and device-specific artifacts remain
unmeasured. Very high-cutoff/low-resonance patches can sound different after the
filter stability correction.

The previous standalone executable was running and locked. The complete matched 0.9 package is in dist/0.9.0; the running app and its older package were left intact.
