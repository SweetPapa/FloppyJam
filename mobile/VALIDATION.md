# Native mobile presentation validation

## September 11 — UAT, native captures and store media

The sections below are historical validation records. Current delivery status is
in [store/RELEASE.md](store/RELEASE.md), and the release procedure is in
[ci/README.md](ci/README.md). Local Android, iPhone and iPad integration passed
before recording stages 1, 6 and 38. Hosted Android, iPhone 17 Pro Max and
iPad Pro 13-inch (M5) integration passed in runs 1005, 1006 and 1010. All six C checks
passed on Windows and Linux; Mac Catalyst archives are universal, signed,
notarized and stapled. Windows signatures are verified and the package is checked
for accidental dependencies on separately installed Visual C++ runtimes.
Visual inspection caught an Android startup screen in one run 1006 capture;
the recorder now requires focused gameplay and successful GLES frames before
starting its recording clock.
The fresh-emulator diagnostics also caught Android replacing the activity during
resource setup. Test UI helpers guard against destroyed activity instances and
report assertions through instrumentation. Only the precise first-boot resource
change or launcher ANR permits one full rerun per cause. A controlled theme change
reported its different recreation flag safely, and ordinary integration/capture
passed afterward with the original 45-second touch deadline.
The initial Xcode 26 iOS archive has passed Apple processing and is assigned to
TestFlight groups. Temporary local capture emulators and simulators are closed.


## Menu refresh — September 10, 2026

- Reviewed the original Vue/TypeScript MainMenu and HowToPlay components and
  English tutorial strings before adapting the native screens.
- Home has Play/Continue, Level Select, How to Play and Settings. A decorative
  swing uses the native ship, tether, magnets and lava; reduced motion freezes it.
- All 40 levels, chapter headings, stars, best times and lock states live in
  Level Select. Back stays outside scrolling content on both platforms.
- The tutorial has a triangular color guide and three short steps. Help and
  settings opened from pause return to the current run without restarting it.
- Native integration checks cover menu separation, all 40 level buttons, Back
  after scrolling, pause/help/settings round trips, completion and saved progress,
  renderer activity, music continuity and backgrounding.
- Android and Mac Catalyst final integration checks passed. iPhone and iPad
  integration checks also passed during the menu work. Screenshots and logs are
  in `.build/menu-review/`.
- Shared presentation regression passed for three aspect ratios and reduced
  motion. The tablet run caught a constraint-ordering issue that was corrected
  before the successful rerun. The iPhone visual review also exposed help text
  widening its panel beyond the screen; a required viewport-width limit and a
  no-horizontal-overflow assertion were added.



Reviewed and updated September 9, 2026. Work is confined to `mobile`; the existing
v4/v6 edits and original `~/code/magLava` project were preserved.

## Ghost and effects refinement

The later September 9 effects pass replaces the crossed-box roamer with the
original game's luminous green eye, adds smooth surface normals and specular
highlights to spheres and collars, layered halos, spectral wisps, orbiting
sparkles, tether energy packets and twinkling shaft motes. Both native platforms
consume this single shared implementation. The ghost body uses the canonical
`ROAMER_SIZE` collision radius, and help text identifies green ghosts as hazards.

All Android emulators and iOS simulators were closed as requested and stayed
closed during this pass. Validation used offscreen Metal, C regressions with
AddressSanitizer/UndefinedBehaviorSanitizer, Android Debug/Release/AAB builds and
lint, and an unsigned iOS device Release build. All passed. The native UI
integration results below are from the preceding 3D port pass; they were not
rerun for the effects-only refinement.

The expanded presentation test covers multiple camera heights in all 40 stages,
including 18 stages with roamers, three aspect ratios, reduced-motion frame
stability, particle reset across stage/retry changes, and a synthetic scene with
64 magnets, 64 ghosts and 64 checkpoints. Decorative effects are budgeted after
solid geometry; crowded scenes reduce tessellation while retaining enemy bodies.

Offscreen previews use the production Apple shader and shared C geometry:
`mobile/.build/effects-review/ghost-and-orbs.png` and `reduced-motion.png`. The
preview gallery is a snapshot fixture rather than an authored campaign stage.
Build and sanitizer logs are in the same directory. `tools/render_preview.swift`
provides the reproducible preview command documented in the README.

## Reference review and changes

- Original `magLava/src/components/game/MobileControls.vue`: restored the red apex
  over the yellow / blue / green base. Both platforms now call the same C layout
  function and use the same shared magnet colors. Controls retain touch-down
  input, accessibility labels and range feedback.
- `v4/src/render.c`: restored a perspective 3D presentation of the existing 2D
  gameplay plane, with recessed shafts, ribs, vents, fans, luminous sockets,
  toroidal target rings, a beveled triangular player, trails, bursts, rippling
  molten surfaces and embers. Native Metal and GLES rasterize shared C meshes
  with depth testing. Six environment palettes recur across ten stage chapters.
- Original `magLava/src/assets/audio/songsforgame`: imported all four full authored
  songs to AAC for native offline playback, replacing the short synthesized
  stand-in. `assets/Audio/soundtrack.json` records source/output hashes and lengths.
  The title plays the main theme; stage tracks rotate consistently on both apps.
  Music and effects have independent toggles, with migration of the former mute
  preference. Pauses and background interruptions suspend audio.
- Matched HUD measurements, triangular tray, chapter headings, menu wording and
  maximum content width. Android uses immersive fullscreen and permits tablet
  rotation; iPhone remains portrait and iPad retains rotation support.

## Checks performed

| Check | Result |
| --- | --- |
| Shared presentation regression | Passed: all 40 stages at multiple camera heights, three aspect ratios and both motion settings; finite perspective vertices, no buffer overflow, input snapshots unchanged |
| Control layout regression | Passed: 320/390-point phones and 768/1024-point tablets; red above Y/B/G, targets inside tray and at least 48 points |
| Frame contract and original gameplay regressions | Passed |
| Campaign audit | Passed: 40 stages across five decision profiles; relaxed stages and deathless Magnetic Subspace checks also pass |
| AddressSanitizer + UndefinedBehaviorSanitizer | All six C tests passed |
| iPhone 16 Pro / iOS 18.6 integration | Passed: Metal frames, control positions, original music duration/assets, actual UIKit touch-down actions, completion, stable progress, next stage, pause, interruption and retry |
| iPad Pro 11-inch (M4) / iOS 18.6 integration | Same native integration test passed |
| Pixel 9 Pro XL / API 36 emulator integration | Passed: GLES frames, control positions, decoding all four full songs, JNI frame equivalence, injected touches, completion, progress, next stage, retry, pause and backgrounding |
| Soundtrack packaging | All four AAC files match byte-for-byte in Android Release and iOS device bundles; obsolete synthesized loop absent |
| Android Debug APK, Release APK and Release AAB | Built successfully |
| Android lint | 0 errors; 7 warnings (English-only strings and existing platform/orientation declarations) |
| iOS simulator and unsigned device Release builds | Built successfully with Xcode 16.4 |
| Mac Catalyst Release build | Built successfully with Metal |
| Visual review | Actual iPhone and Android gameplay inspected; geometry, labels, lava, control order and HUD refined from captures |

The expanded scene stress test peaks at 196,533 vertices against a 196,608-vertex
budget, including the synthetic full-capacity scene. Excess decorative halos
can be omitted without dropping solid bodies.
Camera trigonometry is cached per frame. Rendering culls distant stage objects;
physics remains at the original fixed 60 Hz. Apple uses three GPU buffers guarded
by completion semaphores. Android's GL thread receives a synchronized snapshot
and never accesses the simulation handle.

Build/test logs and screenshots are in `mobile/.build/review`. C sanitizer outputs
are in `mobile/.build/core-sanitize`. Native integration tests exercise ordinary
input and write ordinary debug-app progress; they do not teleport or grant wins.

## Remaining device validation

This is simulator/emulator and build validation. Physical iOS/Android devices
still need a sustained frame-time/thermal, battery, speaker/headphone, haptic and
touch-comfort pass. Android tablet rotation is implemented but was not run on a
physical tablet. iPad integration ran in portrait. Metal uses 4× MSAA where
supported; GLES uses single-sample rendering, so edge antialiasing can differ.

No store upload was made for these changes. Earlier store screenshots and signed
builds depict the previous presentation; refresh them before distribution. See
[store/RELEASE.md](store/RELEASE.md) for the existing submission status.


## September 9 — continuous playlist, impacts, lava home and contrast refinement

- All six CTest checks passed, including the 40-stage campaign, at normal and
  AddressSanitizer/UndefinedBehaviorSanitizer settings. New assertions cover
  persistent hit origin/age through death and respawn, scene rendering after a
  dropped event frame, menu animation/freeze at three aspects, and playlist wrap.
- Actual Android instrumentation and iPhone XCTest passed: native touch
  completion, triangular controls, stable progress, retry, next stage, pause and
  app backgrounding. Both verify that menu pauses leave the current song advancing,
  stage/home transitions preserve it, all four playlist entries advance and wrap,
  and app inactivity freezes playback. Home geometry continues rendering.
- Shared molten-current refinement passed native iPhone integration again and
  the complete normal/sanitized C suites. The final contrast-only change was
  built and visually inspected on both native platforms; no gameplay changes
  followed these tests.
- Final Android Debug APK, Release APK/AAB and lint succeeded (zero errors,
  seven existing warnings). iOS simulator Debug and device Release succeeded.
- Final iPhone and Android home screenshots were inspected with the same 68%
  dark overlay. The temporary iPhone simulator was shut down. The latest Android
  debug build remains open for the user's playtest.
- The four AAC songs retain original durations within 0.1 seconds and are byte
  identical between Android and iOS bundles. Full playlist: 7,792,904 bytes versus
  9,690,414 previously (19.6% smaller). Encoded directly from original OGG sources
  at 128 kb/s AAC-LC stereo, 44.1 kHz. Manifest hashes were checked against assets.
- Signed Android version 1.0 (2) verified with jarsigner, apksigner and aapt2;
  artifact hashes recorded in `.build/store/android-signing-2.json`.
- Evidence: `.build/playlist-review/ios-test.log`, `ios-test-final.log`,
  `android-test.log`, `android-release.log`, `ios-device.log`, `ios-dark-menu.log`,
  `asset-verification.json`, `android-home-dark.png`, `ios-home-dark.png`.
- Store state was inspected, not published: Play build 1 is still a draft;
  Apple has no builds. See `store/RELEASE.md` for build 2 artifacts and blockers.
