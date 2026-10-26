# Fixed MIDI CC reference — preview 0.9

Every control has a fixed function address. Incoming MIDI never depends on the
selected page or track. The same CC parameter locks sound the same when you
visit another page. Program the new addresses on the Digitakt; the old eight
page-following CC assignments from 0.1–0.5 no longer drive the controls.

The screen writes the incoming **CC and MIDI channel under each knob**, including
both curve editors and the hardware FX pages. Right-click a track tile or its
page title to change that track's input channel. Keep channels distinct unless
you intentionally want one MIDI stream to control multiple tracks.

## Sound tracks 1–4

Standalone receive channels default to **9, 10, 11, 12**; DAW defaults to **1–4**.
Source engine banks retain separate fixed CCs, even when that engine is inactive.
Shared filter and amp functions keep the same addresses across all three engines.

| Knob | Synth OSC | Metallic OSC | FM OSC | FILT | AMP | FX |
| --- | --- | --- | --- | --- | --- | --- |
| A | 16 Shape 1 | 24 Tune | 40 Algorithm | 48 Cutoff | 56 Attack | 72 FX1 P1 |
| B | 17 Shape 2 | 25 Model | 41 Ratio A | 49 Resonance | 57 Decay | 73 FX1 P2 |
| C | 18 Tune 2 | 26 Fold | 42 Ratio B | 50 LP–BP–HP | 58 Sustain | 74 FX1 P3 |
| D | 19 OSC mix | 27 Sweep | 43 FM index | 51 Env amount | 59 Release | 75 FX1 P4 |
| E | 20 FM / Xmod | 28 Metal | 44 Decay | 52 Filter attack | 60 Level | 76 FX2 P1 |
| F | 21 Fold | 29 Feedback | 45 Feedback | 53 Filter decay | 61 Pan | 77 FX2 P2 |
| G | 22 Fine detune | 30 Color | 46 Ratio shift | 54 Filter sustain | 62 Velocity | 78 FX2 P3 |
| H | 23 Hard sync | 31 Transient | 47 Brightness | 55 Filter release | 63 Drive | 79 FX2 P4 |

FX controls operate in the standalone. Plugin-source PARAM macros use **80–87**.
Plugin MIX controls use **88–95**: input, FX1 wet, FX2 wet, output, pan, FX1 bypass,
FX2 bypass, receive channel. These remain fixed on every page as well.

The simple Synth engine adds **VOICE** controls: **CC70 Mode** (0–63 Poly,
64–127 Mono Legato), **CC71 Glide** (0–127 maps to `2*(value/127)^2` seconds).
These use the sound track's channel, work on every page, and save independently
for all four tracks. CC64 is the standard sustain pedal. Defaults are Poly and
glide off; glide acts on overlapping notes in Mono Legato.

CC payloads are MIDI 0–127. Displays convert them to function units/choices.
Algorithms have eight choices; metallic models have four; oscillator tune has
49 semitone choices; FM ratios have nine choices. Mixes/amounts, waveform and
filter morphs remain continuous. Insert values use the plugin's own text.

## Modulation curves (tracks 5 and 6)

Default channels are **13** for curve 1 and **14** for curve 2, independently of
which sound track was last selected. Each curve supports **up to eight vertices**,
each with a stable V1–V8 identity. Inserting/removing another vertex does not
renumber existing CCs or LFO destinations. A deleted slot can be reused by a new
vertex; LFOs targeting the deleted vertex are disconnected. X remains ordered and
the first/last vertices retain their X anchors. CCs for unused slots are ignored.
Each curve has **eight free routes**, shown
four at a time on **ASSIGN 1** and **ASSIGN 2**. Any route can target any sound
track and its native or insert parameters; several routes may use the same track.

| Page | A | B | C | D | E | F | G | H |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CURVE | 16 Speed | Selected Vn Bend | Selected Vn X | Selected Vn Y | — | — | — | — |
| SETUP | 24 Depth | 25 Loop | 26 Sync | 27 Bipolar | — | — | — | — |
| ASSIGN 1 | 40 R1 dest | 41 R2 dest | 42 R3 dest | 43 R4 dest | 44 R1 amount | 45 R2 amount | 46 R3 amount | 47 R4 amount |
| ASSIGN 2 | 48 R5 dest | 49 R6 dest | 50 R7 dest | 51 R8 dest | 52 R5 amount | 53 R6 amount | 54 R7 amount | 55 R8 amount |
| LFO 1 | 102 Speed | 103 Amount | 104 Wave | 105 Destination | 106 Phase | 107 Sync | 108 Smooth | 109 Active |
| LFO 2 | 110 Speed | 111 Amount | 112 Wave | 113 Destination | 114 Phase | 115 Sync | 116 Smooth | 117 Active |
| RATE | 16 Curve rate | 96 Curve mult | 102 LFO1 rate | 97 LFO1 mult | 110 LFO2 rate | 98 LFO2 mult | — | — |
| MIDI | 33 Live CC out | 34 Output CC | 35 Output channel | 36 Output min | 37 Output max | — | — | — |

Each vertex parameter has its own incoming CC on the curve's channel:

| Vertex ID | X | Y | Bend / Z |
| --- | --- | --- | --- |
| V1 | 56 | 72 | 88 |
| V2 | 57 | 73 | 89 |
| V3 | 58 | 74 | 90 |
| V4 | 59 | 75 | 91 |
| V5 | 60 | 76 | 92 |
| V6 | 61 | 77 | 93 |
| V7 | 62 | 78 | 94 |
| V8 | 63 | 79 | 95 |

These addresses ignore the UI selection. The graph labels stable vertex IDs;
selecting a vertex displays its exact X/Y/bend CCs above the graph and below the
knobs. The previous selected-point CC17/18/19 aliases are retired. Speed CC16,
Setup (except retired CC28), assignment-route CCs and channel defaults are retained. IDs describe
slots, so their order across the graph need not be numeric after inserting points.

Each curve owns two independent LFOs. Click an LFO's DEST knob to choose a vertex
and X, Y or outgoing Bend/Z. Amount is signed with MIDI64 exactly neutral. Free
base rate is 0.02–20 Hz; Sync selects base cycles of 0.25–32 beats in 0.25-beat steps. The RATE page adds an independent multiplier to the curve and each LFO.
Wave choices are sine, triangle, saw and square. Phase is 0–360 degrees; Smooth
is 0–100 ms. Active toggles the source. New LFOs are unassigned at zero amount.
LFO destinations use `round(CC value * 24 / 127) - 1`: −1 is Off; otherwise
`vertex ID * 3 + axis` with zero-based ID and axes X=0, Y=1, Bend=2. Inactive
vertex slots and anchored endpoint X targets have no effect.

LFOs change effective geometry without rewriting base points. Their offsets add,
then Y/bend are bounded and X is constrained to preserve vertex order. The graph
shows the base curve dimly and the effective curve brightly. R/CC30 restarts the
curve and its LFO phases. Updates remain once per audio buffer.

Amounts range **−100% … 0 … +100%**. MIDI **0 = −100%**, **64 = 0**, **127 = +100%**.
A dragged knob centre and double-click on an amount are also exact zero. New
routes start at zero. Negative amounts invert the modulation direction.

Additional fixed controls on each curve's channel:

- **CC29**: modulation bypass/active (0–63 off; 64–127 on), replacing the old Run
  knob. The CURVE footer is a clickable MOD ON/OFF control; no Run knob remains.
- **CC30**: restart when value is 64–127; keyboard R also restarts.
- **CC80–87**: target sound track for routes 1–8. Values 0–31 select T1,
  32–63 T2, 64–95 T3, 96–127 T4.
- Destination CCs select **Off or a parameter**, on that route's target track:
  destination ID = round(CC value × 48 / 127) − 1. −1 is Off; IDs 0–39 are
  native parameters in the order below; 40–43 are FX1 P1–P4; 44–47 FX2 P1–P4.
- **CC28** changes the editor selection in spatial order for hands-on editing;
  it does not redirect incoming per-vertex CCs. The displayed ID remains stable.
  Use each vertex's own CCs for parameter locks.

Native destination IDs: 0–7 Synth OSC A–H; 8–15 FILT A–H; 16–23 AMP A–H;
24–31 Metallic OSC A–H; 32–39 FM OSC A–H. GUI assignment menus show names and
track choices directly. Depth multiplies each signed route amount; offsets from
all routes/curves add before clamping once. Base knobs and saved patches remain
unchanged. Assignment-menu selection alone does not reset an amount.

## Digitakt I FX controls (track 7)

Companion's input channel defaults to **15**. Incoming controls stay mapped when
you leave this track. Hardware output channels are configured separately in
Utility: FX defaults to channel 16; hardware track sends default to channels 1–8.

| Page | Incoming Companion CC A–H | Outgoing Digitakt CC |
| --- | --- | --- |
| DELAY | 16–23 | 85–92 on FX output channel |
| REVERB | 24–31 | 24–31 on FX output channel |
| DLY SEND | 40–47 | 82 on respective hardware track channels |
| REV SEND | 48–55 | 83 on respective hardware track channels |

Only edits transmit hardware CCs. While audio is active, incoming locks retain
their buffer sample offsets through the MIDI output scheduler. GUI edits are
coalesced and emitted at the next audio block; without audio, the UI timer sends
them. There is no raw MIDI input echo and no automatic hardware state query.

The reported delay TIME discrepancy uses a **+1 display-to-wire correction**:
Companion display 41 sends CC value 42, to match the reported device display 41
rather than 40. This is a user-reported calibration, not a verified universal
Digitakt conversion. The resulting display range is 0–126. Other controls keep
zero correction. Utility offers a per-control −2…+2 correction; changing it sends
nothing and preserves the most recent wire value where representable. Actual
hardware/firmware behavior still needs confirmation. Earlier saved raw values
are migrated to corrected display values without transmission.

## Rate values and curve MIDI output (0.8)

RATE shows existing rate controls alongside new multipliers. Its rate CCs are
aliases for the same functions shown on CURVE/LFO pages, not new page-dependent
addresses. The three multipliers use distinct CC96/97/98 on the curve's input
channel. Multiplier index is `round(value * 14 / 127)` into:
`/16, /8, /6, /4, /3, /2, x1, x2, x3, x4, x5, x6, x8, x12, x16`.

When synced, the base cycle length is `(1 + value) / 4` beats: MIDI11 = 3 beats,
19 = 5 beats, 23 = 6 beats. Actual duration is base beats / multiplier. In free
mode, base Hz retains the existing exponential range and is multiplied instead.
Old saved sync rates are converted to equivalent quarter-beat values at x1.
Right-click a RATE knob on the RATE page to toggle that source's sync mode.

MIDI-page settings use the curve's input channel: CC33 values64–127 enable live
output; CC34 chooses output CC `round(value * 119 / 127)`; CC35 chooses output
channel `1 + round(value * 15 / 127)`; CC36/37 set min/max directly to value.
The outgoing CC uses the chosen OUTPUT channel, not the curve's input channel.
Defaults: curve1 CC20 / curve2 CC21, both output CH12, live OFF. Output is 50 Hz,
7 bit; unchanged live values are coalesced. Recording retains both lanes at
50 Hz even when live output is disabled. CC28 no longer selects a vertex.

## Track 8 mixer

Receive channel defaults to **16** and can be set with the track menu.
Incoming mixer CCs work on every page:

| Page | A | B | C | D | E | F | G | H |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| COMP MIX | 16 Companion1 | 17 Companion2 | 18 Companion3 | 19 Companion4 | 20 Device return | 21 Master | — | — |
| DT MIX | 24 DT1 | 25 DT2 | 26 DT3 | 27 DT4 | 28 DT5 | 29 DT6 | 30 DT7 | 31 DT8 |

Companion/master gain is value/127; Device return gain is 2*value/127. DT MIX forwards
value as **CC95** on that hardware track's configured channel, independently of
the Companion mixer input channel. Recording start/stop and folder/input selection
are explicit GUI actions and have no CC binding.

Recorder0.9 device lanes and monitor inputs are assigned by name in Track8 > REC > INPUTS.
These assignments have no CC binding; incoming/live curve CC maps are unchanged.
