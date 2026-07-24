# MagLava (v4) — C / Raylib 3D port

A standalone, single-executable 3D remake of **MagLava**, the top-down
"color swing" climbing game (originally a TypeScript/Phaser game by Sweet
Papa Technologies). Built for FloppyJam: it fits on a 1.44 MB floppy, has no
external asset files, and targets Windows, macOS, and Linux.

**Rise or Burn.** You don't jump — you *swing*. Fire your magnetic tether at
a color-matched node to arc toward it, chaining swings for combos while the
lava rises beneath you. Same rules, levels, and physics as v1/v2/v3 and the
original.

## Build

```sh
cmake -S . -B build
cmake --build build
./build/maglava
```

The default configure is size-optimized (`MinSizeRel`, dead-strip, static
raylib) and a post-build step **fails if the executable exceeds 1,474,560
bytes** (a 1.44 MB floppy). Current release size is ~590 KB.

Development build with warnings and debug symbols:

```sh
cmake -S . -B build-debug -DMAGLAVA_RELEASE=OFF
cmake --build build-debug
```

Requires raylib 5.x/6.x (`brew install raylib`, or your distro's package). A
static `libraylib.a` is preferred so the result is self-contained.

## Controls

| Action | Keys |
| --- | --- |
| Tether to **RED** node | W / ↑ |
| Tether to **BLUE** node | S / ↓ |
| Tether to **YELLOW** node | A / ← |
| Tether to **GREEN** node | D / → |
| Pause | Esc |
| Restart / retry | R (pause or complete screen) |
| Mute | M (pause) |

Press the color of the nearest node in the direction you want to travel; the
tether pulls you into an arc. Chain swings without stopping to build a combo
multiplier (up to 10×). Don't fall behind — the lava never stops, and
backtracking downward surges its speed 1.5×.

## Stars

- ★ Finish the level
- ★★ Beat the par time
- ★★★ Finish with zero deaths

## Hazards & anomalies

All five original hazards are here — **sweepers**, **pulse rings**, **laser
gates**, **roamers**, and **magnet mines** — plus the level anomalies:
**flashlight** (Cave of Shadows), the **AI rival race** (The Race), and the
**rotating camera** (Spinning World).

## Feel & feedback

The simulation is a faithful 1:1 port; everything below is presentation-layer
polish layered on top of it (no gameplay rules were changed):

- **Target badges & reticles** — each of the four keys shows a contracting
  ring and a `W/A/S/D` badge on the node it would tether to right now, so the
  direction→color mapping is readable at a glance.
- **Combo feedback** — the landing tone pitches up as the combo climbs,
  `+points` popups float off each node, and a draining bar shows the combo
  timeout.
- **Slow motion** — death drops to 0.3× and checkpoints hitch to 0.55×,
  easing back to full speed.
- **Motion ribbon** — a tapering trail follows the player, tinted by the
  armed color.
- **Camera** — velocity look-ahead, gentle horizontal follow, and a subtle
  dolly-out with speed; shakes on landings, deaths, and lava surges.
- **Lava tension** — an edge-only danger vignette (never the center), a
  rumble cue that quickens as it closes, red-hot shaft walls, a wavy crust,
  and rising embers.
- **Readability** — HUD text and the key cluster sit on dark plates, the
  score counts up smoothly, and a right-edge rail tracks your climb against
  the lava.
- **Transitions** — fade-ins on level start and respawn, plus a level-name
  intro card.

Note: all environment geometry (lava, walls, embers) is kept *behind* the
play plane so it can never occlude the player.

## Architecture

The core simulation is deliberately isolated from rendering so it can be
unit-tested headlessly (no GPU):

| File | Role |
| --- | --- |
| `src/maglava.h` | Shared constants, data structs, and the pure sim API |
| `src/sim.c` | Authoritative gameplay: swing physics, lava, hazards, scoring, checkpoints, anomalies (no raylib) |
| `src/levels_gen.h` | The 25 levels, baked from the JSON files by `tools/gen_levels.py` |
| `src/render.c` | 3D presentation — projects the 2D play plane into a neon shaft |
| `src/audio.c` | Fully synthesized sound effects (no audio files) |
| `src/ui.c` | HUD, title, level select, pause, and results screens |
| `src/save.c` | Persistent star/unlock progress |
| `src/main.c` | Window, fixed-60 Hz loop, input, screen state machine |

### Levels

The level data lives in `levels/*.json` — the **same files** used by the
other ports, so content stays compatible. `tools/gen_levels.py` bakes them
into `src/levels_gen.h` (embedded C structs), applying the original
coordinate scaling so the shipped binary needs no JSON parser or data files.
Regenerate after editing a level:

```sh
python3 tools/gen_levels.py
```

## Tests

Headless simulation tests (a scripted climbing bot plus lava/scoring checks):

```sh
cmake -S . -B build-debug -DMAGLAVA_RELEASE=OFF
cmake --build build-debug --target maglava_sim_test
ctest --test-dir build-debug --output-on-failure
```

## Rendering notes

Gameplay math runs in the original 2D game-space (x∈[0,540], y∈[0,5000],
with *decreasing y = up*). `render.c` maps that plane into 3D: the play plane
is `z=0`, walls recede behind it, magnets are glowing spheres, the rising
lava is an emissive slab climbing toward you, and the camera looks up the
well and follows the player with a smoothed lerp. It's a true 3D scene, but
every gameplay decision is 1:1 with the original 2D game.
