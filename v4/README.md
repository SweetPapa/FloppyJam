# Maglava — desktop edition

**Rise or Burn.** Climb an industrial shaft by throwing a magnetic tether at
color-matched nodes while lava rises below. The v4 desktop edition now has
**40 stages: the original 25 plus 15 new stages interwoven through the campaign**.
There is no executable or asset size limit.

## Build and play

From the repository root (CMake, Python 3, and raylib 5.x/6.x required):

```sh
cmake -S v4 -B v4/build -DCMAKE_BUILD_TYPE=Release
cmake --build v4/build -j
./v4/build/maglava
```

On macOS, raylib can be installed with `brew install raylib`. Static raylib is
preferred. Fonts, levels, procedural scenery, sound effects, and music are
embedded: the game does not need a network connection or loose runtime assets.
Distribute the font license alongside the executable:

```sh
cmake --install v4/build --prefix v4/dist
```

Development configuration:

```sh
cmake -S v4 -B v4/build-debug -DMAGLAVA_RELEASE=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build v4/build-debug -j
ctest --test-dir v4/build-debug --output-on-failure
```

## Controls

| Action | Input |
| --- | --- |
| Red tether | W / Up |
| Blue tether | S / Down |
| Yellow tether | A / Left |
| Green tether | D / Right |
| Pause / resume | Escape |
| Retry immediately | R |
| Sound on / off | M |
| Reduced motion on / off | V |
| Borderless fullscreen | F11 |
| Settings | Home button or F2 while paused |
| Exit | Home button or window close |
| Choose stage | Arrows / WASD; Enter or click a card |
| Switch campaign page | Page Up / Page Down; arrows also cross pages |
| Return to stage selection | Q while paused; Escape after completion |

Keys select **colors**, not spatial directions. Badges show the node each key
will target. Within the 400-pixel detection range, targeting prefers the nearest
matching node at least 40 pixels above you, falling back to the nearest matching
node in any direction. This prevents a close, already-passed node from trapping
a repeated-color climb. Pressing an unavailable color preserves your tether.
You can retarget during flight.

Arrivals carry tangential momentum into a damped pendulum swing. Time the next
launch to carry that momentum forward; wait briefly to settle into a calm aim.
The lava checks your anchor height while attached, preserving authored margins.
Backtracking can still trigger a lava surge.

Stars retain the original rules: one for finishing, two for beating par, three
for a deathless finish (even if slower than par). Times measure active simulation
time, excluding pause and respawn countdowns. Original-stage par times were recalibrated against the desktop simulation.
The 15 new stages allow at least 1.5 seconds per main-route landing plus six
seconds. Magnetic Subspace has a targeted 50-second par.

## Saved settings and languages

Home and Pause provide Settings. The lava rise rate defaults to **1.5x** the
level's authored speed; choose 0.5x, 0.75x, 1x, 1.25x, 1.5x, 2x or 3x. The rate
also multiplies temporary backtracking surges and survives retries/checkpoints.
Use 1x for the original pace. Difficulty does not change magnet movement or time.

Language follows the system by default. Choose English, Latin American Spanish,
Brazilian Portuguese, German, Turkish, Japanese or Simplified Chinese. Use
Up/Down to select a settings row and Left/Right or click its left/right half to
change it. Changes save immediately; Escape returns to the previous screen.
Home's Exit button closes the game through normal audio/graphics cleanup.

Version 3 saves preserve existing progress from versions 1 and 2 and add the lava
multiplier and language. Changing language never changes campaign/progress keys.
The shared catalog and font regeneration instructions are in `i18n/README.md`.

## Desktop improvements

- Fixed camera lens with modest look-ahead, aspect-aware framing, and time-based
  smoothing. Player motion interpolates between authoritative 60 Hz ticks;
  rendering follows the monitor refresh rate.
- Persistent input between ticks fixes missed presses on fast displays. Focus
  loss pauses the game. Respawns clear the trail and reset the camera.
- Graphite and muted blue structural panels, solid recessed columns, compact
  magnetic housings, a distinct white player, a fine motion ribbon, and a
  continuous molten surface. Bright colors identify gameplay objects.
- Removed screen-space speed lines, in-world rush streaks, acceleration zoom,
  constant high-speed jitter, spinning player sparks, and wire cages. Checkpoints
  no longer interrupt play with slow motion or a lava-colored flash.
- Roamers use amber crossed housings instead of looking like green magnets.
  Laser warnings use steady amber instead of flashing. The flashlight has a
  wider readable area. Reduced motion disables camera roll, impact shake, death
  slow motion, and full-screen color flashes; ambient motion remains.
- Barlow typography, compact HUD with the original W-above-A/S/D keyboard
  cluster and matching arrow glyphs, checkpoint progress rail, contextual stage
  hints, two pages of stage cards, mouse selection, personal best times and high
  scores, and redesigned title, pause, and completion screens.
- Every checkpoint respawns with a minimum 500-pixel anchor-to-lava clearance.
  Signal Crossing has two added checkpoints. Goal arrival cannot be overwritten
  by a collision in the same simulation tick.

## Campaign and progress

[`levels/campaign.json`](levels/campaign.json) controls order, stable keys, hints,
and anomalies. Original ordering is preserved; new stages alternate warm-ups,
branching climbs, hazard lessons, sprints, and late combinations. Original stages
with the placeholder name “New Level” now have distinct names.

See [campaign design and playtest results](docs/PLAYTEST.md) for the 15 additions,
measured results, and remaining limitations. The new stages retain their original
spacing, lava pressure, and obstacle patterns, with checkpoints every four
landings. Magnetic Subspace received a targeted correction: five separated pulse
rings, slower lava, four checkpoints, and safe clearance around every anchor.

Level JSON remains compatible with the other ports. CMake automatically rebuilds
`src/levels_gen.h` when JSON changes. To regenerate or check it manually:

```sh
python3 v4/tools/gen_levels.py
python3 v4/tools/gen_levels.py --check
```

Progress uses stable stage keys, so reordering cannot transfer stars to unrelated
stages. Desktop progress lives in `~/.maglava_desktop.dat` on macOS/Linux or
`%APPDATA%/.maglava_desktop.dat` on Windows. If it is absent, the game imports the
old `.maglava_save.dat`, maps all 25 original stages, and unlocks inserted stages
up to the old frontier. The old save remains untouched. New saves use temporary
files and replacement; malformed/truncated files load safe defaults. Sound and
reduced-motion preferences are saved with progress.

`MAGLAVA_SAVE_PATH` overrides the path for isolated testing. Demo and screenshot
runs never write progress or preferences.

## Reproducible playtests

```sh
ctest --test-dir v4/build --output-on-failure
./v4/build/maglava_campaign_audit --strict > campaign.csv
./v4/build/maglava_campaign_audit --strict --new-relaxed > relaxed.csv
./v4/build/maglava_campaign_audit --strict --deathless --new-relaxed --level-key level-6b
bash v4/tools/playthrough_screenshots.sh 1,7,15,19,23,27,29,35,39,40
```

The headless audit uses five input profiles, checks finite physics, and requires
all 40 stages to finish in every profile. A second audit gives the 15 new stages
1–2 seconds between decisions and requires at most two retries per run.
Magnetic Subspace must clear all four slower profiles without deaths; its
landings are also tested through a complete pulse cycle with arrival spin.
Additional tests cover targeting,
unavailable colors, pendulum settling, lava death, every checkpoint respawn,
goal finality, anomaly assignment, save migration, and malformed saves.
Native screenshot runs require a desktop/GPU. Optional diagnostics:

- `MAGLAVA_DEMO_DELAY_MS=1500`: pause before each demo decision (default 250 ms).
- `MAGLAVA_PROFILE=1`: print mean, 95th percentile, and maximum frame time on exit.
- `MAGLAVA_SHOT=120`: capture a frame and exit.
- `MAGLAVA_SHOTDIR=/absolute/path`: screenshot destination (create it first).
- `MAGLAVA_SCREEN=select MAGLAVA_CURSOR=39`: read-only menu capture with `MAGLAVA_SHOT`.
- `MAGLAVA_COMPACT=1`: use the minimum 960 × 640 window.

The music remains a live procedural tracker with seeded harmony, percussion,
filtered arpeggios, and crossfading themes. Audition it independently:

```sh
cc -O2 -I v4/src v4/tools/music_preview.c v4/src/music.c -lm -o /tmp/music_preview
/tmp/music_preview 12 20 /tmp/theme.wav
```

Barlow Medium and SemiBold are by Jeremy Tribby, distributed under the
[SIL Open Font License](assets/fonts/OFL.txt). Font data is embedded during build
by `tools/embed_fonts.py`; no system font installation is needed.
