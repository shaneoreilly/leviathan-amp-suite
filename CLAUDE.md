# CLAUDE.md

Guidance for AI coding agents (and humans) working in this repository. Read this first, then
[docs/HANDOFF.md](docs/HANDOFF.md) for the current state, open issues and history, and
[DEVELOPMENT.md](DEVELOPMENT.md) for build and release detail.

## What this is

**Leviathan:** a free, open-source (AGPLv3) guitar amp and effects rig for players. There are two native apps with the same features, presets and file formats:

| | macOS | Windows |
| --- | --- | --- |
| Code | `mac/` (Swift, SwiftUI, CoreAudio; no Xcode project) | `windows/` (C++20, JUCE 9.0.5, CMake) |
| Engine | `mac/Sources/DSP.swift` | `windows/Source/Dsp.cpp`, a line-by-line port |
| Build | `cd mac && ./build.sh` | `cd windows && cmake -B build -G Ninja && cmake --build build --target Leviathan` |

Shared by both apps:
- `shared/nam/`: the Neural Amp Modeler C bridge, with NeuralAmpModelerCore v0.6.0 pinned. The bundled `.nam` captures are GPLv3, credited in `CREDITS.md`.
- `shared/help/explanations.json`: the hover-help text for every control.

The README is for **non-technical players**. Keep it free of build and code detail, which belongs in DEVELOPMENT.md.

## Commands

```sh
# Mac app (also builds the NAM static lib on first run, cached in mac/.build-nam/<arch>/)
cd mac && ./build.sh                       # -> mac/Leviathan.app, signed with the stable local cert
UNIVERSAL=1 ./build.sh                     # arm64 + x86_64
scripts/make_dmg.sh X.Y.Z                  # release DMG in mac/dist/

# Windows app (builds and runs on macOS too, which is how it is tested here)
cd windows && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target Leviathan
LEVIATHAN_NO_INPUT=1 windows/build/Leviathan_artefacts/Release/Leviathan.app/Contents/MacOS/Leviathan

# Engine parity: Swift vs C++ on identical inputs. Run after ANY change to DSP.swift or Dsp.cpp.
windows/tools/compare_with_swift.sh        # every scenario must stay ~70 dB+ below signal (amp/gate/od: exact)

# Preset loudness/peaks over a DI take; --dump writes factory presets as JSON
windows/build/preset_check_artefacts/Release/preset_check <dry.wav> <out-dir>   # target: cmake --build windows/build --target preset_check
```

There is no unit-test suite. Verification means the parity script, `preset_check`, small `swiftc` harnesses (below), and screenshots of test copies.

## Rules that matter

1. **Never quit, restart or relaunch the user's running Leviathan without asking.** They may be recording; a restart once cut off a take. Check with `ps -axo pid,lstart,command | grep Leviathan.app/Contents/MacOS`.
2. **Keep both apps in step.** Every feature, preset, setting and file format exists on both platforms:
   - **Settings keys:** the Swift `Settings` struct (`App.swift`) and `fillDefaults` (`Settings.cpp`) use the same keys in the same order.
   - **Presets:** `FACTORY` lists exist in both `App.swift` and `Settings.cpp`. `.leviathan` preset files and `session.json` are shared.
   - **Hover text** is read from `shared/help/explanations.json`. Never hard-code it.
3. **Engine parity is checked, not assumed.** Change `DSP.swift` and `Dsp.cpp` together, in the same order of operations, then run `compare_with_swift.sh`. Fast math is enabled only for `Dsp.cpp`; the comparison build uses `-ffp-contract=off`.
4. **Real-time safety:** the audio callback (`DSP.process`) must not allocate, lock or touch the file system.
   - NAM models and IR buffers are handed over by pointer swap or double buffering.
   - Old objects are freed later on the main thread.
   - Follow the existing patterns: `NamHost.swap`, `retire()` in Sessions, `setIR` slots.
5. **Never decode stored settings strictly.** Presets, session.json, `.leviathan` files and the saved rig must go through `Settings.lenient` on the Mac or `Settings::fromJSON` on Windows. A strict `JSONDecoder().decode(Settings.self)` fails as soon as a new key exists. This hid recordings in 0.4.3–0.4.5 and dropped saved presets in 0.4.5, and the next save then overwrote them.
6. **Adding a setting:** add it to Swift `Settings` (with a default) and to `fillDefaults` in `Settings.cpp`. Old presets decode by merging over defaults (Mac) or filling defaults (Windows), so new keys are safe.
7. **Adding a hover explanation:** add a key to `explanations.json`. The Mac bundles it via `build.sh`, Windows via `juce_add_binary_data`.
8. **Commits** end with the `Co-Authored-By` / `Claude-Session` trailers your harness supplies. Push only when asked.
9. **Releases are user-visible.** Both apps auto-update from the latest GitHub release, so only tag when the user asks.

## Release (what actually happens)

1. Bump `mac/Info.plist` (`CFBundleShortVersionString` and `CFBundleVersion`) and `windows/VERSION`.
2. Commit, push, then `git tag vX.Y.Z && git push origin vX.Y.Z`.
3. `.github/workflows/release.yml` runs:
   - On `macos-15`: builds the universal DMG, signed with the stable certificate from the `LEVIATHAN_STABLE_P12_*` secrets.
   - Through `windows.yml` on `windows-2025`: builds the installer with ASIO SDK 2.3.4 (sha256-pinned) and Inno Setup.
   - On `ubuntu-latest`: publishes both to a release titled `Leviathan - vX.Y.Z`.
4. Replace the generated notes with written ones: `gh release edit vX.Y.Z --notes-file notes.md`. Write them for players, in the style of earlier releases.
5. If a build fails after tagging, fix it on `main` and rerun with `gh workflow run release.yml --ref main -f version=X.Y.Z`. Don't delete or move tags.
6. If the `dmg` job is cancelled with "not acquired by Runner… capacity constraints", GitHub's macOS arm64 runners are busy, as happened with 0.4.6. Two options:
   - Rerun later with `gh run rerun <id> --failed`.
   - Build it locally and publish by hand:
     1. Check HEAD is the tag.
     2. Run `mac/scripts/make_dmg.sh X.Y.Z`. It signs with the same stable certificate on the owner's Mac.
     3. Verify the build: `lipo -archs` should show both architectures, `codesign -d -r-` should show leaf `ac2ec283…`, and the bundle should hold 34 `.nam` files.
     4. Download the run's `windows-installer` artifact with `gh run download <id>`.
     5. Run `gh release create vX.Y.Z <dmg> <dmg.sha256> <exe> <exe.sha256> --title "Leviathan - vX.Y.Z" --notes-file … --verify-tag`.

**Signing (Mac):** releases must keep the same certificate ("Leviathan Local Signing", leaf `ac2ec283…`). macOS ties the microphone permission to it, so changing the certificate makes every user re-grant access. The Windows builds are unsigned.

## Testing the UI without disturbing the user

- **Run a separate Mac test copy.** Copy `mac/Leviathan.app`, set `CFBundleIdentifier` to `local.leviathan.uitest` with PlistBuddy, `codesign -f -s -` it, and launch with `LEVIATHAN_NO_AUDIO=1`. Add `LEVIATHAN_SCREEN=settings|presets|recordings` to open on another screen. Its separate bundle ID gives it separate settings; seed them with `defaults export local.leviathan.ampsuite - | defaults import local.leviathan.uitest -`.
- **Don't use the same bundle ID.** The single-instance guard makes a second copy with the user's bundle ID exit immediately.
- **The JUCE build** on macOS stores its settings in `~/Library/Application Support/Leviathan/Leviathan.settings`. Back that file up before editing it for a test, and restore it afterwards.
- **Screenshots:** `screencapture -l <windowID> -o -x out.png`, with window IDs from `CGWindowListCopyWindowInfo` filtered by the PID you launched. The display is often asleep; `-l` still works.
- **Stop only what you started.** Kill test copies by PID, not by name pattern.
- **Logic tests:** compile a small `swiftc` harness. Copy `mac/Sources/*.swift` with `@main` stripped, add a `Main.swift`, and link `-import-objc-header shared/nam/lev_nam.h` plus `-Xlinker -force_load -Xlinker mac/.build-nam/arm64/liblevnam.a -lc++`. Use copies of user data, never the originals.

## Where the user's data lives (never delete it without asking)

- **Recordings:** `~/Music/Leviathan/Sessions/<date>/`. Each folder holds `dry.wav`, `wet.wav`, `session.json`, and optionally `untrimmed/` and `wet-original.wav`.
- **App data:** `~/Library/Application Support/Leviathan/`. It holds `NAM/`, `IRs/GodsCab/{44.1,48}/`, `IRs/Library/`, `Captures/`, `Signing/` (the stable certificate's keychain and password), and `Leviathan.settings` (JUCE build).
- **Mac preferences:** `defaults read local.leviathan.ampsuite`.

## Code map (quick)

**Mac (`mac/Sources/`)**
- `App.swift`: `Settings`, `FACTORY` presets, the `Model` (state, audio start/stop, presets, NAM, IRs), the rig UI (panels, `Knob`, `UnitBox`, `Fader`), hover help (`EXPLAIN`, `HoverHelp`, `.explain()`) and `ScrollCatcher`.
- `Shell.swift`: root view, sidebar, top bar, Presets / Recordings / Settings screens, `GridFlow` layout (`RowBreakKey`), `WaveformView`, `TrimOverlay`.
- `Audio.swift`: the CoreAudio HAL engine (aggregate devices, latency).
- `DSP.swift`: the whole signal chain, plus `Params`, shared with the audio thread.
- `Sessions.swift`: recording, playback, re-amp, import, recovery, trim, `LiveWave`.
- `Nam.swift`, `IRLibrary.swift` (God's Cab plus `CabIRLibrary`), `Midi.swift`, `Tempo.swift`, `Updater.swift`, `PresetFiles.swift`, `Feedback.swift`, `Logo.swift`.

**Windows (`windows/Source/`)**
- The same split: `Model`, `Settings`, `Dsp`, `Sessions`, `Library`, `Midi`, `Updater`.
- `UI/Root.cpp` holds the screens, top bar and Recordings; `UI/Rig.cpp` the rig panels; `UI/Widgets.cpp` the knob, fader, `UnitBox` and `layoutGrid`.
