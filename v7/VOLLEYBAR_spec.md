# VOLLEYBAR
### Requirements & Design Specification — v1.0
*Pong grew rods. Foosball learned to glow. Two players, six bars of light, one ball that remembers every hit.*

(Working title — volley + the bar: the rod in your hands and the bar you'd play it in. Rename freely.)

---

## 0. Placement, Build Context & Agent Instructions

- **Repo location:** `v6/` in the games monorepo, fully self-contained (no shared code with sibling folders). Match repo build conventions; else plain `Makefile` (`make`, `make debug`, `make web`, `make check`).
- **Stack:** C11 + raylib. GLSL shaders permitted and expected (this is the "visually stunning" game — raylib's shader pipeline is the tool). No other third-party libraries.
- **Everything procedural:** zero image, model, font, or audio files. All geometry, light, and sound generated. No size ceiling is imposed on this project — quality and beauty win every conflict — but the procedural mandate will keep it lean anyway.
- **Web build is first-class** (the sharing plan): single `app_frame()` entry from day one (`emscripten_set_main_loop` on web, while-loop on desktop), single-threaded sim, audio init on first input gesture on web, `ALLOW_MEMORY_GROWTH=1`, output `index.html/.js/.wasm` for itch.io/GitHub Pages. CI builds web + desktop together so neither silently breaks.
- **This spec is complete — no gated phases.** Section 14 is a tuning ORDER; Section 13 offers optional parallel lanes if FOREMAN wants to fan out after the sim core lands. Conflicts resolve in favor of Section 5 (Sim) then Section 1 (Pillars).
- The soul of this game is **the rally**. If a change makes rallies longer, louder, and more dramatic, it's probably right.

## 1. Pitch & Design Pillars

**Pitch:** A top-down table of smoked glass floating over a void. Each player commands three glowing rods of paddles — goalie, defense, attack — interleaved down the table exactly like foosball. The ball is a comet: every strike makes it faster, brighter, and hotter, until goals land like lightning. One stick and one button plays the whole game (pure Pong DNA); pins, snaps, chips, and english are waiting for anyone who wants to go deeper. Casual and competitive aren't difficulty settings here — they're two doors into the same room.

**Pillars:**
1. **One stick to start, a lifetime to finish.** The GLIDE control scheme (move + flick) is a complete, viable way to play — not a tutorial mode. The GRIP scheme adds foosball's real weapons (pin, snap, chip, english) for players chasing mastery. Both schemes are legal in every mode, at the same table, in the same match.
2. **The rally is the show.** Escalating heat makes every exchange a rising drama with a crowd-noise arc. Points are the punctuation; the rally is the sentence.
3. **Skill OR leisure, by ruleset, not by shame.** STANDARD is a locked, pure, tournament-honest ruleset. PARTY is mutator chaos. RALLY is co-op zen. None is the "real" game more than the others; the menu never implies otherwise.
4. **Stunning must stay readable.** Every beauty pass obeys one law: the gameplay layer (ball, paddles, goal mouths) always wins contrast against the spectacle layer. If a shader ever makes a save harder, the shader loses.
5. **Deterministic sim, honest AI.** Fixed-step physics; same inputs → same match. The AI plays through simulated reaction time and the same physics — it never teleports, never reads inputs, never cheats.
6. **Foosball's law, Pong's heart.** The mechanics honor real foosball's discipline (no spinning, shot clocks, loser serves) and real Pong's purity (a ball, walls, angles, nerve).

## 2. The Table & Formation

Top-down view, table long-axis horizontal. Side walls are live (banks are core play); the short ends are goal walls with a centered goal mouth (≈ 22% of table width) guarded by each goalie.

Six rods, interleaved foosball-style, from left goal to right goal:
**P1-GK(1)** · **P1-DEF(2)** · **P2-ATK(3)** · **P1-ATK(3)** · **P2-DEF(2)** · **P2-GK(1)**
(number = paddle count on that rod; paddles on a rod are rigidly spaced and move together — sliding a rod slides its wall of paddles, exactly like gripping a foosball bar)

The interleave is the strategic engine: your attack rod lives deep in enemy territory, behind their defense — so clearing the ball carelessly feeds the opponent's forwards, passing between YOUR rods is possible and powerful, and every zone of the table belongs to somebody. Rod travel is clamped so paddle walls can never fully seal the table: each rod's coverage leaves at least 1.25 ball-widths of open lane at maximum stretch (lane passes must always exist — walls of light, not walls of "no").

## 3. Controls — Two Doors, One Room

Input: gamepads (preferred), keyboard split (WASD+F / arrows+Rshift), any mix. Every action is remappable. Scheme is chosen per player, per match, changeable between points.

### 3.1 GLIDE (the leisure door — complete and viable)
- **One axis:** slides ALL three of your rods together (classic Pong feel scaled up; the interleave does the tactics for you).
- **One button — FLICK:** the paddle nearest the ball strikes with a firm, satisfying hit. Generous built-in angle assist bends the shot modestly toward the best open lane (assist strength is a visible setting, default ON for GLIDE).
- Soft touches (ball meets an un-flicked paddle) are automatic gentle bunts.
- That is the entire scheme. A first-timer should score within 90 seconds (§16.2).

### 3.2 GRIP (the skill door)
- **Active rod:** you steer one rod at a time; auto-handoff targets the rod best positioned for the ball, shoulder buttons override manually. Inactive rods hold position (they still block passively — positioning them is part of the craft).
- **FLICK (tap):** quick strike. **CHARGED FLICK (hold ≤0.6s):** more pace + visible curve — lateral rod velocity at contact adds english that bends flight and sharpens wall-bank exits.
- **PIN (hold catch as ball arrives):** trap the ball dead under the paddle — foosball's signature control move. While pinned: a 2.0s shot clock ticks, an aim wedge appears, lateral movement drags the ball with you (walk the line like a snake-shot setup), release = SNAP: the fastest shot in the game, +2 heat. The pin is the fantasy centerpiece: catching a screamer dead, dragging it two lanes over, and snapping it top-shelf.
- **CHIP (flick + modifier):** a lofted strike that hops the ball OVER exactly one rod (rendered as scale-up + shadow), landing live. The through-pass and the goalie's nightmare; slower in flight, interceptable at the landing spot.
- **BUNT (soft tap):** cushioned touch, −1 heat, drops the ball short — tempo control and trap setups.
- **No spinning.** Paddles never helicopter. Every strike has windup and recovery frames (heavier for charged/snap). Committing to a flick and missing leaves the lane open for real. This is foosball's no-spin law reborn as fighting-game honesty — reads and timing beat wiggle-spam, forever.

### 3.3 Doubles (2v2 couch)
Foosball doubles, digitized: one teammate owns GK+DEF, the other owns ATK — with a swap-roles button between points. Four players, one table, maximum noise. Each player picks their own scheme.

## 4. The Heat System (the rally engine)

Every paddle contact raises the ball's **heat** one step (0–12). Heat drives:
- **Speed:** each step multiplies ball speed (curve in §5.4) up to a hard cap at step 12 — fast enough to terrify, capped so it stays humanly returnable.
- **Spectacle:** the ball climbs a temperature spectrum — deep cyan → white-gold — its trail lengthens, the table glow rises, the crowd-light field surges, the music adds a layer every 3 steps. Anyone walking past the screen can read the state of the rally at a glance.
- **The mercy beat:** from step 8+, every struck ball holds a 120ms pre-launch shimmer before it flies — a breath that keeps top-speed exchanges reactable instead of random. (The proven escalation-with-mercy pattern; it is what makes max heat a crescendo rather than a coin flip.)
- **Decay & control:** bunts and pins reduce heat by 1; a goal or dead ball resets to 0 (goals scored at step 9+ are "SCORCHERS" — bigger detonation, tracked on the stat card, worth the same 1 point in STANDARD).

## 5. Simulation Specification

### 5.1 Determinism & integration
Fixed timestep **1/240s**, semi-implicit Euler, render-interpolated; all logic on the tick counter, no wall clock; one cosmetic-only PRNG. Same seed + same input streams = bit-identical match (this makes replays, ghosts, and netcode-someday free). Input recording (per-player axis+buttons, per tick) always on; last rally auto-replayable.

### 5.2 Ball
2D disc, continuous (swept) collision against paddles/walls (mandatory: top speed crosses multiple radii per tick). Restitution: walls 0.94, idle paddles 0.65 (soft bunt feel), flicked paddles impart velocity by §5.3. Ball never fully stops in open play: below a minimum speed it receives a gentle center-line drift nudge (no stalemates, no shoving matches with a dead ball).

### 5.3 Strikes
Flick applies outgoing speed = base(heat) along strike normal ± aim deflection; english = k_e × rod lateral velocity at contact, applied as curve (Magnus-style lateral acceleration decaying over 0.5s of flight). Charged flick ×1.3 speed, ×1.8 curve. Snap (from pin) ×1.6 speed, laser-straight, minimal curve — its counter is positioning, not reaction. Chip: ballistic hop with fixed apex, clears exactly the next rod plane, lands 2.5 rod-gaps downfield (aim wedge shows the landing ring to BOTH players — readable, counterable).
Windup/recovery (GRIP): flick 3/8 ticks-of-60 equivalent (50ms/130ms), charged 8/16, snap 4/20. GLIDE flick uses the light 3/8 with assist. Whiffed strikes glow briefly — legible commitment, like a fighting game.

### 5.4 Heat curve (starting values, tune per §14)
`speed(h) = v0 × 1.11^h`, v0 = 3.2 table-widths/min-equivalent (concretely: tune so step 0 crosses the table in ~2.1s, step 12 in ~0.55s). Mercy beat 120ms at h≥8. Heat cap 12.

### 5.5 Foosball law, encoded
- **Loser serves:** after a goal, the conceding side serves from their GK with a 1.5s protected possession.
- **Anti-stall:** ball resting in one side's control (same rod contact/pin without advancing) > 4s → referee pulse warning → auto heat-0 pop toward center. Pins hard-cap at 2.0s.
- **No own-goal misery in GLIDE:** with assist ON, own-half rearward flicks won't target your own goal mouth (deflected to nearest safe lane). GRIP players own their mistakes fully — own goals count and the crowd gasps.

## 6. Modes & Rulesets

| Mode | What it is | Who it's for |
|------|-----------|--------------|
| **VERSUS · STANDARD** | 1v1/2v2, first to 5/7/10, locked rules, no mutators, Table Tilt off, stat card + rally replay after | The competitive door |
| **VERSUS · PARTY** | Same table + chosen mutators; Table Tilt on by default | The ruckus door |
| **RALLY** | 1–2 players co-op keep-up: no goals, shared heat streak, chase your longest volley in an arena that blooms with the streak; failure = gentle reset, zero pressure | The leisure door; also secretly the best training mode |
| **GAUNTLET** | Solo ladder vs 10 AI personalities + a finale; three difficulty tiers per rung; earns palettes/table themes (cosmetic only) | Single-player spine |
| **TRICKSHOT** | 20 authored setup puzzles: "bank-chip past two rods," "pin, walk, snap top-corner," etc.; medals by attempts | Skill-lab with dopamine |

**Mutators (PARTY, individually toggleable):** Multiball (2–3), Big Ball, Pea Ball, Magnet Paddles (slight attract radius), Mirror Match (controls-flip pickups OFF by default — it's funny once), Sudden Heat (start at step 6), Moving Goals (goal mouths oscillate slowly), Golden Goal finale. Mutators never enter STANDARD; the ruleset lock is a promise to competitive players.

**Table Tilt (the all-types-of-folks system):** per-player, pre-match, openly negotiated handicaps — larger/smaller paddles, rod speed ±, assist strength, personal heat cap, extra goal width against them. A shark and their grandparent set the tilt like golf strokes and then play a REAL match where both are trying. The pregame screen frames it as sportsmanship, not charity ("Set the table, then settle it"). Tilt badges appear on the versus screen — visible house rules, no hidden sandbagging.

## 7. AI (Gauntlet & practice partner)

AI acts through the same sim with modeled human limits: reaction latency (180–320ms by difficulty), aim noise, and a decision brain choosing among the same verbs (clear, pass, pin-setup, chip, bank). Ten personalities with legible styles worth learning: the Wall (all defense, punishes impatience), the Gambler (chips constantly), the Metronome (bunts heat down, wins slow), the Showoff (always pins, telegraphs snaps), the Mirror (echoes your habits back at you)… Each has a name, a light-signature color, and two lines of pre-match banter (original characters, text only). AI never uses inputs a human couldn't produce; difficulty tunes latency/noise only — never physics.

## 8. Visual Direction — "Light Stadium Over the Void"

The identity: a slab of smoked glass suspended in darkness; everything that matters is made of light.
- **Table:** subtly reflective glass (screen-space fake reflection of ball/paddle glow), engraved center line and lane markings as faint etched light, paper-thin fresnel edge.
- **Rods & paddles:** bars of translucent light; each player picks a signature color (colorblind-safe pair enforcement). Paddles pulse on contact, flare on charged windup, and burn brighter with the match score.
- **The ball:** the star — a molten core with a refractive halo and a persistent ribbon trail; color rides the heat spectrum (§4). At step 12 it is a white-gold comet dragging the whole arena's light toward it (subtle radial pull shader).
- **Goals:** detonations — shockwave ripple through the glass (displacement shader), the scorer's color floods the void, slow-mo 0.4s, score glyphs shatter-in. SCORCHER goals crack the table glass cosmetically for the next rally.
- **Crowd:** the void isn't empty — fields of bokeh lights bank and swell with heat and gasp (brightness dip) on saves; pure shader, no sprites, reads as ten thousand phones in a dark stadium.
- **Readability law (Pillar 4):** gameplay layer keeps guaranteed contrast (ball outline ≥ fixed luminance delta vs anything under it); spectacle renders beneath or behind. Automated check: a contrast probe in `make check` samples worst-case frames.
- **Comfort:** photosensitivity-safe caps (flash frequency/luminance-delta limits on all effects), reduce-motion setting (kills shake/slow-mo/radial pull, keeps color language), all honored by every effect with no exceptions.

## 9. Audio — the Second Half of Stunning

All synthesized at boot (no files): impacts are pitched by ball speed and filtered by strike type (bunt = felt thud, flick = struck glass, snap = whipcrack, chip = hollow pop, goal = subsonic detonation + choir-like shimmer). Crowd = shaped noise beds that track heat and gasp on saves. Music: generative synthwave engine, 4 layers (pulse bass, arps, pads, lead) — layers enter at heat 3/6/9/12 and the key lifts a half-step at match point for anyone with ears to feel. RALLY mode gets its own beatless ambient set. Sliders: master/music/SFX/crowd.

## 10. Presentation & Flow

Attract-mode title (two AIs rally behind the logo glass). Match flow: scheme select → (PARTY: mutators; Tilt if on) → serve. Between points: 1.2s max — this game respects the "run it back" reflex; instant-rematch is one button from the victory screen. Victory screen: stat card (longest rally, top heat, saves, scorchers, pins landed) + auto-replay of the best rally (from input recording — free via determinism). Pause anywhere; controller-disconnect auto-pause.

## 11. Persistence
One small save: settings, unlocked Gauntlet progress + cosmetic palettes, Trickshot medals, lifetime stat totals (longest rally ever is the crown jewel — display it on the title screen), best RALLY streaks. Local only.

## 12. Module Map
```
v6/
  Makefile  README.md
  src/
    main.c app.c        state machine, app_frame(), modes
    sim.c/.h            Section 5 exactly; pure; headless-testable
    rods.c/.h           rod/paddle state, schemes GLIDE/GRIP, input mapping
    heat.c/.h           heat ladder, mercy beat, scorcher tracking
    rules.c/.h          rulesets, serve/stall/shot clocks, scoring, mutators
    ai.c/.h             personalities, latency model
    fx.c/.h             particles, shockwaves, trails, slow-mo, contrast law
    shaders/*.glsl      table glass, ball halo, crowd bokeh, goal ripple (compiled-in strings)
    synth.c/.h          SFX synthesis + generative music layers
    ui.c/.h             menus, HUD, stat card, Tilt negotiation screen
    replay.c/.h         input recording, best-rally selection, playback
    save.c/.h
```
`sim.c` compiles headless with `make test`: determinism (1000-run bit-identity), swept-collision tunneling suite at heat 12, english/chip/snap behavior asserts, anti-stall triggers. `make check` = build + tests + contrast probe, merge-blocking.

## 13. Optional Parallel Lanes (if FOREMAN fans out)
Contract = §5 sim API + §12 file map, frozen once `sim.c` + `rods.c` pass tests with debug-rendered rectangles (playable gray-box FIRST — the game must already be fun with no shaders; that's the real gate). Then disjoint lanes: **L1** fx + shaders (§8), **L2** synth (§9), **L3** ai (§7) + Gauntlet, **L4** ui + modes + Trickshot authoring, **L5** replay + save. One lane per file set, sequential merges, `make check` green each merge.

## 14. Tuning Protocol (ordered; each gate is a feel test on real humans)
1. **Gray-box rally (pre-beauty).** Two devs/agents-days in: is GLIDE-vs-GLIDE already fun with rectangles? Tune rod speed, table proportions, v0. Nothing else proceeds until yes.
2. **The flick.** Distinct, punchy, controllable at three heats. Tune windup/recovery, assist bend.
3. **Heat curve.** A typical GLIDE rally reaches 5–8; GRIP players can push 10+; step 12 exchanges survivable ~50% with the mercy beat. Tune 1.11 exponent, beat duration.
4. **Pin/snap.** The snap must feel ILLEGAL (and be beatable by positioning). Tune shot clock, drag speed, recovery.
5. **Chip.** Useful ~1-in-4 attempts vs a decent blocker; landing-ring readability check with a first-time defender.
6. **GLIDE vs GRIP equity.** A good GLIDE player takes points off a mediocre GRIP player; a great GRIP player beats a great GLIDE player ~70/30. Tune assist bend and snap recovery — never nerf GRIP's ceiling; raise GLIDE's floor.
7. **AI ladder.** Rung 1 loses to a first-session player by design; rung 10 beats most humans; every personality's gimmick nameable by a stranger after two points.
8. **Beauty pass.** Only now do shaders tune — against the contrast probe and the photosensitivity caps, on top of a game already proven fun in gray.

## 15. Out of Scope
Online multiplayer (determinism keeps the door open; do not build netcode now); level/table editor; character bodies (rods ARE the characters); unlockable gameplay advantages (all unlocks cosmetic); voice/announcer; mobile touch.

## 16. Acceptance Criteria (Definition of Done)
1. `make check` green (determinism, tunneling, contrast probe, photosensitivity caps); desktop (Win/Linux) + web builds run; web playable with two pads on itch-style static hosting.
2. **The 90-second test:** three first-time players on GLIDE each score within 90 seconds of first serve, unprompted, and can name what the glowing meter (heat) means.
3. **The rally test:** two practiced GRIP players produce a 15+ contact rally with at least one pin, one chip, and one bank in a filmed session — and the footage looks like a trailer with zero staging.
4. All five modes complete; Table Tilt negotiable and badge-visible; STANDARD ruleset locked and mutator-free.
5. Ten AI personalities present and distinguishable; AI passes the honesty audit (latency-model inputs only).
6. Gray-box build retained behind a debug flag — proof forever that the fun predates the glow.
7. All content original (names, banter, music, visuals).

---
*Six rods, one comet, no spinning. Make the casual player dangerous, the skilled player glorious, and the rally the whole damn point.*
