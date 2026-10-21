# AmpForge

An original **guitar amplifier simulator** VST3 plug-in written in C++17 on top
of JUCE, built automatically for **Windows x64** by GitHub Actions with
**Visual Studio 2022 (MSVC)**.

* **Plugin name:** AmpForge
* **Formats:** VST3 only (no standalone, no AU, no AAX)
* **Platform:** Windows x64 (64-bit), tested host: Cubase 15
* **Framework:** JUCE **8.0.15**, pinned to commit
  `91ad83ae34a81e0833b1a2b0866f54846370ae53`
* **Build system:** CMake (JUCE CMake API)
* **Language:** C++17
* **Licence:** GNU GPL v3 (see `LICENSE` and `THIRD_PARTY_NOTICES.md`)

No paid SDK and no proprietary dependency is required. The VST 3 SDK (3.8.0,
**MIT** since October 2025) is bundled inside JUCE, so there is nothing to sign
and nothing to download from Steinberg.

---

## Contents

1. [The amp models](#the-amp-models)
2. [Signal flow](#signal-flow)
3. [Parameters](#parameters)
4. [Repository layout](#repository-layout)
5. [Building locally on Windows](#building-locally-on-windows)
6. [Setting up the GitHub repository](#setting-up-the-github-repository)
7. [Running the workflow from the GitHub Actions tab](#running-the-workflow-from-the-github-actions-tab)
8. [The artefact: exact name and path](#the-artefact-exact-name-and-path)
9. [Installing in Cubase 15](#installing-in-cubase-15)
10. [Automated validation](#automated-validation)
11. [Troubleshooting](#troubleshooting)
12. [Real-time safety notes](#real-time-safety-notes)
13. [Limitations](#limitations)
14. [Licensing](#licensing)

---

## The amp models

Three genuinely different signal paths — different filters, a different number
of gain stages, a different clipping law and a different amount of power-supply
sag. They are **not** one path with three EQ presets.

| Model | Path | Character |
|---|---|---|
| **American Clean** | input high-pass 32 Hz → mild bright cap (1.8 kHz, +2.5 dB) → **2** gain stages with a *cubic* soft clip → mid scoop (−2.5 dB @ 900 Hz) → 9.5 kHz low-pass → low resonance @ 95 Hz | wide dynamics, smooth highs, gentle compression |
| **British Crunch** | input high-pass 45 Hz → strong bright cap (2.2 kHz, +6 dB) → **3** stages with an *asymmetric* (even-harmonic) clip → mid hump (+5.5 dB @ 720 Hz) → 7 kHz low-pass → resonance @ 120 Hz | midrange emphasis, responsive breakup, strong power-supply sag (0.28) |
| **Modern High Gain** | input high-pass 105 Hz (tight low end) → bright cap (3 kHz, +8 dB) → **3** stages with a *steep diode-pair* clip → 6.2 kHz low-pass → resonance @ 130 Hz (+3 dB) | tight lows, heavy saturation, long sustain |

Common to all models: a feed-forward **power-supply sag** model (louder playing
pulls the rail down, which is what makes an amp feel "alive"), a
bass/mid/treble tone stack, a presence control, a master volume, and a DC
blocker that removes the offset introduced by asymmetric clipping.

## Signal flow

```
in ──► input meter ──► noise gate ──► amp model ──► cabinet ──► delay ──► reverb ──► DC block ──► soft limiter ──► out
                                      (gain stages, (convolution)(+tone) (+tone)     (20 Hz)      (0.99 ceiling)
                                       tone stack,
                                       presence,
                                       master)
```

* **Noise gate** — threshold (−80…0 dB) and release (20…800 ms), plus an
  internal 1.5 ms attack, a 40 ms hold and 4 dB of hysteresis so that it does
  not chatter on decay tails. One detector serves both channels, so the stereo
  image is never pulled sideways.
* **Cabinet** — three procedurally generated cabinet profiles (4x12 closed,
  2x12 combo, 1x12 open) plus optional **user IR loading** (wav / aiff / flac /
  ogg). IRs are generated and decoded on a worker thread — including an FFT that
  levels them: the average magnitude over 200 Hz…4 kHz is set to 0 dB and the
  loudest point is capped at +12 dB, so a cabinet colours the sound instead of
  silently adding 20 dB of gain — and handed to the convolution engine through
  `loadImpulseResponse()`, which JUCE documents as wait-free.
* **Delay** — digital (clean, interpolated) or analog-inspired (low-pass + DC
  block + soft saturation in the feedback loop, wow & flutter modulation, slight
  right-channel offset). Feedback is clamped to 0.9 and every element in the
  loop has a gain ≤ 1, so the loop gain is always below unity.
* **Reverb** — Room / Spring-inspired / Hall. Schroeder topology (parallel
  damped combs → all-pass chain → tone stage) with per-mode comb lengths,
  damping, pre-delay range and wet filtering; the spring mode adds slow
  pre-delay modulation and a tank resonance.

## Parameters

All 22 parameters are automatable and smoothed.

| Section | Parameters |
|---|---|
| Amp | Amp Model, Gain, Bass, Mid, Treble, Presence, Master |
| Gate | Gate on/off, Threshold, Release |
| Cabinet | Cabinet on/off, Cab Profile (4x12 / 2x12 / 1x12 / User IR) |
| Reverb | Mode (Room / Spring / Hall), Mix, Decay, Pre-delay, Tone |
| Delay | Mode (Digital / Analog), Time, Feedback, Mix, Tone |

## Repository layout

```
AmpForge/
├── CMakeLists.txt                    # juce_add_plugin, VST3 only, C++17
├── LICENSE                           # GPL v3
├── THIRD_PARTY_NOTICES.md
├── README.md
├── .gitignore
├── Scripts/verify_sources.sh         # repository completeness check
├── Source/
│   ├── PluginProcessor.h/.cpp        # graph, parameters, state, bus layouts
│   ├── PluginEditor.h/.cpp           # hardware-style GUI layout
│   ├── ParamIDs.h                    # IDs, ranges, defaults, formatting
│   ├── DSP/
│   │   ├── AmpModel.h/.cpp           # the three amp models
│   │   ├── NoiseGate.h/.cpp
│   │   ├── ReverbEngine.h/.cpp
│   │   ├── DelayEngine.h/.cpp
│   │   ├── CabinetSim.h/.cpp         # generated + user IRs, loader thread
│   │   ├── LevelMeter.h/.cpp
│   │   ├── Biquad.h                  # allocation-free RBJ biquad + one-pole
│   │   └── DelayLine.h               # interpolated circular delay line
│   └── UI/
│       ├── AmpForgeLookAndFeel.h/.cpp
│       ├── MeterComponent.h/.cpp
│       └── OptionSelector.h
├── Tests/
│   ├── CMakeLists.txt                # juce_add_console_app(AmpForgeTests)
│   └── AmpForgeTests.cpp             # JUCE UnitTest suites + main()
└── .github/workflows/build.yml       # CI: verify, build, test
```

## Building locally on Windows

You need **Visual Studio 2022** (MSVC toolset + CMake component) and Git.

```bat
git clone https://github.com/<your-account>/AmpForge.git
cd AmpForge

:: Fetch the pinned JUCE (same commit the CI uses)
git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git ext/JUCE

:: Configure (Visual Studio 2022, x64)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

:: Build the VST3 in Release
cmake --build build --config Release --target AmpForge_VST3 --parallel
```

The bundle appears at

```
build\AmpForge_artefacts\Release\VST3\AmpForge.vst3
```

with the actual binary inside at
`AmpForge.vst3\Contents\x86_64-win\AmpForge.vst3`.

If you already have a JUCE checkout somewhere else, point the build at it:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAMPFORGE_JUCE_DIR=C:\JUCE
```

Optional, on any platform with a JUCE-capable toolchain:

```bash
cmake -S . -B build -DAMPFORGE_BUILD_TESTS=ON
cmake --build build --config Release --target AmpForgeTests
ctest --test-dir build -C Release --output-on-failure
```

## Setting up the GitHub repository

```bash
# 1. create the repository on github.com (leave it empty, no README/licence)

# 2. push this project
cd AmpForge
git init
git add .
git commit -m "AmpForge: JUCE VST3 guitar amp simulator"
git branch -M main
git remote add origin https://github.com/<your-account>/AmpForge.git
git push -u origin main
```

Before the first push, replace the placeholder identity in `CMakeLists.txt`:

```cmake
COMPANY_WEBSITE  "https://github.com/<your-account>/AmpForge"
COMPANY_EMAIL    "you@example.com"
```

and, if you publish the plug-in, pick your own four character
`PLUGIN_MANUFACTURER_CODE` (currently `Afge`) and `PLUGIN_CODE` (`Amp1`).

## Running the workflow from the GitHub Actions tab

1. Push to `main` (or open a pull request against it) — the workflow
   `.github/workflows/build.yml` starts automatically.
2. To start it by hand: open the repository on GitHub → **Actions** → pick
   **build** in the left column → **Run workflow** → choose the branch →
   **Run workflow**.
3. Three jobs run:
   * **Verify repository files** (~10 s) — checks that every expected file exists.
   * **Build VST3 (Windows x64, MSVC)** (~10-20 min) — fetches the pinned JUCE,
     configures with `Visual Studio 17 2022 -A x64`, builds `AmpForge_VST3` in
     Release, verifies the bundle and uploads it.
   * **Automated validation tests** (~10-20 min) — builds `AmpForgeTests` and
     runs it through CTest.
4. When the build job is green, scroll to the bottom of the run page →
   **Artifacts** → **AmpForge-windows-x64-vst3** → click to download.

## The artefact: exact name and path

| Item | Value |
|---|---|
| Artefact name | **`AmpForge-windows-x64-vst3`** |
| Contents | the folder `AmpForge.vst3` (a VST3 bundle) |
| Path inside the build | `build/AmpForge_artefacts/Release/VST3/AmpForge.vst3` |
| Binary inside the bundle | `AmpForge.vst3/Contents/x86_64-win/AmpForge.vst3` |
| CMake target | `AmpForge_VST3` |
| Retention | 30 days (configurable in the workflow) |

Downloading the artefact gives you a zip containing `AmpForge.vst3/…`.

## Installing in Cubase 15

1. Download and unzip the artefact.
2. Copy the whole **`AmpForge.vst3` folder** (not just the file inside it) to the
   VST3 folder Cubase scans. The usual locations are:
   * 64-bit plug-ins, all users: `C:\Program Files\Common Files\VST3`
   * 64-bit plug-ins, current user: `%LOCALAPPDATA%\Programs\Common\VST3`
   (In Cubase 15: **Studio → VST Plug-in Manager** → the gear icon shows the
   exact folder list, e.g. *VST Plug-in Paths*.)
3. Start Cubase 15.
4. **Studio → VST Plug-in Manager** → click the **Refresh / Rescan** button, or
   restart Cubase. AmpForge appears under **Distortion** (the VST3 category is
   `Fx|Distortion`).
5. Insert it on a guitar track: **Inserts → Distortion → AmpForge**.
6. Cubase will offer mono, stereo, mono→stereo and stereo→mono track
   configurations; all four are supported. For a guitar DI track, a **mono**
   track is the natural choice — AmpForge feeds both output channels from the
   mono input and still produces width from the cabinet IR, delay and reverb.

If Cubase reports the plug-in as "blacklisted", see
[Troubleshooting](#troubleshooting).

## Automated validation

`Tests/AmpForgeTests.cpp` is a JUCE `UnitTest` console application registered
with CTest (`AmpForgeTests`). It checks:

* **Parameters** — every parameter exists, defaults match `Source/ParamIDs.h`,
  a save/restore round trip reproduces every value, and a corrupt state block
  leaves the defaults untouched.
* **Processing** — output is finite and bounded by the limiter ceiling at
  44.1 / 48 / 88.2 / 96 kHz; silence produces silence; an impulse produces
  output and its tail decays within five seconds; extreme settings (everything
  at maximum, five seconds of full-scale noise) stay finite and never exceed
  ±1.0.
* **Gain staging** — a 220 Hz sine at −14 dBFS through each of the three amp
  models at the default settings must land at a usable level (measured: 0.21 /
  0.42 / 0.44 peak — clearly audible, nowhere near the limiter) and must not
  develop a DC offset (measured: under 1e-4).
* **Bus layouts** — mono→mono, mono→stereo, stereo→mono and stereo→stereo can
  all be configured and processed.
* **Repository** — every file the build needs is present, and the artefact path
  documented here matches the one the code asserts.

Run it locally with:

```bash
cmake -S . -B build -DAMPFORGE_BUILD_TESTS=ON
cmake --build build --config Release --target AmpForgeTests
ctest --test-dir build -C Release --output-on-failure
```

**What this does not prove:** that the plug-in sounds good, and that it behaves
correctly in every host. A green test run is a floor, not a ceiling — always
listen to it in Cubase before you rely on it.

## Troubleshooting

**"JUCE was not found at …" (CMake configure error)**
JUCE is not vendored. Either run the CI (it checks JUCE out itself), clone it
once (`git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git ext/JUCE`),
or configure with `-DAMPFORGE_JUCE_DIR=<path>`. The folder must contain
`CMakeLists.txt` directly (i.e. `ext/JUCE/CMakeLists.txt`).

**The JUCE download/checkout step fails in Actions**
`actions/checkout` fetches JUCE from GitHub; a transient failure is usually
fixed by re-running the job (**Re-run failed jobs**). If GitHub is unreachable,
fix the `JUCE_COMMIT`/`JUCE_TAG` values: the tag and the commit must agree
(`8.0.15` = `91ad83ae34a81e0833b1a2b0866f54846370ae53`). The workflow
deliberately fails with "JUCE pin mismatch" rather than silently building
against a different JUCE.

**`CMake Error: Could not create named generator "Visual Studio 17 2022"`**
The runner image does not have VS 2022, or you are running the command from a
shell without the MSVC environment. On `windows-latest` this generator is
always present; locally, install the "Desktop development with C++" workload.
Check with `cmake --help | findstr "Visual Studio"`.

**`A C compiler is required to build targets that depend on JUCE`**
The top-level `project()` call must list `C` as well as `CXX` — it does
(`LANGUAGES C CXX`). Do not remove it; JUCE builds zlib/png/jpeg/flac as C.

**Compile errors in JUCE about WebKit / libcurl**
They mean the module flags were not applied. They are set with
`add_compile_definitions()` *before* `add_subdirectory(JUCE)`, because the JUCE
module targets are compiled in a sub-scope. Don't move that block.

**"VST3 bundle not found at …"**
The build succeeded but the artefact is elsewhere. Look at the
**Troubleshooting dump on failure** step, which prints the build tree. The usual
causes: building a different target (`AmpForge_VST3` is the one), or a
non-Release configuration (`build/AmpForge_artefacts/<Config>/VST3/…`).

**The upload step reports "no files were found"**
The verification step before it would have failed first. If the bundle exists
locally but the upload is empty, check that `path:` in the workflow matches
`VST3_BUNDLE` and that the path uses forward slashes.

**Cubase shows the plug-in as blacklisted / does not scan it**
Open **Studio → VST Plug-in Manager**, select AmpForge and un-blacklist it, then
rescan. Make sure you copied the whole `.vst3` folder into a folder that
Cubase actually scans, and that the folder contains
`Contents/x86_64-win/AmpForge.vst3`. A 32-bit host can never load it — this is
an x64-only build.

**The plug-in is silent**
The Gate is on by default at −52 dB: play harder or lower the threshold. Also
check Master (default −10 dB) and Cab (default on).

## Real-time safety notes

* No allocation, no locks, no file I/O, no logging in `processBlock()`.
* Filter coefficient changes are plain float stores (`Source/DSP/Biquad.h`);
  JUCE's `IIR::Coefficients` reference-counted allocation is deliberately
  avoided.
* Denormals are flushed with `juce::ScopedNoDenormals` for the whole block.
* Delay and reverb lines are allocated once in `prepareToPlay()`; changing the
  reverb mode only re-lengthens existing buffers.
* Cabinet IRs are generated/decoded on a worker thread and handed over with the
  wait-free `Convolution::loadImpulseResponse()`.
* The only lock in the project (`CabinetSim::stateLock`) guards the stored user
  IR and is never touched by the audio thread.
* The output stage removes DC (20 Hz high-pass) and limits with a soft knee at
  0.85 rising to a hard ceiling of 0.99, so the plug-in cannot hand a clipped
  sample to the host.

## Limitations

* **Windows x64 only** in CI. The code is platform independent and the test
  suite builds anywhere JUCE does, but the workflow only produces a Windows VST3.
* No tempo sync, no ping-pong delay, no MIDI, no sidechain, no internal
  preset browser (the host's preset system is used instead).
* The cabinet profiles are **synthetic, procedurally generated models**, not
  measurements of real cabinets. The static level trims live in
  `CabinetSim::generateProfile()` if you want to rebalance them by ear.
* The amp models are original designs tuned by the numbers, not emulations of
  any specific commercial amplifier.
* Splash screen: JUCE's splash screen is **not** disabled. Removing it requires
  a commercial JUCE licence (see below).

## Licensing

* **AmpForge** — GNU GPL v3 (`LICENSE`).
* **JUCE 8.0.15** — dual licensed (JUCE 8 EULA free Starter tier, or AGPLv3).
  Read <https://juce.com/legal/juce-8-licence/>. Disabling the JUCE splash
  screen (`JUCE_DISPLAY_SPLASH_SCREEN=0`) is only permitted under a paid JUCE
  licence, so this project does not do it.
* **VST 3 SDK 3.8.0** (bundled in JUCE) — **MIT**. Nothing to sign.
* "VST" is a trademark of Steinberg Media Technologies GmbH; Cubase is a
  product of Steinberg. This project is not affiliated with or endorsed by
  Steinberg or by Raw Material Software.
