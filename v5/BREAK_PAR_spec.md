# BREAK PAR
### Requirements & Design Specification — v1.0
*Mini golf with a cue stick. Pool with a landscape. One ball, eighteen holes, zero mercy.*

(Working title — a "break" in pool, "par" in golf. Rename freely.)

---

## 0. Placement, Build Context & Agent Instructions

- **Repo location:** this game lives in the `v5/` folder of the existing games monorepo, alongside v1–v4. It must be fully self-contained inside `v5/` — no shared code with sibling folders, no dependencies on files outside `v5/`.
- **Build conventions:** match the build conventions used by the earlier games in this repo (inspect v1–v4 before starting). If conventions differ between them, prefer a plain `Makefile` with `make` (release) and `make debug` targets. Primary target: Windows x64 standalone executable. Secondary: Linux x64. Cross-compile via MinGW-w64 is acceptable.
- **Language & libs:** C (C99 or C11), raylib for window/input/render/audio device. No other third-party libraries. No C++ except what raylib itself requires internally.
- **Everything is generated:** no shipped textures, no shipped models, no shipped audio files, no shipped fonts beyond raylib's default. Meshes, textures, palettes, and every sound are procedurally generated at startup or synthesized at runtime. Course data is compact static C arrays compiled into the binary.
- **This spec is complete — no gated phases.** Build the whole game. Section 16 is a tuning ORDER, not a phase plan. If any two sections conflict, Section 5 (Physics) and Section 2 (Hard Constraints) win, in that order.
- **The one thing to protect above all:** the shot must FEEL amazing (Section 10). A finished game with mushy hits loses to an unfinished game with a crunchy strike. When in doubt, spend effort on feel.

---

## 1. Pitch & Design Pillars

**Pitch:** It looks like mini golf — 18 whimsical 3D holes with ramps, bumpers, gaps, and gimmicks. But you're not putting: you're shooting pool. You walk the cue around the ball, pick your line, pick your power, and pick your ENGLISH — where the cue tip strikes the ball. Draw the ball back off a bank. Ride running english around a corner. Smash a combo through an object ball into a bonus pocket. Sink the cup in the fewest strokes. Sometimes going crazy helps; other times tact and skill is key.

**Design pillars — every decision gets tested against these:**

1. **The cue is the character.** Spin, banks, and combos are what make this pool and not mini golf. Every hole must reward at least one pool technique (bank, english, draw/follow, or combo). If a hole can be beaten optimally by "aim straight, full power," redesign it.
2. **Two answers per hole.** Every hole has a safe, readable route that an average player can par, AND a discoverable hero line (risk/reward shortcut) that can birdie-or-worse. "Going crazy" must be a real strategy with real downside.
3. **Simple verbs, fiendish courses.** The input never grows past aim + power + spin. All depth and novelty lives in course design, never in added mechanics or menus. (This is the Kirby's Dream Course lesson: abandon realism, keep the shot model tight, put all the imagination into the holes.)
4. **Physics is the authority.** One deterministic simulation drives everything. No scripted ball movement, no cheat magnetism into the cup, no canned outcomes. If a trick shot works, it works because the sim says so — that's what makes replays and bragging real.
5. **Deterministic.** Same hole + same shot inputs = same result, every time, on every machine. No wall-clock time in game logic. Moving obstacles are phase-locked to the shot clock (see 6.4).
6. **Fits on the floppy.** Total fileset ≤ 1,474,560 bytes, standalone native binary, fully offline. This is a hard ceiling, not a target (see Section 13).

---

## 2. Hard Constraints

These come from the 1.44MB contest this game may enter. Violating any of them makes the build worthless for that purpose:

- Total fileset (executable + any data files it needs) ≤ **1,474,560 bytes**.
- Standalone **native** executable. Not browser-based. No installer required.
- Fully **offline**. No downloading, no streaming, no network calls of any kind.
- **Original IP only.** No borrowed characters, logos, music, or recognizable trademarks. All names, art, and sounds original.
- Runs on stock Windows 10/11 x64 with no runtime installs (statically link the C runtime or use raylib defaults that avoid DLL dependencies).

---

## 3. Player Experience & Core Loop

**Session shape:** Title → course select (Front 9 / Back 9 / Full 18) → play holes in order → scorecard between holes → final scorecard with totals and best-hole callouts → back to title. A full 18 should take a first-time player 25–40 minutes; a skilled player 12–15.

**Per-shot loop (the heartbeat of the game):**

1. **SURVEY** — free orbit camera around the resting ball. Player reads the hole. No timer.
2. **AIM** — a direction is always live (the cue rendered behind the ball shows it). Aim guide shows the projected path to first contact plus a short rebound stub (see 4.3).
3. **DIAL** — player sets spin on the cue-ball face widget (default: center) and commits to power via the meter.
4. **STRIKE** — cue animates a real backswing-and-strike. Crack. Ball goes.
5. **RIDE** — camera chases the ball. The world reacts. Player watches physics resolve — this is the payoff phase, it must be gorgeous and readable.
6. Ball comes to rest (or holes out / hazards out) → stroke count updates → back to 1.

**Per-hole loop:** tee-off → shots until holed or stroke cap (8 = "RACKED", auto-complete at cap) → hole-out celebration scaled to score vs par → scorecard → next hole.

The SURVEY→STRIKE loop must be fast to re-enter. A player who knows what they want should be able to go from ball-at-rest to struck ball in under 3 seconds. Never put a menu, confirmation, or unskippable animation inside the loop.

---

## 4. Shot System (Input, Aim, Power, Spin)

Input targets keyboard+mouse first; keyboard-only must be fully playable (contest machines may lack a mouse a player wants to use).

### 4.1 Aiming
- Mouse-drag (or Left/Right keys) rotates aim direction around the ball on the horizontal plane. Fine-adjust modifier (hold Shift) slows rotation 8× for surgical banks.
- The cue stick renders behind the ball on the aim line at all times during AIM. The cue is the reticle.
- Vertical cue elevation is **out of scope** (no massé, no jump shots via elevation). Airborne play comes from terrain (ramps), not the cue.

### 4.2 Power
- Hold Space (or LMB on the ball): power meter fills 0→100 in ~0.9s, then oscillates back down and up continuously until release. Release = strike at current value. This punishes both greed and hesitation, and it's legible to anyone watching.
- Power maps to cue-ball launch speed non-linearly: `v = v_max * pow(p, 1.35)` where p ∈ [0,1]. The curve gives fine control at low power (touch shots around the cup) without nerfing the top end. `v_max` starting value: 9.0 m/s (tune per Section 16).
- A soft-tap floor: minimum strike is 4% power, so a mis-release never produces a zero-length "wasted" stroke.

### 4.3 Aim guide
- A dotted line from the ball along the aim direction to **first contact only** (wall, obstacle, or object ball), then a short 1.5-ball-diameter rebound stub showing the *center-ball* rebound direction off that surface.
- The stub deliberately IGNORES spin. Spin outcomes are the player's skill to learn — the guide teaches geometry, the player learns english. This is the single most important skill-preservation decision in the game; do not "improve" the guide to account for spin or show multiple bounces.
- On object-ball contact, show the classic pool ghost-ball: object ball's departure direction line (short), nothing for the cue ball. Pool players will feel at home; new players learn the tangent visually.

### 4.4 Spin (English) — the pool heart of the game
- A cue-ball face widget (2D circle, bottom-left HUD): a dot marks the strike point. Arrow keys / WASD (or click) move the dot within 70% of ball radius from center. Center by default; resets to center each hole (not each shot — deliberate: leaving your last spin dialed is a small mastery reward and a small trap).
- Vertical offset = follow (top) / draw (bottom). Horizontal offset = left/right english.
- Effects (implemented in the sim, not as scripted outcomes):
  - **Follow:** ball carries forward roll; after a full ball-ball hit it continues forward instead of stopping.
  - **Draw:** backspin; after a full hit the ball pulls back toward the shooter. Draw decays with distance traveled (backspin bleeds off via cloth friction), so long-range draw is naturally weaker — teach this in Hole 3.
  - **Side english:** modifies cushion rebound. Running english widens the rebound angle and preserves speed; check english narrows the rebound and kills speed. This is THE tool for multi-bank routes.
  - **Stun:** center-ball at the right speed arrives sliding with no roll; full hit stops the cue ball dead. The positional-play fundamental.
- Squirt/deflection and collision-induced throw (real-pool subtleties) are **deliberately omitted** — the aim line always means what it shows for the cue ball's initial direction. Arcade honesty beats simulation purity.

### 4.5 Quality-of-life
- `R` during RIDE: instant-skip ball to rest (sim runs to completion at max speed internally — result identical, determinism preserved).
- `Tab`: toggle scorecard overlay. `Esc`: pause (resume / restart hole [+strokes stand] / quit to title).
- Restarting a hole resets strokes for that hole to 0 — this is a casual-friendly game; the scorecard tracks "restarts used" as a gentle honesty flag instead of forbidding it.

---

## 5. Physics Specification (Ground Truth)

This section is the contract. Everything else observes it.

### 5.1 Integration & determinism
- Fixed physics timestep: **1/240 s**, semi-implicit Euler, accumulated from frame time; render at display rate with interpolation between physics states.
- All game logic consumes the physics tick counter, never wall-clock. Moving obstacles, animations that affect collision, and hazard timers all derive from tick count since shot start (see 6.4).
- Use `float` consistently; avoid `sin/cos` in per-tick integration paths where accumulation error could diverge (precompute obstacle motion as functions of tick, not incremental deltas). No randomness anywhere in simulation. One `xorshift` PRNG for cosmetic-only effects (particles), seeded per hole, never read by sim code.

### 5.2 Ball model & motion states
The ball is a sphere: radius **R = 0.0286 m**, mass **m = 0.17 kg** (standard pool ball; keeps all constants in familiar territory). State: position, velocity `v`, angular velocity `ω` (full 3D vector — english lives here).

On the surface, the ball is in one of three regimes, exactly as real billiards models do:

1. **SLIDING** — contact-point velocity `u = v + ω × (R·n̂)` is non-zero. Friction `μ_slide` acts opposite `u`, decelerating `v` and torquing `ω` toward rolling. This is where draw/follow/stun physics happen.
2. **ROLLING** — `u ≈ 0` (threshold 0.005 m/s): pure roll. Deceleration by `μ_roll` only. Side-spin component about the vertical axis persists and decays independently via `μ_spin` (it barely affects a straight roll — it matters at the next cushion).
3. **AT REST** — `|v| < 0.01 m/s` while rolling and no slope force exceeding static threshold → snap to rest, end RIDE.

Starting friction constants (cloth): **μ_slide = 0.20**, **μ_roll = 0.010**, **μ_spin = 0.044** (vertical-axis spin angular decay ∝ μ_spin·g/R). These are the classic billiards-simulation values; tune from here per surface zone (Section 6.3) and per feel (Section 16).

### 5.3 Cue strike
Strike applies velocity `v` along the aim direction (magnitude from the power curve, 4.2) and angular velocity from the strike-point offset `(a, b)` on the ball face:
- `ω_side = -(a / R) · k_spin · |v|` (about vertical axis)
- `ω_topback = (b / R) · k_spin · |v|` (about the horizontal axis perpendicular to aim; +b top = follow, −b = draw)
- `k_spin` starting value **2.2** (dimensionless spin gearing — this is a primary feel knob; real pool is ≈ 5/2·offset but games want it hotter so english reads on short distances).

### 5.4 Ball–cushion (walls & rails)
All vertical-ish surfaces use the cushion model:
- Normal restitution **e_wall = 0.85** (real rails measure ≈ 0.98 at low speed, but a slightly lossy wall makes multi-bank routes controllable and keeps balls from ping-ponging forever; super-bumpers override this upward).
- Tangential cushion friction **μ_wall = 0.14** (the measured real-world value): this is what converts side-english into changed rebound angle and speed. Implement as an impulse pair: normal impulse from restitution, tangential impulse capped by `μ_wall ×` normal impulse, applied against the contact-point slip — running/check english falls out naturally.
- Speed-dependent restitution: above 4 m/s, scale `e_wall` down linearly to 0.75 at `v_max` (hard smashes shouldn't rebound live — rewards touch).

### 5.5 Ball–ball (object balls)
- Instantaneous impulse collision, restitution **e_ball = 0.95**, along the line of centers. Tangent-line rule emerges: cue ball departs near-perpendicular on cut shots.
- Post-impact, the cue ball's retained spin (follow/draw) then acts through the SLIDING regime — draw-back and follow-through emerge from 5.2, do not script them.
- Ball–ball friction (throw) omitted (see 4.4).

### 5.6 Terrain & 3D motion
- Holes are heightfield-plus-prisms: a coarse triangle-mesh floor (slopes, ramps, bowls) with wall prisms. Gravity **g = 9.81 m/s²** projects onto the surface tangent for slope acceleration.
- Ball leaves the ground (ramp lip, drop) → AIRBORNE state: pure ballistic flight, ω preserved. Landing: normal restitution **e_floor = 0.45**, ground friction impulse applies spin coupling (a draw-heavy ball visibly checks up on landing — juicy and correct).
- Continuous collision for the ball against all geometry (swept sphere) — at 1/240s with `v_max` 9 m/s the ball moves ~0.04m/tick ≈ 1.3 ball radii, so tunneling through thin walls is a real risk without sweeping. Do not skip this.

### 5.7 The cup (hole capture)
- The cup is a cylindrical depression, radius **2.0·R**, depth 1.5·R, with a rim lip.
- No magnetism. Capture is honest: ball over the cup opening loses floor support, falls in; too fast and it flies across or rims out. Rim-out (ball enters, orbits the lip, escapes) must be POSSIBLE — it's the most dramatic near-miss in golf games and players scream at it in the best way. Model the rim as a torus-ish contact (sphere vs. inner cylinder edge is sufficient).
- Captured = ball center below cup rim plane with speed insufficient to escape → HOLED. Slow-mo + celebration (Section 10).

### 5.8 Hazards & resets
- **Void/water:** ball falls past kill-plane or enters a water volume → +1 penalty stroke, ball respawns at its position before the shot (not the tee). Splash/fall effect, quick.
- **Scratch pockets** (Section 7.3): same penalty flow, distinct sound and message ("SCRATCH!").
- Respawn placement must be deterministic and never inside geometry (store pre-shot rest position; it was valid by construction).

---

## 6. Course & Obstacle Design

### 6.1 Design rules (apply to every hole)
1. **One gimmick per hole.** Each hole introduces or remixes ONE idea. Two gimmicks may coexist only in the Back 9 and only after each appeared solo earlier.
2. **Readable from survey.** The full solution space must be visible from orbit-camera survey — no hidden information, no leap-of-faith shots. Surprises live in execution, not in knowledge.
3. **Safe route + hero line.** The safe route pars for a player using center-ball only. The hero line saves 1–2 strokes and requires a named pool technique (bank, draw, running english, combo) with a real fail cost (hazard, scratch pocket, or bad position). Multiple viable approaches with meaningful risk/reward is the through-line of good course design — every hole gets both.
4. **Par honesty.** Par = strokes for the safe route executed competently. Front 9 pars: 2–3. Back 9 pars: 3–4. Total course par ≈ 52.
5. **Fail forward.** A missed hero shot should usually leave a playable (worse) position, not an auto-hazard. Reserve brutal punishment for the two "casino" holes (6.5).

### 6.2 Obstacle vocabulary
Everything is built from this kit. No one-off mechanics outside it.

| # | Obstacle | Behavior | Why it's fun |
|---|----------|----------|--------------|
| 1 | **Rail** | Standard cushion (5.4) | The default wall; banks are core |
| 2 | **Super-bumper** | Mushroom/round post, restitution 1.4, adds fixed outward impulse, *boing* | Pinball chaos, comeback tool |
| 3 | **Kicker pad** | Floor arrow pad: sets ball speed to fixed value along arrow direction | Speed reset = long-route enabler |
| 4 | **Ramp / hill** | Heightfield slope; launches airborne at lips | Verticality, flight, landings |
| 5 | **Half-pipe / channel** | U-channel that funnels and carries the ball | Guided thrill sections |
| 6 | **Rough felt zone** | Floor patch, μ_slide 0.5 / μ_roll 0.04, dark color | Golf "rough": kills hero overshoots |
| 7 | **Ice patch** | μ_slide 0.05 / μ_roll 0.002, glossy | Touch-shot terror, slide comedy |
| 8 | **Sand trap** | Extreme drag: caps speed at 1.2 m/s inside, exit costs momentum | Classic hazard; escapable, annoying |
| 9 | **Void / water** | Kill volume → +1, respawn (5.8) | Stakes |
| 10 | **Scratch pocket** | Pool pocket that PENALIZES (7.3) | Pool-native hazard; aiming AT pockets vs AVOIDING them in one game |
| 11 | **Warp pocket** | Enter pocket A, eject from pocket B, speed preserved, brief tunnel cam | Pool-native teleport; signature obstacle |
| 12 | **Sweeper / spinner** | Rotating bar or orbiting blocker, phase-locked to shot clock (6.4) | Timing puzzles that are fair |
| 13 | **Gate + target** | Path blocked until a target (button or specific object ball) is struck | Multi-shot puzzle structure |
| 14 | **Tilt platform** | Section whose slope is a function of shot-clock ticks | Late-game routing spice |
| 15 | **Object ball** | Full physics peer (Section 7) | Combos, blockers, keys |

### 6.3 Surface zones
Floor triangles carry a surface ID (felt / rough / ice / sand / boost) that indexes a friction table. Zone edges are color-coded with strong palette contrast — a player must read surface type at a glance from orbit distance.

### 6.4 Moving obstacles & determinism
Every mover's pose is a pure function `pose(t)` of ticks since SHOT START, and all movers reset to phase 0 when the shot begins. This means: what you see during AIM is exactly what you get, timing routes are plannable, and the sim stays deterministic. Movers freeze during AIM at their phase-0 pose. (This is a deliberate departure from wall-clock movers in other golf games — it converts "wait for the gap" into "plan the gap," which suits a shot-planning game.)

### 6.5 The 18 holes
Front 9 teaches the language; Back 9 speaks it fluently. Per-hole briefs (FOREMAN: build these as data + geometry; layouts are yours to author within the brief, design rules 6.1 bind you):

| Hole | Par | Gimmick / brief |
|------|-----|-----------------|
| 1 | 2 | Straight lane, one dogleg rail. Teaches aim+power. Hero: one bank cuts the corner. |
| 2 | 2 | Bank alley: cup behind a blocker, must play one cushion. Teaches rails. |
| 3 | 2 | Draw range: cup on a ledge behind a gap; overshoot falls off. Teaches draw to stop/pull back. Hero: draw off the back wall. |
| 4 | 3 | English corridor: S-curve where running english holds the line. Teaches side spin. |
| 5 | 3 | First object ball: dead-straight combo into the cup vs. safe route around. Teaches combos. |
| 6 | 3 | Ramp + landing green with rough collar. Teaches air + landing spin. |
| 7 | 3 | Warp pockets debut: warp is the safe route; overland hero line over a narrow bridge. |
| 8 | 3 | Scratch-pocket minefield: wide green peppered with scratch pockets; bonus pocket (7.2) tucked in the middle. First real greed test. |
| 9 | 3 | Front-9 exam: bank + draw + one sweeper. |
| 10 | 3 | RACK HOLE debut (7.4): pot the 8-ball to open the cup. |
| 11 | 3 | Ice rink with rails: multi-bank on low friction; check english is the tool. |
| 12 | 4 | The Long One: kicker-pad chain, three route options of varying risk. |
| 13 | 3 | Casino hole #1: par-3 safe loop OR a full-power hole-in-one line over two ramps between scratch pockets. |
| 14 | 3 | Object-ball billiards room: cluster of balls; blast (chaos) vs. surgical picks; two bonus pockets. |
| 15 | 4 | Tilt platforms + warp pair; timing route planning. |
| 16 | 3 | RACK HOLE 2: 8-ball guarded by a sweeper; combo through a guard ball to free it. |
| 17 | 3 | Casino hole #2: everything is bumpers; the "going crazy" hole — a hard smash usually ends up somewhere interesting; a genius bank aces it. |
| 18 | 4 | Finale showpiece: multi-tier descent using every taught verb, warp finish into a stadium green with the cup on a slow island tilt. Must produce a screenshot-worthy final shot. |

---

## 7. Pool Identity Systems

These four systems are what make the game POOL. All four ship.

### 7.1 Object balls
Numbered/colored balls placed by hole design; full physics peers of the cue ball (5.5). Uses: blockers to negotiate, combo tools (shoot through them into the cup or a bonus pocket), keys (rack holes). Object balls that enter void/water respawn at their start position at next shot start (deterministic; they're props, not scores — EXCEPT in the two systems below).

### 7.2 Bonus pockets (the "going crazy" engine)
Some holes have gold-rimmed pockets. Pot an OBJECT ball into a bonus pocket → **−1 stroke** on this hole (floor at 0). Pot the CUE ball into one → it's still a scratch (+1, respawn) — greed cuts both ways, and the gold rim tempts exactly the shot that scratches you. Cap: max 2 bonus captures per hole. This one system creates the entire "chaos can pay" fantasy while the stroke counter keeps "tact and skill" as the reliable baseline.

### 7.3 Scratch pockets
Standard-looking pool pockets placed as hazards. Cue ball in → "SCRATCH!", +1 stroke, respawn at pre-shot position (5.8). Object ball in → ball is simply gone for the remainder of the hole (removed; can open routes — occasionally the smart play).

### 7.4 Rack holes (the Dream Course inversion)
On holes 10 and 16 the cup starts sealed (visibly capped, desaturated). An 8-ball sits somewhere defended. Pot the 8-ball into ANY pocket on that hole → dramatic uncap animation, cup goes live. This inverts the objective mid-hole (first play pool, then play golf) and is the game's most memorable structural trick. Keep it to exactly two holes so it stays special.

---

## 8. Scoring & Progression

- Stroke play vs. par, golf terms on the banner (Eagle/Birdie/Par/Bogey…), pool-flavored flair text underneath ("CLEAN BREAK", "HUSTLED", etc. — writer's choice, original phrasing only).
- Stroke cap 8 per hole → "RACKED" (auto-advance, score 8). Keeps worst-case pace bounded.
- Scorecard between holes: per-hole strokes vs par, running total, restarts-used flag.
- Persistence: one tiny local save file (`breakpar.sav`, <1 KB, plain binary) storing best score per hole, best 9/18 totals, and holes-in-one count. No settings beyond volume + fullscreen toggle (also saved).
- Final screen: 18-hole card, total vs par, and the three best moments by simple heuristics (longest holed shot, most banks in a holed shot, any ace) listed as text callouts.

---

## 9. Camera & Presentation

- **Survey/Aim cam:** orbit around ball, pitch clamped 15°–80°, zoom 3 steps. Mouse drag / arrow keys.
- **Ride cam:** smoothed chase behind ball velocity, pulls wider with speed; cuts to a fixed "cup cam" when the ball is within 6·R of the cup at low speed (the tension zoom — this shot sells every near-miss and every drop).
- **Warp transitions:** 200ms tunnel blur, camera re-acquires at exit pocket.
- Camera never clips through walls: simple sphere-cast pullback.
- **Hole intro:** 2.5s automated flyover from cup back to tee on first arrival (skippable) — doubles as the readability pass for design rule 6.1.2.

---

## 10. Juice Inventory (feel checklist — all items ship)

1. **The strike:** cue backswing scales with power; on release, 2-frame hitstop, synth "crack" whose brightness scales with power, small radial shockwave decal, camera kick (1–3px) at power >60%.
2. **Rolling audio:** filtered noise loop, cutoff/gain tracking ball speed; distinct timbre per surface zone (ice rings, rough is dull).
3. **Cushion hits:** thump + tiny wall flex scale animation; pitch tracks impact speed. Super-bumpers: musical *boing* on a pentatonic set, light bloom pulse.
4. **Ball–ball:** classic pool clack (bandpassed click), both balls flash 1 frame.
5. **Spin visibility:** cue-ball texture is a generated two-tone pattern so spin READS at a glance; a faint colored trail tints by spin type (draw = cool, follow = warm, side = green tint). Subtle, not a light show.
6. **Rim-out:** dedicated rattling loop while orbiting the lip; if it escapes, a beat of silence, then a sad slide-whistle-ish synth dip. Make the tragedy funny.
7. **Hole-out:** slow-mo ramp (0.3× over final approach when capture is certain), cup swallow sound, confetti burst scaled to score vs par, banner slam.
8. **Ace anywhere:** full replay of the shot from a cinematic side angle (re-simulate from recorded inputs — determinism makes this free), "ACE" banner, best-moment flag.
9. **Scratch:** record-scratch-style synth + red vignette flash, quick respawn. Punchy, never sluggish.
10. **Idle charm:** ball breathes a subtle sheen; background elements (flags, warp pocket glow) animate on the cosmetic PRNG.

---

## 11. Audio (all synthesized)

- raylib audio device; one mixer, all SFX synthesized at startup into short PCM buffers (crack, clack, thump, boing set, swallow, splash, banners) from parameterized recipes (noise bursts, sine/tri plucks, filtered sweeps). Zero audio files shipped.
- Music: a light generative ambient loop per 9 (two moods), tracker-style: a 16-step pattern engine driving 3 synth voices (bass, pad blips, brush-noise percussion), patterns as bytes in code. Music ducks 6 dB during RIDE so physics audio stars. Volume sliders: master/music/SFX.
- Total audio code + data budget: ≤ 40 KB.

---

## 12. Rendering & Art Direction

- raylib 3D: flat-shaded low-poly, strong palette discipline. One global directional light + hemisphere ambient; blob shadow under every ball (a soft quad — no shadow mapping).
- Palette: generated per 3-hole set from a base hue walk — felt greens shift toward twilight blues across the 18 so the course visibly "ages" as you play. Surface-zone colors (6.3) stay constant and high-contrast across all palettes.
- Geometry: all procedural — floors from heightfield data, walls extruded from 2D outlines, obstacle meshes (mushroom bumpers, pockets, cue stick, flags) built from primitives at load. Course source data target ≤ 2 KB per hole.
- Text: raylib default font, scaled; banners drawn as text + shapes. No font files.
- Resolution: 1280×720 default window, fullscreen toggle, render scales with window.
- Performance floor: 60 fps on integrated graphics (Intel UHD-class) — geometry counts are tiny; the risk is particle overdraw, so pool/reuse particles (cap 512).

---

## 13. Size & Performance Budget

Hard ceiling 1,474,560 bytes total fileset. Working budget:

| Item | Budget |
|------|--------|
| Engine + game code (compiled) | ≤ 350 KB |
| raylib (static, LTO'd, unused modules stripped) | ≤ 700 KB |
| Course data (18 holes, compiled-in) | ≤ 40 KB |
| Audio recipes + pattern data | ≤ 40 KB |
| Headroom | ≥ 300 KB |

- Build flags: `-Os -flto -ffunction-sections -fdata-sections -Wl,--gc-sections`, strip symbols. Compile raylib from source with unused modules disabled (no models loading, no image file formats beyond none, no camera module if unused).
- If over budget after stripping: 1) prune raylib further, 2) shrink audio recipes, 3) only then consider executable compression (UPX) — flag it to Forrester first, since packed executables can trip contest machines' AV.
- `make size` target prints total fileset bytes and fails the build if over ceiling. Run it in CI/checks from day one.

---

## 14. Module Map & File Layout

```
v5/
  Makefile
  README.md            (build + controls + one-paragraph pitch)
  src/
    main.c             loop, state machine (TITLE/SELECT/PLAY/CARD/FINAL)
    physics.c/.h       Section 5 exactly; owns all sim state; pure (no rendering)
    shot.c/.h          aim/power/spin input state, guide computation, strike handoff
    course.c/.h        hole data tables, geometry gen, surface zones, movers pose(t)
    obstacles.c/.h     obstacle behaviors (Section 6.2 kit)
    pool.c/.h          object balls, pockets (bonus/scratch/warp), rack-hole logic
    camera.c/.h        Section 9
    juice.c/.h         Section 10: particles, hitstop, slow-mo, banners
    synth.c/.h         Section 11: SFX synthesis, pattern music engine
    render.c/.h        Section 12: mesh gen, palettes, HUD, cue-ball widget
    save.c/.h          Section 8 persistence
    replay.c/.h        input-record + re-sim for ace replays and debugging
  data/
    holes.h            18 hole definitions (generated or hand-authored C arrays)
```

- `physics.c` must compile standalone with a headless test harness (`make test`): scripted shots asserting determinism (same inputs → bit-identical rest positions across 1000 runs) and sanity (draw pulls back, running english widens rebound, rim-out possible). These tests are part of Definition of Done.
- `replay.c` records per-shot inputs (aim angle, power, spin offsets) — 16 bytes/shot. Free replays, free bug repros ("timing feels off" reports arrive with a replay attached).

---

## 15. Difficulty, Assists, Fairness

- No difficulty modes. The safe-route/hero-line structure IS the difficulty system.
- The aim guide (4.3) is always on — this game's challenge is planning and execution, not deprivation. Do not add an option to extend the guide.
- First-bogey tip system: one-line contextual hints ("Draw spin pulls the ball back after contact — try ↓ on the ball face") shown at most once each, max 5 total in the game, only after a relevant failure. Never interrupt AIM.

---

## 16. Tuning Protocol (ordered — the numbers to hammer, and the test each must pass)

Everything ships built; this is the order in which to make it GOOD. Each gate is a feel test on a real human (Forrester), not a metric.

1. **The strike (4.2, 5.3, 10.1).** Flat ground, no obstacles. Test: hitting the ball at three power levels feels *crunchy* and distinct with eyes closed (audio alone tells you the power). Tune: hitstop frames, crack synthesis, power curve exponent, `v_max`.
2. **Stop distance (5.2).** Test: a 50% power shot on flat felt travels a predictable, memorizable distance; three shots in a row land within one ball-width of each other and the player *feels* they could lag to a line. Tune: μ_slide/μ_roll.
3. **Draw & follow (5.3, 5.5).** Straight full hit on an object ball at 40% power. Test: max draw pulls back ≥ 1.5 ball-travel-lengths and looks dramatic; follow trickles forward naturally; stun stops dead. Tune: `k_spin`, μ_slide.
4. **Cushion english (5.4).** One-cushion bank at 45°. Test: max running vs max check english produce obviously different rebound angles (≥ 15° apart) and the difference is visible in the ball's path without HUD help. Tune: μ_wall, e_wall.
5. **The cup (5.7).** Test: a dead-weight ball always drops; a firm ball rims out sometimes; a smashed ball flies over. Rim-outs occur on maybe 1 in 8 aggressive putts — enough to fear, not enough to hate. Tune: cup radius multiplier, rim restitution.
6. **Hole 1–3 par honesty (6.1.4).** A center-ball-only player pars each within 3 attempts. Retune layouts, not physics, from here on — physics is FROZEN after gate 5 (all later balance is course-side; this protects every earlier gate).
7. **The casino holes (13, 17).** Test: the hero line hits ≈ 1 in 4 for a player who's tried it ten times, and failing it is funny (bumper chaos) not deflating (instant void). Tune: layout geometry, scratch-pocket placement.
8. **Full-18 pace.** A complete run ≤ 40 min first time. Trim RIDE lengths (kicker speeds, μ tweaks per zone allowed here as course-side surface table edits) if any hole regularly exceeds 3 minutes.

---

## 17. Acceptance Criteria (Definition of Done)

1. `make` produces a standalone Windows x64 executable; `make size` passes (≤ 1,474,560 bytes total fileset); runs on a clean Windows 10 VM with no extra DLLs.
2. `make test` passes: determinism suite (bit-identical re-sims), spin behavior asserts, tunneling test (max-power shot at thinnest wall, 10k trials, zero pass-throughs).
3. All 18 holes completable; stroke cap works; scorecard, save file, and final screen function; deleting the save file yields a clean first-run.
4. All four pool systems (7.1–7.4) present and reachable in normal play.
5. Every juice item in Section 10 present.
6. 60 fps on integrated graphics at 720p throughout, including hole 18.
7. All content original (names, art, sounds, text) — nothing referencing existing games, brands, or music.
8. README documents controls, build, and the safe-route/hero-line idea in one paragraph each.

## 18. Out of Scope (do not build)

Multiplayer of any kind; level editor; cue elevation/massé/jump shots; online anything; difficulty settings; controller support (stretch only after all acceptance criteria pass); localization; achievements beyond the save-file bests.

---

*End of spec. Physics is the contract, courses are the content, the strike is the soul.*
