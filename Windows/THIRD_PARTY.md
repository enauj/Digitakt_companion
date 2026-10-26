# Third-party dependencies

- **JUCE 8.0.16-security**, commit
  `ea77444750de45e036e0279bb0291144bf8e3688`.
  Source: https://github.com/juce-framework/JUCE
  Used for Windows UI, audio devices and MIDI. License: AGPLv3 or a commercial
  JUCE licence. This local prototype does not establish commercial distribution
  rights. Choose and comply with the applicable licence before distribution.
  The full framework notice is in `third_party/JUCE/LICENSE.md` (also copied
  as `dist/JUCE-LICENSE.md`). Dependency-specific notices are retained in the
  JUCE source tree.
- **Steinberg ASIO headers**, bundled with JUCE in
  `modules/juce_audio_devices/native/asio`. Licensed under the Steinberg ASIO
  License or GPLv3; see the bundled `LICENSE.txt`. ASIO device drivers are
  supplied separately by the hardware vendor and are not bundled here.
- **Shared synthesizer**: uses this project's `Audio/AudioDSP.c`. The metallic
  and FM engines are independent prototype implementations. No DigiSophie or
  DT-FM upstream code was downloaded or incorporated for this Windows port.
- **clap-juce-extensions**, commit
  `7adee3a1bd4684d4caa5601100e364abccff4b4d`.
  Source: https://github.com/free-audio/clap-juce-extensions
  MIT-licensed adapter used to export the Companion CLAP instrument. The
  standalone CLAP host adapter is authored in `Source/ClapHost.cpp`; the export
  adapter does not provide CLAP hosting.
- **CLAP SDK 1.2.7**, commit `29ffcc273be7c7c651f6c9953b99e69700e2387a`,
  and **CLAP helpers**, commit `a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc`,
  are MIT-licensed submodules of the adapter. Their notices are retained in
  `third_party/clap-juce-extensions/clap-libs/` and copied with local packaging.
- **VST3 SDK**, bundled with JUCE under
  `modules/juce_audio_processors_headless/format_types/VST3_SDK`, MIT licence.
  Used for VST3 instrument export and hosting. VST2 is not included.

This directory does not assign a new licence to pre-existing project source.
