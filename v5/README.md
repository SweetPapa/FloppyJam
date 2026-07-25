# BREAK PAR (v5)

**Mini golf with a cue stick. Pool with a landscape. One ball, eighteen holes, zero mercy.**

It looks like mini golf — eighteen whimsical 3D holes with ramps, bumpers, gaps
and gimmicks. But you are not putting, you are shooting pool. You walk the cue
around the ball, pick your line, pick your power, and pick your **english** —
where the cue tip strikes the ball. Draw the ball back off a bank. Ride running
english around a corner. Smash a combo through an object ball into a gold pocket
worth a stroke. Sink the cup in the fewest strokes. Sometimes going crazy helps;
other times tact and skill is the whole answer.

The table is set up in the middle of a city at two in the morning: neon rails,
festoon lights on poles, a wet plaza with traffic circling it, a skyline that
is different on every hole, a ferris wheel somewhere over your shoulder, and a
jazz trio playing the whole time.

Single native executable, no asset files of any kind. Every mesh,
palette, texture and sound is generated at startup or synthesised at runtime,
and all eighteen holes are compiled-in C arrays. The skyline included — it is
seeded off the hole index, so hole 7 has the same view every time you play it
and nobody ever shipped a bitmap of it.

## Build

```sh
make            # release: -Os -flto, stripped
make run        # build and play
make debug      # -O1 -g, warnings, no LTO
make size       # prints the total fileset and fails over 1,474,560 bytes
make clean
```

Needs a C99/C11 compiler and raylib 5.x/6.x (`brew install raylib`, or your
distro package). The Makefile finds raylib through `pkg-config` and falls back
to `-lraylib`. macOS and Linux build out of the box.

Windows x64 is the primary target for the 1.44 MB contest. Cross-compile with
MinGW-w64 against a MinGW raylib:

```sh
make windows RAYLIB_WIN=/path/to/mingw-raylib     # -> breakpar.exe
```

That link line is `-static -static-libgcc -mwindows`, so the result runs on a
stock Windows 10/11 machine with no runtime installs and no DLLs beside it.
Sizes against the 1,474,560-byte ceiling. The dev build links a system raylib;
a shipped build is statically linked, with raylib trimmed by
`../scripts/build-raylib.sh` (the game loads no models, fonts or audio files, so
every decoder in raylib is dead weight — untrimmed, Windows comes to 1,837,056
and blows the ceiling):

| build | size |
| --- | --- |
| dev, shared raylib | 170 KB |
| macOS universal, static | 1,166,416 |
| Windows x64, static, with icon | 1,432,064 |

## Controls

Keyboard-and-mouse first; **keyboard alone is fully playable**.

| Action | Input |
| --- | --- |
| Aim | `←` / `→` — detented, one audible click per notch. Tap for exactly one click, hold to sweep |
| Fine aim | hold `SHIFT` (same detents, six times smaller) |
| Invert aim | OPTIONS → INVERT AIM swaps which arrow turns the cue left |
| Change camera view | `↑` — cycles over the shoulder → high angle → overhead plan → down the cue |
| Aim the cue at the hole | `↓` — points the cue straight at it and re-frames the current view |
| Power | hold `SPACE`, release to strike — the meter climbs, falls, climbs |
| English (cue-ball face) | `A` / `D` left and right, `W` follow, `S` draw |
| Centre the english | `C` |
| Orbit camera | drag **any** mouse button, or `Q` / `E` to swing |
| Zoom (4 steps) | mouse wheel, or `Z` / `X` |
| Square the camera up behind your aim | `V` |
| Trajectory preview on/off | `P` (also in OPTIONS) — off by default |
| Skip the ball to rest | `R` during the ride — identical result, sim runs at full speed |
| Scorecard | `TAB` |
| Pause / restart hole / options | `ESC` |

The **arrow keys aim** (left/right rotate the cue) and the **left hand shapes the
shot**: `A`/`D`/`W`/`S` move the strike point on the cue-ball face while `Q`/`E`
swing the camera. The mouse is camera-only, so nothing fights over a button.
Aiming is detented rather than continuous: each notch clicks, so you can line
up off a rail and count three clicks left instead of nudging a slider and
hoping. A coarse detent is 0.60 deg (about 15 cm of aim at a far cup) and a
fine one is 0.10 deg (~2.5 cm), tight enough to thread a gap on the far side of
the hole. `↓` lines the cue up straight at the hole. The two aim keys can be
swapped in OPTIONS.

`↑` steps through four camera views, and the one you pick comes back for every
shot after it — pick a framing once and the game keeps giving it to you.

- **Over the shoulder** — the working three-quarter view.
- **High angle** — pulled back and up: read the whole line at once.
- **Overhead plan** — near-vertical, scaled to the distance to the hole, so a
  dogleg or a blind ledge can be read as geometry instead of guessed at.
- **Down the cue** — eye on the felt behind the ball, looking along the line.
  In this one the camera *is* the aim, so turning the cue turns the view and
  `Q`/`E` step aside until you leave it.

English resets to centre at the start of each **hole**, not each shot: leaving
your last spin dialled in is a small mastery reward and a small trap.

## Trajectory preview (optional)

`P`, or OPTIONS → TRAJECTORY. **Off by default.** With it on, the full path the
ball will take is drawn while you aim, with markers at each contact, faint
lines for any object ball that gets moved, a ring where the ball comes to
rest, and a one-line verdict — `IN THE CUP`, `SCRATCH`, or how far short.

It is not an estimate. The simulation is deterministic, so the preview copies
the world and plays the shot out with the real solver — `make testcourse`
asserts across 576 shots on all eighteen holes that the predicted end point
matches the played one to **0.00000 m** with zero outcome mismatches. The only
error left is your own timing on the power meter.

What it shows depends on what you are doing:

- **Holding SPACE** — the exact path for the power you would release at *right
  now*, updating live as the meter swings. Watch the line reach the cup and let go.
- **Aim settled** — the game hunts for the power that finishes nearest the
  objective and shows that line. This is the "if you hit it optimally" answer.
  The search is spread over several frames so it never costs a dropped frame
  (0.67 ms/frame live, and the hunt is 3 candidate sims per frame).

A note on why it is opt-in: the spec argues at length (4.3, 15) that the guide
should show geometry only and never spin, because learning what english does
*is* the game, and it explicitly says not to add an option to extend the guide.
That reasoning is sound and it stays the shipped default — but it is your call,
so the assist is here, one key away, and the save file remembers it.

## Safe route and hero line

Every hole has two answers. The **safe route** can be parred by a player who
only ever hits centre ball and pays attention to power — no pool technique
required, no leap of faith, everything visible from the survey camera. The
**hero line** is a shorter route hiding in the same geometry that saves one or
two strokes and needs a named technique: a bank, a draw, running english, or a
combo through an object ball. It always has a real cost when you miss — a
hazard, a scratch pocket, or simply terrible position — so "going crazy" is a
genuine strategy with a genuine downside rather than a free win. The two casino
holes (13 and 17) tilt that trade hard toward chaos on purpose; everywhere else
a missed hero shot should leave you playable, just worse off.

## The four pool systems

- **Object balls** — full physics peers of the cue ball. Blockers, combo tools, keys.
- **Gold pockets** — pot an *object* ball into one for **−1 stroke** (max 2 a hole).
  Pot the *cue* ball into one and it is still a scratch. Greed cuts both ways.
- **Scratch pockets** — cue ball in means +1 and a respawn where you were. An
  object ball in is simply gone for the rest of the hole, which occasionally
  opens the route you wanted.
- **Rack holes** (10 and 16) — the cup starts capped. Pot the 8-ball into any
  pocket and it opens. First you play pool, then you play golf.

## Feel and feedback

The strike scales continuously with power: crack brightness and pitch, a sub-thump
layered in above 42%, one crisp frame of hitstop, shockwave radius, dust count,
camera kick, a rim flash past 78%, and a word plus percentage that lands right
where the power meter was. You can tell how hard you hit it with your eyes shut,
and release is immediate — the ball leaves the frame after you let go.

On release the meter **freezes at the power you actually let go of** and holds
there, dimmed, with its label switched from `PWR` to `HIT`, until the ball comes
to rest. It used to empty the instant the ball left, which threw the number away
at exactly the moment you were watching the consequences and trying to work out
whether to hit the next one harder.

Aiming is **detented**. Each notch of `←`/`→` clicks audibly and ticks the compass
strip on the HUD, so a line can be counted rather than eyeballed, and `SHIFT`
subdivides the same notch for surgical banks.

There is a **breeze** on every hole — deterministic from the hole index, so a hole
always feels the same. It drifts motes across the green, leans and flutters the
flag, and pushes confetti around. It is deliberately **cosmetic only** and is never
read by `physics.c`: adding wind to the simulation would invalidate every tuning
gate, and a wind gauge that did not move the ball would lie to the player. So
there is no gauge — just weather.

Everything that pays off says so where it happened: bank three or more rails and
the count pops off the cushion you just hit, a gold pocket floats `-1 STROKE`
over itself, and a birdie or better brings a horn stab and a room full of people.
Frequent events wash the edge of the frame rather than the middle of it, so the
feedback never hides the table you are trying to read.

## Art direction

Night, neon, and nothing on disk. The felt walks green to twilight blue across
the eighteen exactly as it always did, but it now sits under a city: a banded sky
dome, a couple of hundred stars, three distance rings of buildings with lit
windows and rooftop signs, a wet plaza with ring roads and traffic on them,
searchlights, a blimp and a ferris wheel. Each hole draws a different one from a
hash of its index — same hole, same skyline, forever, for about two kilobytes of
table.

Close in, the table is an island of light: neon tubes along every rail, festoon
bulbs strung between poles all the way round, booths and speaker stacks at the
corners, and a beacon standing over the cup. Each hole also picks one **accent**
colour off a neon wheel, and that single number drives the flag, the beacon, the
plaza rings and the HUD trim together.

The whole backdrop is roughly five thousand triangles and costs about 2 ms of CPU
a frame, which keeps the integrated-graphics floor in the spec intact.

## Music

A swing jazz combo, generated a sample at a time and mixed in stereo through a
slap delay. Seven players: a walking upright with a filter envelope and a finger
transient, a Rhodes comping voice-led rootless shells on the Charleston, a swung
ride with a bell that only rings on the turnaround, brushes that never leave the
head, a feathered kick, a vibraphone, and a tenor with delayed vibrato and a
short glide between notes.

Three charts of **sixteen bars** each — a lazy major turnaround for the menus, a
bright minor ii-V-i for the front nine, and a slow minor blues for the back nine,
because by hole ten it should feel late. Sections rotate every chorus (head, horn
solo, vibes solo), so a nine takes you through the whole cycle without hearing
the same thing twice.

The melody is the part that took the work. Picking each note at random from the
right scale still says nothing, which is why the first version noodled. What runs
now commits to a **rhythmic cell** for two bars, follows a **contour** of mostly
stepwise motion, snaps to a chord tone whenever a bar turns over, and then
answers itself: the same cell comes back with the contour inverted. That is the
cheapest trick that makes a generated line sound composed.

```sh
make testmusic   # renders build/music-{menu,front,back}.wav and checks them
```

That harness stubs out raylib's audio calls, so the combo renders headless on any
build machine. It writes WAVs you can actually listen to, and asserts the failure
modes a generative bed is prone to — silence, DC offset, clipping, a dead channel,
and a chorus that repeats verbatim.

The stub deliberately models one raylib behaviour rather than being permissive:
`UpdateAudioStream` does not append, it fills one whole sub-buffer and **zero-fills
whatever the caller did not supply**. With the default sizing that sub-buffer is
`deviceSampleRate/30` — 1600 frames at 48 kHz — and the engine was writing 512.
Every chunk was 512 frames of music followed by 1088 frames of silence: a 32% duty
cycle gated at 30 Hz, which is exactly why the music sounded muffled while the
sound effects were fine (those are `Sound` objects and never touch the streaming
path). The fix is `SetAudioStreamBufferSizeDefault(MUS_FRAMES)` before
`LoadAudioStream`, with `MUS_FRAMES` large enough that raylib cannot quietly raise
it past what we generate. The suite now asserts zero short writes.

It has also earned its keep twice on the musical side: it
caught a chart-index bug that sent the front nine to the *menu* progression and
left the back-nine blues unreachable for the whole game, and a bass octave that
put the root at 27–33 Hz, below the low E of a real double bass and inaudible on
a laptop while still eating most of the mix.

## Physics

`src/physics.c` is the contract; everything else observes it. Fixed 1/240 s
timestep, semi-implicit Euler, adaptively substepped so the ball never advances
more than a quarter of the thinnest wall in the level per micro-step. The ball
carries a full 3D angular velocity and lives in the standard three billiard
regimes — sliding, rolling, at rest — so draw, follow and stun are *emergent*,
not scripted. Cushions apply a normal impulse plus a Coulomb-capped tangential
impulse at the contact point, which is where running and check english come
from. The cup has no magnetism: a dead-weight putt drops, a firm one can rattle
the lip and escape, a smashed one flies straight over.

No wall-clock anywhere in game logic; moving obstacles are pure functions of
ticks-since-shot-start and reset to phase zero every stroke, so what you see
while aiming is exactly what you get. The only randomness in the binary is a
cosmetic xorshift used for particles, never read by the simulation.

Tuning note: the friction constants are hotter than real pool cloth
(`μ_roll` 0.090 rather than 0.010, `μ_wall` 0.34 rather than 0.14, `k_spin` 5.0
rather than 2.5). BREAK PAR's holes are 10–16 m long, so a 50 % shot has to stop
in about 5 m rather than 60, and english has to read across a whole hole rather
than a 2.8 m table. Section 16's gates are feel tests, and these are the numbers
that passed them.

## Tests

```sh
make test         # headless physics suite — no window, no audio device
make testcourse   # geometry, preview fidelity, and a bot that plays all 18 holes
make testcamera   # camera smoothness, wall clearance, frame-rate independence
```

`make test` covers the Definition of Done: 1000 bit-identical re-sims of the
same shot, the power curve and stop distance, draw/stun/follow ordering and
draw's decay with distance, ≥15° of separation between max running and max
check english off a 45° bank, honest cup capture (dead weight drops, firm
rattles, smashed flies over), all four pool systems, the surface table, and
10 000 max-power shots into the thinnest rail with zero pass-throughs.

`make testcourse` builds every hole and asserts the tee, cup, pockets and object
balls all sit on real floor, that no wall is thinner than the substepper's
guarantee, and that every table fits its limits. It then runs a search bot over
all eighteen holes; every hole is completed, and every hole has a line the bot
finds in one or two strokes, which is the hero line existing in geometry rather
than in the design document. `./bp_test_course -h` re-runs it with a coarser
planner and human-sized execution error as a difficulty probe — it is a rough
signal, not a gate, because a greedy bot never plays for position.

`make testcamera` holds the eye to two invariants. It must not judder — measured
as frame-to-frame change in step size relative to the mean, over the opening
flyover and a full orbit on every hole. And it must **never end a frame inside
geometry**: 51,840 sampled frames, every hole at four pitches and four zooms
through a full revolution, zero inside. That number was 2,133 before the fix, so
the check is load-bearing rather than decorative.

## Layout

```
v5/
  Makefile
  README.md
  BREAK_PAR_spec.md
  src/
    main.c       loop and state machine (TITLE/SELECT/PLAY/CARD/FINAL)
    core.h       vector math and every tuning constant
    physics.c/.h the simulation; builds standalone, knows nothing about drawing
    course.c/.h  hole tables -> world, geometry queries
    shot.c/.h    aim, power meter, english, aim guide
    camera.c/.h  survey / ride / cup / flyover / cinema
    render.c/.h  palettes, procedural geometry, neon HUD, screens
    scenery.c/.h the city: sky dome, skyline, plaza, funfair, table dressing
    juice.c/.h   particles, sparkles, hitstop, slow-mo, popups, banners
    synth.c/.h   every sound effect plus the swing-jazz combo
    save.c/.h    breakpar.sav — bests, aces, volumes, fullscreen
    replay.c/.h  16 bytes per shot; ace replays are free because the sim is exact
  data/
    holes.h      the eighteen, as C arrays
  tests/
    test_physics.c
    test_course.c
    test_camera.c
```

All names, geometry, text and sounds are original.
