<p align="center">
  <img src="docs/icon.png" width="160" alt="Leviathan icon">
</p>

<h1 align="center">Leviathan</h1>

<p align="center"><b>An open, low-latency guitar rig for macOS and Windows.</b><br>
Real amps through Neural Amp Modeler captures, built-in amp models, any cab IR. Free, open source, no account.</p>

<p align="center">
  <img src="docs/screenshot.png" alt="Leviathan main window: input, gate, pitch, amp, pedalboard, cabinet, EQ, delay, reverb and master">
</p>

---

## Features

**Amps**
- **Real amps with Neural Amp Modeler (NAM).** NAM is the open-source technology behind Kemper-style "profiles". Leviathan ships with 11 captures of real amps:
  - Peavey 5150 and 6505+
  - Mesa/Boogie Mark IV
  - Splawn Pro Mod
  - Marshall JVM
  - Laney GH100S
  - ENGL
  - Soldano-style drive
  - Fender Twin

  You can add any `.nam` file, and thousands are free at [tone3000.com](https://www.tone3000.com). Every capture is level-matched when it loads.
- **Built-in three-channel amp.** Clean, Rust and Hot voicings, 4× oversampled so high gain stays smooth, with gain, bass, mid, treble, presence, depth and volume.
- **Your own captures** with the experimental capture tools (see below).

**Rig**
- **Pedalboard:** drag pedals to reorder them. Includes compressor, wah (manual or auto), fuzz, overdrive, chorus, flanger, phaser and tremolo.
- **Noise gate** with a fast attack, so pick transients aren't softened.
- **Pitch / "Play as":** tune the whole guitar up or down by up to an octave (Eb → Bb, E → Drop D voicings, and so on). The shifter follows the pitch you play, so single notes and riffs add only about 7–12 ms. Every tuning shows its string notes, for example "Drop C · C G C F A D".
- **Cabinet:** your own impulse responses (any WAV), a multi-mic *God's Cab*-style library browser with A/B mic blend, or built-in cab models. Low and high cut.
- **IR library:** add whole folders of cab IRs, such as the band-tone IR packs from [tone3000.com](https://www.tone3000.com), with **Cabinet → Add IRs…**. Search by band or song, click to hear each one, or step through them with ‹ ›.
- **8-band graphic EQ, delay and reverb, master output.** The output never hard-clips: a limiter holds peaks at −1 dBFS without adding latency.
- **Tempo and metronome:** a click with beat lights, tap tempo, and a delay that locks to note lengths (1/4, dotted 1/8, triplets…). Tempo follows incoming MIDI clock.
- **Layout:** panels sit on a grid, can be dragged into any order and collapsed, and the window reopens at the size you left it.

**Presets**
- **26 factory presets,** all level-matched:
  - **11 NAM real-amp presets:** Modern Metal 5150, Groove Chug 6505+, Thrash Rhythm Mark IV, Crunch JVM, Clean Twin and more.
  - **10 band-inspired tones:** Metallica-, Slayer-, Pantera-, Megadeth-, Sabbath-, Maiden-, Meshuggah-, Entombed-, Rammstein- and Opeth-ish.
  - The original rhythm, lead and clean presets.
- **Your own presets,** with favourites. Re-save a factory preset with your changes and restore the original any time.
- **Preset files:** export one preset or all of them as `.leviathan` files, and import them by button, by dropping them on the Presets screen, or by double-clicking them. They work across Mac and Windows.

**Control**
- **MIDI:** click **MIDI** in the top bar, click any knob or ON/OFF switch, then move a control on your MIDI device to assign it.
  - Expression pedals work on any knob, Wah position for example.
  - Footswitches can turn units on and off, step through presets, tap tempo, or toggle the tuner and metronome.
  - Program Change picks presets in list order.
- **Tuner** with selectable tuning and reference pitch (A4 = 430–450 Hz).

**Recording**
- **Each take saves** the dry DI and the processed output side by side, plus the exact rig settings that were used.
- **Recordings screen** with waveform, transport and take details. Delete a take from the list with right-click or the Delete key.
- **Live re-amp:** loop a take's dry signal through the rig while you tweak.
- **Offline re-amp:** render a take through the current rig, NAM captures included, into a new file. The original is kept.
- **Import** WAV recordings or whole folders of them, either single DI takes or dry/wet pairs. Takes are dated from their file names (for example `Dry_20261104_18:18:44`), and Leviathan offers to move the originals to the Trash afterwards.
- **No lost takes:** a take cut off by a crash or quit is recovered automatically, and quitting mid-take finishes it properly.

**App**
- **Built-in updates:** Leviathan checks GitHub at launch and every 6 hours, shows a banner when a new version is out, and installs it in place on Mac and Windows.
- **Settings** has a left-hand navigation (Playing / Sounds / App).
- **Latency display:** an honest round-trip estimate, broken down into input, buffers and output. Warnings for slow outputs (Bluetooth, monitor speakers, EQ/routing apps), for persistent dropouts (the buffer is raised automatically), and for an interface that is clipping its input.
- **Report bugs and request features** from **Settings → Help & feedback**. It opens a GitHub form with your version, OS and audio setup filled in.
- **Zoom** from 60 % to 160 %, and a choice of accent colour.

## Install

### macOS

1. Download the latest **`Leviathan-x.y.z.dmg`** from [Releases](../../releases).
2. Open it and drag **Leviathan** into **Applications**.
3. Double-click **Leviathan** in Applications. Releases aren't notarized by Apple yet, so the first time macOS blocks it with a "can't be checked" message. Click **Done**.
4. Open **System Settings → Privacy & Security** and scroll down to **Security**. Next to *"Leviathan.app" was blocked to protect your Mac*, click **Open Anyway**. Confirm with your password or Touch ID. After this one-time step, it opens normally.

   <img src="docs/open-anyway.png" width="560" alt="System Settings → Privacy &amp; Security, showing &quot;Leviathan.app was blocked to protect your Mac&quot; with an Open Anyway button">

5. Allow microphone access when asked. Leviathan needs it to hear your audio interface. macOS remembers the choice.

Requires macOS 13 Ventura or later, on Apple silicon or Intel.

### Windows

1. Download the latest **`Leviathan-Setup-x.y.z.exe`** from [Releases](../../releases).
2. Run it. The installer isn't code-signed yet, so Windows SmartScreen may say *"Windows protected your PC"*. Click **More info → Run anyway**.
3. It installs for your user account, so no administrator rights are needed, and adds Leviathan to the Start menu. `.leviathan` preset files open in Leviathan.
4. In **Settings → Audio**, pick your interface. **ASIO** (your interface's own driver) gives the lowest latency. **Windows Audio (Exclusive Mode)** is the next best.

Requires Windows 10 or 11, 64-bit. Updates install from inside the app, with no need to run the installer again.

Both platforms work best with an audio interface for your guitar, ideally one with its own headphone output.

## Getting the lowest latency

Most of the delay you feel comes from the audio devices, not the DSP. The full rig uses only a few percent of each buffer.

1. **Use your interface for output too.** One device means one clock, with no resampling between two devices. When input and output are separate devices, crackles are more likely, and Leviathan raises the buffer automatically if dropouts persist.
2. **Avoid the slow outputs:**
   - Bluetooth (100 ms or more).
   - Monitor or TV speakers over HDMI/DisplayPort (often 20–50 ms that the OS can't report).
   - System EQ or routing apps such as eqMac.
   - Laptop speakers.
3. **Pick the smallest buffer that doesn't crackle** in **Settings → Audio**. On Windows, use the ASIO driver if your interface has one.
4. **Set your interface's input gain so it never clips.** Your hardest playing should stay out of the red on the interface itself. Leviathan warns you when the input hits full scale, because a clipping DI makes every tone harsh and fizzy. Use **Input Gain** in Leviathan for more level.

The latency figure in the top bar turns orange above 15 ms. Hover over it for the breakdown.

## Where to get amp captures and cab IRs

Leviathan plays two kinds of file:

| Kind | File | What it is | Add it with |
| --- | --- | --- | --- |
| **NAM capture** | `.nam` | A neural model of a real amp, pedal or full rig | **Amp → Add .nam…** |
| **Impulse response (IR)** | `.wav` | A speaker cabinet and microphone (or a band's tone curve) | **Cabinet → Add IRs…** (files or folders) or **IR file…** |

**NAM captures (`.nam`)**
- [TONE3000](https://www.tone3000.com): the largest free library, with thousands of amps, pedals and rigs. Filter by **NAM**; tones marked **IR** download as WAV cab files instead.
- [pelennor2170/NAM_models](https://github.com/pelennor2170/NAM_models): a big community collection on GitHub, and the source of the captures bundled with Leviathan. **Code → Download ZIP** gets them all.
- [Neural Amp Modeler](https://www.neuralampmodeler.com): the project behind the format, with links to more captures and to the trainer for capturing your own amp.

**Cabinet IRs (`.wav`)**
- [TONE3000](https://www.tone3000.com): free cab IRs and band-tone IR packs (filter by **IR**).
- [OwnHammer](https://www.ownhammer.com) and [ML Sound Lab](https://ml-sound-lab.com): free and paid cab packs.
- *God's Cab* by Wilkinson Audio: a free multi-mic Mesa 4x12 pack that Leviathan has a dedicated browser for (see below). Its original site is offline; search for "God's Cab IR" to find a copy.

Amp-only captures need a cab IR after them. Full-rig captures already include a cab, so switch Leviathan's Cabinet off for those.

## NAM captures

- **Pick a capture** from the amp's model picker. **Input** sets how hard you hit it; **Output** sets its level.
- **Add your own** with **Add .nam…** (in the amp panel or **Settings → Library**).
- **Amp-only vs full-rig captures:** amp-only captures play through Leviathan's cabinet. For a "full rig" capture that already includes a cab, switch the cabinet off.
- **Bundled captures** come from the community collection [pelennor2170/NAM_models](https://github.com/pelennor2170/NAM_models) (GPLv3). Each is credited by creator in [`shared/nam/captures/CREDITS.md`](shared/nam/captures/CREDITS.md).

## Cabinet impulse responses

Real impulse responses sound much better than the built-in cab models. Load any IR WAV with **Cabinet → IR file…**.

For collections, use **Cabinet → Add IRs…** (or **Settings → Library → Cab IRs**) and pick files or whole folders. They're copied into `IRs/Library/` in Leviathan's data folder, and presets remember which IR they use. IRs longer than about 85 ms are trimmed to the engine's length with a short fade; on a 194-IR tone pack, what's trimmed is a median 57 dB below the IR.

For the multi-mic library browser, put a pack of *God's Cab*-style IRs in folders for each sample rate, under `IRs/GodsCab/44.1/` and `IRs/GodsCab/48/` in Leviathan's data folder (see below). IR packs are third-party content and aren't included in this repository.

## Amp captures (experimental)

`mac/Capture/` contains tools to model another amp or plugin as a black box:

1. `python3 make_test.py` writes a test signal: sweeps, noise and synthetic guitar.
2. Run that signal through the amp you want to capture. Save the result to `~/Music/Leviathan/Capture/wet/<name>.wav`.
3. `python3 fit.py` fits a Wiener–Hammerstein model plus a cabinet FIR and writes `<data folder>/Captures/<name>.json`.
4. In the app, choose the capture from the amp's model picker.

The tools need Python 3 with `numpy` and `scipy`. Clean and moderately driven rigs capture well. For high-gain amps, use NAM instead.

## Keyboard shortcuts

| Key (Mac / Windows) | Action |
| --- | --- |
| Space | Start / stop audio |
| ⌘R / Ctrl+R | Record / stop recording |
| ⌘S / Ctrl+S | Save the current preset |
| ⌘= ⌘− ⌘0 / Ctrl+= Ctrl+− Ctrl+0 | Zoom in / out / actual size |
| Return | Play / pause (Recordings) |
| Delete | Move the selected recording to the Trash / Recycle Bin |

## Where things are stored

| What | macOS | Windows |
| --- | --- | --- |
| Data folder (NAM, IRs, captures) | `~/Library/Application Support/Leviathan/` | `%APPDATA%\Leviathan\` |
| Your NAM captures | `…/Leviathan/NAM/` | `…\Leviathan\NAM\` |
| God's Cab IRs | `…/Leviathan/IRs/GodsCab/` | `…\Leviathan\IRs\GodsCab\` |
| IR library (added IRs) | `…/Leviathan/IRs/Library/` | `…\Leviathan\IRs\Library\` |
| Recordings | `~/Music/Leviathan/Sessions/` | `Music\Leviathan\Sessions\` |
| Settings and presets | `~/Library/Preferences/local.leviathan.ampsuite.plist` | `%APPDATA%\Leviathan\Leviathan.settings` |
| Local signing keychain (Mac source builds) | `~/Library/Application Support/Leviathan/Signing/` | |

## Build from source

### macOS

You need the Xcode Command Line Tools (`xcode-select --install`); the full Xcode app isn't needed. The first build also fetches and compiles NeuralAmpModelerCore, which needs `git` and takes about half a minute.

```sh
git clone https://github.com/shaneoreilly/leviathan-amp-suite.git
cd leviathan-amp-suite/mac
./build.sh
open Leviathan.app
```

**Microphone permission is kept across rebuilds.** macOS ties privacy permissions to an app's code signature, and an ad-hoc signature changes with every build.
- On first run, `build.sh` creates a self-signed **"Leviathan Local Signing"** certificate in its own keychain and signs every build with it.
- That keychain lives at `~/Library/Application Support/Leviathan/Signing/`, not in your login keychain.
- Because the signature stays the same, you grant the microphone once.

To sign with your own certificate instead, set `LEVIATHAN_SIGN_IDENTITY="Developer ID Application: …"`.

| Command | What it does |
| --- | --- |
| `./build.sh` | Development build for this Mac's architecture |
| `UNIVERSAL=1 ./build.sh` | Apple silicon + Intel |
| `scripts/make_dmg.sh 0.1.0` | Universal build packaged as `dist/Leviathan-0.1.0.dmg`, plus its SHA-256 |
| `tools/make_icon.sh` | Regenerate the app icon and README logo from `Sources/Logo.swift` |

### Windows

The Windows app is C++ with [JUCE](https://juce.com). You need Visual Studio 2022 or later (with "Desktop development with C++"), CMake 3.22+ and git. JUCE and NeuralAmpModelerCore are fetched at pinned versions by CMake.

```sh
cd windows
cmake -B build -A x64 -DLEVIATHAN_ASIO_SDK=C:\path\to\ASIOSDK   # ASIO is optional
cmake --build build --config Release --target Leviathan
```

For ASIO, download the [Steinberg ASIO SDK](https://www.steinberg.net/asiosdk) (used under GPLv3) and pass its folder. The installer is built with Inno Setup from `windows/installer/Leviathan.iss`.

The same project also builds on macOS (`cmake -B build -G Ninja`), which is how the Windows app is tested alongside the Mac one.
- `windows/tools/compare_with_swift.sh` renders identical test signals through both audio engines and checks they match. The amp, gate and overdrive are bit-identical, and NAM runs the same code in both.
- `preset_check` renders a DI take through every factory preset and reports loudness and peaks.

## Releasing

Push a version tag. GitHub Actions builds the universal Mac DMG (`release.yml`) and the Windows installer (`windows.yml`, with the ASIO SDK checksum-pinned), then publishes both to a new release:

```sh
git tag v0.1.0
git push origin v0.1.0
```

To make Mac releases that open without the "Open Anyway" step, add these repository secrets. They need an Apple Developer account:

- `MACOS_CERT_P12_BASE64` and `MACOS_CERT_PASSWORD`: your *Developer ID Application* certificate, exported as a `.p12` file.
- `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD`: used for notarization.

## Project layout

```
mac/                     macOS app (Swift, SwiftUI, CoreAudio)
  Sources/
    App.swift            app, model, settings, factory presets, rig UI
    Shell.swift          sidebar, top bar, Presets / Recordings / Settings screens
    Audio.swift          CoreAudio HAL engine (aggregate devices, latency, dropout handling)
    DSP.swift            all signal processing: gate, pitch, pedals, amp, NAM, EQ, cab, delay, reverb, limiter
    Nam.swift            NAM captures: folder, loading, hand-off to the audio thread
    Sessions.swift       recorder, recordings library, playback, re-amp, import, recovery
    Midi.swift           MIDI learn, footswitch actions, Program Change, clock
    Tempo.swift          tempo, metronome, tap
    Updater.swift        self-update from GitHub releases
    PresetFiles.swift    .leviathan import / export
    Feedback.swift       Help & feedback, About
    IRLibrary.swift      impulse-response library browser
    Logo.swift           the logo as stroke geometry (in-app mark, icon, SVG)
  Capture/               amp-capture test signal + model fitter (Python)
  scripts/               sign.sh (stable signing), make_dmg.sh (release package)
  build.sh
windows/                 Windows app (C++, JUCE): same features, same presets and file formats
  Source/                Dsp.cpp (port of DSP.swift), Model, Settings, Sessions, Midi, Updater, UI/
  installer/             Inno Setup script
  tools/                 engine comparison with the Swift engine, preset checker
shared/nam/              Neural Amp Modeler bridge used by both apps, plus the bundled captures
.github/                 release workflows, issue forms
index.html, serve.sh     browser prototype (Web Audio); run ./serve.sh
docs/                    logo, icon and screenshots for this README
```

## Credits

- The logo is a line drawing based on a classic public-domain *Moby-Dick* illustration of the white whale breaching.
- NAM captures are by members of the Neural Amp Modeler community; see [CREDITS.md](shared/nam/captures/CREDITS.md).
- Third-party software is listed in [NOTICE.md](NOTICE.md):
  - NeuralAmpModelerCore, Eigen and nlohmann/json
  - JUCE and the Steinberg ASIO SDK, for the Windows app

## License

Leviathan is free software by Shane O'Reilly, released under the [GNU AGPLv3](LICENSE) with the additional terms in [NOTICE.md](NOTICE.md):
- You can use, share and change it.
- Anything made from it must also be open source under the AGPLv3, and must keep the credit "Leviathan by Shane O'Reilly".

Found a bug or have an idea? Use **Settings → Help & feedback** in the app, or [open an issue](https://github.com/shaneoreilly/leviathan-amp-suite/issues/new/choose).
