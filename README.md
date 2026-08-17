# Pitch Panic VST3

Pitch Panic is a native VST3 pitch pedal built from the neighboring One Button example. It is a real-time dual-head granular shifter, not a UI-only mockup.

## What it does

- Whammy, dive-bomb, octave-up, octave-down, dual-octaver, harmony, and micro-detune modes
- Full expression treadle with independent heel/toe pitches, curve, fine tune, and chromatic locking
- Two pitch voices with independent shifts and levels
- Three interpolation qualities: linear, cubic, and eight-tap windowed sinc
- Adjustable grain window, pitch momentum, window shape, feedback, and buffer freeze
- Dry/wet routing, drive, gate, high-pass, low-pass, input/output trim, width, stereo detune, and output containment
- Envelope-driven and LFO-driven pedal motion
- Sample-accurate VST3 automation, project/preset state, 32/64-bit audio, mono/stereo buses, and click-smoothed bypass
- MIDI Expression CC11 and MIDI Pitch Bend map directly to the expression treadle

The editor intentionally puts every system on one dense, proportionally resizable panel. Every interactive control is backed by a host-automatable parameter and affects the DSP.

## Download

Tagged versions automatically publish two archives on the GitHub Releases page:

- `Pitch-Panic-macOS-universal.zip` (Apple Silicon and Intel)
- `Pitch-Panic-Windows-x64.zip`

Extract the archive and install `PitchPanic.vst3` in the standard VST3 directory for your platform.

## Build locally

Requirements: CMake 3.25+, Xcode Command Line Tools, Git, and a recursive Steinberg VST3 SDK checkout. From this directory:

```bash
./build_macos_arm.sh
```

The script reuses `../vst3sdk` when present, completes missing SDK submodules, builds the plug-in, runs Steinberg's VST3 validator as a post-build step, and runs the standalone DSP smoke test. The resulting bundle is printed at the end (normally under `build/VST3/Release/PitchPanic.vst3`).

To install it for the current user:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R "build/VST3/Release/PitchPanic.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

Rescan VST3 plug-ins in the DAW, insert **Pitch Panic**, and assign a physical expression pedal to CC11 for pedal-style operation.

For Windows x64, use a Developer PowerShell for Visual Studio 2022:

```powershell
.\build_windows_x64.ps1
```

The Windows script downloads the pinned VST3 SDK when needed, builds Release, runs the validator and smoke tests, and writes the plug-in under `build-windows\VST3\Release`.

## Automated releases

GitHub Actions builds and tests macOS universal and Windows x64 artifacts on every push and pull request. Push a semantic version tag to publish a release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

## DSP notes

The shifter continuously writes each channel into a 250 ms ring buffer. Two read heads per voice sweep the buffer at the requested pitch ratio and crossfade with complementary windows. Near unison the engine fades toward the live signal to avoid stationary dual-head comb filtering. Feedback and output paths are bounded, non-finite samples are intercepted, and all audio-path storage is allocated before processing.
