# DTX Companion for Windows — preview 0.9

Open the packaged `Digitakt Companion.exe`. This release is in `dist/0.9.0/`
when the previous executable is running. Close the current Companion before
launching the new version (the app allows one instance). The standalone app and VST3/CLAP instrument
exports share a single-page yellow/black pixel interface. Home arranges the
tracks in two rows of four.

## Tracks and navigation

- **1–4:** independent Synth, Metallic or Four-op FM engines, each with four
  voices, its own patches, receive channel and insert chain. Defaults are
  Synth / Metallic / FM / Synth. Standalone can also use a VST3/CLAP source.
- **5–6:** two independent modulation curve editors with eight free routes each.
- **7:** Digitakt I delay/reverb and all eight hardware tracks' own sends.
- **8:** Companion/Digitakt mixer and multitrack recorder.

Right-click a sound tile, click its **...**, or right-click its page title to
assign an engine. Right-click tracks 1–8 to set their input channels. Yellow title
or Escape returns home. Space plays a test note on the last sound track; **!**
sends panic. LOAD/SAVE stores projects; Utility opens device/routing/transport
settings. All four sound tracks keep sounding while you edit other pages.

## Fixed MIDI addresses

**Incoming parameter locks no longer depend on the selected page.** Each function
has its own CC; numbers and channels appear beneath the knobs, including the
curve editors. Use [MIDI_CC_REFERENCE.md](MIDI_CC_REFERENCE.md) for the complete
map and update the Digitakt's CC assignments. Earlier eight page-following CC
assignments are retired; old patches still load their sound settings.

Standalone sound channels default to 9–12; DAW sound channels to 1–4. Curve
channels default to 13/14; Companion hardware-FX input to 15. Share channels only
for intentional layering. Synth OSC uses CC16–23, Metallic OSC24–31, FM40–47,
filter48–55, amp56–63 and inserts72–79. Source PARAM macros use80–87; MIX88–95.

Built-in OSC/FILT/AMP pages display DSP units and choices. Drag vertically; Shift
is fine adjustment. Double-click a sound parameter to reset it. Waveform/filter
type are continuous morphs; model/algorithm/tuning/ratios/sync are stepped.

## Modulation curve editors

Each editor has **CURVE / SETUP / ASGN1 / ASGN2 / LFO1 / LFO2 / RATE / MIDI**:

- CURVE: drag square vertices. Double-click empty graph space to add one (up to
  8 total, including endpoints); right-click an interior point to delete. Endpoints retain X=0/1 and
  interior points cannot cross. Knobs are **SPEED / CURVE / POINT X / POINT Y**.
- SETUP: **DEPTH / LOOP / SYNC / BIPOLAR**. Free rates are 0.05–12 Hz;
  synced cycle lengths are a quarter beat through 32 beats. Loop off holds the
  final point; R restarts. Sync requires playing transport.
- ASSIGN 1/2: click each upper destination knob to choose any sound track and
  native or insert parameter. Four routes appear per page, eight total per
  editor. Several routes may address the same track or parameter.
- Lower route amounts are **−100% / centre 0 / +100%**. MIDI64 and double-click
  restore exact zero. New route amounts start at zero; set an amount to hear it.
- The Run knob is removed. The clickable CURVE footer toggles **MOD ON/OFF**;
  CC29 also switches it, CC30 or R restarts. New curves default active.

Both curves keep running independently of the visible page. Depth, curve output
and signed route amount multiply; offsets sum before clamping. Native DSP and
standalone insert modulation preserve base knob/automation values and saved
plugin states. Timing remains once per audio buffer, not audio-rate modulation.
Vertices have stable **V1–V8 IDs** and their own X/Y/bend CCs. The graph labels
these IDs; the selected point's addresses appear above the graph and beneath
its knobs. Inserting/deleting another point does not redirect an existing ID.
New points use a free ID; deleting a point disconnects its LFO targets. See the
CC reference for all 24 vertex addresses on each curve's channel. Previous
selected-point CC17/18/19 aliases are retired; direct vertex CCs ignore selection.

**LFO1 / LFO2** are independent sources on each curve. Click DEST to choose a
vertex's X, Y or outgoing Bend/Z. Adjust signed Amount, Speed, waveform (sine,
triangle, saw, square), phase, sync, smoothing and Active. Free rates span
0.02–20 Hz; sync selects quarter-beat through 32-beat cycles in 0.25-beat steps, including 3, 5 and 6 beats. New LFOs start
unassigned at neutral zero. R/CC30 restarts their phases along with the curve.

The dim curve and numbered squares are base points for editing. The bright
curve shows the current result of LFO modulation. Base values remain editable
and saved unchanged. X stays ordered, endpoint X remains fixed, and Y/bend clamp
to valid ranges; an endpoint's outgoing bend has no effect when no segment
follows it. LFOs add when they share a destination. Points, stable IDs, bends,
channels, both LFOs, all routes and page choices save with the project.

## Rate multipliers and MIDI curve output

The **RATE** page groups each source with its own multiplier:
**CURVE RATE / CURVE MULT / LFO1 RATE / LFO1 MULT / LFO2 RATE / LFO2 MULT**.
Right-click a RATE knob on this page to switch that source between Hz and BPM
sync. Sync is also available on SETUP and the individual LFO pages.

Base synced durations are 0.25–32 beats in quarter-beat steps, so 3, 5 and 6
beats are exact choices. The speed multipliers are **/16, /8, /6, /4, /3, /2,
x1, x2, x3, x4, x5, x6, x8, x12, x16**. A 6-beat base with x2 completes a cycle
in 3 beats; /2 takes 12 beats. In free mode the multiplier scales Hz instead.
Base rate and multiplier remain separate, saved controls. Older synchronized
projects migrate their previous durations with x1 and retain their timing.
The redundant SETUP vertex selector and its CC28 are removed.

On each curve's **MIDI** page, choose **CC OUT / CC NUMBER / OUT CH / MIN VALUE /
MAX VALUE**. Live output defaults OFF. Enable it to send the resulting curve as
7-bit CC through the standalone's selected MIDI output, or through the DAW
plugin's MIDI output. Defaults are CC20 and CC21 on output channel 12; choose
addresses appropriate to the receiving device. This output channel is separate
from the curve's input channel. Updates are limited to 50 Hz and unchanged live
values are coalesced. Bipolar output maps -1/0/+1 to 0/64/127 before the selected
min/max range; depth is included. Reversing min/max inverts the output.

## Track 8 mixer and recorder

**COMP MIX** controls Companion 1–4 levels, the device stereo return gain and
the shared master. Return monitoring remains an explicit Routing / latency
setting. Companion levels are the same saved levels used by each track's MIX
control; the master is a separate, smoothed output gain.

**DT MIX** edits all eight Digitakt track levels using CC95 on the hardware track
channels configured under Digitakt I FX / track sends. Unset values display --;
Companion does not query the hardware. Configure the MIDI output and enable
Digitakt RECEIVE CC/NRPN first. These are hardware faders, not local per-input
monitor gains; the Digitakt's pre/post-fader USB setting determines what changes
in its individual capture streams.

**REC** has RECORD/STOP, FOLDER and INPUTS. The recorder detects the selected
interface's input/output channel names, then lets you allocate up to64 named
mono/stereo device lanes. Use Add lane, All mono or All pairs; assign left/right
channels, arm lanes and click Enable assigned inputs. External inputs, main
returns, internal tracks and driver loopback inputs can all be selected when
exposed by the device. No Digitakt-specific input layout is assumed.

Companion1–4 stereo stems and two modulation CC lanes always record. An optional
Companion mix captures the final app output. Device outputs are listed as
playback destinations; actual hardware output/loopback capture requires a driver
input. Each available lane gets its own24-bit WAV, alongside Modulation.mid and
Recording.json. All files share a start and sample count. Device lanes are taken
before monitoring; Companion stems include effects/track levels/alignment and
precede master, while the optional mix is post master.

Select any supported audio interface in Audio / MIDI, then allocate its exposed
inputs. The Digitakt I Overbridge ASIO table includes main L/R, eight internal
tracks and external L/R; other devices/modes can expose different layouts.
The same recorder supports Digitakt II or other devices without special presets.
Driver channel exposure determines what can be recorded. DT MIX / DT FX controls
remain Digitakt I-specific MIDI controls.

Assignments save device/channel names and physical indices, so disabling an
earlier input does not redirect a lane. Missing, disabled or wrong-device lanes
are listed as unavailable and omitted. Device changes safely finalize a take;
background disk writes and a shared FIFO retain the existing recorder behavior.
Monitor left/right are now independent device input selectors. See
[RECORDING.md](RECORDING.md) for complete setup, migration and timing boundaries.

## Simple synth voice and glide

The simple Synth engine adds a **VOICE** page: choose **Mono Legato** with MODE,
set GLIDE time (0–2 seconds), and overlap held notes. Pitch slides to the latest
note while oscillator phase, envelopes and filter state continue. Releasing the
latest note returns to an older held note. Separate notes start at their own
pitch; Poly remains the default. CC70 controls Mode and CC71 Glide on the sound
track's channel, independently of the visible page. The standard sustain pedal
(CC64) holds/release-latches keys as well. Metallic/FM retain polyphonic behavior.

All native engines now use a 4 ms switch-and-ramp correction on active voice
retrigger/steal, removing the measured reset-boundary step without adding note
latency. A high-cutoff/low-resonance filter stability bound was also corrected.
Research, before/after measurements and limitations are in
[DSP_RETRIGGER_NOTES.md](DSP_RETRIGGER_NOTES.md). Listening on the actual setup is
still useful; other sources of clicks are not ruled out by the numerical test.

## Tempo and transport

Click the home BPM/play area, press **T**, or open Utility > Tempo / transport.

- **Digitakt source** follows incoming MIDI clock/start/continue/stop. Tempo is
  estimated across a full beat; the BPM display refreshes at most four times per
  second with a small deadband to suppress pulse jitter.
- **Companion source** provides a 20–300 BPM control plus Start/Restart, Continue
  and Stop. It generates 24-PPQN clock from the audio timeline and schedules it
  through the selected MIDI output's background thread. Enable the DT's clock
  and transport receive, and use one master. Clock can continue while stopped
  so the receiver retains tempo. Incoming clock/transport is ignored in master
  mode and never echoed.
- Choose the Digitakt output in Utility > Digitakt I FX / track sends. The audio
  device must be running to generate clock. Source and BPM save; restoring a
  project does not automatically issue Start/Continue. Device changes stop the
  active master transport on the next running audio block.
- DAW plugin builds follow their host's BPM and transport instead.

## Stereo return and relative latency

Utility > **Audio / MIDI** now allows up to two input channels. Select the audio
input carrying the Digitakt's stereo mix and the output you listen to, enable the
input channels, then open **Routing / latency** and enable return monitoring.
Monitoring defaults off; return level and routing settings save with sessions.

This adds the selected stereo input to Companion's four sound tracks. Driver
buffer size, reported input/output latency and return peak are displayed. A
**−250…+250 ms** relative adjustment delays the Companion synths for positive
values, or the hardware return for negative values. Insert-chain alignment is
also applied to the return. Use a sharp click from both sources to measure the
relative offset and adjust; this cannot erase the device's round-trip latency.

For a USB-return experiment, the Digitakt I settings are **USB AUDIO/MIDI**,
**INT TO MAIN OFF**, and **USB TO MAIN 0 dB**; listen to the Digitakt output after
routing its capture stream through Companion. Verify that the selected capture
stream excludes USB playback before enabling monitoring to avoid feedback.
The settings are described in the [Digitakt I manual, sections 15.7 and 15.9.1](https://www.elektron.se/wp-content/uploads/2025/07/Digitakt-User-Manual_ENG_OS1.52A_250708.pdf).
This setup is an option to test on the hardware, not a measured synchronization
fix. Separate hardware streams are captured through assigned Overbridge ASIO inputs on track 8; the app does not implement the Overbridge protocol itself. A low driver buffer and manual timing measurement are still relevant.

## VST3 / CLAP instruments and inserts

In standalone, choose a VST3/CLAP instrument on any sound track. CHAIN contains
source, FX1 and FX2. PARAM has eight assignable macros; right-click to choose a
source or insert parameter. MIX has input/wet/output/pan/bypass/channel controls.
Built-in tracks expose their two serial inserts on FX: first four parameters
of FX1 above, first four of FX2 below. Click a row's name to load/open/remove an
insert, change wet mix or bypass it. Select an entire `.vst3` bundle directory.

The slot menu has direct browse shortcuts to **Common Files/VST3** and **Common
Files/CLAP**, their per-user `LOCALAPPDATA/Programs/Common/` equivalents, and the
bundled plugins. Missing directories are disabled. Browse remembers the last
folder for the current editor. No scan or system-wide install is performed.

Editors use fitted owned floating windows; oversized fixed editors scroll within
the display. Notes are filtered by each track's channel and normalized to channel
1 inside hosted sources. Mono/stereo main buses, serial wet/dry and latency
alignment (up to 192,000 samples) are supported. Plugins run in process. Nested
hosting, hardware devices and return monitoring are standalone-only.

## Digitakt I FX and sends

Track 7 pages are DELAY / REVERB / DLY SEND / REV SEND. They control hardware
parameters and hardware track sends. Choose the MIDI output in Utility, enable
Receive CC/NRPN on the DT, and match its FX and track channels (output defaults
16 and 1–8). Input mapping is separate: Companion listens on channel15 by default.
Only edits send CCs; loading or reconnecting does not replay stored values.

Delay TIME has a configurable **+1 display-to-wire correction** for the reported
one-step discrepancy; other controls default to zero. The calibration panel in
Utility can change any control's correction from −2 to +2 without sending it.
The MIDI payload stays within0–127, so correction can limit the display range.
This is based on the reported device behavior and needs confirmation on the DT;
it is not assumed to be a universal conversion for every parameter/firmware.
`--` means no value was set here; displayed settings are not read back from hardware.

## Projects, builds and boundaries

Current projects save all four tracks (including glide), two eight-route curves
with stable vertex IDs and two LFOs each, channels, pages,
insert states, hardware calibration, transport source/BPM and return settings.
0.6 and earlier curves with up to eight points retain their geometry and receive
stable IDs. Larger curves are simplified to eight while keeping their endpoints;
the complete pre-limit point array is retained in the saved curve's
`originalPoints` field for recovery. The original project file is not rewritten
by loading. New LFOs are unassigned, and older voices restore as Poly/glide off.
0.4 moves its FX selection to track7; 0.4/0.5 four-route curves retain their track
assignments and positive amounts, with four additional unassigned neutral routes.
0.1–0.3 shared patches migrate to independent sound tracks. Track1 retains the
previous DAW automation IDs; other tracks use their own prefixes. Missing plugin
states are retained for later restore. Session state is saved on normal exit at
`%APPDATA%/DigitaktCompanion/windows-session.json`.

Use `build.ps1` to build/test/package the standalone, complete VST3 bundle and
CLAP file in `dist/`. To package beside a running version, use
`build.ps1 -PackageDirectory dist/0.9.0`. DSP-only builds use `build.ps1 -DSPOnly`. Pinned dependencies
and license notices are in THIRD_PARTY.md; exact checks are in VALIDATION.md.
Physical MIDI/audio loopback timing, firmware-specific display conversion,
listening quality, ASIO and broad DAW/plugin compatibility need real-hardware
checks. Implementation status and broader animated design ideas remain in
`FUTURE_IDEAS.md` (source copy at `../FUTURE_IDEAS.md`). Track8 is the mixer/recorder.
