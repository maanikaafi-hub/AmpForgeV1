# Maani Mechanical Metronome

A complete native Android metronome app in Kotlin and Jetpack Compose. The interface is an illustrated dark-walnut mechanical instrument with an engraved 40–208 BPM scale, sliding brass weight, pendulum, tactile transport controls, six time signatures, volume, and three original synthesized tick voicings.

## Requirements

- Android Studio Ladybug (2024.2.1) or newer
- JDK 17
- Android SDK Platform 35 and Build Tools 35.0.0
- Android 8.0 / API 26 or newer device or emulator

No network, microphone, storage or notification permission is requested. The sound is synthesized locally; preferences stay on-device.

## Open and run

1. Open the `android/` directory in Android Studio.
2. Allow Gradle sync and install the Android 35 SDK platform if prompted.
3. Select the `app` run configuration and deploy to a device (API 26+).

Or from a shell with JDK 17 and Android SDK configured:

```bash
cd android
./gradlew testDebugUnitTest
./gradlew assembleDebug
```

The installable debug APK is written to:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

Install it over USB with:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

A GitHub Actions workflow (`.github/workflows/android-metronome.yml`) runs the JVM tests, builds the debug APK, and publishes that APK as a 30-day workflow artifact.

## Release APK

The release build can be created unsigned for local inspection with:

```bash
./gradlew assembleRelease
```

Output: `app/build/outputs/apk/release/app-release-unsigned.apk` unless an optional signing configuration is supplied. To create an installable, distributable signed build, create a private keystore outside source control, then create `android/keystore.properties` (this file and `*.jks` are git-ignored):

```properties
storeFile=/absolute/path/to/your-release-key.jks
storePassword=YOUR_STORE_PASSWORD
keyAlias=YOUR_KEY_ALIAS
keyPassword=YOUR_KEY_PASSWORD
```

Then run:

```bash
./gradlew assembleRelease
```

Keep the keystore and passwords private and backed up. Never commit either. Android Studio's **Build > Generate Signed Bundle / APK** can also create/manage the signing key.

## Audio design

- `audio/TickSynth.kt` generates deterministic 48 kHz PCM-16 ticks from a short broadband escapement transient, resonant wood-body modes, damped inharmonic metal partials and a low pendulum-weight thump. Downbeats are a little louder, lower and longer, with an additional faint upper resonance. The three voicings change the synthesized timbre without sample files or licensing requirements.
- `audio/MetronomeAudioEngine.kt` owns a single `AudioTrack` on a dedicated Android audio-priority thread. It renders tick PCM before use and mixes it into small streaming blocks. Tempo deadlines are monotonic and advanced from the previous deadline to avoid cumulative drift; a missed sleep/resume interval is resynchronized instead of generating a burst. Tick duration is below the shortest beat interval (208 BPM), so events never overlap.
- `audio/BeatSequencer.kt` provides deterministic measure position and first-beat accents, independently testable from Android.
- Beat callbacks contain their monotonic target time and update Compose's pendulum phase. Compose animation is presentation only: it does not schedule sound or determine beat timing.
- Android audio focus loss, transient loss and duck requests pause playback; the app does not resume unexpectedly. Playback stops when the activity backgrounds. The screen remains awake only during playback.

## Settings

BPM (40–208), time signature (2/4–7/4), volume and tick voicing are stored using private `SharedPreferences`. The plus/minus controls adjust one BPM at a time; drag or tap the engraved scale for direct tempo selection.

## Tests

`./gradlew testDebugUnitTest` covers the 40/60/120/208 BPM interval calculation, scale-position round trips, tempo labels, accent positions across all six meters, drift-safe deadline arithmetic, deterministic synthesized samples, accent level, and distinct voicings.

## Project layout

```text
app/src/main/java/com/maani/metronome/
  MainActivity.kt                 audio focus + screen-awake lifecycle
  audio/BeatSequencer.kt          pure measure/accent calculations
  audio/MetronomeAudioEngine.kt   dedicated AudioTrack scheduler/thread
  audio/TickSynth.kt              original, deterministic mechanical tick DSP
  data/MetronomePreferences.kt    local persisted settings
  model/Tempo.kt                  tempo math, markings, nonlinear scale
  model/TimeSignature.kt          2/4 through 7/4
  model/TickStyle.kt              synthesized sound voicings
  ui/MechanicalMetronome.kt       illustrated custom Compose Canvas
  ui/MetronomeScreen.kt           controls and responsive screen
  ui/MetronomeTheme.kt            walnut/brass palette
  viewmodel/MetronomeViewModel.kt state and UI/audio boundary
app/src/test/                     JVM unit tests
```

## Validation note

The repository's development container must have JDK 17, Gradle's distribution, Android SDK 35 and access to the Google/Maven dependency repositories to produce an APK locally. The CI workflow provides the same reproducible build on an Android-capable runner.
