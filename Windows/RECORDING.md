# Device-based mixer and recorder — Windows preview 0.9

Track 8 has COMP MIX, DT MIX and REC pages. Close the previous Companion before
opening the executable in dist/0.9.0; only one standalone instance runs.

## Allocate recorder lanes

1. Choose an audio driver/device in UTILITY > Audio / MIDI.
2. Open track 8 > REC > INPUTS. The panel detects the selected device's complete
   input/output names and displays which channels are enabled. Outputs are
   playback destinations. Recorder lanes capture device inputs, including driver
   loopback channels or external inputs when the driver exposes them.
3. Add named lanes, or use **All mono** / **All pairs** to replace the lane list
   with assignments to the detected inputs. These two buttons replace current
   device-lane assignments. Up to 64 device lanes can address physical input
   indices 1–64. Odd final inputs in All pairs become mono lanes.
4. For each lane, choose Mono or Stereo, select Input/Left and (for stereo) Right,
   and set its name. Stereo channels can be non-adjacent. The checkbox arms the
   lane; X removes it. A stereo lane needs both inputs available.
5. Click **Enable assigned inputs** to enable armed lanes' device channels and
   the selected monitor return channels. Disabled inputs remain visible in the
   list. Device changes finalize an active take, so assignments and enable
   controls are frozen while recording.
6. Optionally select **Record Companion mix**. Companion sound tracks 1–4 and both
   modulation CC lanes are always recorded. The monitor left/right selectors
   are independent of the recorder lane assignments; monitoring remains an
   explicit toggle in Routing / latency.
7. Use REC > FOLDER to choose a parent folder, then RECORD. STOP / SAVE flushes
   and finalizes a new timestamped take folder. Audio must be running/prepared.

No Digitakt channel layout is built into the recorder. To capture Digitakt I
main, eight internal tracks and external L/R, allocate lanes to those names if
reported by the selected driver. Elektron's Overbridge ASIO table lists Main L/R
at inputs1–2, tracks1–8 at3–10, and External In L/R at11–12. Use the detected list
as the source of truth; another device, USB mode or driver can expose a different
layout. Digitakt II and unrelated interfaces use the same generic lane editor.
The separate DT MIX and DT FX controls still use Digitakt I MIDI mappings.

[Elektron Overbridge ASIO channel tables](https://www.elektron.se/wp-content/uploads/2026/03/Overbridge-User-Manual_ENG_260304.pdf)

## Files and sources

| File | Content |
| --- | --- |
| 01-Lane name.wav … | Armed and available device lanes, mono or stereo as selected |
| Companion-1.wav … Companion-4.wav | Four stereo synth/plugin stems |
| Companion-Mix.wav | Optional final stereo app output, after master |
| Modulation.mid | Tempo plus one CC lane for each modulation curve |
| Recording.json | Device input/output inventory, assignments, availability and completion |

File names include a lane-number prefix, so duplicate names remain separate.
Windows-invalid filename characters are sanitized. Unavailable, disabled-input,
wrong-device and disarmed lanes are reported in Recording.json and omitted from
WAV output. Four Companion stems still exist when no hardware lanes are assigned.

WAVs are 24-bit PCM at the current sample rate. All streams use one callback start
and sample counter. Device lanes contain the assigned input streams directly,
before return monitoring gain/delay. Companion stems include inserts, track
levels and alignment but precede the shared master. Optional Companion mix
captures the final output including monitor return and master. It is the signal
sent to the enabled stereo output pair, not a recording of the interface's
physical DAC or all hardware outputs. To record an actual output return, select
an input/loopback channel exposed by the driver. Signals beyond full scale clip
in PCM files.

Both MIDI CC lanes record at50Hz even when live CC OUT is off. Their CC number,
channel, depth and min/max mapping follow each curve's MIDI page. The MIDI file
uses960PPQ with tempo changes and end markers matching the audio duration. It
does not record incoming notes or every incoming hardware MIDI message.

Recording.json stores the driver/device name, full input/output names, enabled
channel masks, per-lane saved names/indices, physical one-based channel numbers
and compact active-input positions. Zero indices mean unavailable. Disk writes
run on a background thread. FIFO overflow, missing active capture data or write
failure stops the take and marks it incomplete.

## Assignment persistence

Routes bind to the backend/device name and channel names/physical indices,
rather than positions in the enabled-input list. Disabling an earlier input
therefore does not shift later recordings. A unique channel name can follow a
channel reorder within the same device. Ambiguous renamed channels and other
devices stay unavailable until explicitly assigned. Returning to the same named
device restores its matching assignments. The driver/device name is the identity
available through the audio API; two physical units with identical driver names
cannot always be distinguished.

Preview0.8 projects migrate their old enabled-input positions when the current
device inventory is available. Check those migrated assignments against the
source device before recording. New project states use layout version7 and
include recorderRouting and independent monitor input choices.

## Mixer and timing

COMP MIX has Companion1–4 levels, device return gain and master. DT MIX controls
the eight Digitakt I hardware levels over CC95. It does not control generic
interface preamps or add individual local monitors for every input. Digitakt's
own pre/post-fader USB setting affects its captured streams.

Shared capture timing does not remove physical device/input latency. When
monitoring, choose a source excluding the playback return to avoid feedback.
A hardware main and individual stems can contain the same sound; decide which
to use in a DAW mix. Optional Companion mix also contains the Companion stems.
No Overbridge protocol integration or automatic physical latency calibration is
implemented. The DAW versions record Companion stems, optional app mix and MIDI;
standalone owns the actual device inventory and input assignments.

Automated checks use42 simulated input channels and26 output WAVs, including
external L/R, plus sparse enabled channels, arbitrary stereo pairs, mono lanes,
saved routes, older-state migration, changed-device protection and explicit
failure handling. Actual hardware and sustained disk performance remain to be
checked on the user's setup.
