# Developing Leviathan

Everything about building, testing and releasing Leviathan. For using the app, see the [README](README.md). For the project's current state, open issues and the reasoning behind decisions, see [docs/HANDOFF.md](docs/HANDOFF.md). Agent guidance is in [CLAUDE.md](CLAUDE.md).

Leviathan is native apps with the same features, presets and file formats:

- **macOS:** Swift and SwiftUI on CoreAudio, in `mac/`.
- **Windows:** C++ with [JUCE](https://juce.com), in `windows/`. It also builds on macOS, which is how it's tested alongside the Mac app.
- **Android:** the same JUCE sources as Windows (`windows/Source`), built by `android/build.sh`. Android-only behaviour is behind `JUCE_ANDROID` (and `onAndroid` in `Library.h`).

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

## Build for Android

You need the Android SDK with platform 35, build-tools 35, NDK 28.1.13356709 and CMake 3.31.6 (`ANDROID_HOME`, default `~/Library/Android/sdk`), and JDK 17 or 21 (`JAVA_HOME`, default Homebrew's `openjdk@21`). JUCE and NeuralAmpModelerCore come from the Windows build's `windows/build/_deps` when present, otherwise they're cloned into `android/.deps`.

| Command | What it does |
| --- | --- |
| `android/build.sh` | Release build: `android/dist/Leviathan-X.Y.Z.apk` and its `.sha256` |
| `android/build.sh install` | The same, then installs and launches it on the connected phone (adb, USB or Wi-Fi) |
| `android/build.sh probe [install]` | The feasibility probe in `android/Probe`: round-trip latency, callback load, NAM CPU cost |

How it works:
- **Project:** JUCE's CMake API doesn't target Android, so `build.sh` builds JUCE's Projucer, writes a `.jucer` listing every file in `windows/Source`, the NAM sources, the bundled captures and `explanations.json`, and runs Gradle on the generated project. `android/Builds`, `android/.deps`, `android/dist` and the `.jucer` are generated (gitignored).
- **JUCE patches:** `build.sh` patches a copy of JUCE's modules (`android/.deps/JUCE-patched`, versioned by `patch_version`; the Windows build's JUCE is untouched):
  - The Oboe device drops input that piles up when the input runs on a faster clock than the output (a USB interface and the phone speaker), uses callbacks of exactly the buffer setting (Android's own burst can be 1 ms), double-buffers the output and turns on Oboe's performance hint.
  - Every `PopupMenu` (combo boxes and JUCE's audio-device lists included) can be handed to `levNativePopupMenu`, which `UI/NativeMenus.cpp` shows as a native Android list dialog.
- **Signing:** release builds are signed with Leviathan's permanent Android key, because Android only installs an update signed with the same key as the installed app. The key is `~/Library/Application Support/Leviathan/Signing/android-release.jks` with its password in `android-keystore-password` next to it (override with `LEV_ANDROID_KEYSTORE` and `LEV_ANDROID_KEYSTORE_PASSWORD`). Without it, `build.sh` warns and falls back to the machine's debug key; such a build can't update, or be updated by, a released one.
- **Differences from desktop** (all behind `JUCE_ANDROID` / `onAndroid`):
  - Full screen, landscape, the screen kept on; the back gesture returns to the rig and never closes the app.
  - It opens with the amp off, and off releases the audio device (`Model::stop` closes it; `start` reopens the saved setup). Leaving the app does the same, unless it's recording or live re-amping: the device's exclusive streams would otherwise starve other apps' audio.
  - The buffer starts at the lowest the phone offers and only changes when you change it (no automatic step-up; dropouts show a "Tap to resolve" banner).
  - Recordings live in the app's own Music folder (`Android/data/local.leviathan.ampsuite/files/Music/Leviathan`), unless a recordings folder is chosen. The bundled captures are unpacked from the app on first launch.
  - Recordings folder: the folder picker returns a document-tree link; `pickedDirectoryPath()` turns links into the phone's own storage (`com.android.externalstorage.documents`, internal or SD card) into ordinary paths and refuses anything else (Google Drive has no path). Recording there needs "All files access" (`MANAGE_EXTERNAL_STORAGE`, asked for only then; `hasAllFilesAccess()` / `requestAllFilesAccess()`). Without it the chosen folder counts as unavailable and takes go to the app's folder.
  - File pickers return `content://` links: `pickedFiles()`, `writePicked()` and `pickedFolder()` in `Library.cpp` copy them to and from ordinary files. "Open folder" and "Show in Explorer" are hidden; deleting a take asks, since there's no Recycle Bin.
  - Share (recordings) uses the system share sheet; "for messages" is AAC in an `.m4a` written by `writeM4A` (the phone's encoder, Leviathan's own MP4 writer).
  - Menus, drop-downs and name prompts are native Android dialogs; the top bar wraps to two rows and scrolls away with the rig.
- **Updates:** the in-app updater downloads the release's `.apk`, checks it against its `.sha256` and hands it to Android's installer through an androidx `FileProvider` (`android/res/xml/lev_update_paths.xml`). Android asks to confirm, and the first time to allow installs from Leviathan.

**Testing on a phone:** enable USB or Wi-Fi debugging and `adb` can install, launch and read the log (`adb logcat`). Android 17 relaunches an updated app by itself; `build.sh install` only launches it when it isn't already running (a second launch makes a JUCE app quit).

## Recordings folder

Both apps record into `Music/Leviathan/Sessions` unless Settings → Library → Recordings folder picks another one (Mac: `UserDefaults` `recordingsFolder`; JUCE: `recordingsFolder` in `Leviathan.settings`). Cloud drives work on desktop because their apps mount as folders. A chosen folder that isn't there at launch (the drive isn't running) isn't created: takes go to the default folder and the yellow bar says so. Moving copies each session folder, checks every file's size, then deletes the original, and deletes the old folder once nothing but system clutter (`.DS_Store`, `desktop.ini`, `Thumbs.db`) is left. Recording is refused while a move runs. Code: `Sessions::changeRoot` (C++) and `Sessions.changeRoot` (Swift).

## Diagnostic log

Settings → About → Debug log (`debugLog` setting; off by default) writes `Logs/Leviathan-<date>.log` next to the app data, one per launch, the last 5 kept, each capped at 2 MB: the system and CPU (with its vector extensions), the audio device and latency on every change, start/stop, NAM loads, every yellow-bar message and, every 2 s while running, callback load (average and peak), NAM's share of real time and dropouts. Code: `windows/Source/DebugLog.cpp` and `mac/Sources/DebugLog.swift` (same format). The engine only adds the time spent in `lev_nam_process` to `Params::namSeconds`; the model writes the lines on the message thread.

## Checks

- **`windows/tools/compare_with_swift.sh`** renders the same test signals through both engines and reports the difference for each scenario: amp, gate, overdrive, cab, pedals, pitch, NAM and metronome. Amp, gate and overdrive are bit-identical, and the others differ only by rounding (about 70 dB or more below the signal). Run it after any change to `DSP.swift` or `Dsp.cpp`.
- **`preset_check <dry.wav> <out-dir>`** renders a DI take through every factory preset and reports loudness and peaks. Use it to keep presets level-matched. `--dump` writes the factory presets as JSON, for comparison with the Mac app's.
- **`shared/nam/tools/nam_check.cpp`** compares Leviathan's NAM output with NAM's own renderer.

## Releasing

1. Bump the version in `mac/Info.plist` (`CFBundleShortVersionString` and `CFBundleVersion`) and in `windows/VERSION` (Android uses it too).
2. Push a tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

3. GitHub Actions does the rest:
   - `.github/workflows/release.yml` builds the universal Mac DMG.
   - `windows.yml` builds the Windows installer. The ASIO SDK it downloads is checksum-pinned.
   - `android.yml` builds the signed APK, with the key from the `LEVIATHAN_ANDROID_KEYSTORE_BASE64` and `LEVIATHAN_ANDROID_KEYSTORE_PASSWORD` secrets. A release fails without them rather than publish an APK that can't update the installed app.
   - All three are published to a release titled "Leviathan - vX.Y.Z".

The in-app updaters read the latest GitHub release.

### Mac signing and the microphone permission

macOS remembers the microphone permission per app identity *and* code signature. An ad-hoc signature changes with every build, so users would be asked again after every update, and again each time they switched between a release and a local build. To prevent that, `mac/scripts/sign.sh` picks a signature in this order:

1. **Developer ID**, when `LEVIATHAN_SIGN_IDENTITY` is set. In CI this comes from the `MACOS_CERT_*` secrets (see below).
2. **The project's stable self-signed certificate** ("Leviathan Local Signing"). In CI it's imported from the `LEVIATHAN_STABLE_P12_BASE64` and `LEVIATHAN_STABLE_P12_PASSWORD` secrets, which hold the same certificate that local builds on the maintainer's Mac use. So releases and local builds share one permission that survives updates.
3. **Locally:** the "Leviathan Local Signing" certificate in `~/Library/Application Support/Leviathan/Signing/`, created on first build.
4. **Ad-hoc:** only in CI when none of the above is available.

To rotate the stable certificate:
1. Export the identity from the signing keychain: `security export -k <keychain> -t identities -f pkcs12 -P <password> -o id.p12`. The keychain's password is in `keychain-password` next to it.
2. Store it with `base64 -i id.p12 | gh secret set LEVIATHAN_STABLE_P12_BASE64` and `gh secret set LEVIATHAN_STABLE_P12_PASSWORD`.
3. Delete `id.p12`.

Users are asked for the microphone once more after the first release with a new certificate.

### Notarized releases

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
    Sessions.swift       recorder, recordings library, playback, re-amp, import, recovery, trim, live waveform
    Midi.swift           MIDI learn, footswitch actions, Program Change, clock
    Tempo.swift          tempo, metronome, tap
    Updater.swift        self-update from GitHub releases
    PresetFiles.swift    .leviathan import / export
    Feedback.swift       bug reports and feature requests; Settings > About
    IRLibrary.swift      God's Cab browser and the cab IR library
    Logo.swift           the logo as stroke geometry (in-app mark, icon, SVG)
  Capture/               amp-capture test signal + model fitter (Python)
  scripts/               sign.sh (stable signing), make_dmg.sh (release package)
  build.sh
windows/                 Windows and Android app (C++, JUCE)
  Source/                Dsp.cpp (port of DSP.swift), Model, Settings, Sessions, Library, Midi, Updater, UI/ (NativeMenus: Android dialogs)
  installer/             Inno Setup script
  tools/                 engine comparison with the Swift engine, preset checker
android/                 Android build: build.sh (project generator, JUCE patches, signing), res/ (FileProvider paths), Probe/
shared/
  nam/                   Neural Amp Modeler bridge (lev_nam.h/.cpp), fetch/build scripts, bundled captures
  help/                  explanations.json: hover text for every control, used by both apps
.github/                 release workflows, issue forms
docs/                    logo, icon and screenshots for the README
```

## Licensing

Leviathan is AGPLv3 with the additional terms in [NOTICE.md](NOTICE.md). Third-party code:
- NeuralAmpModelerCore (MIT), Eigen (MPL-2.0), nlohmann/json (MIT).
- JUCE (AGPLv3) and the Steinberg ASIO SDK (GPLv3), for the Windows app.

The bundled NAM captures are GPLv3, credited in `shared/nam/captures/CREDITS.md`.
