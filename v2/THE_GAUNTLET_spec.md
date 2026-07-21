# THE GAUNTLET
### Full Game Specification — v1.0
*A 90-second rhythm gauntlet. One keyboard. Everyone watching.*

---

## 1. Pitch & Pillars

**Pitch:** A pass-the-keyboard party score-attack. The game generates a song from a 6-character seed, then throws rapid-fire rhythm microgames at the player while the BPM climbs. Between phrases, the player drafts risk/reward modifier cards that mutate both the music and the difficulty. Death is loud and funny. Next player grabs the keyboard.

**Design pillars — every decision gets tested against these:**

1. **Spectator-first.** A drunk person across the room must understand what the player is being asked to do, see whether they did it, and want the next card pick to be greedier. If a spectator can't follow it, cut it.
2. **The music is the authority.** One sample clock in the audio callback is ground truth for everything: charts, judging, visuals, modifier activation. Nothing owns time except the sequencer.
3. **Failure is content.** Missing must be funnier than succeeding is impressive. Wipe-outs are the moments people talk about at the party.
4. **60–120 second turns.** Sacred. No run structure, modifier, or screen may push a turn past ~2 minutes.
5. **Deterministic.** Same seed + same inputs = same run. No wall-clock time in game logic anywhere.

**Contest constraints (hard):** total fileset ≤ 1,474,560 bytes; native binary (no browser, no streamed/downloaded data); original IP; offline; deadline Sept 4, 2026 23:39 KST.

---

## 2. Game Flow

```
BOOT → ATTRACT → SEED SELECT → COUNT-IN → RUN (the game) → WIPE → SCORE ENTRY → LEADERBOARD → ATTRACT
                                              ↑______________________________________|
                                                     (next player, same seed)
```

### 2.1 ATTRACT
- Harmonograph artwork generated from the current session seed drifts on screen; the seed's track plays at low intensity (pad + sparse percussion).
- Local leaderboard (top 10, three-initial entries) cycles with the art.
- Any key → SEED SELECT.

### 2.2 SEED SELECT
- Three options, one line each:
  - **PARTY SEED** — the locked session seed (default; keeps the leaderboard fair). First boot generates one.
  - **DAILY SEED** — hash of the local date. (Deterministic from date; no network.)
  - **ENTER SEED** — 6-character entry, alphabet below.
- Changing the Party Seed prompts: "This clears the leaderboard. Hold ENTER to confirm."

### 2.3 COUNT-IN
- 2 bars. Bar 1: player name/initials entry is *skipped here* (entered after death — keeps startup instant). Big "READY" from the announcer voice on beat 1, "GO" on the downbeat of the run.
- The first microgame's icon and verb are shown during the count-in.

### 2.4 RUN
Described fully in §3–§6.

### 2.5 WIPE (death)
- Instant: record-scratch SFX (pitch-drop on the master), all stems collapse to the lone pad over 500 ms, gameplay freezes with a full-screen desaturation, huge "WIPED" stamp slams in with screen shake.
- 1.5 s hold (crowd laugh window), then score tally: base → modifier multipliers shown stacking one by one (Balatro-style count-up, ~2 s max).

### 2.6 SCORE ENTRY / LEADERBOARD
- Three-initial arcade entry, left/right/enter. Leaderboard highlights the new row.
- "PASS THE KEYBOARD" flashes. Any key → COUNT-IN with the same seed.

---

## 3. Core Loop (RUN)

A run is a sequence of **phrases**. One phrase = 4 bars of the generated track = one **microgame**. After every 2nd phrase, a **card pick** interrupts for exactly 1 bar (music continues; a drum-only "breakdown bar" plays under the pick so the groove never stops).

```
PHRASE(microgame) → PHRASE(microgame) → CARD PICK (1 bar)
→ PHRASE → PHRASE → CARD PICK → ... until 3 misses (WIPE)
```

- **HP: 3 misses total** (a "miss" = failing a phrase's success condition, defined per microgame in §5). Each miss: one stem drops out for the next phrase, screen edge cracks (persistent visual damage), announcer groans.
- There is no healing except the INSURANCE card (§6).
- **Heat tier** increases every 4 phrases (§7). Heat drives BPM, timing windows, stem count, and visual intensity.
- A run at skilled play lasts ~90–110 seconds (≈ 12–16 phrases). Math: 4 bars/phrase at 120→180 BPM ≈ 6–8 s/phrase.

---

## 4. Timing, Judging & Input

**This section is the make-or-break of the entire game. Build and tune it before anything else exists.**

### 4.1 Clock
- Audio callback owns a monotonically increasing `uint64_t sample_clock`. The sequencer advances inside the callback. All musical positions (bar, beat, sixteenth) derive from it.
- The game thread reads an atomic snapshot each frame: `{sample_clock, bpm, samples_per_beat, beat_phase}`.
- Input events are timestamped on arrival (high-resolution timer), then mapped to sample time via a continuously updated linear mapping (`sample_clock` vs. host time, EMA-smoothed) so judging is sample-accurate even though input arrives on the game thread.
- **Latency calibration:** a 10-second calibration screen reachable from ATTRACT (hold C): tap along to a click, we compute median offset, store it, apply it to all judgments. Persist to a config file beside the binary. Default offset 0.

### 4.2 Judgment windows (per note, milliseconds, before scaling)
| Judgment | Window | Score | Feedback |
|---|---|---|---|
| PERFECT | ±45 ms | 100 | White flash ring, pitched "ding" that plays the chord tone |
| GOOD | ±90 ms | 50 | Small ring, dull tick |
| MISS | outside / no input | 0 | X stamp, note fragment shatters |

- Windows scale with Heat: multiply by `window_scale[tier]` (§7), floor at 60% of base.
- A phrase's success condition is per-microgame (§5), but the default is: **≥ 70% of judged notes at GOOD or better, and no more than 2 outright MISSes.** Fail the condition → lose 1 HP.
- **Every judgment must produce audio + visual feedback within the same frame.** The pitched PERFECT ding uses the current chord's tones so streaks literally play melodies — this is the core "feel" reward.

### 4.3 Input map
- Primary keys: **F, G, H, J** (four lanes, home-row, works on any keyboard). SPACE = universal single-input microgames. All microgames declare which subset they use, and the on-screen prompt shows literal keycaps.
- ESC held 1 s = abort run (counts as wipe, no score entry). No pause — pausing mid-run is a leaderboard integrity hole and kills party momentum.

### 4.4 Debug/tuning keys (compiled out of release via `#ifdef TUNING`)
F1/F2 window scale ±5%, F3/F4 BPM ±5, F5 toggle judgment overlay (shows ms offset per hit), F6 skip to next tier, F7 force card pick, F8 infinite HP, F9 seed reroll, F10 toggle stem solo cycling. Print current values top-left.

---

## 5. Microgames

### 5.1 Framework
Each microgame is a struct of function pointers + a data table:

```c
typedef struct {
    const char*  name;          // "SNIPE"
    const char*  prompt;        // "PRESS ON 4!"
    uint8_t      keys;          // bitmask of F,G,H,J,SPACE
    uint8_t      icon_id;       // 16x16 1-bit icon in the icon sheet
    uint8_t      min_tier;      // don't schedule before this Heat tier
    void (*build_chart)(Chart*, const Genome*, const PhraseCtx*); // reads sequencer events
    void (*update)(MicroState*, const InputEvents*, const ClockSnap*);
    void (*draw)(const MicroState*);
    bool (*judge_phrase)(const MicroState*);   // success condition
} Microgame;
```

- `build_chart` never invents timing: it **selects events from the sequencer's already-authored pattern data** (kick hits, hat hits, chord changes, rests). The chart *is* the music. This is non-negotiable — it's why everything stays locked.
- **Telegraph:** the *next* microgame's icon + prompt + keycaps appear one full bar before it starts, in the top banner, big enough for spectators. The announcer speaks its name syllable-stab on the bar line.
- **Scheduler:** pseudo-random order from the run's RNG stream (seeded), constraints: no immediate repeats, respect `min_tier`, REST and BLIND never back-to-back, FINALE only appears at tier 5+.

### 5.2 The roster (15)

| # | Name | Keys | Verb (prompt) | Chart source | Success condition | Notes / why it's funny |
|---|---|---|---|---|---|---|
| 1 | TAP | SPACE | "TAP THE KICK" | Kick pattern | Default (§4.2) | The tutorial-by-osmosis game; always the first phrase of every run (min_tier 0, forced first) |
| 2 | UPBEAT | SPACE | "HIT THE *AND*" | Offbeat eighths | Default | Everyone rushes it |
| 3 | ALTERNATE | F,J | "LEFT RIGHT LEFT" | Hi-hat 8ths/16ths | Default | Classic DDR-adjacent stamina |
| 4 | HOLD | SPACE | "HOLD... RELEASE!" | Pad swell → downbeat | Release within GOOD window of target beat; early release = miss | Slow-burn tension; crowd holds breath with them |
| 5 | REST | any | "DON'T. TOUCH. ANYTHING." | A 2-bar break in the arrangement (sequencer mutes all but pad) | Zero inputs during the rest region | Someone always touches it. Peak comedy. Any input = instant phrase fail with a huge honk |
| 6 | SNIPE | SPACE | "ONLY BEAT 4" | Beat 4 of each bar | All 4 snipes GOOD+; any extra press = miss | Precision showpiece |
| 7 | DOUBLE | F+J | "BOTH ON THE ACCENT" | Chord-change beats | Chord (both within 30 ms of each other, on time) | Tests two hands, reads great |
| 8 | STAIRS | F,G,H,J | "RUN UP THE KEYS" | Ascending 4-note arpeggio (pluck voice) | Sequence in order, each note judged | The arpeggio is audible — you play what you hear |
| 9 | RAPID | F,J | "MASH THE ROLL" | One bar of 16ths (snare roll rendered by sequencer) | ≥ 80% of 16th slots filled with alternating hits | Stamina spike; flailing looks hilarious |
| 10 | ECHO | F,G,H,J | "COPY THAT" | Bell plays a 4-note motif bar 1–2; player repeats bars 3–4 | Correct keys in order, timing GOOD+ | Simon-says on the beat; motif comes from the melody generator so it's always in key |
| 11 | BLIND | SPACE | "DRUMS OUT — KEEP TIME" | Kick pattern, but drum stems gain=0 for the phrase | Default, windows ×1.3 (be kind) | Internal-clock test; spectators tap the table to sabotage/help |
| 12 | SWAP | F,G,H,J | "KEYS REMAPPED!" | Hat pattern; lane→key map scrambles at phrase start (shown big) | Default | Mirror-brain comedy |
| 13 | MEASURE | SPACE | "HOLD EXACTLY 4 BEATS" | Free start, target duration | Release within ±120 ms of exactly 4 beats after press | Pure feel; show a giant growing bar |
| 14 | CALLBACK | F,G,H,J | "ANSWER THE BELL" | Bell asks on bar 1 & 3, player answers on 2 & 4 | Each answer GOOD+ | Conversational rhythm; charming |
| 15 | FINALE | all | "EVERYTHING." | Union of kick+hat+chord events across all 4 keys + space | ≥ 60% (merciful) | Tier 5+ only. The wall. Reaching it *is* the flex; the crowd counts down to it |

Icons: 16×16 1-bit glyphs, one 240×16 strip, drawn as scaled rects (no texture file needed — bake as a `uint16_t[16]` per icon in a C array).

---

## 6. Modifier Cards

### 6.1 Draft mechanics
- Every 2nd phrase: the track drops to a drum-only breakdown bar; two cards slide in, big; player picks with F (left) / J (right); no pick by bar's end → the **left card auto-applies** (crowd knows this; hesitation has a cost; keeps runs moving).
- Modifiers **stack multiplicatively** and persist until wipe unless marked one-shot. Active modifiers show as a row of small chips under the score, and their multiplier product shows as "×N.N" beside the score.
- Card pool is drawn from the run RNG stream; no duplicates of active persistent cards; safety cards (§6.3) guaranteed to appear as one of the two options if the player is at 1 HP (mercy rule — keeps turns from ending anticlimactically).

### 6.2 Risk cards
| Card | Effect | Mult | Implementation |
|---|---|---|---|
| TURBO | +12 BPM immediately (quantized to next downbeat) | ×1.5 | Clock ramp over 1 beat |
| DEAF | Drum stems muted permanently | ×2.0 | Gain mask |
| MIRROR | F↔J, G↔H swapped | ×1.5 | Input remap |
| TIGHT | Judgment windows ×0.7 | ×2.0 | Window scale |
| FOG | Notes/prompts invisible until 1 beat before they're due | ×1.75 | Draw gate on chart events |
| DENSE | build_chart selects double density (16ths where 8ths) | ×1.75 | Chart density flag |
| SPIN | Playfield rotates ±10° with the bass amplitude | ×1.25 | Cosmetic-ish; nauseating with MIRROR, which is the point |
| STROBE | Background flashes to the beat, lanes desaturate | ×1.25 | Cosmetic pressure |
| WILD | Applies a random hidden risk card, revealed after 1 phrase | ×2.5 | RNG; announcer laughs |
| ALL-IN | One-shot: next phrase only, any MISS = instant wipe | ×3.0 on that phrase's score | Flag |

### 6.3 Safety / economy cards
| Card | Effect | Mult |
|---|---|---|
| INSURANCE | Heal 1 HP (max 3) | ×0.75 (permanent) |
| METRONOME | Audible click on every beat from now on | ×0.8 (permanent) |
| BANK | One-shot: immediately add (current score × 0.25) as flat points, no multiplier growth | ×1.0 |
| BREATHER | Next card pick is skipped (2 extra calm phrases) | ×0.9 |

Balance intent: a greedy player at tier 6 can be sitting on ×8–×15. The leaderboard should be dominated by people who stacked risk and survived, not by long careful runs — verify in tuning, adjust multipliers, not scores.

---

## 7. Heat & Difficulty Curve

Heat tier increments every 4 phrases. Per tier (values are the starting tuning table — expose in TUNING build):

| Tier | BPM (base 112) | Window scale | Stems active | Visual |
|---|---|---|---|---|
| 0 | 112 | 1.00 | pad, kick | Dark, thin lines |
| 1 | 120 | 1.00 | +hat | Beat rings appear |
| 2 | 128 | 0.95 | +bass | Geometry breathes, palette warms |
| 3 | 138 | 0.90 | +pluck | Particle trails |
| 4 | 148 | 0.85 | +bell melody | Symmetry blooms, trails persist |
| 5 | 158 | 0.78 | full arrangement | Full bloom; FINALE unlocked |
| 6+ | +8/tier | 0.72 floor at 0.60 | full + ornaments | Screen edges glow; announcer hypes |

- BPM changes always ramp over exactly 1 beat, applied on a downbeat.
- Stem entrances are gain fades quantized to the downbeat (already the engine's contract).
- **The arrangement escalating with tier is the spectator's progress bar** — the room can *hear* how deep a run is.

---

## 8. Music Engine

Carried over from the v1 build, with the microgame event bus added. Restating the contract:

### 8.1 Architecture
- 48 kHz, stereo, float32 internal, proper audio callback (raylib `AudioStream` with a ring buffer, or miniaudio callback directly). The **sequencer runs inside the callback**, emitting voice note-ons at exact sample offsets within each buffer. No per-frame audio rendering, ever.
- All stems always running from sample 0; layer control = per-stem gain only. Gain changes are enqueued and applied at bar boundaries by the sequencer.
- Master bus: soft-clip limiter, one-pole lowpass (used by WIPE collapse and damage states), master pitch (used by record-scratch: exponential pitch drop over 400 ms).
- Announcer + SFX are two additional mixer channels, not stems.

### 8.2 Genome & generation
```c
typedef struct {
    uint32_t rng_state;      // from seed
    uint8_t  root, scale;    // scale ∈ {major, minor, dorian, mixolydian}
    uint8_t  chord_family;   // index into the 7 proven progressions (v1 list)
    uint8_t  euclid_k, euclid_n;  // percussion skeleton, e.g. 5/8
    uint8_t  bass_pattern, motif_shape, palette;
    uint16_t bpm_base;       // 104–124
} Genome;
```
- Chord families: I–V–vi–IV, I–iii–IV–iv, I–vi–ii–V, I–IV–I–V, i–VI–III–VII, I–IV–vi–V, two-chord modal vamp. Variation via inversion, sus, add6/add9, register, omission — table-driven.
- Percussion from Euclidean patterns; hats subdivide; fills every 4th bar (also the microgame telegraph moment — the fill *is* the audio telegraph).
- Bell motifs: 4-note phrases from the scale, contour tables — these feed ECHO and CALLBACK directly.
- **Event bus:** the sequencer publishes its authored events (kick@sample, hat@sample, chord-change@sample, rest regions, motif notes) into a lock-free ring the game thread reads. `build_chart` consumes these. The chart is the score.

### 8.3 Voices (purpose-built, from v1)
Pad (2 detuned band-limited saws → LP, slow env), Bass (sine + quiet triangle harmonic, rounded env), Pluck (triangle → fast-decay LP env), Bell (2-op inharmonic sine pair), Kick (pitched sine drop + click), Hat (filtered noise, short), Snare/roll (noise burst + tone). Polyphony cap 16 voices; voice stealing oldest-first.

### 8.4 Announcer
Synthesized syllable stabs: short formant-ish bursts (band-passed pulse + noise, pitch contour per syllable). Words are 1–3 stabs with pitch patterns: "READY" (low-high), "GO!" (high), "NICE" (mid, warm), "WIPED" (descending minor 3rd, pathetic), each microgame name (1–2 stabs). ~10 lines of DSP + a table of `{syllable_count, pitch[], dur[]}`. It should sound like a Speak & Spell's cool cousin, not TTS.

---

## 9. Presentation & UI

### 9.1 Layout (1280×720 logical, integer-scale to display; fullscreen default, F11 windowed)
```
┌──────────────────────────────────────────────┐
│  TOP BANNER: next-game icon+name+keycaps     │  ← spectator strip, 120 px, huge type
├──────────────────────────────────────────────┤
│                                              │
│           PLAYFIELD (microgame draws)        │
│     beat rings from center on every beat     │
│                                              │
├──────────────────────────────────────────────┤
│ SCORE ×MULT      chips[mods]      HP ♥♥♥ TIER│  ← 64 px status bar
└──────────────────────────────────────────────┘
```
- One bitmap font, two sizes (8×8 base, drawn ×3 and ×6). Numbers get a third, huge size for score pops.
- Playfield standard elements every microgame inherits: center pulse ring on each beat, lane guides when lanes are used, judgment text pops at hit location.
- Colors: 4 palettes selected by genome, each = bg, dim, main, accent, hot (5 colors). All drawing uses palette indices. Damage state desaturates by lerping toward dim.

### 9.2 Juice inventory (all cheap, all mandatory)
Screen shake on MISS and WIPE (decaying noise offset, 4 px max); hit-stop 30 ms on PERFECT chains ≥ 4; multiplier count-up ticker on card pick; phrase-clear stamp with 1-bar particle burst; persistent crack overlays per HP lost (3 authored polyline sets); trails via previous-frame fade buffer (draw last frame at 92% alpha — the v1 trick); everything eases, nothing teleports (cubic ease-out, 100–200 ms).

### 9.3 Attract art
Harmonograph renderer from v1: 2–3 damped sine pairs parameterized by the genome, drawn as fading polyline over 20 s, palette-colored. This is the game's "cover art" and it's free.

---

## 10. Scoring

```
phrase_score = Σ note_judgments (PERFECT 100 / GOOD 50)
             × tier_bonus (1.0 + 0.1·tier)
             × Π active_modifier_mults
run_score    = Σ phrase_scores  (+ BANK deposits)
```
- Combo: consecutive PERFECTs across phrases drive a FLOW meter (cosmetic intensity: trail length, particle rate, announcer excitement). Combo does **not** multiply score — score pressure lives in the cards, so the crowd's greed and the player's fingers stay the only two variables. (If tuning shows careful play beats greedy play, raise card mults; never add combo scoring.)
- Leaderboard: top 10, initials + score + tier reached + seed. Stored in a flat file beside the binary (`gauntlet.scores`, fixed 10×16-byte records). Tier reached is the bragging stat.

---

## 11. Technical Plan

### 11.1 Stack
- C99, raylib (statically linked), `-Os -flto`, stripped. Target: single binary, Windows + Linux builds (submit Windows; contest machines are Windows-safe bet).
- No files shipped besides the binary (+ auto-created scores/config). No textures, no audio files, no fonts on disk — font and icons are C arrays.

### 11.2 Module map (with LOC guesses — total ~6–7k)
```
main.c            state machine, frame loop                 ~300
clock.c           sample clock, input→sample mapping, cal   ~250
audio.c           callback, mixer, master fx                ~400
sequencer.c       genome, patterns, event bus, gain quant   ~700
voices.c          7 voices + announcer + sfx                ~600
chart.c           event selection, judging, windows         ~400
micro/*.c         15 microgames                             ~1500 (~100 ea)
cards.c           pool, draft UI, effect application        ~400
heat.c            tier table, ramps                         ~100
draw.c            palette, font, primitives, juice, trails  ~800
screens.c         attract/seed/wipe/scores                  ~500
persist.c         scores + config file                      ~150
rng.c             splitmix/pcg, stream splitting            ~80
```
- RNG: one seeded generator, **split into named streams** (music, scheduler, cards, cosmetics) so cosmetic draws never perturb gameplay determinism.

### 11.3 Size budget (expectation, not target)
Binary ~300–500 KB with raylib static + LTO. If it lands smaller, fine — *do not* seek bytes to spend. If it somehow threatens 1.4 MB, first suspects: raylib modules you can compile out (models, no need), and `-Os` actually applied to raylib itself.

### 11.4 Determinism & replay (cheap, worth it)
Because inputs are sample-timestamped and everything else derives from seed: log `(sample_time, key, up/down)` per run to a ring buffer. On WIPE, keep the last run's log in memory; pressing R on the leaderboard replays it as a ghost (input playback through the same pipeline). This is a debugging superpower (perfect repro of any feel complaint) and a party feature (rewatch the wipe) for ~100 LOC.

---

## 12. Tuning Protocol (how we hammer it into fun)

The spec is complete but the *game* lives in these numbers. Non-negotiable process:

1. **First playable = TAP only**, full pipeline (clock → chart → judge → feedback → juice), BPM ramp active, tuning keys live. The two-hour test: does nailing beat 4 at 160 BPM feel crunchy, does missing feel funny? Tune windows/feedback until yes. Everything else inherits this feel.
2. Add microgames in roster order 1→15; each must pass "spectator test" (someone watching understands it without explanation) before the next goes in.
3. Cards enter only after 8+ microgames exist. First balance pass: play 10 runs greedy, 10 careful — greedy must win the board or mults go up.
4. Party playtest with ≥ 3 humans before any polish pass. Watch the *spectators*, not the player: if they're not reacting to card picks and wipes, the multipliers and WIPE staging need work, not the microgames.
5. Freeze content 1 week before deadline; final week is tuning + wipe/juice polish + builds on a clean machine.

---

## 13. Out of Scope (v1.0, explicit)

Online anything; 2-player simultaneous; gamepad support (keyboard only — party constraint, contest safe); music style variety beyond the genome's range; difficulty select (Heat *is* the difficulty); pause; settings menu beyond calibration; localization (English + icons; prompts are ≤ 4 words by design).
