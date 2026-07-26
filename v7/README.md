# VOLLEYBAR (v7)

**Pong grew rods. Foosball learned to glow. Two players, six bars of light, one
ball that remembers every hit.**

A top-down table of smoked glass over a void. Each player commands three
glowing rods — goalie, defence, attack — interleaved down the table exactly
like foosball, so your attack bar lives *behind* their defence and a lazy
clearance feeds their forwards. The ball is a comet: every strike makes it
faster, brighter and hotter until goals land like lightning.

One stick and one button plays the whole game. Pins, snaps, chips and english
are waiting for anyone who wants to go deeper. Casual and competitive are not
difficulty settings here — they are two doors into the same room.

Single native executable, no asset files of any kind. Every pixel of the table,
every bar of light, every impact and the whole four-layer synthwave score is
generated at runtime. The four GLSL shaders are compiled-in strings.

## Build

```sh
make            # release: -Os -flto, stripped
make run        # build and play
make graybox    # the pre-beauty build: rectangles, no shaders
make debug      # -O1 -g, no LTO
make check      # tests + contrast probe + build + size gate. Merge-blocking.
make size       # shipped bytes against the 1,474,560-byte floppy ceiling
make clean
```

Needs a C11 compiler and raylib 5.x/6.x (`brew install raylib`, or your distro
package). The Makefile finds raylib through `pkg-config` and falls back to
`-lraylib`. macOS and Linux build out of the box.

| build | size |
| --- | --- |
| dev, shared raylib | 118 KB |

The web build is first-class, not a port — `app_frame()` has been the only
per-frame function since the first commit, and `main.c` is the only file that
knows which platform is driving it:

```sh
make web RAYLIB_WEB=/path/to/emscripten-raylib   # -> build/web/index.html|.js|.wasm
```

Windows x64 cross-compiles with MinGW-w64 against a MinGW raylib:

```sh
make windows RAYLIB_WIN=/path/to/mingw-raylib    # -> volleybar.exe
```

## Controls

Gamepads are preferred, the keyboard is a peer rather than a fallback, and any
mix works at the same table. Every key is remappable in OPTIONS → KEYS.

### GLIDE — the leisure door, and a complete way to play

| Action | Keyboard (P1 / P2) | Pad |
| --- | --- | --- |
| Slide **all three** of your bars | `W` `S` / `↑` `↓` | left stick / d-pad |
| FLICK | `F` / `RSHIFT` | A |

That is the entire scheme. Angle assist bends your shot modestly toward the
best open lane, soft touches bunt themselves, and with the assist on you cannot
flick into your own goal. A first-timer should score inside ninety seconds.

### GRIP — the skill door

| Action | Keyboard (P1 / P2) | Pad |
| --- | --- | --- |
| Steer the **active** bar | `W` `S` / `↑` `↓` | left stick |
| Change bar | `Q` `E` / `,` `M` | shoulders |
| FLICK (tap) | `F` / `RSHIFT` | A |
| CHARGED FLICK | hold `F` after the jab, release | hold A |
| PIN (hold as it arrives) | `G` / `/` | X |
| SNAP | release PIN | release X |
| BUNT | tap PIN | tap X |
| CHIP | `R` + FLICK / `.` + FLICK | Y + A |

Auto-handoff puts the bar the ball is coming at under your stick; the shoulders
override it. Inactive bars hold position and still block — where you left them
is part of the craft.

**No spinning.** Every strike has windup and recovery frames. Commit to a flick
and miss, and the lane behind it is open for real, and it glows so both players
can read it.

## The heat

Every struck ball climbs one step, 0 to 12. Heat drives the ball's speed, its
colour from deep cyan to white-gold, the length of its trail, the crowd, and
which of the four music layers are playing. Bunts and pins take a step off; a
goal resets it.

From step 8 up, every struck ball holds a **120 ms shimmer** before it flies —
the mercy beat, and the reason a maximum-heat exchange is a crescendo rather
than a coin flip. Goals scored at step 9 or better are **SCORCHERS**: bigger
detonation, tracked on the stat card, and worth exactly the same one point.

## Modes

| | |
| --- | --- |
| **VERSUS · STANDARD** | 1v1 or 2v2, locked rules, no mutators. The competitive door. |
| **VERSUS · PARTY** | Same table, eight mutators, Table Tilt on. The ruckus door. |
| **RALLY** | Co-op keep-up. No goals, one shared streak, no pressure. The leisure door, and secretly the best training mode. |
| **GAUNTLET** | Ten personalities and a finale, three difficulty tiers, cosmetic unlocks. |
| **TRICKSHOT** | Twenty posed shots, medals for doing them in fewer tries. |

**Table Tilt** is the handicap system, negotiated out loud before the match:
paddle size, rod speed, assist strength, a personal heat cap, a wider goal to
defend. A shark and their grandparent set the tilt like golf strokes and then
play a real match where both are trying. The tilts show as badges on the HUD —
visible house rules, no hidden sandbagging. STANDARD strips every one of them.

## What the tests actually check

`make check` is merge-blocking and runs headless: no window, no GPU, no audio
device, no clock.

```
make test       239 checks — the sim and the rules
make contrast   575 checks — the readability probe and the comfort caps
```

- **Determinism.** One seed played a thousand times must produce a
  bit-identical match, and two sims forked from one snapshot must not diverge
  over two thousand ticks. This is what makes the replays real: a replay is not
  an approximation of the rally, it *is* the rally, run again from the recorded
  inputs.
- **Tunneling.** 57,267 shots at 5.32 units/second — a ball crossing its own
  radius every tick — fired dead at a paddle from every angle and every point
  across its face, and swept across bar speeds so a bar closing on the ball is
  covered too. Zero may pass through.
- **The open-lane law.** Every rod, at every offset it can reach, under every
  Table Tilt anyone can set, must leave a lane of at least 1.25 ball widths.
  The goalie may never seal its own mouth.
- **Containment.** Two minutes of both sides slamming their bars at random:
  the ball never leaves the table except through a mouth.
- **The table is fair.** Mirror the match through `x -> -x` and the two sides
  swap onto each other exactly, because the interleave is symmetric. So the
  same inputs handed to the other player must produce the mirrored ball, tick
  for tick — 45 seconds of it, at zero drift. This is the test that earns the
  word "locked" in the STANDARD ruleset, and it found two real side biases: the
  english term and the snap's aim wedge both pointed the wrong way in y for one
  end of the table.
- **The strike vocabulary.** A flick raises heat one step and outruns a bunt; a
  swept bar imparts english and a still one does not; the snap is faster than
  the flick and straighter than a curve; the bunt cools the ball; the chip
  leaves the glass.
- **The foosball law.** The referee pulses on possession that never advances,
  and a pin that is never released is forced out by its shot clock.
- **The AI honesty audit.** It cannot push a stick past its stops, press a
  button that does not exist, or move a rod it does not own — checked over
  twelve thousand ticks of live play. Difficulty is verified to change only
  reaction latency and aim noise.
- **The contrast probe.** The ball's outline against every spectacle surface
  the renderer can put under it — six palettes, thirteen heat steps, seven
  surfaces including a goal detonation flooding the void and a scorcher
  white-out. Worst case, not average case. Also: the two sides always differ in
  *luminance*, not only in hue, so a player who cannot separate the colours can
  still separate the bars.
- **The comfort caps.** An effect asking to strobe black-to-white at 60 Hz
  comes out at 0 Hz, while a one-second fade passes through untouched.

## Where the tuning stands

§14 is an ordered protocol whose every gate is a feel test on real humans, so
what follows is what can be measured without them — AI against AI, which
bounds the shape of the game but cannot pass §14's gates on its own.

| | |
| --- | --- |
| Step 0 crosses the table in | 2.11 s (§5.4 asks ~2.1) |
| Step 12 crosses in | 0.60 s (§5.4 asks ~0.55) |
| Reaction latency across the ladder | 179–316 ms (§7 asks 180–320) |
| Mean rally, tier 2 | 35–47 contacts |
| GRIP win rate vs GLIDE, same personality, both ends | 50% / 45% / 60% by tier |

That last row is the one to read carefully. §14.6 wants a great GRIP player to
beat a great GLIDE player about 70/30, and GRIP's advantage does grow with
skill here — but 60% is not 70%, and AI self-play cannot settle it. This
opponent reaches for pins, chips and charged flicks *stochastically*; a human
reaches for them because of what the other player just did. GRIP's ceiling is
where the spec put it — the snap is still the fastest shot in the game, the pin
still stops the ball dead, the chip still goes over a bar — and per §14.6
nothing here was solved by nerfing it. What the numbers do establish is
Pillar 1: GLIDE takes points off GRIP at every tier, so the leisure door is a
real way to play and not a tutorial.

The three gates that need humans — the 90-second test, the filmed 15-contact
rally, and the GLIDE/GRIP equity check — are open.

## Decisions

Four places where this build made a call the spec left open or where following
it literally produced a worse game. Marked in the source at the point of the
decision.

**The size ceiling.** §0 says none is imposed. It ships under one anyway —
1,474,560 bytes, the FloppyJam rule shared with v1–v6 and enforced by
`make size`. Exactly the call v6 made. Nothing here was cut for bytes; the
procedural mandate meant it never cost any. Current headroom: 1,321 KB.

**Heat counts blows struck, not contacts.** §4 says every paddle contact raises
the ball one step. With twelve paddles on the table, that pinned every rally at
heat 12 within seconds and then ping-ponged the ball between the middle bars
forever, with nothing able to slow down enough to be threaded past anything —
nobody scored, ever. A deflection off a bar nobody was steering now bleeds
energy (restitution 0.65 of what arrived, §5.2 exactly) and does not add heat.
The meter reads as the drama of the exchange rather than as a contact tally,
and a deadened ball becomes something you can pick up and work. §0 resolves
conflicts in favour of §5 and then the Pillars; this is that resolution.

**Charged flick is a jab you lean on.** §3.2 has FLICK as a tap and CHARGED as
a hold of up to 0.6 s, which cannot both fire from the same press without
putting 150 ms of latency on every tap. So the flick fires on press, and
holding *after* it recovers winds the bar up visibly; releasing throws the
charged shot. Tap stays instant, the hold still reads as a windup.

**The chip lands short of §5.3's number.** It clears exactly the next rod plane
and comes down in the lane behind it. "Lands 2.5 rod-gaps downfield" would have
carried it past the rod after that as well, which contradicts the same
sentence's "clears exactly the next rod plane". The gameplay rule won.

## Module map

```
v7/
  Makefile  README.md  web/shell.html
  src/
    main.c app.c        state machine, app_frame(), modes, input
    sim.c/.h            Section 5 exactly; pure; headless-testable
    rods.c/.h           rod state, GLIDE/GRIP, the five verbs
    heat.c/.h           the ladder, the mercy beat, scorchers
    rules.c/.h          rulesets, serve, clocks, scoring, mutators, Tilt
    ai.c/.h             ten personalities, the latency model
    fx.c/.h             the world drawn; particles, detonations, the contrast law
    shaders.h           table glass, ball halo, crowd bokeh, goal ripple
    palette.c/.h        the colour law; raylib-free so the probe can test it
    synth.c/.h          every sound, and the generative score
    ui.c/.h             menus, HUD, stat card, the Tilt negotiation screen
    replay.c/.h         input recording, best-rally selection, playback
    save.c/.h           one small local file
  tests/
    test_sim.c  test_rules.c  test_contrast.c
```

`sim.c`, `rods.c`, `heat.c`, `rules.c`, `ai.c` and `replay.c` are raylib-free
and build standalone. That is not a tidiness preference — it is what lets the
suites run a thousand matches on a CI box with no GPU, and it is the boundary
the §13 parallel lanes were meant to be cut along.

## Development flags

```sh
./volleybar --graybox              # no shaders, flat rectangles, same sim
./volleybar --demo                 # both sides on the AI, straight into a match
./volleybar --demo --shot 400 --out frame.png
```

`--shot` renders N frames, saves one and exits, so CI can prove the renderer
still produces a frame and a beauty-pass change can be looked at without anyone
being at the machine.

The gray-box build is kept forever on purpose. It is the build the rally was
tuned in, before a single shader existed, and it is the standing proof that the
fun predates the glow.
