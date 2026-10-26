# Retrigger clicks and glide — preview 0.7

Investigated on 25 October 2026 before changing the DSP.

## Is this a known synthesis issue?

Yes. A sudden change in waveform amplitude or oscillator phase can generate a
broadband transient. Resetting a sounding voice's envelope to zero, clearing a
filter's stored energy or stealing a voice can cause such a discontinuity.
Miller Puckette describes muting and switch-and-ramp as remedies in
[The Theory and Technique of Electronic Music, section 4.3](https://msp.ucsd.edu/techniques/v0.11/book-html/node61.html).
JUCE also demonstrates smoother voice releases and recommends smoothing stepped
frequency changes in its [MIDI synth tutorial](https://juce.com/tutorials/tutorial_synth_using_midi_input/)
and [sine synth tutorial](https://juce.com/tutorials/tutorial_sine_synth/).

Possible approaches include preserving phase/envelope state for legato notes,
briefly fading the replaced voice, overlapping a short old-voice tail with the
new one, or cancelling the discontinuity with a decaying correction. Overlap
costs additional active voices; fading before a reset can defer the new attack.

## What the app was doing

`Audio/AudioDSP.c` reset the whole Voice structure on a same-note retrigger or
voice steal. That cleared phases, filter integrators, amplitude and filter
envelopes immediately while their previous contribution could be nonzero.

A deterministic test captured the last output sample before each replacement
and the first sample after it, at 32 different note ages for every engine and
sample rate. The old 48 kHz peak boundary jumps were:

| Engine | Same-note retrigger | Voice steal | After correction, both |
| --- | --- | --- | --- |
| Synth | 0.469375968 | 0.483443618 | 0.000000000 |
| Metallic | 0.451338261 | 0.447881252 | 0.000000000 |
| Four-op FM | 0.488338262 | 0.557012737 | 0.000000000 |

Values are absolute digital sample differences on a full-scale ±1 scale.
All three engines also passed at 44.1 and 96 kHz; the regression tolerance is
1e-6. Original diagnostic output is kept in `build/click-before.txt`.

## Implemented correction

On an active voice reset/steal, the synth compares the new raw sum with its
previous output sum, cancels the difference at that sample, then ramps the
correction to zero over **4 ms**, before the existing output saturation and pan.
A later rapid retrigger starts from the corrected last sum. No extra rendering
voices, heap allocation, lookahead or note-on delay are introduced. Explicit
panic still clears everything immediately.

While testing glide with a clean high-cutoff tone, a second defect appeared:
the explicit state-variable filter could become unstable at low resonance.
For this integrator ordering the stability inequality is
`f² + 2*f*damping < 4`. The old fixed `f <= 0.95` bound does not always satisfy it.
The new bound is `min(0.95, 0.98*(sqrt(damping² + 4) - damping))`, retaining margin.
This is a code-derived stability fix, separate from the retrigger correction.
Very high cutoff/low-resonance patches can consequently sound different.

## Simple synth glide

The Synth engine has a VOICE page: **MODE** selects Poly or Mono Legato, and
**GLIDE** sets 0–2 seconds. Defaults remain Poly and glide off. Mono Legato uses
last-note priority and slides only between overlapping held notes; the first
note after silence starts at its own pitch. The envelope, oscillator phases and
filter state continue across a legato change. Releasing the latest note returns
to an older held note. Captured velocity changes are smoothed over 3 ms.

Pitch ramps linearly in semitones for the chosen time, giving a musical pitch
slide. A 100 ms A3→A4 glide measured about 236.9 Hz over its early window and
440.0 Hz after settling at all three sample rates. Tests also check note return,
zero glide, separate notes, sustain-pedal hold/release and clearing correction
tails with panic. Metallic/FM stay polyphonic; their retrigger correction applies.

## Limits of the result

This measures DSP samples before the audio device, not perceived loudness or
DAC output. A zero replacement-boundary jump does not prove that every patch is
free of clicks. Hard waveform/sync changes, abrupt parameter modulation, output
clipping and driver underruns can produce other artifacts. Listening on the
actual setup remains useful. The correction intentionally changes a few
milliseconds of the replacement transient; it is not intended to preserve the
previous hard-reset click as part of a patch's sound.
