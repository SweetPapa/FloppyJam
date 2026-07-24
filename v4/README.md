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
- **Momentum** — a single eased speed curve drives the whole presentation. It
  deliberately hugs zero while you hang on a node (heavy, coiled, waiting)
  and ramps hard once you commit, so acceleration reads as a release. It has
  a fast attack and slow release, so motion blooms instantly and lingers.
- **Anime speed lines** — screen-space streaks radiating from a vanishing
  point thrown ahead of your travel, re-scattering ~18×/sec for a hand-inked
  look. Weighted hard toward *acceleration* rather than raw speed, so they
  punctuate each launch and fade, instead of becoming permanent furniture.
- **Rush streaks** — in-world dashes scrolling past along the travel axis, so
  speed is felt in the shaft itself.
- **Motion ribbon** — a tapering trail follows the player, tinted by the
  armed color; swells into a comet at speed and thins to almost nothing at
  rest.
- **Camera** — velocity look-ahead, gentle horizontal follow, and a dolly +
  FOV kick that widens with momentum (tight when still, thrown wide on
  launch), plus high-speed micro-jitter; shakes on landings, deaths, and lava
  surges.
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

## The world

The shaft is generated, not authored. A per-level palette (a base hue plus
its complement) colours a duct assembled bay by bay from `hash(level, bay)`,
so every level looks like its own place while staying one coherent world:

- **Panelled walls** with per-bay shading, and mirrored service hatches
- **Ventilation fans** turning slowly deep in the wall, light spilling from
  their housings — the motif the whole look hangs on
- **Light conduits** running the full height of both walls with pulses
  travelling up them
- **Chevrons** stencilled on the walls, pointing the way up
- **Drifting motes** at mixed depths for genuine parallax
- **Depth fade** so the duct recedes into darkness above and below

Everything is mirrored left/right, so it reads as engineered and symmetrical.

## Soundtrack

The music is generated live — there are no audio files anywhere in the
project. `src/music.c` is a small tracker: a 16th-note sequencer driving
kick, snare, hat, bass, a plucked arp and a six-oscillator pad through a
tempo-synced ping-pong delay and a `tanh` saturator.

The formula that keeps it musical for *any* seed:

> **root note + 7-note scale + a hand-picked diatonic chord progression**

Every melodic voice draws only from the current chord's tones, so it can
never land on a wrong note. The seed (the level id) picks the key, scale,
progression, tempo, groove and arp pattern — never whether the harmony is
valid. Per-bar variation is re-derived from `hash(seed, bar)`, so it keeps
evolving and never audibly loops. Lava proximity and your own speed drive an
intensity control that opens the filter and thickens the percussion.

Audition any level's theme without launching the game:

```sh
cc -O2 -I src tools/music_preview.c src/music.c -lm -o music_preview
./music_preview 12 40 level12.wav     # seed, seconds, output
```

It prints the theme it generated, e.g.
`key=B dorian  prog=[0 6 3 4]  bpm=102  groove=1  arp=1`.

## Architecture

The core simulation is deliberately isolated from rendering so it can be
unit-tested headlessly (no GPU):

| File | Role |
| --- | --- |
| `src/maglava.h` | Shared constants, data structs, and the pure sim API |
| `src/sim.c` | Authoritative gameplay: swing physics, lava, hazards, scoring, checkpoints, anomalies (no raylib) |
| `src/levels_gen.h` | The 25 levels, baked from the JSON files by `tools/gen_levels.py` |
| `src/render.c` | 3D presentation — projects the 2D play plane into a neon shaft |
| `src/audio.c` | Synthesized sound effects + the music stream (no audio files) |
| `src/music.c` | Procedural soundtrack generator (pure C, renderable offline) |
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
