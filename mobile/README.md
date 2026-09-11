# MagLava for iOS, macOS and Android

A native mobile version of v4: all **40 stages**, magnetic pendulum movement,
rising lava, checkpoints, five hazard types, three anomalies, stars, and personal
bests. The mobile apps live here; the desktop game and authored level files stay
in `v4`.

- **iOS 16+**: Swift, UIKit controls and menus, Metal 3D rendering, AVAudioPlayer,
  native haptics and UserDefaults. iPhone portrait; iPad also supports landscape.
- **macOS 13+**: Mac Catalyst using the same Swift/Metal game and sandboxed
  storage. Mouse controls, arrow keys, W/S/A/D or R/B/Y/G; Escape pauses.
  Up/W selects red, Down/S blue, Left/A yellow, and Right/D green,
  matching the Windows/Linux desktop controls.
- **Android 8+ / API 26+**: Kotlin, native Views, OpenGL ES 2.0 3D rendering,
  Choreographer frame pacing, MediaPlayer/SoundPool, haptics and SharedPreferences.
  Targets API 36; builds ARM64 and x86-64 libraries with 16 KB page alignment.
- **Shared C**: compiles `v4/src/sim.c` directly. No raylib, WebView, JavaScript
  engine, duplicated physics, or third-party runtime framework is required.

The original triangular controls are restored: **red above yellow / blue / green**.
The four large buttons select colors, not directions. Each node carries its
matching R/B/Y/G letter. Outlines show available targets. Taps are buffered until
an authoritative tick; unavailable colors preserve the tether. Rendering follows
the display while physics stays at 60 Hz, with player, camera, lava and hazard
interpolation. Menus suspend gameplay while music continues; app inactivity
suspends gameplay, rendering and audio. Reduced motion also follows the OS
preference and removes camera roll and the trail.

## Open and run

The home screen has **Play / Continue**, **Level Select**, **How to Play**, and
**Settings**, with a decorative swing rendered by the native game scene. The
preview never advances a level or changes saved progress, and freezes with
reduced motion. All 40 levels, chapter headings, stars and best times live in
Level Select. Back stays visible while browsing long menus.

How to Play adapts the original Vue/TypeScript card: an objective, the triangular
color guide, three short steps and star goals. Help and settings opened during a
run return to pause without restarting. Android Back and Mac Escape follow the
same menu hierarchy; from pause they resume play.

### iOS

Open [`ios/Maglava.xcodeproj`](ios/Maglava.xcodeproj), select the **Maglava** scheme,
choose an iPhone simulator, and Run. The checked-in project needs no project
generator or Swift package download. Python 3 must be available to its level
build phase. The project was built with Xcode 16.4.

```sh
# From the repository root:
xcodebuild -project mobile/ios/Maglava.xcodeproj -scheme Maglava \
  -configuration Debug -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath mobile/.build/ios CODE_SIGNING_ALLOWED=NO build
```

The bundle identifier is `dev.fofo.maglava` and the configured team is
`6Y5SZ2K5XY`. Signing keys and provisioning profiles are kept outside source
control. The icon, launch color, font license, and privacy manifest are bundled.
Choose **My Mac (Mac Catalyst)** to run the Mac version. Store archives require
Xcode 26 or later. Merges into `uat` build current store SDK archives and desktop
previews; see [CI operations](ci/README.md) and [store release status](store/RELEASE.md).

### Android

Open [`android`](android) in Android Studio. Install Android SDK 36, NDK
27.0.12077973 and CMake 3.22.1. Use JDK 17 or 21 and make Python 3 available to
CMake. Configure the SDK through Android Studio's `local.properties`, or export
`ANDROID_HOME` to your installed SDK. Do not commit machine-specific paths.

```sh
cd mobile/android
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk

# Unsigned release artifacts; configure your own signing for distribution.
./gradlew :app:assembleRelease :app:bundleRelease
```

The Gradle wrapper pins Gradle 8.14.3, Android Gradle Plugin 8.13.2 and Kotlin
2.2.21. Initial Gradle/SDK dependency resolution requires network access; the
game itself is offline and requests no network permission. The release App
Bundle is `app/build/outputs/bundle/release/app-release.aab`.

## Update the levels once

Edit **[`../v4/levels`](../v4/levels)**, including `campaign.json` for ordering,
stable keys, anomalies, and hints. Build either app. Both builds invoke
[`tools/generate_levels.py`](tools/generate_levels.py), which delegates all
coordinate conversion, ID hashing, obstacle defaults and validation to the
original `v4/tools/gen_levels.py` importer. It generates a private header in each
build directory. No copies of the JSON or desktop generated header are used.
Only desktop keyboard wording in hints is adapted to touch controls.

Original `1280px` and explicit `540px` formats retain their exact interpretation.
The current simulation's capacity is 40 campaign entries, 64 magnets and 64
hazards per stage. To add stages beyond 40, update the canonical `LEVEL_COUNT`
and canonical generator's campaign validation together; native lists use the
reported count automatically. Do not rename stable keys when reordering levels.
Stars, best time, and best score are stored under each stable key. The unlocked
frontier is derived from completed keys, so inserted earlier stages are playable.

Progress and settings survive app restarts. A live run resumes after a transient
interruption, with a manual Resume button. If the OS terminates the process, the
next launch returns to stage selection; in-progress runs are not serialized.
Desktop save files are not imported into these new mobile app sandboxes.

## Verify gameplay and native integration

```sh
cmake -S mobile -B mobile/.build/core -DCMAKE_BUILD_TYPE=Debug
cmake --build mobile/.build/core -j
ctest --test-dir mobile/.build/core --output-on-failure
```

This runs the original gameplay regressions, all 40 stages across five decision
profiles, the relaxed new-stage and Magnetic Subspace audits, and mobile-specific
input buffering, frame-rate equivalence (60/120/240 Hz), pause, invalid frame
deltas, bounded catch-up, campaign metadata, and respawn interpolation checks.

For iOS, choose **Product → Test**, or:

```sh
xcodebuild -project mobile/ios/Maglava.xcodeproj -scheme Maglava \
  -configuration Debug -destination 'platform=iOS Simulator,name=iPhone 16 Pro' \
  -derivedDataPath mobile/.build/ios CODE_SIGNING_ALLOWED=NO test
```

The XCTest verifies Metal frame submission, the triangular controls and original
soundtrack, then runs the first stage through actual UIKit color-button actions,
asserts completion and stable-key progress, advances to stage two, and verifies
pause, the application interruption callback, and retry. Debug-only entry points
allow `--level N` at launch. Release builds exclude these entry points.

For Android, start an emulator and run:

```sh
cd mobile/android
./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:lintDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w dev.fofo.maglava.test/dev.fofo.maglava.SmokeRunner
```

The standalone platform instrumentation runner prints `PASS` or `FAIL` with a
stack trace. It verifies JNI rate equivalence, confirms native GLES rendering,
checks the triangular control positions and decodes all four original songs,
finishes stage one using injected down/up touch events, checks saved stars, and
exercises next stage, retry, pause and actual activity backgrounding. It does not
require AndroidX or a downloaded testing framework. It writes ordinary test
progress in the installed debug app; use an emulator dedicated to development.
A debug Android launch accepts `adb shell am start -n dev.fofo.maglava/.MainActivity
--ei level 1` after stopping the app.

See [`VALIDATION.md`](VALIDATION.md) for the checks actually performed and limits.

## Shared frame contract

`shared/mobile_core.h` exposes an opaque game handle. Each instance belongs to a
single thread; both apps call it only from their UI/frame thread. A bounded
8-entry FIFO consumes one press per 60 Hz tick. Events are ORed across catch-up
ticks and cleared on the next rendered frame. Delta times are capped at 100 ms.
Pause discards queued input and accumulated time. Respawn snaps interpolation and
camera to the new checkpoint. Native renderers never mutate simulation state. Android publishes snapshots under
a lock to its GL thread; that thread owns its own scene and never reads the game
handle. Apple renders on the main thread and uses three Metal vertex buffers
protected by GPU completion semaphores.

A reusable 1,256-float snapshot crosses JNI once per frame; Swift uses the same C
API. The layout is versioned by source and must be changed on both platforms:

| Header offsets | Meaning |
| --- | --- |
| 0–3 | Interpolated player X/Y, lava Y, camera Y |
| 4–11 | State, won, score, combo, deaths, elapsed, checkpoint, immunity |
| 12–17 | Camera roll, magnet/hazard/checkpoint counts, stars, climbed height |
| 18–25 | Rival active/X/Y, race lost, events, attached/target indices, color |
| 26–29 | Progress fraction, par seconds, anomaly, lava clearance |
| 30–34 | Four predicted target indices, then level number |
| 35–38 | Last impact X/Y, impact age (-1 before any hit), presentation clock including respawn |
| 40 + i × 6 | Magnet X/Y, color, alive, goal, attached/targeted |
| 424 + i × 10 | Hazard type, X/Y, alive, angle, radius, laser phase, length, end X, polarity |
| 1064 + i × 3 | Checkpoint Y, reached, respawn magnet index |

Coordinates remain the original 540-unit shaft with decreasing Y upward.
The beveled triangular ship fits the player's 20-unit collision scale. Sweepers, pulse
rings, laser beams and mines use the simulation's exact dimensions. Roamers are luminous green ghosts with a tracking eye and spectral wisps,
with a solid body matching the canonical 22-unit collision radius. A gold ring marks the
exit, violet marks the rival, and darkness is a rendering mask for flashlight
stages. Native buttons stay outside the rotating world.

## Assets and maintenance

Barlow fonts and the OFL license are referenced directly from `v4/assets/fonts`.
The native app icon is drawn by `tools/generate_icon.swift`; the Android icon is
a vector drawable. `assets/Audio` contains small generated sound cues and **all four original authored
MagLava songs**, imported from the original `magLava` project. The playlist starts with
`magLava-main-theme`, continues through `magLava-game-bg-1`, `-2` and `-3`, and
wraps indefinitely. AVQueuePlayer and a prebuffered MediaPlayer pair advance
between complete songs. Home, settings, pause, results, retries and stage changes
never replace or pause the soundtrack. Only music mute, OS audio interruption or
app inactivity suspend it; returning to the foreground resumes at the same position.
Gameplay itself still requires Resume after an interruption. Music and effects
have separate settings; everything works offline.

The original OGG files are encoded directly to AAC-LC, 128 kb/s stereo at 44.1 kHz
with FFmpeg. All four full songs total 7,792,904 bytes (7.8 MB), 19.6% smaller than
the prior mobile AAC assets. `assets/Audio/soundtrack.json` records original and
packaged SHA-256 hashes, encoding settings, sizes and full durations (99–143 seconds).

`shared/presentation.c` owns the 3D geometry, perspective camera, surface lighting,
additive halos, lava mesh, particles, chapter palettes, magnet colors, soundtrack
playlist order and control rectangles. Metal and GLES only rasterize its triangles.
Both use a depth buffer, opaque geometry followed by additive effects, and the
same gameplay plane. Environmental geometry and lava remain behind that plane.
Reduced motion disables camera movement, trails, bursts, drifting embers, fan
rotation and lava ripples. Apple uses 4× MSAA when supported; GLES uses its native
single-sample framebuffer.

The shaft has recessed walls, service vents, fans, ribs, light conduits and
polished magnet collars and smooth, reflective colored cores. Soft layered
halos, orbiting sparkles, tether energy packets and capture bursts share one
effects implementation. Hits add two expanding shockwaves, glowing fragments,
spark trails, a short localized light pulse and a small camera kick. Persistent
impact metadata survives dropped frames, and its clock continues during respawn.
Reduced motion substitutes a quiet hit ring and freezes decorative animation.
The home screen uses a separate shared lava scene at 30 FPS, behind a matching
68% dark overlay on both platforms. It never advances gameplay while on a menu.
Dense scenes reduce mesh detail and budget decorative light after solid geometry.
Ten chapter headings organize the 40 stages, with six
recurring environment palettes. Native menus share wording, bounded content
width, control size and HUD spacing. Phones stay portrait; iPad and Android
tablets can rotate.

```sh
python3 mobile/tools/generate_audio.py # effects only
python3 mobile/tools/import_soundtrack.py ~/code/magLava # requires ffmpeg
swift mobile/tools/generate_icon.swift \
  mobile/ios/Maglava/Assets.xcassets/AppIcon.appiconset/AppIcon.png
python3 mobile/tools/generate_xcode.py # only when project structure changes
```

Platform references: [MetalKit](https://developer.apple.com/documentation/metalkit),
[GLSurfaceView](https://developer.android.com/reference/android/opengl/GLSurfaceView),
[Android 16 KB pages](https://developer.android.com/guide/practices/page-sizes).

## Preview graphics without booting an emulator

On macOS, the offscreen preview tool uses the shared scene and the production
Metal shader. It creates PNGs directly without a simulator or application window.
The `gallery` option is a synthetic snapshot fixture showing the four colors,
a roamer, a tether and lava; a stage number instead previews authored content.

```sh
cmake --build mobile/.build/core -j
swiftc -import-objc-header mobile/shared/presentation.h \
  mobile/tools/render_preview.swift mobile/.build/core/libmaglava_core.a \
  -o mobile/.build/render-preview
mobile/.build/render-preview mobile/.build/gallery.png gallery
mobile/.build/render-preview mobile/.build/gallery-reduced.png gallery reduced
```
