# PULSEWING
### Requirements & Design Specification — v1.0
*A rail shooter flown with a heartbeat. Tap to rise, fall to dive, thread the ruins, and never stop moving.*

(Working title — you pulse to fly. Rename freely.)

---

## 0. Placement, Build Context & Agent Instructions

- **Repo location:** `v8/` in the games monorepo, fully self-contained. Match repo build conventions; else plain `Makefile` (`make`, `make debug`, `make web`, `make check`).
- **Stack:** C11 + raylib, GLSL shaders permitted (sky, light, and speed are the spectacle). No other libraries.
- **Everything procedural:** zero shipped assets. Terrain, ruins, ship, sky, and all audio generated. Stage data = compact C arrays.
- **Web build first-class** (house pattern): single `app_frame()`, single-threaded, audio init on first gesture on web, `ALLOW_MEMORY_GROWTH=1`, `index.html/.js/.wasm` output for itch/Pages. CI builds web + desktop together.
- **No size ceiling.** Quality wins. Procedural keeps it small anyway.
- **This spec is complete — no gated phases.** Section 15 is a tuning ORDER. Conflicts resolve in favor of §4 (Flight Model) then §9 (Restart Law) then §1 (Pillars).
- **Original IP only.** Star Fox and Flappy Bird are named in this doc as design references; nothing in the game may reference either (no names, quotes, ship shapes, sound-alikes, or bird-and-pipe iconography).

## 1. Pitch & Design Pillars

**Pitch:** A behind-the-ship 3D rail flier through golden-hour sky ruins. Your craft flies itself forward, forever. You control exactly one thing the way a heartbeat controls a body: TAP, and the ship pulses upward; do nothing, and gravity takes it. Thread the gaps in oncoming arches, walls, and rings. Lasers and a deflecting roll are right there when you want them — an entire shooter layered on top of one perfect button. Die instantly, restart faster, fly further.

**Pillars:**
1. **One input is the whole truth.** The tap is sacred: it sets your vertical fate instantly, identically, every time. Everything else in the game — shooting, rolling, drifting — is optional seasoning on a dish that is complete with one button. The full campaign main route must be finishable without ever firing a shot.
2. **The restart IS the game.** Fail instantly, fly again instantly — under one second from impact to airborne, no menus, no loading, ever. Protect this number like the frame rate.
3. **On rails, in flow.** The rail does the steering so the player's entire mind lives in one clean channel: altitude and timing. Difficulty comes from gate placement and pace, never from camera, controls, or clutter.
4. **Fair at speed.** Every obstacle is readable at current velocity with time to respond. Deaths must always replay in the player's head as "I tapped late," never "I couldn't see it."
5. **Short campaign, tall replay.** The Star Fox 64 doctrine: a single run is brief; branching routes, medals, and secrets make the tenth run different from the first.
6. **Deterministic** (house law): fixed-step sim, seeded spawns, input-recorded ghosts and replays for free.

## 2. Core Loop & Run Structure

**Moment-to-moment:** the world streams toward you on a rail spline (banked curves, climbs, dives — the rail itself is choreography). Obstacles arrive as readable silhouettes against the sky. You tap to rise through gaps, hold fire to clear destructibles, roll to deflect. Every gate passed ticks the score with a rising musical chime; every clean center-ring pass feeds the combo. One touch of terrain or an un-deflected hit (per ruleset) ends the run: quick death bloom, tap, airborne again.

**Session shapes:**
- **CAMPAIGN:** 9 authored stages (90–150s each) on a branching route map (§8). Story-light: text-card vignettes, a rival wing, a storm to outfly. Finishable in a sitting; built to be re-flown.
- **ENDLESS:** infinite procedural run, classic one-hit rules, daily seed + free seed, local leaderboard, best-run ghost flying beside you.
- **PURE:** endless with flap only — no fire, no roll, no drift. The monastery. Separate leaderboard.

## 3. Controls & Verbs

Input: keyboard, mouse, or gamepad; every action remappable; entire game playable on a single button (menus included: tap-to-cycle).

| Verb | Input | Effect |
|------|-------|--------|
| **PULSE** | tap (space / click / A) | Sets vertical velocity to +V_pulse instantly — not additive, not ramped. The Flappy law: the tap means exactly the same thing at every moment of the game |
| **FIRE** | hold (F / RMB / X) | Auto-fire twin lasers along the flight vector; generous aim cone (§6). Optional always-autofire toggle |
| **ROLL** | R / LB / B | 0.45s aileron roll: deflects all projectiles (the rail-shooter homage), flashes gold on a successful deflect. Cooldown 1.5s. NEVER protects against terrain — the world is always honest |
| **DRIFT** | stick/arrows left-right (optional) | Small lateral offset (±6m) within the corridor: reaches bonus rings and secret-route triggers. The main route never requires it (§5 guarantee) |

No other inputs exist. PURE mode disables all but PULSE.

## 4. Flight Model (the contract)

- Fixed timestep **1/240s**, semi-implicit Euler, render-interpolated; all logic on ticks; one seeded PRNG for spawns (cosmetic PRNG separate).
- **Forward:** speed is owned by the rail — base **26 m/s**, authored per segment (calm 22 → finale 38; endless ramps +1 m/s per 400m, capped 40). Player never controls throttle.
- **Vertical:** gravity **g = 23 m/s²** downward. PULSE sets `vy = +7.8 m/s` (replaces current vy). Terminal fall −15 m/s. Corridor height 32m, soft ceiling/floor of dense cloud/mist that kills like terrain (with 0.3s of warning turbulence + rising alarm before contact — a fair boundary, not a surprise).
- **Attitude is cosmetic:** nose pitches to vy (+18° at peak rise, −35° at terminal dive), ship banks with rail curves and drift. Physics collides a single capsule; the pretty ship never changes the hitbox (generous: capsule is 85% of visual ship).
- **Feel targets (tune, §15.1):** one pulse from level flight lifts ~1.3 ship-heights; pulse cadence to hold altitude ≈ 2.2 taps/sec; a full corridor climb takes ~7 taps.
- Continuous swept collision (speeds cross multiple ship-lengths per tick at finale pace). Deaths resolve on the tick, deterministically.

## 5. Obstacle & Gate Vocabulary

All silhouette-first: shapes readable against the sky at maximum distance fog allows (minimum 2.2s of reaction time at current speed — enforced by an automated spawn-distance check in `make check`).

| # | Obstacle | Behavior |
|---|----------|----------|
| 1 | **Arch gate** | The pipe reborn: stone arch with a gap; +1 gate score. Gap height scales with difficulty (4.5 → 2.6 ship-heights) |
| 2 | **Center ring** | Golden ring in some gaps; threading its center = PERFECT (+combo). Risk/reward per gate |
| 3 | **Shard wall** | Full-corridor wall with 1–2 offset gaps; the altitude decision-maker |
| 4 | **Rolling ring** | Rotating gate whose gap orbits; timing, not twitch (period ≥ 2.4s) |
| 5 | **Destructible barricade** | Cracked slab blocking the easy line; FIRE opens it (3–8 hits). A pure dodger always has a harder open line — shooting buys comfort, never survival |
| 6 | **Turbine column** | Vertical wind: updraft/downdraft zones (±40% effective gravity) marked by streaming particles. Flappy with weather |
| 7 | **Canyon squeeze** | Rail dives into a ruin trench: ceiling AND floor close in; pulse discipline test |
| 8 | **Gate pair (scissors)** | Two walls sliding vertically in opposition; thread the crossover moment |
| 9 | **Cloud bank** | 0.6s of soft whiteout with silhouettes still faintly visible; tension, always followed by ≥1.5s of open air |
| 10 | **Secret ring** | Off-center shimmer ring reachable only by DRIFT or roll-momentum; feeds route branches (§8) |

Composition rules: one new obstacle type per campaign stage; never two novel types in the same breath; endless unlocks types by distance in the same order the campaign taught them.

## 6. Combat Layer (optional by design, delicious by intent)

- Lasers: hitscan-feel projectiles (80 m/s) along flight vector with a 6° assist cone; impacts pop with light. No ammo, no heat — hold forever.
- Enemies never block survival on the main line; they gate SCORE and ROUTES:
  - **Drone strings:** 3–5 popcorn drones flying sine arcs through gaps — each kill +combo; killing a full string = string bonus.
  - **Turret pylons:** mounted on arches, telegraphed 3-shot bursts (1.0s windup glow). Roll deflects; deflected shots destroy the turret (the satisfying counter — deflect-to-kill).
  - **Mine chains:** slow drifting lines; shootable, dodgeable, worth more shot.
  - **The Rival:** a black-glass wing that appears on medal-pace runs, mirroring your altitude on a 1.2s delay and dropping mines in your wake — literally your own flying, weaponized against you. Outfly your past self.
- Combo: gates, perfects, kills, and deflects feed one multiplier (×1–×8); any hit or missed gate resets it. Combo drives medal scores and the music's intensity layer.

## 7. Bosses (campaign stages 3, 6, 9)

Boss = a flying fortress that IS the level: pattern walls you thread while shooting exposed cores between waves.
- **S3 — The Gatekeeper:** a colossal moving arch-fortress cycling scissor walls; cores open after each threaded pattern.
- **S6 — The Storm Shepherd:** a leviathan whose body segments surface and dive through the corridor like living pipes; turbine breath shifts your gravity mid-pattern.
- **S9 — The Mirror Armada:** the Rival returns with escort drones and a finale that quotes every obstacle type at speed; its last phase is a pure one-button gauntlet — the game's thesis, played loud.
Bosses are beatable without firing on the main route (longer pattern endurance ends them) but firing is faster and worth a medal tier.

## 8. Campaign Structure — the Route Map

Nine stages, three altitude tiers (Low Road / Ridge / High Road), Star Fox-style: every run starts at Stage 1 and ends at Stage 9, ~5 stages per run, branches chosen by PLAY:
- Finish a stage with medal-pace combo, or thread its hidden secret rings, and the exit gate lifts you a tier; play safe and the low road carries you on.
- Higher tiers: tighter gaps, richer enemies, better scores, different skies — difficulty as geography the player earns, not a menu.
- **Medals** per stage (Bronze/Silver/Gold) from score + no-hit + perfects; Golds unlock cosmetic ship trails/palettes and EXPERT rules (one-hit campaign). Medal chase is the long game; the route map screen shows conquered paths — the replay engine in one picture.

## 9. Death & The Restart Law

Impact → 0.25s crash bloom + score tick-up → input live → tap → airborne at the stage start (campaign) or run start (endless). **Impact-to-airborne < 1.0s, measured in CI** (an automated input-replay test asserts the frame count). No death quotes, no menus in the loop, no "continue?", nothing between the player and the next attempt. Best-distance marker ghosts silently at your record point. In endless, the seed persists for the session (mastery of today's world), daily seed locked to the date.

## 10. Assists & Rulesets (house pattern: doors, not shame)

- **Campaign default:** 3 hull pips (terrain still one-hit-kills; pips absorb projectiles/mines only — altitude sins are always fatal, keeping the Flappy covenant).
- **Assist dials (per profile, badge shown on scores):** gate width ×1.15, gravity ×0.85 ("featherwind"), turret rate −25%, practice-any-reached-stage.
- **EXPERT:** one-hit everything, unlocked by any 5 Golds; its own medal shade.
- **PURE:** described §2; assists off; the leaderboard of monks.
- Photosensitivity caps + reduce-motion (kills FOV kicks, shake, whiteout — keeps silhouettes and color language) honored by every effect.

## 11. Visual Direction — "Sunfall Archipelago"

Golden-hour forever: an ocean of cloud beneath, chains of floating islands and ancient arch-ruins ahead, a low sun that never sets. Low-poly flat-shaded geometry under a gradient sky with god-ray shafts (shader), long soft fog that makes every obstacle a clean dark silhouette first and a detailed ruin second — beauty in service of Pillar 4.
- **The ship:** an origami-folded glider of pale ceramic and light, twin ribbon contrails that pulse bright on every tap (your heartbeat, visible), gold flash on deflects.
- **Route skies:** Low Road = amber haze, Ridge = clear gold, High Road = violet stratosphere with aurora threads; finale = storm-dark with the sun as a blade on the horizon.
- **Speed language:** FOV eases 70→82 with pace, wind streaks past the camera, cloud-punches whump. Perfects ring-flash; combo ×8 sets the contrails on fire.
- Readability law + contrast probe in `make check`, same as house standard.

## 12. Audio (all synthesized)

Engine = filtered drone whose pitch rides vy (rising sigh on pulse, deepening dive on fall — the game is playable by ear); tap = soft wing-thrum; gates chime up a pentatonic ladder with combo (reset drops the ladder — you HEAR your streak); deflect = bright metallic ping; death = short bloom, never a sad sting (respect the restart tempo). Music: generative layers per route tier, intensity follows combo; boss themes assemble from stage motifs. PURE mode: wind and chimes only.

## 13. Module Map
```
v8/
  Makefile  README.md
  src/
    main.c app.c        state machine, app_frame(), modes
    sim.c/.h            §4 exactly; pure; headless-testable
    rail.c/.h           spline, segments, speed ownership, camera targets
    spawn.c/.h          obstacle/enemy placement: authored tables + seeded endless gen, reaction-time check
    combat.c/.h         lasers, drones, turrets, mines, rival, combo
    boss.c/.h           three bosses as pattern scripts
    routes.c/.h         campaign map, tiers, medals, unlocks
    fx.c/.h + shaders/  sky, god rays, contrails, fog, blooms (compiled-in GLSL)
    synth.c/.h          SFX + generative layers
    ghost.c/.h          input recording, ghosts, replays, CI restart-timer test
    ui.c/.h             HUD, route map, leaderboards, settings
    save.c/.h           medals, bests, dailies, settings
```
`make test`: determinism (bit-identical replays), swept-collision suite at 40 m/s, reaction-time spawn audit, restart-law frame assert. `make check` adds contrast probe + photosensitivity caps. All merge-blocking.

## 14. Optional Parallel Lanes (if FOREMAN fans out)
Gray-box gate first (house law): `sim` + `rail` + `spawn` with cubes and capsule must already be compulsive — the tap-death-tap loop proven fun before any gold light exists. Then disjoint lanes: **L1** fx/shaders + ship, **L2** combat + rival, **L3** bosses, **L4** routes/medals/ui, **L5** synth, **L6** ghost/leaderboards. Frozen sim API, sequential merges, `make check` green each.

## 15. Tuning Protocol (ordered; feel-tested on humans)
1. **The pulse.** Gray-box, empty corridor, then lone arches. The g / V_pulse / forward-speed triangle is THE game; tune until holding altitude feels like drumming and diving feels like falling in a dream. Nothing proceeds until a tester voluntarily says "one more."
2. **Restart law.** Measure it, hit it, lock it.
3. **Gate fairness.** 20-gate gauntlet at three speeds: every death must self-report as "late tap." Tune fog distance, silhouette contrast, gap floor (never below 2.6 ship-heights on any main route).
4. **Fire layer.** Barricades and drone strings: shooting must feel like relief and greed at once — never homework. Tune assist cone, hit counts.
5. **Roll & turrets.** Deflect window generous (whole 0.45s), deflect-to-kill lands as the coolest thing a new player discovers by accident in session one.
6. **Route thresholds.** Medal-pace and secret-ring branch triggers: a decent player finds ONE branch naturally per run; the map screen makes them hunt the rest.
7. **Endless ramp.** Daily seed to 1000m survivable by a practiced GLIDE— *pulse-only* player; speed cap honest with the reaction-time audit.
8. **Beauty pass last**, against the probes, on top of proven gray fun.

## 16. Acceptance Criteria (Definition of Done)
1. `make check` green (determinism, tunneling, reaction-time audit, restart-law timer, contrast + photosensitivity probes); desktop + web builds; two-button-total play verified end to end.
2. **First-flight test:** three first-timers each pass 3 gates within their first minute, unprompted, and restart without being told how.
3. **The covenant test:** full campaign main route completed by a tester using PULSE only — no fire, no roll, no drift.
4. Campaign (9 stages, 3 tiers, 3 bosses, medals), Endless (daily + free seed + ghost), and PURE all complete; route map renders conquered paths.
5. Rival appears on medal-pace runs and demonstrably mirrors the player's recorded altitude.
6. Gray-box retained behind a debug flag.
7. All content original — zero references to the two inspirations or any other IP.

## 17. Out of Scope
Online leaderboards/multiplayer; free-flight/all-range segments; ship upgrades or unlockable gameplay advantages (cosmetics only); wingman chatter systems; mobile touch build (web build already tap-friendly if it happens to run there, but do not target it); level editor.

---
*One button, one sky, one second to try again. If the tap feels perfect, everything else is decoration.*
