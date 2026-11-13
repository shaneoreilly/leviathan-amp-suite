<p align="center">
  <img src="docs/icon.png" width="160" alt="Leviathan icon">
</p>

<h1 align="center">Leviathan</h1>

<p align="center"><b>A free guitar amp and effects rig for your Mac, Windows PC or Android phone.</b><br>
Plug in your guitar, pick a sound, play. Real amp tones, pedals, cabinets, a tuner and a recorder in one app. No account, no subscription.</p>

<p align="center">
  <img src="docs/screenshot.png" alt="Leviathan main window: input, gate, pitch, amp, pedalboard, cabinet, EQ, delay, reverb and master">
</p>

---

## Contents

- [1. What you need](#1-what-you-need)
- [2. Install](#2-install)
  - [2.1 Mac](#21-mac)
  - [2.2 Windows](#22-windows)
  - [2.3 Android](#23-android)
  - [2.4 Updates](#24-updates)
- [3. Getting started](#3-getting-started)
- [4. What's inside](#4-whats-inside)
- [5. Getting more sounds](#5-getting-more-sounds)
- [6. Sounding your best](#6-sounding-your-best)
- [7. Keyboard shortcuts](#7-keyboard-shortcuts)
- [8. Where your files are](#8-where-your-files-are)
- [9. Help and feedback](#9-help-and-feedback)
- [10. Credits and license](#10-credits-and-license)
- [11. Release history](#11-release-history)

---

## 1. What you need

- **A Mac** (macOS 13 Ventura or later), **a Windows PC** (Windows 10 or 11, 64-bit) **or an Android phone** (Android 9 or later, 64-bit).
- **An audio interface:** the small box you plug your guitar into, such as an iRig, Focusrite Scarlett or Boss Katana over USB. On a phone, plug it into the USB-C port, through a USB-C adapter or hub if its cable doesn't fit (see [Android](#23-android)).
- **Headphones or speakers.** Plugging headphones into the interface itself sounds best and has the least delay.

## 2. Install

### 2.1 Mac

1. Download the latest **`Leviathan-x.y.z.dmg`** from the [Releases page](../../releases).
2. Open it and drag **Leviathan** into your **Applications** folder.
3. Double-click **Leviathan** in Applications. The first time, macOS says it "can't be checked" and won't open it. Click **Done**.
4. Open **System Settings → Privacy & Security** and scroll down. Next to *"Leviathan.app" was blocked to protect your Mac*, click **Open Anyway**, then confirm with your password or Touch ID. You only do this once.

   <img src="docs/open-anyway.png" width="560" alt="System Settings → Privacy &amp; Security, showing &quot;Leviathan.app was blocked to protect your Mac&quot; with an Open Anyway button">

5. When asked, **allow microphone access**. This is how Leviathan hears your guitar through the interface. macOS remembers your answer, including after updates.

### 2.2 Windows

1. Download the latest **`Leviathan-Setup-x.y.z.exe`** from the [Releases page](../../releases).
2. Run it. If Windows says *"Windows protected your PC"*, click **More info → Run anyway**.
3. Leviathan appears in your Start menu.

### 2.3 Android

1. On your phone, download the latest **`Leviathan-x.y.z.apk`** from the [Releases page](../../releases).
2. Open it. The first time, Android asks you to allow installing apps from your browser (or Files app): tap **Settings**, turn on **Allow from this source**, go back and tap **Install**.
3. Open **Leviathan** and **allow microphone access** when asked. This is how it hears your guitar through the interface.

**Connecting your interface:**
- A **USB interface** (such as an iRig HD) plugs into the phone's USB-C port. If its cable is USB-A or Micro-USB, use a USB-C **OTG adapter** or a USB-C hub.
- If the interface doesn't come on, the phone may not be able to power it: use a hub with its own power supply.
- When it's connected properly, the phone's **USB preferences** show *USB controlled by: This device*.

Leviathan opens with the sound off: tap **Start** at the top when you're ready to play. It runs full screen in landscape and keeps the screen on while it's open. The back gesture returns to the rig rather than closing the app, so a stray swipe mid-song won't cut your sound. When you switch to another app, Leviathan stops playing so it doesn't get in the way of other apps' sound. Recording carries on if you were recording.

### 2.4 Updates

Leviathan checks for new versions itself. When one is out, a banner appears at the top of the window: click **Update** and it installs and restarts. You never need to download it again. On Android, tap **Update**, then **Update** again when Android asks you to confirm. The first time, Android also asks you to allow installs from Leviathan.

## 3. Getting started

1. **Plug your guitar into your audio interface** and connect the interface to your computer.
2. Open Leviathan. In the **Input** panel (top left), **pick your interface** and the input your guitar is plugged into.
3. Press **Start** at the top (or the Space bar).
4. **Pick a sound** from the preset menu at the top, or step through presets with **‹ ›**.
5. Play. Use **Input Gain** to set how hard you hit the amp, and **Output** in the Master panel for the overall volume.

**Not sure what a knob does?** Hover over it. A strip at the bottom of the window explains every knob, slider and button. You can turn this off in **Settings → Appearance**.

**Turning knobs:** drag up or down, or scroll over the knob. Hold **Shift** for fine steps. The **↺** under each knob puts it back to its starting point.

## 4. What's inside

**Amps**
- **Real amps.** Leviathan plays *NAM captures*: recordings of real amplifiers turned into models, similar to Kemper profiles. It comes with 34 of them:
  - **Metal:** Peavey 5150, 5150 II, 6505+, 6534+, JSX and XXX; Mesa/Boogie Mark IV; Splawn Pro Mod; ENGL E530 and Savage; Suhr high-gain; Yerasov Meshuggah; Marshall JCM2000 and JCM900; Laney GH100S; Sovtek MIG-50 (doom); Boss HM-2 "Swedish chainsaw" and Line 6 Uber Metal pedals.
  - **Rock:** Marshall JVM; Orange Rockerverb; Victory V4 Countess; Ceriatone King Kong; Friedman Dirty Shirley; Soldano-style drive; Jet City; Laney crunch.
  - **Clean, blues and indie:** Fender Twin; Marshall JVM clean; Vox AC15 Top Boost; Magnatone Super 59.

  You can add thousands more for free (see [Getting more sounds](#5-getting-more-sounds)).
- **Leviathan's own amp** with three channels: Clean, Rust and Hot.

**Effects**
- **Pedals:** compressor, wah (with a pedal or automatic), fuzz, overdrive, chorus, flanger, phaser and tremolo, plus **Blade**: a high-gain distortion inspired by the EAE Longsword, with a 3-band EQ, a mid boost and three clipping modes. Drag pedals into any order.
- **Noise gate** to silence hiss between notes.
- **Pitch:** play songs in other tunings without retuning, up or down by up to an octave. Set your guitar's tuning (standard, drop and open tunings, DADGAD) on the guitar row, then pick what you want to sound like on the speaker row: a tuning, or search for an artist (about 1,000 of them, rock, blues, metal, punk, pop and country) to hear their tuning. Artists in a tuning your guitar can't get to by shifting, such as open G from standard, show greyed out.
- **Cabinet:** the speaker and microphone sound. Use the built-in cabinets, or add your own speaker files (IRs) and search through them by band or song.
- **Graphic EQ, delay and reverb.**
- **Master output** that never distorts, however loud you turn it.

**Presets**
- **26 ready-made sounds,** all at a similar volume:
  - **Real-amp presets:** Modern Metal 5150, Groove Chug 6505+, Thrash Rhythm Mark IV, Crunch JVM, Clean Twin and more.
  - **Band-inspired tones:** Metallica-, Slayer-, Pantera-, Megadeth-, Sabbath-, Maiden-, Meshuggah-, Entombed-, Rammstein- and Opeth-ish.
- **Save** your changes to a preset, or **Save as** a new one. Mark favourites with a star.
- **Share presets** as `.leviathan` files. They work on Mac, Windows and Android; double-click one to load it on a computer, or import it from **Presets** on a phone.

**Practice**
- **Tuner** with standard and alternate tunings.
- **Metronome** with beat lights and tap tempo. The delay can follow the tempo too.

**Recording**
- **Press Rec** to record. Leviathan saves your clean guitar signal and the finished sound side by side, along with the exact settings you used. While you record, the top bar shows the take's waveform.
- **Recordings screen:** listen back, click anywhere on the waveform to jump there, rename and delete takes.
- **Trim takes:** click **✂ Trim** (or drag on the waveform) and set the selection with the yellow handles. Then choose **Trim to selection** to keep only that part, or **Trim selection** to cut that part out (the join is smoothed, so it doesn't click). The original is kept, so **Restore untrimmed** always brings the full take back.
- **Share takes** to other apps (Mac and Android): **Amped, for messages** (a small file that plays in WhatsApp, Messages and email), **Amped, full quality**, or **Raw guitar** for re-amping in another app.
- **Re-amp:** play an old take back through a different sound, live or into a new file, without playing it again.
- **Import** recordings from other apps, single files or whole folders.
- **Takes are never lost.** If the app quits or crashes mid-take, the recording is recovered next time.

**Control with your feet**
- **MIDI footswitches and expression pedals:** click **MIDI** at the top, click a knob or switch, then move the pedal or press the switch you want to use. Switches can turn effects on and off, change presets, tap the tempo, or open the tuner.

**Layout**
- Panels can be dragged into any order and folded away with the arrow on each one.
- The window remembers its size. Zoom in or out with the **− +** buttons at the bottom left.
- **On a phone:** drag a dial up or down to turn it, and touch any control to see what it does. Menus and lists open as Android's own pickers, and the top bar scrolls away with the rig to leave more room.

**When something needs fixing**
- If Leviathan shows a problem in the yellow bar at the top (no audio device, crackles, a missing amp capture and so on), click **Click to resolve** (**Tap to resolve** on a phone) to go straight to the setting that fixes it.

## 5. Getting more sounds

Leviathan uses two kinds of sound file:

| Kind | What it is | File | How to add it |
| --- | --- | --- | --- |
| **NAM capture** | A real amp, pedal or whole rig | `.nam` | **Amp → Add .nam…** |
| **Impulse response (IR)** | A speaker cabinet and microphone | `.wav` | **Cabinet → Add IRs…** (files or a whole folder) |

**Where to get amps (`.nam` files)**
- [TONE3000](https://www.tone3000.com): thousands of free amps, pedals and rigs. When searching, choose **NAM**. Tones marked **IR** are cabinets and download as `.wav` files.
- [pelennor2170/NAM_models](https://github.com/pelennor2170/NAM_models): a big free collection, where Leviathan's own amps come from. Click **Code → Download ZIP** to get them all, then unzip.
- [Neural Amp Modeler](https://www.neuralampmodeler.com): the project behind NAM, with links to more.

**Where to get cabinets (`.wav` files)**
- [TONE3000](https://www.tone3000.com): free cabinets and band-tone packs (choose **IR**).
- [OwnHammer](https://www.ownhammer.com) and [ML Sound Lab](https://ml-sound-lab.com): free and paid cabinet packs.
- *God's Cab*: a free, popular Mesa 4x12 pack with many microphones, which Leviathan has its own browser for. Search for "God's Cab IR" to find a copy, and put it in the God's Cab folder listed under [Where your files are](#8-where-your-files-are). On Android, use **Settings → Library → God's Cab → Add…** and pick the pack's folder.

**Tip:** some amp captures are "full rig" captures that already include a cabinet. For those, switch Leviathan's **Cabinet** off.

## 6. Sounding your best

- **Stop your interface from distorting.** Turn the gain knob *on your interface* down until your hardest playing never lights it red. A distorting interface makes every sound harsh and fizzy. Leviathan warns you when this happens; use **Input Gain** in Leviathan if you need more level.
- **Listen through your interface.** Headphones or speakers plugged into the interface have the least delay and the fewest crackles.
- **Avoid slow outputs.** Bluetooth headphones, TV or monitor speakers, and laptop speakers all add noticeable delay. The number at the top (for example *~9 ms*) shows the delay. It turns orange when it's high; hover over it to see why.
- **Crackles?** Go to **Settings → Audio** and choose a larger buffer. On Windows, choose your interface's **ASIO** driver if it has one. On Android, Leviathan starts on the smallest buffer your phone offers (the least delay) and keeps whichever size you pick; a heavy amp capture may need a step or two up.
- **On a phone:** listen through the interface or wired headphones. The phone's own speaker adds about 20 ms of delay, and Bluetooth headphones far more.

## 7. Keyboard shortcuts

On a computer:

| Mac | Windows | Action |
| --- | --- | --- |
| Space | Space | Start / stop sound |
| ⌘R | Ctrl+R | Record / stop recording |
| ⌘S | Ctrl+S | Save the current preset |
| ⇧⌘S | Ctrl+Shift+S | Save as a new preset |
| T | T | Show / hide the tuner |
| ⌘1 ⌘2 ⌘3 ⌘4 | Ctrl+1 Ctrl+2 Ctrl+3 Ctrl+4 | Rig / Presets / Recordings / Settings |
| ⌘= ⌘− ⌘0 | Ctrl+= Ctrl+− Ctrl+0 | Zoom in / out / normal size |
| Return | Enter | Play / pause (Recordings) |
| Delete | Delete | Delete the selected recording |

## 8. Where your files are

| What | Mac | Windows |
| --- | --- | --- |
| Recordings | `~/Music/Leviathan/Sessions/` | `Music\Leviathan\Sessions\` |
| Your amp captures (`.nam`) | `~/Library/Application Support/Leviathan/NAM/` | `%APPDATA%\Leviathan\NAM\` |
| Your cabinet files (IR library) | `~/Library/Application Support/Leviathan/IRs/Library/` | `%APPDATA%\Leviathan\IRs\Library\` |
| God's Cab pack | `~/Library/Application Support/Leviathan/IRs/GodsCab/` | `%APPDATA%\Leviathan\IRs\GodsCab\` |
| Settings and presets | `~/Library/Preferences/local.leviathan.ampsuite.plist` | `%APPDATA%\Leviathan\Leviathan.settings` |

**Keeping recordings somewhere else** (another drive, Google Drive, Dropbox, iCloud Drive): go to **Settings → Library → Recordings folder** and click **Change folder…**. Cloud drives work when their desktop app is installed, because it shows up as a folder. Leviathan offers to move your existing takes too: each one is copied, checked, then removed from the old folder, and the old folder is deleted once it's empty. **Use default** goes back to the standard folder. If the folder isn't available when Leviathan starts (for example, the drive isn't running), new takes go to the standard folder until it's back.

**Opening these folders:**
- **Mac:** in Finder, press **⇧⌘G** and paste the path.
- **Windows:** paste the path into the File Explorer address bar.
- **Either:** use **Open folder** in **Settings → Library**.

**On Android,** Leviathan keeps your amps, cabinets, presets and settings inside the app. Add files with the **Add…** buttons (**Amp**, **Cabinet**, or **Settings → Library**, including **God's Cab**), and send takes elsewhere with **Share**. Recordings are in `Android/data/local.leviathan.ampsuite/files/Music/Leviathan/Sessions/`, which you can open from a computer over a USB cable (choose **File transfer** on the phone). **Uninstalling Leviathan deletes everything it keeps, recordings included,** so share or copy off any takes you want first.

To keep recordings in a folder of your own on the phone (for example `Music/Leviathan`), use **Settings → Library → Recordings folder → Change folder…**. Android asks you once to turn on **All files access** for Leviathan. Takes in your own folder show up in file managers and sync apps, and stay when you uninstall Leviathan. Google Drive folders can't be recorded into directly on a phone: use **Share**, or a sync app on your folder.

## 9. Help and feedback

Found a problem or have an idea? In Leviathan, go to **Settings → About** and click **Report a bug** or **Request a feature**. It opens a form with your setup already filled in.

**Crackles, dropouts or something odd?** Turn on **Settings → About → Debug log → Record a diagnostic log**, play until the problem happens, then use **Share log…** (Mac and Android) or **Copy log** (Mac and Windows) and attach or paste it into your bug report. The log records your computer or phone, the audio device and how hard Leviathan is working (including how much the amp capture takes), never your playing. It's off until you turn it on. You can also [open an issue on GitHub](https://github.com/shaneoreilly/leviathan-amp-suite/issues/new/choose).

## 10. Credits and license

- The included amp captures were made by members of the Neural Amp Modeler community; see [CREDITS.md](shared/nam/captures/CREDITS.md).
- The logo is based on a classic public-domain *Moby-Dick* illustration.
- Other software used by Leviathan is listed in [NOTICE.md](NOTICE.md).

Leviathan is free, open-source software by Shane O'Reilly, under the [GNU AGPLv3](LICENSE) license with the extra terms in [NOTICE.md](NOTICE.md). You can use it, share it and change it. Anything made from it must also be open source and keep the credit "Leviathan by Shane O'Reilly".

Building Leviathan yourself, or want to contribute? See [DEVELOPMENT.md](DEVELOPMENT.md).

## 11. Release history

The main changes in each release. Full notes are on the [releases page](https://github.com/shaneoreilly/leviathan-amp-suite/releases).

| Version | Highlights |
| --- | --- |
| 0.8.0 | Search about 1,000 artists in Pitch to hear their tuning; open tunings and DADGAD; matching dropdowns; text colour; full-width top bar; ⌘1–4 shortcuts |
| 0.7.2 | Recovers by itself when a USB interface stops responding |
| 0.7.1 | Light theme and your own background colour; Wet / Dry playback; less pitch-shifter lag |
| 0.7.0 | New look; cabinet browser with Size, Length and Tone; preset tags and search; output EQ |
| 0.6.2 | Cheaper amp captures on Windows; Android fixes |
| 0.6.1 | Keep recordings anywhere, Google Drive included; diagnostic log |
| 0.6.0 | Android; one-click fixes; sharing takes |
| 0.5.1 | New icon; easier trimming; endless gate release |
| 0.5.0 | Second amp, doubler, new reverb, smoother gate; Undo |
| 0.4.8 | What's new shown before you update |
| 0.4.7 | Cleaner low chords in the pitch shifter; longer gate release |
| 0.4.6 | Saved presets load after updating; 34 amp captures; preset sorting |
| 0.4.5 | Missing recordings back; Blade distortion pedal |
| 0.4.4 | No more repeated microphone prompts; trim recordings |
| 0.4.3 | Cabinet IR library; explanations on hover; Save as; live waveform |
| 0.4.2 | Updates hard to miss; one copy running at a time |
| 0.4.1 | Tidier Settings; safer recordings; notes shown for every tuning |
| 0.4.0 | Real amps through Neural Amp Modeler captures; 21 new presets |
| 0.3.0 | Windows; open source (GNU AGPLv3); bug reports from the app |
| 0.2.1 | MIDI control; metronome and tempo-synced delay; preset files; in-app updates |
| 0.1.2 | No more freezes when switching audio devices |
| 0.1.1 | First release, for Mac |
