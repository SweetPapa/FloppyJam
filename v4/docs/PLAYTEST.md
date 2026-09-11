# Campaign design and playtest report

Tested on 2026-09-06, macOS / Apple M4 Pro / raylib 6.0.

## Magnetic Subspace difficulty correction

Player feedback identified **Magnetic Subspace** (`level-6b`, currently slot 22)
as the actual difficulty spike. The broad reduction to the new stages has been
reversed: their original node spacing, lava speeds, obstacle counts, laser timing,
and roamer speeds are restored. They retain checkpoints every four main-route
landings, moderately generous pars, 50-pixel sweeper arms at 55 degrees/second,
and pulse placement that leaves safer resting areas.

Subspace previously combined 14 pulse rings, lava rising at 60 pixels/second,
one checkpoint, and an exactly overlapping duplicate magnet. A conservative
full-orbit collision check found 31 of 36 nodes within a ring's potential reach.
The earlier fast bot could clear this using post-death immunity, concealing how
punishing it was to pause and read the route. In focused baseline tests, the
1-second profile needed two retries; the other three profiles failed after
12 retries each.

Subspace now has five separated rings at 24 pixels/second, lava at 34 pixels/second,
four checkpoints, a 50-second par, and 35 distinct magnets. Every anchor has room
for the full tether orbit outside every ring's maximum reach. The original route
positions are preserved apart from removing the overlapping duplicate. The
triangle-shaped keyboard controls and all other original stage data are unchanged
by this correction.

The four focused tests (1, 1.5, and 2 seconds before choosing a color, plus a
nearest-forward-node strategy at 1.5 seconds) now all finish **without a death**.
The slower nearest-node route takes 52.6 seconds, so par still rewards some pace.
A regression test also waits eight seconds at every Subspace node with maximum
arrival spin, checking an entire pulse cycle without immunity. See
[subspace-playtest-results.csv](subspace-playtest-results.csv).

The restored new stages clear **60/60 comfort runs**, 45 without deaths, with no
run requiring more than two retries. This keeps challenge in their hazards and
swing patterns. See [the comfort results](relaxed-playtest-results.csv).

## Findings and changes

The original greedy-bot run cleared 24/25 stages; Gauntlet Run failed, and several
others repeatedly backtracked into the lava. Forward-preferred color targeting
resolved those traps without changing the swing force, homing, detection radius,
or maximum speed. Empty color presses now keep the tether attached. Checkpoint
respawns restore a usable lava margin, and Signal Crossing gained two recovery
points after the 750 ms input test exposed a repeating death sequence.

The presentation audit removed overlapping acceleration effects and prioritized
stable framing, a distinct player silhouette, readable color targets, and steady
hazard warnings. Native captures also exposed new laser gates nearly coincident
with landings; those gates now sit between node rows. Reactor Waltz and Escape
Velocity introduce hazard types in successive sections instead of stacking all
of them on the same crossing. The flashlight was widened after visual review.

## Fifteen new stages

Original stages retain their relative order. Short recovery climbs alternate
with hazard lessons; later additions extend the taught patterns with wider arcs,
branch choices, and more pressure. New-stage pars allow at least 1.5 seconds per main-route landing plus six seconds,
rounded up to two seconds. Original-stage pars retain the earlier calibration,
except Subspace's targeted 50-second par.

| Slot | Stage | Design intent | Par |
| --- | --- | --- | --- |
| 02 | Stepping Stones | Alternating four-color warm-up | 22s |
| 04 | Switchback | Repeated-color steps and lateral cuts | 24s |
| 07 | Split Decision | Twin branches that rejoin | 24s |
| 10 | Clockwork | Two sweepers with room for recovery | 24s |
| 12 | Blue Hour | A quiet curved recovery climb | 24s |
| 15 | Airlock | Timed gates with an open side lane | 26s |
| 18 | Heartbeat | Three pulses beside the outer lanes | 26s |
| 20 | Lantern Walk | A readable short darkness challenge | 26s |
| 23 | Polarity Garden | Fields that bend wide swing paths | 26s |
| 25 | Slingshot Alley | Long alternating cross-shaft slings | 28s |
| 27 | Braided Current | Branch choices around timed gates | 28s |
| 30 | Victory Lap | A clean sprint after the rival race | 26s |
| 33 | Reactor Waltz | Sweep, pulse, and gate sections in sequence | 30s |
| 36 | Eye of the Storm | Outer escape routes around center patrols | 30s |
| 39 | Escape Velocity | Wide arcs combining the campaign lessons | 34s |

## Automated simulation results

Every run uses normal color inputs through `sim_update`. There are no teleports,
forced victories, deleted hazards, or immunity overrides. The ordinary profiles
choose the highest available matching target after a fixed delay. The hazard-aware
profile simulates candidate flights and rejects immediate collisions; it is a
local planner, not a human player. Existing 10-second post-death immunity remains
part of the shipped rules, so a clear with deaths is not evidence of a clean route.

| Profile | Stages completed | Deathless clears |
| --- | --- | --- |
| instant | 40/40 | 24 |
| 250ms | 40/40 | 21 |
| 500ms | 40/40 | 17 |
| 750ms | 40/40 | 18 |
| hazard-aware | 40/40 | 31 |

All **200/200** runs completed. The strict CTest gate requires every profile to
clear every stage. [Full results](playtest-results.csv) include time, retries,
and score for each stage/profile combination.

The initial four CTest checks passed in normal and AddressSanitizer + UndefinedBehaviorSanitizer
builds: generated campaign consistency, core simulation regressions, the 200-run
campaign audit, and save format/migration tests. Recovery tests exercise every
authored checkpoint with an unsafe stored lava snapshot. No sanitizer failures
were observed. The current build passes all six normal-build checks, including
comfort tests for the new stages and a deathless comfort test for Subspace.

## Native rendering checks

A native bot with an approximately quarter-second decision delay cleared 12
representative stages: 01, 02, 07, 15, 18, 19, 23, 27, 29, 35, 39, and 40. After
final gate/flashlight adjustments, another native pass cleared 15, 19, 27, 33,
and 39. That is **13 distinct stages** checked in the renderer, including all
three anomaly types. Title, completion, gameplay, darkness, hazard encounters,
and the second campaign page at 960 x 640 were visually reviewed.

The initial presentation pass recorded 7,269 frame samples: **9.30 ms mean, 16.64 ms p95,
58.61 ms maximum**, including screenshot captures and stage transitions. This is
one desktop measurement, not a cross-hardware performance guarantee. The camera
uses time-based smoothing, while the player interpolates a fixed 60 Hz simulation.

The earlier broad easing pass cleared Clockwork, Airlock, and Heartbeat with
zero deaths. Those recordings predate the restoration of their challenge.

The current Subspace correction was also verified in the native renderer at a
1.5-second decision delay: **22.63 seconds, zero deaths**. Screenshots from the
lower, middle, and upper sections were inspected for ring and landing clearance.

Raw native summaries are in [native-playtest.txt](native-playtest.txt). Local PNG
captures are in `v4/artifacts/` (ignored by Git); run the screenshot script to
recreate them. Demo and screenshot modes do not write progress or preferences.

## Limits of this pass

These are automated playtests with visual inspection, not a human campaign
completion. Bots know the level geometry even in darkness and do not measure how
quickly a new player understands a route. Not every stage has a proven deathless
route. Further human feedback remains useful for pacing and route readability. Windows
and Linux builds were not executed in this environment. Reduced motion changes
presentation only; the campaign and scores use the same physics.
