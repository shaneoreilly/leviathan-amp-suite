<p align="center">
  <img src="docs/icon.png" width="160" alt="Leviathan icon">
</p>

<h1 align="center">Leviathan</h1>

<p align="center"><b>An open, low-latency guitar rig for macOS.</b><br>
Bring any amp: built-in models, your own captures, any cab IR. Free, open source, no account.</p>

<p align="center">
  <img src="docs/screenshot.png" alt="Leviathan main window: input, gate, pitch, amp, pedalboard, cabinet, EQ, delay, reverb and master">
</p>

---

## Features

- **Three-channel amp**: Clean, Rust and Hot voicings, 4× oversampled so high gain stays smooth. Knobs for gain, bass, mid, treble, presence, depth and volume.
- **Pedalboard** you can drag to reorder: compressor, wah (manual or auto), fuzz, overdrive, chorus, flanger, phaser and tremolo.
- **Noise gate** with a fast attack, so pick transients aren't softened.
- **Pitch / "Play as"**: tune the whole guitar up or down by up to an octave (Eb → Bb, E → Drop D voicings, and so on). The shifter follows the pitch you play, so single notes and riffs add only about 7–12 ms.
- **Cabinet**: built-in cabinet models, or your own impulse responses (any WAV). Supports a two-mic A/B blend and low/high cut.
- **8-band graphic EQ** with click-and-drag faders. Double-click a fader to reset it.
- **Delay and reverb**, and a **master output**.
- **Tempo and metronome**: a click with beat lights, tap tempo, and a delay that locks to note lengths (1/4, dotted 1/8, triplets…). Tempo follows incoming MIDI clock.
- **Preset files**: export one preset or all of them as `.leviathan` files, and import by button, by dropping them on the Presets screen, or by double-clicking in Finder.
- **Built-in updates**: Settings → Updates checks GitHub releases and installs the new version in place, no DMG needed.
- **MIDI control**: click **MIDI** in the top bar, click any knob or ON/OFF switch, then move a control on your MIDI device to assign it. Expression pedals work on any knob (Wah position, for example). Footswitches can turn units on and off, step through presets, tap tempo, or toggle the tuner and metronome. Program Change picks presets in list order.
- **Tuner** with selectable tuning and reference pitch (A4 = 430–450 Hz).
- **Presets**: factory and your own, with favourites. Re-save a factory preset with your changes and restore the original any time.
- **Recording**:
  - Each take saves the dry DI and the processed output side by side, plus the exact rig settings that were used.
  - **Recordings** screen with waveform, transport and take details.
  - **Live re-amp**: loop a take's dry signal through the rig while you tweak.
  - **Offline re-amp**: render a take through the current rig into a new file. The original is kept.
  - **Import**: bring in WAV recordings, either single DI takes or dry/wet pairs.
- **Latency display**: an honest round-trip estimate, broken down into input, buffers and output. The app warns you when an output device is slow (Bluetooth, monitor speakers, EQ/routing apps).
- **Zoom** from 60 % to 160 % (sidebar − / +, ⌘= / ⌘− / ⌘0), and a choice of accent colour.

## Install

1. Download the latest **`Leviathan-x.y.z.dmg`** from [Releases](../../releases).
2. Open it and drag **Leviathan** into **Applications**.
3. Double-click **Leviathan** in Applications. Releases aren't notarized by Apple yet, so the first time macOS blocks it with a "can't be checked" message. Click **Done**.
4. Open **System Settings → Privacy & Security** and scroll down to **Security**. Next to *"Leviathan.app" was blocked to protect your Mac*, click **Open Anyway**. Confirm with your password or Touch ID. After this one-time step, it opens normally.

   <img src="docs/open-anyway.png" width="560" alt="System Settings → Privacy &amp; Security, showing &quot;Leviathan.app was blocked to protect your Mac&quot; with an Open Anyway button">

5. Allow microphone access when asked. Leviathan needs it to hear your audio interface. macOS remembers the choice.

Requires macOS 13 Ventura or later, on Apple silicon or Intel. Use an audio interface for your guitar, ideally one with its own headphone output.

## Build from source

You need the Xcode Command Line Tools (`xcode-select --install`). The full Xcode app isn't needed.

```sh
git clone https://github.com/<you>/leviathan-amp-suite.git
cd leviathan-amp-suite/mac
./build.sh
open Leviathan.app
```

**Microphone permission is kept across rebuilds.** macOS ties privacy permissions to an app's code signature, and an ad-hoc signature changes with every build. On first run, `build.sh` creates a self-signed **"Leviathan Local Signing"** certificate in its own keychain and signs every build with it. The keychain lives at `~/Library/Application Support/Leviathan/Signing/`, not in your login keychain. Because the signature stays the same, you grant the microphone once. To sign with your own certificate instead, set `LEVIATHAN_SIGN_IDENTITY="Developer ID Application: …"`.

| Command | What it does |
| --- | --- |
| `./build.sh` | Development build for this Mac's architecture |
| `UNIVERSAL=1 ./build.sh` | Apple silicon + Intel |
| `scripts/make_dmg.sh 0.1.0` | Universal build packaged as `dist/Leviathan-0.1.0.dmg`, plus its SHA-256 |
| `tools/make_icon.sh` | Regenerate the app icon and README logo from `Sources/Logo.swift` |

## Releasing

Push a version tag. GitHub Actions (`.github/workflows/release.yml`) builds the universal DMG and attaches it to a new release:

```sh
git tag v0.1.0
git push origin v0.1.0
```

To make releases that open without the "Open Anyway" step, add these repository secrets. They need an Apple Developer account:

- `MACOS_CERT_P12_BASE64` and `MACOS_CERT_PASSWORD`: your *Developer ID Application* certificate, exported as a `.p12` file.
- `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD`: used for notarization.

## Getting the lowest latency

Most of the delay you feel comes from the audio devices, not the DSP. The full rig uses only about 3 % of each 32-sample buffer. To get the lowest latency:

1. **Use your interface for output too.** One device means one clock, with no resampling between two devices.
2. **Avoid the slow outputs:**
   - Bluetooth (100 ms or more).
   - Monitor or TV speakers over HDMI/DisplayPort (often 20–50 ms that macOS can't report).
   - System EQ or routing apps such as eqMac.
   - The MacBook's built-in speakers (about 18 ms of processing).
3. **Pick the smallest buffer that doesn't crackle.** Go to **Settings → Buffer**; try 32 samples, or 16 on a fast machine. **Settings → Engine** shows peak DSP load and counts dropouts.

The latency figure in the top bar turns orange above 15 ms. Hover over it for the breakdown.

## Cabinet impulse responses

Leviathan ships with modelled cabinets, but real impulse responses sound much better. Load any IR WAV with **Cabinet → IR file…**.

For the multi-mic library browser, put a pack of *God's Cab*-style IRs here, in folders for each sample rate:

```
~/Library/Application Support/Leviathan/IRs/GodsCab/44.1/...
~/Library/Application Support/Leviathan/IRs/GodsCab/48/...
```

IR packs are third-party content and aren't included in this repository.

## Amp captures (experimental)

`mac/Capture/` contains tools to model another amp or plugin as a black box:

1. `python3 make_test.py` writes a test signal: sweeps, noise and synthetic guitar.
2. Run that signal through the amp you want to capture. Save the result to `~/Music/Leviathan/Capture/wet/<name>.wav`.
3. `python3 fit.py` fits a Wiener–Hammerstein model plus a cabinet FIR and writes `~/Library/Application Support/Leviathan/Captures/<name>.json`.
4. In the app, choose the capture from the amp's model picker.

The tools need Python 3 with `numpy` and `scipy`. Clean and moderately driven rigs capture well. Very high-gain rigs with gates and drive pedals are beyond what this simple model can fit.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| Space | Start / stop audio |
| ⌘R | Record / stop recording |
| ⌘S | Save the current preset |
| ⌘= / ⌘− / ⌘0 | Zoom in / out / actual size |
| Return | Play / pause (Recordings) |

## Where things are stored

| What | Where |
| --- | --- |
| Recordings | `~/Music/Leviathan/Sessions/` |
| Captures | `~/Library/Application Support/Leviathan/Captures/` |
| IR library | `~/Library/Application Support/Leviathan/IRs/` |
| Settings and presets | `~/Library/Preferences/local.leviathan.ampsuite.plist` |
| Local signing keychain (source builds) | `~/Library/Application Support/Leviathan/Signing/` |

## Project layout

```
mac/
  Sources/
    App.swift        app, model, presets, rig UI
    Shell.swift      sidebar, top bar, Presets / Recordings / Settings screens
    Audio.swift      CoreAudio HAL engine (aggregate devices, latency, dropout handling)
    DSP.swift        all signal processing: gate, pitch, pedals, amp, EQ, cab, delay, reverb
    Sessions.swift   recorder, recordings library, playback, re-amp, import
    IRLibrary.swift  impulse-response library browser
    Logo.swift       the logo as stroke geometry (in-app mark, icon, SVG)
  Capture/           amp-capture test signal + model fitter (Python)
  Resources/         AppIcon.icns
  scripts/           sign.sh (stable signing), make_dmg.sh (release package)
  tools/             icon generator
  Leviathan.entitlements
  build.sh
.github/workflows/   release.yml: tag → DMG on GitHub Releases
index.html, serve.sh browser prototype (Web Audio); run ./serve.sh
docs/                logo and icon for this README
```

## Credits

The logo is a line drawing based on a classic public-domain *Moby-Dick* illustration of the white whale breaching.

## License

Leviathan is free software by Shane O'Reilly, released under the [GNU AGPLv3](LICENSE) with the additional terms in
[NOTICE.md](NOTICE.md): you can use, share and change it, but anything made from it must also be open source under the
AGPLv3 and keep the credit "Leviathan by Shane O'Reilly". The Windows app is built with JUCE (AGPLv3) and the
Steinberg ASIO SDK (GPLv3).

Found a bug or have an idea? Use **Settings → Help & feedback** in the app, or
[open an issue](https://github.com/shaneoreilly/leviathan-amp-suite/issues/new/choose).

