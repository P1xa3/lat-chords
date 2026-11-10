# LTA // CHORDS

Original open-source chord progression generator and playable **VST3 instrument for Windows and macOS**, plus **AU for macOS** (FL Studio, Logic Pro and other compatible hosts). Similar *workflow* to one-click chord tools, with independent code, algorithms and interface. Not affiliated with Unison Audio or Chord Genie; no proprietary code, samples, or AI model copied.

## Complete replacement package (November 2026)

This archive contains the **full files**, not snippets. Copy everything inside the extracted project folder to the **root** of your GitHub repository, replacing the existing files. In particular, keep the `Source`, `tests`, and hidden `.github/workflows` directory structures intact.

Fixes in this edition:

- Corrected `juce::ComboBox` compilation error in `PluginEditor.cpp`.
- Renamed a private piano-roll helper to avoid hiding JUCE's `Component::hitTest` method.
- Made the interface layout fit both the default size and the supported narrower window sizes.
- Kept the chosen chord richness consistent in all styles (triads are now actually triads).
- Added missing self-contained C++ chord engine tests and their include path.
- Supplied complete macOS and Windows GitHub Actions builds with artifact upload.

**Verification:** The chord engine compiles under GCC 17-standard mode and passes tests locally. Building and loading the full JUCE AU/VST3 still require a macOS/Windows runner and DAW testing; this package is not represented as precompiled or fully host-verified.

## Included features

- 12 keys × major / natural minor; 4, 8 or 16 bars.
- Six styles: Dreamy, Dark, Soul, Classic, Cinematic, Rage.
- Triads, seventh chords, ninth chords; root, smooth, or open voicings.
- Reroll the whole progression or one chord, invert, shuffle, transpose, undo / redo.
- Interactive piano roll: click to add, drag a note to move it, Alt-drag near its right edge to resize, right-click or double-click to remove it.
- Host-transport tempo sync; built-in lightweight synthesized preview and incoming MIDI piano sound.
- MIDI file export with 960 PPQ, and native OS drag-out of MIDI to the DAW (host permitting).
- Save / load `.ltac` presets and session-state recall.
- Flat original pink and white UI.

**Scope:** This does not reproduce Unison's private AI training, exact sounds, preset library or protected branding. The generator is an original rule-based + randomized music-theory implementation; it is not an AI model. Live VST3 MIDI routing may vary by host; MIDI file export is the reliable route into another FL Studio instrument.

## Build online - no Visual Studio or Xcode installation required

1. Open the existing GitHub repository (`P1xa3/lat-chords`) or create a new repository.
2. Unzip this source package. Upload **all files and folders from inside the extracted project folder** at the repository root, including the hidden `.github` folder. On a Mac, Finder's Command-Shift-Period toggles hidden files. If uploading via the web editor, ensure `.github/workflows/build-macos.yml` actually appears under the repository's Code tab.
3. Replace the old source/configuration files and commit the changes to the `main` branch.
4. Open **Actions** in the repo. If prompted, enable workflows. Run **Build LTA CHORDS (Windows VST3)** or **Build LTA CHORDS (macOS AU + VST3)**. A push to `main` will trigger both workflows automatically.
5. Once the chosen run has a green tick, open it and scroll to **Artifacts**. Download **LTA-CHORDS-Windows-VST3** for Windows or **LTA-CHORDS-macOS-Universal-VST3-AU** for Mac.
6. Extract its ZIP and follow `INSTALL_WINDOWS.txt` or `INSTALL_MACOS.txt`. The macOS artifact includes universal Intel + Apple Silicon VST3 and AU bundles.

If an Actions run fails, open the red job step for the actual compiler error. Builds run on GitHub Windows runners, not on your computer. JUCE is fetched by CMake during the build. Don't put proprietary code or licence keys into a public repository.

## Local building (optional)

On Windows, requires CMake 3.24+, Git and Visual Studio 2022 Desktop Development with C++ tools:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LTA_CHORDS_VST3
ctest --test-dir build -C Release --output-on-failure
```

The plugin bundle is normally under `build/LTA_CHORDS_artefacts/Release/VST3/`.

On macOS, with Xcode, Git and CMake 3.24+:

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target LTA_CHORDS_VST3 LTA_CHORDS_AU
ctest --test-dir build -C Release --output-on-failure
```

macOS workflows output ad-hoc signed, **not notarized**, development plugin bundles. Logic's AU format is not created by the Windows workflow.

## Known limitations

- Windows and macOS builds have **not been compiled or loaded in DAWs in this environment**; each needs a GitHub Actions build and a DAW smoke test. The plain C++ music-theory engine tests pass locally.
- Embedded synth is a basic oscillator voice, not a multisampled piano or licensed factory sound.
- File drag from an embedded VST UI may be restricted by some host window systems. EXPORT MIDI is provided as fallback.
- Chord spellings use a simple enharmonic display (e.g. D# may be displayed as Eb).
- Live MIDI output for VST3 instruments is host-dependent. Use exported files for predictable routing.

## Open-source licensing

This project is licensed under **AGPL-3.0-or-later** to be compatible with JUCE's AGPL option. You must satisfy JUCE's applicable licensing conditions if you redistribute or commercially distribute this plugin. JUCE is downloaded separately during compilation; this archive does not redistribute JUCE source or binaries. See https://www.gnu.org/licenses/agpl-3.0.en.html and https://juce.com/legal/juce-8-licence/ for details.
