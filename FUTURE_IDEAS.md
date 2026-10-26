# Future ideas and implementation status

Captured on 25 October 2026; status updated 26 October 2026. The broader visual
redesign remains a future idea. Vertex LFOs are implemented in preview 0.7, and
the track8 mixer/recorder and modulation CC output in preview0.8, generalized to device-based recording in0.9.

## Track 8 multitrack recorder — implemented in preview 0.8

The recorder now detects the selected audio device's input/output inventory and
supports up to64 named mono/stereo input lanes, including external inputs when
exposed, four Companion stems, optional final app mix and both CC lanes. Shared
capture timing/background disk streaming remain; source bindings persist by
device/channel identity. See `Windows/RECORDING.md` for boundaries. This includes
the original eight-track hardware proposal without fixing the layout to Digitakt.

## Vertex modulation — implemented in preview 0.7

Two independent LFOs per curve can target stable vertex X, Y or outgoing Bend/Z,
with free/synced rate, signed amount, waveform, phase and smoothing. Effective
geometry is displayed while base points remain editable. The notes below record
the original proposal; a separate third spatial/depth dimension is not added.

- Add a new page for LFO modulation of individual vertices' X, Y and Z controls.
- Define Z before implementation: it could represent outgoing curve/tension or
  a separate depth dimension. The present editor has X, Y and outgoing bend.
- Consider per-vertex rates, waveforms, phase, depth, sync and destination enables.
- Keep X ordered, retain endpoint constraints, and keep Y/Z within their ranges.
- Animate the effective vertex positions while preserving the editable base
  curve and saving the LFO configuration independently for curves 1 and 2.

## Fresher interface with parameter animations

- Keep the single-page workflow and recognizable pixel/hardware character, while
  refreshing hierarchy, spacing and visual feedback.
- Supplement knobs with graphics showing the function they control, inspired by
  newer Elektron interfaces: waveforms, FM algorithm topology, filter response,
  envelopes, modulation motion, pan and FX activity.
- Show the current action of a parameter, including modulation, instead of
  relying exclusively on a knob and number.
- Explore readable animation timing, reduced-motion controls and display scaling.
- Prototype visual alternatives before changing the production design. No such
  animation redesign is part of the current changes.
