# Leviathan: handoff notes

The state of the project, for whoever picks it up next. For the current version, see the latest GitHub release (`gh release view`).
- [CLAUDE.md](../CLAUDE.md): working rules and commands.
- [DEVELOPMENT.md](../DEVELOPMENT.md): build and release.
- [README.md](../README.md): the player-facing description.

## Where things stand

- **Released:** every release carries both installers, and auto-update works on both platforms. `v0.2.0` is the one tag with no release: its CI failed and 0.2.1 superseded it.
- **CI:** `ci.yml` builds the Mac app and runs the engine parity check on every push to `main` and on pull requests; `windows.yml` builds the Windows installer when `windows/` or `shared/` change.
- **Secrets:** `LEVIATHAN_STABLE_P12_BASE64` and `LEVIATHAN_STABLE_P12_PASSWORD` hold the Mac signing certificate. The Apple Developer ID and notarization secrets (`MACOS_CERT_*`, `APPLE_*`) are *not* set.
- **Owner's setup:**
  - Interface and outputs: an iRig HD into a Mac (Apple silicon), listening through an HDMI monitor (H27P22) or headphones.
  - Daily copy: the latest release, in `/Applications`. A development build also lives in `mac/Leviathan.app`.
  - Bundled NAM captures: 34, mostly metal, all from pelennor2170/NAM_models at the pinned commit and credited in `shared/nam/captures/CREDITS.md`.
  - Data: real recordings back to 2018 in `~/Music/Leviathan/Sessions`, and a 194-IR Tone3000 "METAL BAND TONES" pack in `IRs/Library/`.

## Not yet verified

These are built and reasoned through, but nobody has tried them on real hardware or on screen.

1. **The Windows app on a real Windows PC.** It's built and tested on macOS through JUCE, and CI builds the installer, but nobody has run it on Windows. Untested there:
   - ASIO and WASAPI audio.
   - MIDI.
   - NAM loading.
   - The silent self-update.
   - The Recycle Bin and folder-import dialogs.
   - The Windows-only code paths: `Updater.cpp` installer launch and the `%APPDATA%` paths.
2. **Mac features never tested with real audio:**
   - **Interaction:** scroll-to-turn on dials (`ScrollCatcher`), and dragging the trim handles.
   - **Recording and playback:** the live recording waveform, playback starting audio automatically, and the new metronome click on the owner's monitor speakers. Testing these would have meant restarting the owner's app.
3. **Windows trim:** `Sessions::trimFiles` builds but hasn't been run. The Mac version was tested on a copy of a real 96 s take: exact slice, backup kept.
4. **The microphone fix (0.4.4)** is verified by signature: the release DMG's designated requirement equals the dev build's. The owner hasn't yet confirmed that the prompt stopped.

## Known issues and rough edges

- **Metronome audibility.** The owner reported "the metronome doesn't work". The engine renders it correctly (offline test and beat lights). The likely cause is HDMI monitor speakers muting short sounds, so 0.4.3 made the click longer. If it still isn't heard, try routing through headphones, raising the level, or a short pre-roll noise floor.
- **The owner's interface was clipping its input** (iRig HD gain too high). This made every tone sound "like FM radio". Leviathan now warns on input clipping. If tone complaints return, check this first.
- **NAM sample-rate mismatch:** a capture trained at a different sample rate plays with a warning but isn't resampled.
- **Long IRs** are cut at 4096 samples (`DSP.maxIR`), with a fade.
- **Delay wrapping:** in the default layout, Delay wraps onto its own row at common window widths. It wasn't in the owner's requested order.
- **The Python capture fitter** (`mac/Capture/`) predates NAM, which gives better results for high-gain amps. Both apps still load the models it writes, so it stays until that feature is removed.

## Backlog (owner's wishes and sensible next steps)

**Most value:**
1. Test on a real Windows PC (see "Not yet verified").

**Features players expect:**
2. A plugin version (VST3/AU). JUCE makes the Windows side straightforward; the Mac app would need the JUCE build or an AU wrapper.
3. Stereo output (a doubler or room). Currently the rig is mono until the reverb.
4. A looper or backing-track player. The engine already has a backing slot (`bk*` in `Params`).
5. Undo, and A/B comparison of settings.
6. Resampling NAM captures recorded at other sample rates.

**Distribution:**
7. An Apple Developer ID and notarization (removes "Open Anyway"; CI is already wired for the `MACOS_CERT_*` / `APPLE_*` secrets).
8. Windows code signing (removes SmartScreen).

## Decisions and why

- **Two native apps rather than one cross-platform UI.**
  - The Mac app came first, in SwiftUI.
  - Windows was added with JUCE for low-latency ASIO and full feature parity.
  - The engines are kept identical by a parity script, not by sharing code. Only NAM is shared C++.
- **NAM (NeuralAmpModelerCore) for "Kemper-grade" amps.**
  - The owner wanted real-amp tones; tone-matching services such as Tone3000/ToneAdapt have no usable API.
  - NAM is open source and matches its reference renderer bit-for-bit.
  - Captures are level-normalised at load to −13 dB (`LEV_NAM_TARGET_LEVEL`).
- **License: AGPLv3, with section 7 additional terms in NOTICE.md.** The owner wants it free and credited, and wants derivatives to stay open source. That means attribution to "Leviathan by Shane O'Reilly", plus marking of modified versions.
- **Output limiter (−1 dBFS, 80 ms release), not hard clipping.** Hard clipping at a hot Master caused the crackle complaints.
- **Single instance per app.** Two copies fought over the audio interface and shared one update-check timestamp, which hid an update banner.
- **Updates:**
  - Checks at every launch and every 6 hours.
  - A banner across the top, not just a dot.
  - The Mac self-updater replaces the app bundle; the Windows one runs the Inno installer silently.
- **Stable Mac signing certificate in CI.** Ad-hoc signatures made macOS re-ask for the microphone after every update, and when switching between dev and release copies. The certificate is self-signed, so it isn't trusted by Gatekeeper; it only keeps the identity stable.
- **Trim keeps `untrimmed/`.** Recordings are irreplaceable, so trimming is always reversible.
- **Recording recovery.** Takes cut off by a quit or crash are rebuilt at startup, by fixing the WAV headers and writing `session.json`. This exists because a restart once lost a take.
- **README for players; DEVELOPMENT.md for builders.** This was the owner's explicit request.

## History (condensed)

| Version | Highlights |
| --- | --- |
| 0.1.x | Mac app: rig, presets, recording and re-amp, MIDI, tempo and metronome, preset files, self-update |
| 0.2.1 | Layout grid, drag and drop, collapse; CI fix (Swift type-check timeout) |
| 0.3.0 | Windows app (JUCE) at parity; AGPL license; bug and feature forms |
| 0.4.0 | NAM with 11 bundled captures; 21 new presets; output limiter; input-clip warning |
| 0.4.1 | Settings navigation; delete and recover recordings; folder import; dates from file names; tuning notes |
| 0.4.2 | Update banner, check every launch, single instance |
| 0.4.3 | Cab IR library; two-row layout; input picker; scroll and reset on dials; hover explanations; Save as; live recording waveform; metronome click; seek fixes |
| 0.4.4 | Stable Mac signing (microphone permission sticks); trim recordings |
| 0.4.5 | Hidden-recordings fix (lenient session.json); seek while playing; no-input playback; no built-in mic fallback; Space plays on Recordings; Blade pedal (Longsword-inspired distortion, 9th pedal, maxChain 9) |
| 0.4.6 | Saved presets load after updates (lenient decode; unreadable ones kept); 23 more NAM captures (34 total); Presets columns NAME / MODIFIED / LAST PLAYED (YYYY-MM-DD HH:MM, import date fallback); decluttered Amp and Cabinet panels; Master output picker; rows fill the width |
| 0.4.7 | Low power chords stay clean in the pitch shifter (30 ms search, centred 10 ms window); collapsing a panel moves nothing else; INPUT/MASTER pickers level; gate Release to 2 s; CI on every push |
| 0.4.8 | What's new popup in the update banner: release notes rendered in the app (paragraphs as bullets, NYC release time, every version since yours), Update Now / Ignore; update progress and errors in the banner |
| 0.5.0 | Amp B (dual amp, mid/side, polarity-aligned captures); Doubler; FDN reverb replaces Freeverb; Gate Depth and two-stage gate (no hang-then-drop through high gain); Undo; unsaved-changes prompt; Settings in 5 sections on one grid with hover help; 48-column rig grid; clearer audio-start errors |

Full detail is in the GitHub release notes for each version.

## Access you'll need

- **Repository:** `github.com/shaneoreilly/leviathan-amp-suite`. You need push rights and access to Actions secrets.
- **Signing keychain:** to build Mac releases locally with the same identity, copy `~/Library/Application Support/Leviathan/Signing/` from the owner's Mac. CI already has the certificate as secrets.
- **Toolchains:**
  - Mac: Xcode Command Line Tools.
  - Windows: Visual Studio 2022+, CMake 3.22+, Ninja for macOS builds of the JUCE app, and Inno Setup for the installer.
  - Optional: the ASIO SDK for local Windows builds.
