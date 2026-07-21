# THE GAUNTLET

A 90-second pass-the-keyboard rhythm gauntlet. One keyboard. Everyone watching.

Native C99 + raylib. Single binary, no shipped assets — the font, the icons, the
music and the announcer are all generated from code. Implemented from
`THE_GAUNTLET_spec.md`; section references in the source point back at it.

## Build

```sh
make            # release: -Os -flto, stripped
make TUNING=1   # tuning build: debug keys from §4.4 compiled in
make run        # build + run windowed
```

Requires raylib (found via `pkg-config`, else `-lraylib`).

## Run

```sh
./gauntlet                    # fullscreen, the way it's meant to be played
./gauntlet --windowed         # windowed
./gauntlet --seed GNTLET      # force a 6-character seed
./gauntlet --autoplay         # robot player (soak testing / tuning)
./gauntlet --soak 120         # auto-start a run, quit after N seconds
./gauntlet --idle             # auto-start with no input, to watch the wipe path
./gauntlet --force-game 8     # pin the scheduler to one microgame (roster index)
```

Two files are created beside the binary: `gauntlet.scores` (leaderboard + party
seed) and `gauntlet.cfg` (latency calibration offset).

## Controls

| | |
|---|---|
| `F G H J` | the four lanes |
| `SPACE` | single-input microgames |
| `F` / `J` | pick the left / right modifier card |
| `ESC` (hold 1 s) | abort the run — counts as a wipe |
| `F11` | toggle fullscreen |
| `C` (hold, on ATTRACT) | latency calibration |
| `R` (on LEADERBOARD) | replay the last run from its input log |

No pause. Pausing mid-run is a leaderboard integrity hole and it kills party
momentum (§13).

### Tuning build only (§4.4)

`F1`/`F2` window scale ±5%, `F3`/`F4` BPM ±5, `F5` judgment overlay,
`F6` skip a tier, `F7` force a card pick, `F8` infinite HP, `F9` reroll the seed,
`F10` cycle stem solo.

## How it fits together

```
main.c          frame loop, input timestamping, autoplay, replay feeder
game.c          run engine: scheduling, judging feedback, cards, HP, scoring
screens.c       attract / seed select / calibration / wipe / tally / scores
audio.c         the audio callback: transport, mixer, master FX, command ring
sequencer.c     genome, the pure per-bar pattern generator, chord tables
voices.c        7 synth voices + the announcer's syllable stabs
chart.c         charts in musical coordinates, judging windows, phrase stats
clock.c         host-time <-> sample-time mapping, latency calibration
cards.c         the 14 modifier cards
heat.c          the tier table
draw.c          palettes, 8x8 font, 16x16 icon sheet, trails, particles, cracks
persist.c       leaderboard + config files
rng.c           seeded RNG, split into named streams
micro/          the 15 microgames + their shared scaffolding
```

### The two ideas that hold it up

**One clock.** A `uint64_t` sample counter advances inside the audio callback and
nothing else owns time. The sequencer runs in that callback and publishes each
bar's 16-step sample grid to the game thread one bar ahead. Input is timestamped
on arrival and mapped into sample time through an EMA-smoothed linear fit, so
judging is sample-accurate even though input arrives on the game thread.

**The chart *is* the music.** `seq_bar_pattern()` is a pure function of
`(genome, bar, flavor)`. The audio thread plays it; the game thread calls the
exact same function to build charts. A microgame never invents timing — it
selects from events the sequencer already authored. Charts are written in
`(bar, step)` and resolved to sample times as each bar's grid arrives, which is
also why the one-bar telegraph and the BPM ramps line up for free.

Everything derives from a 6-character seed, and the RNG is split into named
streams (music / scheduler / cards / cosmetics) so a cosmetic draw can never
perturb gameplay. Same seed plus same inputs is the same run — which is exactly
what makes the replay feature ~40 lines.

## Tuning notes

Verified by soak (`--autoplay`, two seeds, 200 s each): all 15 microgames
schedule and clear, HP/wipe/tally/score-entry/hand-off cycle round-trips, and
the leaderboard persists. A sample-perfect robot sits around ×18 at tier 4 and
×50+ by tier 7 — above the ×8–×15 the spec wants at tier 6, but a robot never
misses and so never pays for greed. The multipliers are left at the spec's
values; §12 says to move them only on playtest evidence, and three humans in a
room is the evidence that matters.

## Known deviations from the spec

- Icons are authored as 8×8 art and expanded to the specified 16×16 1-bit sheet
  at init. Same glyph size on screen, half the source to eyeball.
- A phrase finalises on its closing bar line, so the late half of the judgment
  window for a note on the final 16th is cut. Charts therefore avoid that step;
  RAPID, whose success condition is a percentage, is the only game that lands
  near it and is tolerant by design.
- FINALE opens every tier from 5 up rather than sitting in the random draw, so
  the wall arrives on a beat the room can count down to (§5.2's intent).
- BPM changes are scheduled one bar ahead of their downbeat, because the bar's
  step grid is published a bar early. The ramp itself is still exactly one beat
  on the downbeat, as specified.
