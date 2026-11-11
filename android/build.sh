#!/bin/bash
# Leviathan for Android, built locally from the same sources as the Windows app (windows/Source).
#
#   android/build.sh                 # the app   -> android/Builds/Android/app/build/outputs/apk/release_/release/*.apk
#   android/build.sh install         # ...and install + launch it on the connected phone (adb, USB or Wi-Fi)
#   android/build.sh probe [install] # the feasibility probe (android/Probe): latency, callback load, NAM CPU cost
#
# Release builds are signed with Leviathan's permanent Android key: Android only installs an update signed with
# the same key as the installed app (like the Mac's "Leviathan Local Signing" certificate). The key is
#   LEV_ANDROID_KEYSTORE           default ~/Library/Application Support/Leviathan/Signing/android-release.jks
#   LEV_ANDROID_KEYSTORE_PASSWORD  default: the file android-keystore-password next to it
# (CI: the LEVIATHAN_ANDROID_KEYSTORE_* secrets). Without it the build falls back to this machine's debug key and
# says so; such a build can't update, or be updated by, a released one. The app also lands in
# android/dist/Leviathan-X.Y.Z.apk with a .sha256, which the release attaches and the in-app updater verifies.
#
# Needs: the Android SDK with platform 35, NDK 28.1.13356709 and CMake (ANDROID_HOME, default ~/Library/Android/sdk),
# and JDK 17 or 21 (JAVA_HOME, default Homebrew's openjdk@21). JUCE 9.0.5 and NeuralAmpModelerCore v0.6.0 are taken
# from the Windows build's FetchContent folders when present, otherwise cloned into android/.deps.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
target=app
if [ "${1:-}" = probe ]; then target=probe; shift; fi
repo="$(cd "$here/.." && pwd)"
deps="$here/.deps"

export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
if [ -z "${JAVA_HOME:-}" ] && [ -d /opt/homebrew/opt/openjdk@21 ]; then   # macOS + Homebrew; CI sets JAVA_HOME
  export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
fi

juce="${LEV_JUCE_DIR:-$repo/windows/build/_deps/juce-src}"
nam="${LEV_NAM_DIR:-$repo/windows/build/_deps/nam-src}"
if [ ! -d "$juce/modules" ]; then
  juce="$deps/JUCE"
  [ -d "$juce/modules" ] || git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git "$juce"
fi
if [ ! -f "$nam/NAM/dsp.cpp" ]; then
  nam="$deps/NeuralAmpModelerCore"
  if [ ! -f "$nam/NAM/dsp.cpp" ]; then
    git clone https://github.com/sdatkinson/NeuralAmpModelerCore.git "$nam"
    git -C "$nam" checkout 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842   # v0.6.0, as in windows/CMakeLists.txt
    git -C "$nam" submodule update --init Dependencies/eigen
  fi
fi

# Patched copy of JUCE's modules (the Windows build's copy stays untouched). JUCE's Oboe device reads exactly one
# buffer of input per output callback and never drops the surplus, so when the input device (e.g. a USB interface)
# runs on a slightly faster clock than the output (the phone speaker), input piles up and latency creeps up to the
# input buffer's capacity. The patch discards the backlog before each read and counts what it dropped.
# It also fixes the callback size to the buffer setting (otherwise Android picks its burst, as little as 48 frames,
# and NAM's per-call cost plus any scheduling hiccup on a 1 ms deadline crackles), double-buffers the output, and
# turns on Oboe's performance hint (ADPF) so Android keeps the CPU clocked up for the audio thread.
patch_version=3   # bump when the patch below changes
patched="$deps/JUCE-patched"
if [ "$(cat "$patched/.lev-patched" 2>/dev/null)" != "$patch_version" ]; then
  rm -rf "$patched"; mkdir -p "$patched"
  cp -R "$juce/modules" "$patched/modules"
  python3 - "$patched/modules/juce_audio_devices/native/juce_Oboe_android.cpp" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
anchor = "                    auto result = nativeInputStream->read (inputStreamNativeBuffer.data(), numFrames, 0);\n"
drain = """                    // Leviathan: drop input backlog (input clock faster than output clock), keeping one spare buffer
                    if (auto avail = nativeInputStream->getAvailableFrames(); avail && avail.value() > 2 * numFrames)
                    {
                        auto excess = avail.value() - numFrames;
                        const auto room = (int32_t) (inputStreamNativeBuffer.size() / (size_t) jmax (1, numInputChannels));
                        while (excess > 0)
                        {
                            auto r = nativeInputStream->read (inputStreamNativeBuffer.data(), jmin (excess, room), 0);
                            if (! r || r.value() <= 0) break;
                            excess -= r.value();
                            levInputDropped.fetch_add (r.value(), std::memory_order_relaxed);
                        }
                    }

"""
assert s.count(anchor) == 1, "JUCE Oboe input read not found: update the patch for this JUCE version"
s = s.replace(anchor, drain + anchor)
first_ns = s.index("namespace juce\n{\n") + len("namespace juce\n{\n")
s = s[:first_ns] + "\nstd::atomic<int64_t> levInputDropped { 0 };   // Leviathan: input frames discarded to stop drift\n" + s[first_ns:]

def swap(old, new):
    global s
    assert s.count(old) == 1, "JUCE Oboe stream setup changed: update the patch for this JUCE version: " + old[:60]
    s = s.replace(old, new)
swap("""            builder.setPerformanceMode (oboe::PerformanceMode::LowLatency);
""", """            builder.setPerformanceMode (oboe::PerformanceMode::LowLatency);
            // Leviathan: callbacks of exactly the buffer setting (Android's own burst can be 1 ms)
            if (newCallback != nullptr && newBufferSize > 0)
                builder.setFramesPerDataCallback (newBufferSize);
""")
swap("""                stream->setBufferSizeInFrames (newBufferSize);
""", """                stream->setBufferSizeInFrames ((newCallback != nullptr ? 2 : 1) * newBufferSize);   // Leviathan: double-buffered output

            // Leviathan: ADPF performance hint, so the CPU stays clocked up for the audio thread
            if (stream != nullptr && newCallback != nullptr)
                stream->setPerformanceHintEnabled (true);
""")
open(p, "w").write(s)
PY
  # Native Android pickers: every JUCE pop-up menu (combo boxes, menus, the audio device lists) can be handed to
  # Leviathan's levNativePopupMenu (windows/Source/UI/NativeMenus.cpp), which shows an Android list dialog instead.
  python3 - "$patched/modules/juce_gui_basics/menus/juce_PopupMenu.cpp" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
anchor = """int PopupMenu::showWithOptionalCallback (const Options& options,
                                         ModalComponentManager::Callback* userCallback,
                                         [[maybe_unused]] bool canBeModal)
{
"""
hook = """#if JUCE_ANDROID
// Leviathan: shows a menu natively; calls done with the chosen item (nullptr if none). Returns false to fall back.
std::function<bool (const PopupMenu&, std::function<void (const PopupMenu::Item*)>)> levNativePopupMenu;
#endif

"""
body = """   #if JUCE_ANDROID
    if (levNativePopupMenu != nullptr && getNumItems() > 0)
    {
        struct Holder { std::unique_ptr<ModalComponentManager::Callback> cb; };
        auto keep = std::make_shared<PopupMenu> (*this);
        auto holder = std::make_shared<Holder>();
        holder->cb.reset (userCallback);

        if (levNativePopupMenu (*keep, [keep, holder] (const Item* item)
            {
                if (item != nullptr && item->action != nullptr)
                    item->action();

                if (holder->cb != nullptr)
                    holder->cb->modalStateFinished (item != nullptr ? item->itemID : 0);
            }))
            return 0;

        userCallback = holder->cb.release();   // not shown natively: JUCE's own menu below
    }
   #endif

"""
assert s.count(anchor) == 1, "JUCE PopupMenu changed: update the native-menu patch for this JUCE version"
s = s.replace(anchor, hook + anchor + body)
open(p, "w").write(s)
PY
  echo "$patch_version" > "$patched/.lev-patched"
fi
juce_modules="$patched/modules"

# Projucer generates the Gradle project (JUCE's CMake API doesn't target Android)
projucer="$deps/juce-tools/extras/Projucer/Projucer_artefacts/Release/Projucer.app/Contents/MacOS/Projucer"
[ "$(uname)" = Darwin ] || projucer="$deps/juce-tools/extras/Projucer/Projucer_artefacts/Release/Projucer"
if [ ! -x "$projucer" ]; then
  cmake -S "$juce" -B "$deps/juce-tools" -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_BUILD_EXTRAS=ON >/dev/null
  cmake --build "$deps/juce-tools" --target Projucer
fi

keystore="${LEV_ANDROID_KEYSTORE:-$HOME/Library/Application Support/Leviathan/Signing/android-release.jks}"
keypass="${LEV_ANDROID_KEYSTORE_PASSWORD:-}"
[ -z "$keypass" ] && [ -f "$(dirname "$keystore")/android-keystore-password" ] && keypass="$(cat "$(dirname "$keystore")/android-keystore-password")"
if [ "$target" = app ] && { [ ! -f "$keystore" ] || [ -z "$keypass" ]; }; then
  echo "WARNING: no release key ($keystore): signing with this machine's debug key. Such a build can't update a released one." >&2
  keystore=""; keypass=""
fi

python3 - "$here" "$repo" "$juce_modules" "$nam" "$target" "$keystore" "$keypass" <<'PY'
import glob, hashlib, os, sys
from xml.sax.saxutils import quoteattr as q
here, repo, juce_modules, nam, target, keystore, keypass = sys.argv[1:]
rel = lambda p: os.path.relpath(p, here)
fid = lambda p: hashlib.sha1(p.encode()).hexdigest()[:6]

def file_el(path, compile=True, resource=False, scheme=None):
    extra = f' compilerFlagScheme="{scheme}"' if scheme else ''
    return (f'<FILE id="{fid(path)}" name={q(os.path.basename(path))} compile="{int(compile)}" '
            f'resource="{int(resource)}" file={q(rel(path))}{extra}/>')
group = lambda name, items: f'<GROUP id="{fid(name)}" name="{name}">' + "".join(items) + "</GROUP>"
code = lambda paths: [file_el(p, p.endswith(".cpp"), scheme="fastmath" if p.endswith("Dsp.cpp") else None) for p in paths]

win = os.path.join(repo, "windows/Source")
bridge = [os.path.join(repo, "shared/nam/lev_nam.cpp"), os.path.join(repo, "shared/nam/lev_nam.h")]
namsrc = sorted(glob.glob(os.path.join(nam, "NAM/*.cpp")) + glob.glob(os.path.join(nam, "NAM/*/*.cpp")))
caps = sorted(glob.glob(os.path.join(repo, "shared/nam/captures/*.nam")))
version = open(os.path.join(repo, "windows/VERSION")).read().strip()
major, minor, patch = (int(x) for x in version.split("."))
icon = os.path.join(repo, "docs/icon.png")

if target == "app":
    name, ident, pkg, orient = "Leviathan", "LevApp", "local.leviathan.ampsuite", "landscape"
    sources = sorted(glob.glob(os.path.join(win, "*.cpp")) + glob.glob(os.path.join(win, "*.h"))
                     + glob.glob(os.path.join(win, "UI/*.cpp")) + glob.glob(os.path.join(win, "UI/*.h")))
    groups = (group("Source", code(sources)) + group("NAM", code(bridge + namsrc))
              + group("Resources", [file_el(p, False, True) for p in caps + [os.path.join(repo, "shared/help/explanations.json")]
                                                    + [os.path.join(repo, "shared/fonts/Inter-" + w + ".ttf") for w in ("Regular", "SemiBold", "Bold")]])
              + group("Icon", [file_el(icon, False, False)]))
    modules = ["juce_core", "juce_events", "juce_data_structures", "juce_graphics", "juce_gui_basics", "juce_gui_extra",
               "juce_audio_basics", "juce_audio_devices", "juce_audio_formats", "juce_audio_processors",
               "juce_audio_processors_headless", "juce_audio_utils", "juce_dsp", "juce_cryptography"]
    defines = (f'NAM_SAMPLE_FLOAT=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 '
               f'LEVIATHAN_VERSION=&quot;{version}&quot;')
    iconAttr = f' bigIcon="{fid(icon)}"'
    # in-app updates: the downloaded APK goes to Android's installer through androidx FileProvider
    # REQUEST_INSTALL_PACKAGES: in-app updates. MANAGE_EXTERNAL_STORAGE: "All files access", asked for only when you
    # choose a recordings folder on the phone's storage (Settings > Library > Recordings folder)
    manifest = ('<manifest><uses-permission android:name="android.permission.REQUEST_INSTALL_PACKAGES"/>'
                '<uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE"/><application>'
                '<provider android:name="androidx.core.content.FileProvider" android:authorities="local.leviathan.ampsuite.updates" '
                'android:exported="false" android:grantUriPermissions="true"><meta-data android:name="android.support.FILE_PROVIDER_PATHS" '
                'android:resource="@xml/lev_update_paths"/></provider></application></manifest>')
    dependency = "implementation('androidx.core:core:1.13.1')"
    iconAttr += (f' androidManifestCustomXmlElements={q(manifest)} androidAdditionalResourceFolders="res"'
                 f' androidDependencies={q(dependency)}')
    if keystore:
        iconAttr += f' androidKeyStore={q(keystore)} androidKeyStorePass={q(keypass)} androidKeyAlias="leviathan" androidKeyAliasPass={q(keypass)}'
else:
    name, ident, pkg, orient = "LeviathanProbe", "LevPrb", "local.leviathan.probe", "portrait"
    groups = (group("Source", code([os.path.join(here, "Probe/Main.cpp")]))
              + group("Engine", code([os.path.join(win, "Dsp.cpp"), os.path.join(win, "Dsp.h")]))
              + group("NAM", code(bridge + namsrc))
              + group("Captures", [file_el(p, False, True) for p in caps]))
    modules = ["juce_core", "juce_events", "juce_data_structures", "juce_graphics", "juce_gui_basics", "juce_audio_basics", "juce_audio_devices"]
    defines = "NAM_SAMPLE_FLOAT=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0"
    iconAttr = ""

mods = "".join(f'<MODULE id="{m}" showAllCode="1" useLocalCopy="0" useGlobalPath="0"/>' for m in modules)
paths = "".join(f'<MODULEPATH id="{m}" path={q(juce_modules)}/>' for m in modules)
headers = ";".join([win, os.path.join(repo, "shared/nam"), nam,
                    os.path.join(nam, "Dependencies/eigen"), os.path.join(nam, "Dependencies/nlohmann")])

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated by android/build.sh; edit the script, not this file. -->
<JUCERPROJECT id="{ident}" name="{name}" projectType="guiapp" version="{version}" companyName="Leviathan"
              bundleIdentifier="{pkg}" jucerFormatVersion="1" useAppConfig="0"
              addUsingNamespaceToJuceHeader="0" compilerFlagSchemes="fastmath" defines="{defines}">
  <MAINGROUP id="{ident}G" name="{name}">{groups}</MAINGROUP>
  <MODULES>{mods}</MODULES>
  <JUCEOPTIONS JUCE_USE_ANDROID_OBOE="1"/>
  <EXPORTFORMATS>
    <ANDROIDSTUDIO targetFolder="Builds/Android" androidMinimumSDK="28" androidTargetSDK="35"
                   androidVersionCode="{major * 10000 + minor * 100 + patch}"{iconAttr}
                   microphonePermissionNeeded="1" androidScreenOrientation="{orient}" fastmath="-ffast-math"
                   androidEnableContentSharing="1">
      <CONFIGURATIONS>
        <CONFIGURATION isDebug="0" name="Release" androidArchitectures="arm64-v8a" headerPath={q(headers)}/>
      </CONFIGURATIONS>
      <MODULEPATHS>{paths}</MODULEPATHS>
    </ANDROIDSTUDIO>
  </EXPORTFORMATS>
</JUCERPROJECT>
'''
for old in glob.glob(os.path.join(here, "*.jucer")): os.remove(old)
open(os.path.join(here, f"{name}.jucer"), "w").write(xml)
PY

jucer="$(ls "$here"/*.jucer)"
rm -rf "$here/Builds/Android/app/src/main/assets"   # stale assets from the other target
"$projucer" --resave "$jucer"
echo "sdk.dir=$ANDROID_HOME" > "$here/Builds/Android/local.properties"

cd "$here/Builds/Android"
chmod +x gradlew
./gradlew --no-daemon assembleRelease_Release
apk="$(find app/build/outputs/apk -name '*.apk' | head -1)"
echo "APK: $here/Builds/Android/$apk"
if [ "$target" = app ]; then   # the release asset, named like the DMG and installer, with its checksum
  version="$(tr -d '[:space:]' < "$repo/windows/VERSION")"
  mkdir -p "$here/dist"
  cp "$apk" "$here/dist/Leviathan-$version.apk"
  (cd "$here/dist" && shasum -a 256 "Leviathan-$version.apk" > "Leviathan-$version.apk.sha256")
  echo "Release APK: $here/dist/Leviathan-$version.apk"
fi

if [ "${1:-}" = "install" ]; then
  "$ANDROID_HOME/platform-tools/adb" install -r "$apk"
  pkg=$([ "$target" = probe ] && echo local.leviathan.probe || echo local.leviathan.ampsuite)
  "$ANDROID_HOME/platform-tools/adb" shell pm grant "$pkg" android.permission.RECORD_AUDIO 2>/dev/null || true
  activity=$("$ANDROID_HOME/platform-tools/adb" shell cmd package resolve-activity --brief "$pkg" | tail -1 | tr -d '\r')
  # Android 17+ relaunches an updated app by itself; a second launch on top of that makes a JUCE app quit
  sleep 3
  [ -n "$("$ANDROID_HOME/platform-tools/adb" shell pidof "$pkg" | tr -d '\r')" ] || "$ANDROID_HOME/platform-tools/adb" shell am start -n "$activity" >/dev/null
fi
