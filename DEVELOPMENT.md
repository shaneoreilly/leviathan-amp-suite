# Developing Leviathan

Everything about building, testing and releasing Leviathan. For using the app, see the [README](README.md).

Leviathan is two native apps with the same features, presets and file formats:

- **macOS:** Swift and SwiftUI on CoreAudio, in `mac/`.
- **Windows:** C++ with [JUCE](https://juce.com), in `windows/`. It also builds on macOS, which is how it's tested alongside the Mac app.

The two apps share:
- **NAM:** both use the same Neural Amp Modeler bridge (`shared/nam/`).
- **Hover explanations:** both read `shared/help/explanations.json`.
- **Sound engine:** the C++ engine (`windows/Source/Dsp.cpp`) is a line-by-line port of `mac/Sources/DSP.swift`, and is checked against it.

## Build on macOS

You need the Xcode Command Line Tools (`xcode-select --install`); the full Xcode app isn't needed. The first build also fetches and compiles NeuralAmpModelerCore (pinned in `shared/nam/fetch_nam.sh`). This needs `git` and takes about half a minute.

```sh
git clone https://github.com/shaneoreilly/leviathan-amp-suite.git
cd leviathan-amp-suite/mac
./build.sh
open Leviathan.app
```

| Command | What it does |
| --- | --- |
| `./build.sh` | Development build for this Mac's architecture |
| `UNIVERSAL=1 ./build.sh` | Apple silicon + Intel |
| `scripts/make_dmg.sh 0.1.0` | Universal build packaged as `dist/Leviathan-0.1.0.dmg`, plus its SHA-256 |
| `tools/make_icon.sh` | Regenerate the app icon and README logo from `Sources/Logo.swift` |

**Microphone permission across rebuilds.** macOS ties privacy permissions to an app's code signature, and an ad-hoc signature changes with every build. So that you only grant the microphone once:
- On first run, `build.sh` creates a self-signed **"Leviathan Local Signing"** certificate and signs every build with it.
- The certificate lives in its own keychain at `~/Library/Application Support/Leviathan/Signing/`, not in your login keychain.
- To sign with your own certificate instead, set `LEVIATHAN_SIGN_IDENTITY="Developer ID Application: …"`.

**Test switches** (environment variables for the Mac app):
- `LEVIATHAN_NO_AUDIO=1` opens the interface without starting audio.
- `LEVIATHAN_SCREEN=settings|presets|recordings` opens on that screen.

Only one copy runs at a time per bundle identifier. To run a test copy next to your everyday one, change `CFBundleIdentifier` in the copy's `Info.plist` and re-sign it (`codesign -f -s - Copy.app`).

## Build on Windows

You need:
- Visual Studio 2022 or later, with "Desktop development with C++".
- CMake 3.22 or later.
- git.

CMake fetches JUCE and NeuralAmpModelerCore at pinned versions.

```sh
cd windows
cmake -B build -A x64 -DLEVIATHAN_ASIO_SDK=C:\path\to\ASIOSDK   # ASIO is optional
cmake --build build --config Release --target Leviathan
```

For ASIO, download the [Steinberg ASIO SDK](https://www.steinberg.net/asiosdk) (used under GPLv3) and pass its folder. The installer is built with Inno Setup from `windows/installer/Leviathan.iss`.

To build on macOS for testing, run `cmake -B build -G Ninja` and then `cmake --build build --target Leviathan`. `LEVIATHAN_NO_INPUT=1` runs it output-only, with no microphone prompt.

## Checks

- **`windows/tools/compare_with_swift.sh`** renders the same test signals through both engines and reports the difference for each scenario: amp, gate, overdrive, cab, pedals, pitch, NAM and metronome. Amp, gate and overdrive are bit-identical, and the others differ only by rounding (about 70 dB or more below the signal). Run it after any change to `DSP.swift` or `Dsp.cpp`.
- **`preset_check <dry.wav> <out-dir>`** renders a DI take through every factory preset and reports loudness and peaks. Use it to keep presets level-matched. `--dump` writes the factory presets as JSON, for comparison with the Mac app's.
- **`shared/nam/tools/nam_check.cpp`** compares Leviathan's NAM output with NAM's own renderer.

## Releasing

1. Bump the version in `mac/Info.plist` (`CFBundleShortVersionString` and `CFBundleVersion`) and in `windows/VERSION`.
2. Push a tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

3. GitHub Actions does the rest:
   - `.github/workflows/release.yml` builds the universal Mac DMG.
   - `windows.yml` builds the Windows installer. The ASIO SDK it downloads is checksum-pinned.
   - Both are published to a release titled "Leviathan - vX.Y.Z".

The in-app updaters read the latest GitHub release.

To make Mac releases that open without the "Open Anyway" step, add these repository secrets (they need an Apple Developer account):

- `MACOS_CERT_P12_BASE64` and `MACOS_CERT_PASSWORD`: your *Developer ID Application* certificate, exported as a `.p12` file.
- `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD`: used for notarization.

## Amp capture tools (experimental)

`mac/Capture/` models another amp or plugin as a black box (a Wiener–Hammerstein model plus a cabinet FIR). This predates NAM support. For high-gain amps, use NAM instead.

1. `python3 make_test.py` writes a test signal: sweeps, noise and synthetic guitar.
2. Run that signal through the amp you want to capture. Save the result to `~/Music/Leviathan/Capture/wet/<name>.wav`.
3. `python3 fit.py` fits the model and writes `~/Library/Application Support/Leviathan/Captures/<name>.json`.
4. In the app, choose the capture from the amp's model picker.

The tools need Python 3 with `numpy` and `scipy`.

## Engine notes

- **Signal chain:** input gain → gate → pitch → pedals (in chain order) → amp (built-in, NAM or capture) → graphic EQ → cabinet (FIR convolution, up to 4096 samples) → delay → reverb → master. A peak limiter at the end holds the output at −1 dBFS. The metronome and recording playback are mixed in after the limiter.
- **Built-in amp:** 4× oversampled.
- **NAM captures:** each one is measured when it loads and normalised to the same level (`LEV_NAM_TARGET_LEVEL`).
- **Long cabinet IRs** are cut to the convolution length with a 5 ms fade.
- **Input clipping** is detected on raw input samples at or above −0.1 dBFS.

## Project layout

```
mac/                     macOS app (Swift, SwiftUI, CoreAudio)
  Sources/
    App.swift            app, model, settings, factory presets, rig UI, hover help, scroll handling
    Shell.swift          sidebar, top bar, Presets / Recordings / Settings screens, grid layout
    Audio.swift          CoreAudio HAL engine (aggregate devices, latency, dropout handling)
    DSP.swift            all signal processing: gate, pitch, pedals, amp, NAM, EQ, cab, delay, reverb, limiter, metronome
    Nam.swift            NAM captures: folder, loading, hand-off to the audio thread
    Sessions.swift       recorder, recordings library, playback, re-amp, import, recovery, live waveform
    Midi.swift           MIDI learn, footswitch actions, Program Change, clock
    Tempo.swift          tempo, metronome, tap
    Updater.swift        self-update from GitHub releases
    PresetFiles.swift    .leviathan import / export
    Feedback.swift       Help & feedback, About
    IRLibrary.swift      God's Cab browser and the cab IR library
    Logo.swift           the logo as stroke geometry (in-app mark, icon, SVG)
  Capture/               amp-capture test signal + model fitter (Python)
  scripts/               sign.sh (stable signing), make_dmg.sh (release package)
  build.sh
windows/                 Windows app (C++, JUCE)
  Source/                Dsp.cpp (port of DSP.swift), Model, Settings, Sessions, Library, Midi, Updater, UI/
  installer/             Inno Setup script
  tools/                 engine comparison with the Swift engine, preset checker
shared/
  nam/                   Neural Amp Modeler bridge (lev_nam.h/.cpp), fetch/build scripts, bundled captures
  help/                  explanations.json: hover text for every control, used by both apps
.github/                 release workflows, issue forms
index.html, serve.sh     early browser prototype (Web Audio); run ./serve.sh
docs/                    logo, icon and screenshots for the README
```

## Licensing

Leviathan is AGPLv3 with the additional terms in [NOTICE.md](NOTICE.md). Third-party code:
- NeuralAmpModelerCore (MIT), Eigen (MPL-2.0), nlohmann/json (MIT).
- JUCE (AGPLv3) and the Steinberg ASIO SDK (GPLv3), for the Windows app.

The bundled NAM captures are GPLv3, credited in `shared/nam/captures/CREDITS.md`.
