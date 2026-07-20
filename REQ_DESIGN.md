# Songs From Bad Sectors (FloppyJam)
## Complete Game Requirements and Design Specification

**Document version:** 1.0  
**Status:** Build-ready design baseline  
**Date:** July 20, 2026  
**Primary target:** Windows x64 native executable  
**Implementation:** C99 + statically linked raylib 6.0, with a size-minimized custom build  
**Contest target:** 2P Game Arcade 1.44MB Game Development Contest 2026

---

## 0. Purpose of This Document

This document specifies the complete intended game, not merely a prototype or minimum viable product. It is designed to be handed directly to an AI coding agent or human developer and implemented in ordered passes until the complete release is produced.

The implementation passes are sequencing instructions, not optional scope tiers. Every requirement marked **Required** belongs in the final game unless it is explicitly moved to **Out of Scope** through a recorded design decision.

The central implementation rule is:

> Build the technical spine first, but always build toward the complete game defined here.

## Document Map

- Sections 1–7 define the product, final scope, core loop, and game modes.
- Sections 8–13 specify pacing, controls, arena behavior, Euclidean patterns, corruption, and recovery.
- Sections 14–18 specify deterministic generation, composition, synthesis, audio timing, and visual direction.
- Sections 19–25 specify UI, technical architecture, run coordination, persistence, performance, and build size.
- Sections 26–32 provide formal requirements, balance targets, testing, implementation passes, and final acceptance criteria.
- Sections 33–35 provide the risk register, AI-coder guardrails, and creative north star.

---

# 1. Executive Summary

**Songs From Bad Sectors** is a warm, visually striking, procedurally generated audiovisual arcade game that fits entirely within a 1.44MB floppy disk image capacity.

A six-character seed generates:

- A complete two-to-three-minute musical composition
- Its harmony, rhythm, instrumentation, and arrangement
- A matching playable disk-shaped arena
- Euclidean rhythm patterns represented as notes, glowing nodes, petals, and hazards
- A deterministic sequence of visual forms, palettes, and effects
- The timing and pressure profile of a pursuing corruption echo

The player controls a glowing read head that glides across a circular disk. Musical pulses appear as recoverable sectors arranged around rings. The player routes through those sectors while a delayed corruption echo retraces prior movement on the beat, gradually turning the player’s own path into danger.

The game is relaxing in tone but active in decision-making. It does not require precision rhythm-game performance. Every action works at any time, while actions performed near a beat become stronger, smoother, and more visually expressive.

The player’s goal is to recover as much of the generated song as possible before the final read completes. Recovery progressively restores musical layers and transforms the world from a dim, sparse signal into a rich geometric bloom.

The final product must be:

- A standalone native executable
- Fully playable without a browser or network connection
- Free of external runtime dependencies
- At or below **1,474,560 bytes** after extraction, including executable, runtime, notices, and any shipped files
- Immediately understandable and enjoyable within the first thirty seconds
- Completeable in approximately two to three minutes per run
- Deterministic by seed
- Visually impressive despite containing no shipped textures, music recordings, models, or conventional asset pack

---

# 2. Verified Contest Constraints

The official contest page establishes the following requirements:

1. The total extracted package must not exceed **1,474,560 bytes**.
2. The game must be a newly created original work made after the contest announcement.
3. Any engine is allowed, but the executable and runtime count toward the size limit.
4. The game must be playable as a standalone executable; browser games are prohibited.
5. Solo and team submissions are allowed.
6. Judging focuses on whether the game is complete, whether it respects the capacity limit, and whether it is fun.
7. The official schedule lists the submission deadline as **September 4, 2026 at 23:39**. A separate rule line on the same page says 11:59 PM, creating an internal inconsistency. The project must treat 23:39 KST as the latest possible deadline but target submission at least 24 hours earlier.

Sources:

- Official contest page: https://2pgarcade.com/contest-144mb.html
- raylib repository and license: https://github.com/raysan5/raylib

## 2.1 Compliance Policy

The release pipeline must fail if the extracted submission package exceeds 1,474,560 bytes by even one byte.

The project should target a release package of **1,350,000 bytes or less**, leaving at least 124,560 bytes of safety margin for packaging variance, notices, and late fixes.

The shipped package should ideally contain only:

```text
SongsFromBadSectors.exe
README.txt
```

No DLL, browser, installer, asset directory, configuration file, shader file, audio file, or font file may be required.

All fixed data should be compiled into the executable.

---

# 3. Product Vision

## 3.1 One-Sentence Pitch

> Every six-character seed creates a song and its playable recovery pattern: glide through glowing sectors, pulse with the beat, and stay ahead of corruption advancing through your own path.

## 3.2 Player Fantasy

The player is not composing music manually and is not fighting a hostile army. The player is recovering a lost piece of music from a damaged disk.

At the beginning of a run, only a faint signal remains. As the player restores sectors:

- Rhythm returns
- Harmony becomes clearer
- The arena gains color
- Geometry becomes more elaborate
- Trails persist longer
- The complete song emerges

A successful run should feel like rescuing something beautiful.

## 3.3 Emotional Tone

The game should feel:

- Warm
- Curious
- Dreamlike
- Melancholic but hopeful
- Smooth rather than frantic
- Technically magical
- Relaxing without becoming passive

The game must not feel:

- Harsh
- Noisy
- Punishing
- Militaristic
- Like a precision rhythm test
- Like an idle visualizer
- Like a generic neon twin-stick shooter

## 3.4 Design Pillars

### Pillar 1: One rhythm, many expressions

The same Euclidean rhythm must be legibly expressed as sound, spatial nodes, flower petals, timing pulses, and gameplay opportunities.

### Pillar 2: Warm arcade flow

Movement should feel smooth and elegant, but the player must continually make routing decisions under mild pressure.

### Pillar 3: Music is gameplay state

Musical layers are tied to visible recovery progress. The player should understand why an instrument entered, disappeared, or returned.

### Pillar 4: Procedural, but curated

The seed selects from carefully constrained musical and visual grammars. Randomness must never be used as an excuse for ugly output, bad harmony, impossible routing, or incoherent visuals.

### Pillar 5: Immediate readability

A first-time player should understand the basic loop without reading a manual:

- Bright sectors are good
- Dark echo is dangerous
- The world pulses with the music
- Recovering sectors restores the song

### Pillar 6: The size limit is the identity

The procedural music and graphics are not merely compression tricks. The concept must feel inseparable from a floppy disk containing “unlimited songs.”

---

# 4. Target Audience and Play Context

## 4.1 Primary Audience

- Game-jam and arcade-culture judges
- Players who enjoy short replayable score runs
- Players who enjoy generative music and mathematical visuals
- Players who like rhythm-informed play but dislike strict note judgment
- Developers and technical players attracted to procedural systems

## 4.2 Expected Session

- First session: 3–8 minutes
- Individual run: approximately 2:10–2:50 depending on BPM
- Restart time: under 2 seconds
- Full understanding of controls: under 30 seconds
- Seed comparison or replay: immediate

## 4.3 Accessibility Intent

The game should be playable without musical training. Beat alignment is rewarded but never required to move, repair, or finish.

The game must support keyboard and common gamepads.

The game should avoid relying solely on color to distinguish recoverable sectors from corruption. Shape, brightness, motion, and line treatment must also differ.

---

# 5. Final Scope

## 5.1 Included in the Final Game

- Complete title and boot presentation
- Six-character deterministic seed generation and entry
- Random seed generation
- Three gameplay pressure presets using the same core systems:
  - Bloom
  - Flow
  - Pulse
- One complete generated song per seed
- A two-to-three-minute generated run
- Four principal musical stems:
  - Pad
  - Percussion
  - Bass
  - Pluck/lead
- Runtime audio synthesis with no shipped recordings
- Shared sample-accurate transport
- Euclidean rhythm generation
- Circular disk arena with multiple rings
- Smooth inertial movement
- One context-independent Pulse action
- Beat-strengthened Pulse behavior
- Recoverable musical sectors
- One corruption system: delayed path echo
- Layer addition and temporary subtraction
- Recovery percentage
- Integrity system where applicable by mode
- Final audiovisual bloom or graceful failure resolution
- Results screen with seed, mode, recovery, and replay controls
- Pause and restart
- Local settings saved where possible
- A size-verification and release-packaging pipeline

## 5.2 Explicitly Out of Scope

The following must not be added unless the entire required game is complete, polished, under budget, and a deliberate scope change is approved:

- Online services
- Browser build as the contest submission
- Online leaderboards
- Multiplayer or two-player mode
- Campaign progression
- Narrative cutscenes
- Character upgrades
- Inventory systems
- Weapons or shooting
- Boss fights
- Multiple enemy archetypes
- Branching musical arrangements selected during play
- User-authored music
- Level editor
- Mod support
- Downloaded or streamed content
- Conventional texture or audio asset packs
- Precision note lanes, “Perfect/Great/Miss” judgments, or forced rhythm inputs
- Multiple scoring categories
- Procedural systems whose output is not visible or useful to the player

---

# 6. Core Game Loop

## 6.1 High-Level Loop

1. Player starts the game.
2. Player accepts a random seed or enters a six-character seed.
3. Player selects Bloom, Flow, or Pulse.
4. The seed generates the song genome, arrangement, patterns, arena, palette, and run script.
5. The run begins with a faint pad and minimal geometry.
6. The player glides through recoverable sectors.
7. The player uses Pulse to accelerate, repair, and pass safely through dangerous situations.
8. Euclidean events trigger notes, visual pulses, node blooms, and corruption movement.
9. Recovery adds music and visual complexity.
10. Corruption retraces the player’s past path, forcing route changes.
11. The final read occurs after the fixed musical arrangement ends.
12. The game displays recovery percentage and the seed.
13. The player can replay the same seed, change mode, or generate a new seed.

## 6.2 Moment-to-Moment Verb

The core player verb is:

> Read the emerging rhythmic pattern, choose a route, and stay ahead of the echo.

The game must never collapse into only “move toward the next glowing object.” The corruption trail, ring layout, pulse timing, and limited route space must create repeated small decisions.

## 6.3 Success

A run always reaches a musical endpoint unless the player loses all integrity in Flow or Pulse mode.

Success is measured by **Recovery Percentage**:

```text
recovered weighted sector value / total recoverable weighted sector value
```

The result should be shown as an integer percentage from 0–100%.

Suggested result language:

- 0–39%: SIGNAL FRAGMENT
- 40–69%: PARTIAL RECOVERY
- 70–89%: TRACK RESTORED
- 90–99%: NEAR-PERFECT READ
- 100%: NO BAD SECTORS

## 6.4 Failure

In Flow and Pulse mode, the player has finite integrity. Losing all integrity ends the interactive portion early.

Failure must still resolve musically:

- Lead disappears first
- Bass fades next
- Percussion thins
- Master low-pass closes
- The world dims
- The original pad remains for a brief final cadence

The results screen still shows recovered percentage. Failure should feel regretful, not punitive.

Bloom mode has no early game-over state.

---

# 7. Game Modes

All modes use the same seed, music, arena, and node patterns. Only pressure and forgiveness parameters change.

## 7.1 Bloom

Purpose: relaxed audiovisual exploration.

- No early game over
- Corruption echo delay: 10 beats
- Corruption ribbon width: 70% of Flow
- Contact removes a small amount of recovery but not integrity
- Pulse baseline strength: 110% of Flow
- Beat bonus window: widest
- Intended for players who want the complete song and visual bloom

## 7.2 Flow — Default

Purpose: intended core experience.

- Integrity: 3
- Corruption echo delay begins at 8 beats and reduces to 6 beats in later phases
- Standard ribbon width
- Contact removes one integrity, corrupts one recent sector when available, and triggers temporary musical subtraction
- Pulse has meaningful route and timing value
- Intended to feel relaxed in presentation but active in play

## 7.3 Pulse

Purpose: replay challenge.

- Integrity: 2
- Corruption echo delay begins at 6 beats and reduces to 4 beats
- Corruption ribbon width: 115% of Flow
- Denser late-run patterns
- Slightly faster player maximum speed and stronger beat bonus
- Contact penalty is unchanged rather than made harsher; pressure comes from routing
- Results clearly display PULSE mode so scores are not confused with Flow

---

# 8. Run Structure and Pacing

## 8.1 Musical Length

A complete run is **56 bars** in 4/4 time.

At the allowed BPM range of 84–112:

- 84 BPM: approximately 160 seconds
- 96 BPM: approximately 140 seconds
- 112 BPM: approximately 120 seconds

This produces a two-to-three-minute experience without requiring a fixed wall-clock duration.

## 8.2 Arrangement Phases

| Phase | Bars | Primary Purpose | Music | Gameplay |
|---|---:|---|---|---|
| Boot | 4 | Establish signal | Pad only | Spawn player, show first pulse, no damage |
| Signal | 8 | Teach recovery | Pad + sparse pluck hints | Simple patterns, no active echo for first phrase |
| Pulse | 12 | Introduce pressure | Percussion enters | Corruption echo appears, generous delay |
| Flow | 12 | Main routing challenge | Bass enters | Multiple ring choices, stronger echo presence |
| Bloom | 12 | Maximum expression | Lead becomes fully active | Richest visuals, denser patterns |
| Final Read | 8 | Resolution | Full arrangement or damaged variant | Final combined pattern and closing bloom |

Total: 56 bars.

## 8.3 Phrase Length

A standard gameplay phrase lasts 4 bars.

Each phrase defines:

- One Euclidean pattern
- One node ring or connected pair of rings
- One voice emphasis
- One corruption behavior parameter set
- One color emphasis
- One completion opportunity

Four-bar phrases provide enough time to read and route through a pattern without making each pattern overstay its welcome.

## 8.4 First Thirty Seconds

By the end of approximately thirty seconds, the player must have experienced:

- Movement
- A visible and audible Euclidean pulse
- Collecting at least three sectors
- Pulse activation
- The first instrument layer addition
- The first telegraphed corruption movement

No text tutorial should be required beyond a brief control hint.

---

# 9. Player Controls and Movement

## 9.1 Keyboard

- Move: WASD or Arrow Keys
- Pulse: Space, Z, or Enter
- Pause: Escape or P
- Restart from pause/results: R
- Confirm menu: Enter or Space
- Back: Escape

## 9.2 Gamepad

- Move: Left analog stick or D-pad
- Pulse: South face button or right trigger
- Pause: Start/Menu
- Confirm: South face button
- Back: East face button

## 9.3 Movement Model

The player moves continuously in two dimensions with acceleration and drag.

Recommended logical values, subject to tuning:

```text
acceleration            900 units/s²
max_speed               240 units/s
linear_drag              3.2 /s
pulse_impulse           150 units/s
beat_bonus_impulse       up to +90 units/s
boundary_soft_radius     92% of arena radius
boundary_force          increasing smooth repulsion
```

Movement requirements:

- Direction changes must feel responsive but not twitchy.
- The player must retain enough momentum to create flowing curves.
- Reversing direction must be possible without several seconds of helpless drift.
- The player must never become permanently trapped against the boundary.
- Movement behavior must be framerate-independent.

## 9.4 Pulse Action

Pulse is a single multifunction action.

At all times, Pulse:

- Applies an impulse in the current input direction, or current velocity direction if no input is present
- Expands a short repair radius around the player
- Briefly intensifies player glow and trail
- Provides a very short corruption contact grace period

Near a musical beat, Pulse becomes stronger rather than merely “correct.”

Pulse must never fail because of timing.

## 9.5 Pulse Cooldown

Pulse uses a short cooldown, recommended at 0.55–0.75 seconds depending on mode.

A cooldown arc or glow recharge around the player should communicate availability without a conventional HUD bar.

## 9.6 Beat Resonance Function

Beat strength should be continuous, not tiered.

Recommended conceptual function:

```text
resonance = exp(-0.5 * (distance_to_nearest_beat / sigma)^2)
```

Where:

- `distance_to_nearest_beat` is measured in seconds or samples
- `sigma` is approximately 100–140 ms depending on mode
- Pulse output interpolates between baseline and maximum using resonance

This avoids visible judgment labels while still making beat-aware play feel better.

---

# 10. Arena and Spatial Design

## 10.1 Arena Form

The playfield is a circular disk viewed from above.

It contains:

- A central hub
- Three principal concentric recovery rings
- Soft radial lanes or visual spokes
- Generated node positions around ring steps
- A read sweep rotating around the center
- A bounded outer edge

The player is not locked to a ring. Rings are spatial guides and pattern anchors, not movement rails.

## 10.2 Logical Coordinate System

Recommended internal world:

```text
arena center: (0, 0)
arena radius: 300 logical units
ring radii: 105, 190, 270 units
```

Values may scale to the selected internal rendering resolution.

## 10.3 Read Sweep

A soft radial read line rotates around the disk in musical time.

Requirements:

- One full rotation per bar by default
- Angular position derived from the audio transport, not accumulated per render frame
- Triggers visual emphasis when crossing each step
- Makes the relation between angular node positions and musical sequence legible
- Remains visually subtle enough not to become a clock-hand obstacle

## 10.4 Sector Nodes

Each phrase generates `n` possible step positions and `k` active positions.

Allowed initial step counts:

```text
8, 12, 16
```

Recommended pulse ranges:

```text
8 steps:  3–6 active
12 steps: 4–8 active
16 steps: 5–10 active
```

Each possible position is represented by a faint anchor. Active Euclidean steps become recoverable glowing nodes or petals.

## 10.5 Node States

```text
DORMANT
TELEGRAPHING
AVAILABLE
RECOVERED
CORRUPTED
EXPIRED
```

### Dormant

Visible only as a faint geometric anchor.

### Telegraphing

Brightens before its corresponding musical event.

### Available

Can be collected or repaired by player contact or Pulse radius.

### Recovered

Changes color, contributes to Recovery Percentage, and remains as stable geometry.

### Corrupted

Darkened, asymmetric, or fragmented. May be recovered again if phrase timing permits.

### Expired

No longer affects score after phrase transition but may remain as decorative history.

## 10.6 Collection Rules

A node becomes available at its first event within the phrase and remains available until:

- Recovered
- Corrupted after recovery
- Phrase expires

The player can collect a node without exact timing. Collecting close to its next pulse adds a stronger audiovisual response but does not change base recovery value.

## 10.7 Spatial Safety Rules

The generator must guarantee:

- No required node spawns outside the arena.
- No node spawns inside the central visual dead zone.
- At least one safe route exists between successive primary targets under Flow parameters.
- The first two phrases cannot produce unavoidable corruption contact.
- No two active nodes overlap closer than the player collision diameter plus margin.
- Arena patterns remain legible at the internal resolution.

---

# 11. Euclidean Rhythm System

## 11.1 Role

Euclidean rhythm is the primary cross-domain procedural primitive.

A generated pattern `E(k, n)` must drive:

- Percussion or note triggers
- Active sector positions
- Petal count and placement
- Visual pulse timing
- Read-sweep highlights
- Phrase route options
- Selected corruption advance events where appropriate

This is the correspondence the player should actually perceive.

## 11.2 Pattern Generation

A compact deterministic implementation may use a bucket or floor-difference method rather than a large general algorithm.

Conceptual formulation:

```text
pulse[i] = floor((i + 1) * k / n) != floor(i * k / n)
```

Then rotate the bitset by a seed-derived offset.

The exact implementation may use bit masks for `n <= 16`.

## 11.3 Pattern Constraints

- Avoid `k = 0` and `k = n` in gameplay phrases.
- Avoid repeating the identical `(k, n, rotation)` for adjacent phrases.
- Do not increase density by more than approximately 35% between adjacent phrases unless entering Final Read.
- Maintain at least one rest gap in standard patterns.
- Pattern rotation must be selected so the first active event is not always at step zero.

## 11.4 Musical Mapping

Different stems may interpret the same bitset differently:

- Percussion: direct trigger or accented trigger
- Bass: trigger on selected subset of pulses, sustain through rests
- Lead: map pulse index to scale degree
- Pad: change voicing at phrase boundaries, not every pulse

The shared pattern should remain recognizable without forcing every instrument to play every pulse.

## 11.5 Visual Mapping

- `n` defines anchor positions around the ring.
- `k` defines active petals/nodes.
- Rotation defines angular phase.
- Density influences brightness and trail activity.
- Recovered pulses complete the visible flower.

---

# 12. Corruption System: Bad Sector Echo

## 12.1 Design Goal

Corruption provides mild, rhythmic pursuit and route denial without introducing a conventional enemy roster.

The player’s own movement creates the future hazard.

## 12.2 Core Behavior

The game continuously records the player’s recent path into a fixed-size circular buffer indexed by musical time.

The corruption echo renders and occupies the player’s past path after a mode-dependent delay measured in beats.

Example:

```text
current player position: transport beat 32
Flow echo delay: 6 beats
corruption head position: recorded player position at beat 26
```

The echo therefore chases the player while turning previous routes into dangerous ribbons.

## 12.3 Beat-Driven Motion

The echo head advances to the next recorded beat sample on quarter-note boundaries.

Visual interpolation may keep the ribbon smooth, but the threatening advance must visibly pulse on the beat.

The player should be able to anticipate:

> The shadow will occupy that part of my path on the next beat.

## 12.4 Path Sampling

Record player position at a fixed musical subdivision, recommended at 1/8 or 1/16 note resolution.

Use a fixed-size ring buffer large enough for the maximum Bloom delay plus ribbon persistence.

No allocation is permitted during play.

## 12.5 Ribbon Geometry

The corruption path is drawn as:

- A dark central line
- A broader translucent desaturated halo
- Broken or asymmetric edge fragments
- Beat-synchronized contraction and expansion

The ribbon must remain visible against every palette.

## 12.6 Collision

Collision uses distance to recent ribbon segments rather than exact rendered pixels.

On collision in Flow/Pulse:

1. Consume one integrity.
2. Apply 0.75–1.0 seconds of invulnerability.
3. Corrupt the most recently recovered eligible node, if one exists.
4. Immediately duck the lead and close the master low-pass slightly.
5. Schedule musical restoration on a later downbeat.
6. Push the player away from the ribbon.

On collision in Bloom:

- Do not consume integrity.
- Reduce recovery value modestly or corrupt one recent node.
- Apply the same softer audiovisual feedback.

## 12.7 Anti-Frustration Rules

- The corruption ribbon must never spawn directly on the player at the moment it first activates.
- Player invulnerability must prevent repeated damage from a single contact.
- The player must receive a visual warning before the echo becomes collidable.
- The echo delay may shorten only at phrase or phase boundaries.
- The game must not create multiple independent corruption behaviors in the final scope.

---

# 13. Integrity and Recovery

## 13.1 Integrity

Integrity is represented by three small arcs or lights around the central hub or player, not by a large health bar.

- Bloom: infinite/no early failure
- Flow: 3 integrity
- Pulse: 2 integrity

Integrity does not regenerate during a standard run.

## 13.2 Recovery Value

Nodes may have weighted value according to phase:

```text
Boot/Signal node: 1
Pulse node:       1
Flow node:        2
Bloom node:       2
Final node:       3
```

This prevents early tutorial sectors from dominating the result and makes the finale meaningful.

## 13.3 Recovery Percentage

Recovery percentage should update smoothly but be calculated exactly from integer recovered and total weights.

A recovered node that is later corrupted loses its contribution unless restored again before expiry.

## 13.4 No Combo Score

The final game has no visible combo, multiplier, accuracy grade, or multi-part scoring system.

Player mastery is expressed through higher Recovery Percentage, cleaner movement, and a richer final audiovisual state.

---

# 14. Seed and Procedural Genome

## 14.1 Seed Format

The player-facing seed is six characters from a 32-symbol alphabet that excludes ambiguous characters.

Recommended alphabet:

```text
23456789ABCDEFGHJKMNPQRSTUVWXYZ
```

Exclude:

```text
0 O 1 I L
```

Six base-32 characters provide approximately 30 bits of player-facing variation.

## 14.2 Input Behavior

- Lowercase input is normalized to uppercase.
- Invalid characters are ignored or mapped only if unambiguous.
- Short entries may be deterministically padded using a hash.
- Random seed generation must use a changing runtime entropy source, but generated stage content must depend only on the resulting displayed seed.
- The last played seed should be retained for the current session and optionally persisted.

## 14.3 Seed Expansion

Hash the six-character seed into a 64-bit master value, then derive independent streams.

Required independent streams:

```text
music RNG
layout RNG
gameplay RNG
visual RNG
```

Changing particle count must never alter music or node layout.

## 14.4 Song Genome

Recommended structure:

```c
typedef struct SongGenome {
    uint64_t master_seed;
    int bpm;
    int root_note;
    int mode_id;
    int progression_id;
    int palette_id;
    int primary_steps;
    int density_profile;
    int rotation_profile;
    float swing;
    float warmth;
    float visual_symmetry;
} SongGenome;
```

## 14.5 Determinism Requirement

The same seed and mode must produce:

- The same BPM
- The same progression
- The same note and drum events
- The same phrases and node locations
- The same palette
- The same initial corruption timing

Player-dependent corruption path naturally differs based on input.

---

# 15. Music Generation

## 15.1 Music Direction

The music should sound like a small warm instrumental found on obsolete media, not stereotypical harsh chiptune.

Primary qualities:

- Soft
- Rounded
- Melodic
- Repetitive in a pleasant way
- Harmonically safe
- Slightly nostalgic
- Clear enough for the rhythm-to-node relationship to be perceived

## 15.2 BPM

Select integer BPM from a curated set rather than an unrestricted range.

Recommended set:

```text
84, 88, 92, 96, 100, 104, 108, 112
```

Avoid extreme tempos.

## 15.3 Scales and Modes

Recommended set:

- Major pentatonic
- Minor pentatonic
- Ionian with restrained scale degrees
- Dorian
- Mixolydian

Avoid unconstrained chromatic generation.

The lead and bass must use scale-degree tables, not arbitrary semitone selection.

## 15.4 Chord Families

Select from a small curated table such as:

- I–V–vi–IV
- I–iii–IV–iv
- I–vi–ii–V
- I–IV–I–V
- i–VI–III–VII
- I–IV–vi–V
- i–IV modal vamp
- I–IV modal vamp

Every progression must have hand-authored inversion and register rules.

## 15.5 Voicing Rules

- Keep pad voices within a compact mid register.
- Move individual voices by the smallest practical interval between chords.
- Avoid more than one low-register third below the bass safety threshold.
- Do not allow lead and bass to repeatedly double in the same octave.
- Favor added sixths, ninths, and suspensions only from curated templates.
- Clamp all MIDI-equivalent notes to purpose-built instrument ranges.

## 15.6 Arrangement

All stems share one transport from sample zero.

Layer gains are controlled by the run phase and recovery state.

Default phase mapping:

```text
Boot:       pad
Signal:     pad + sparse lead hints
Pulse:      pad + percussion + lead hints
Flow:       pad + percussion + bass
Bloom:      pad + percussion + bass + lead
Final Read: all available layers, modified by damage state
```

## 15.7 Layer Changes

- Planned layer entrances occur on the next bar downbeat.
- Damage feedback may duck immediately using a click-safe ramp.
- Full layer restoration must be quantized to a downbeat.
- Mid-bar instrument startup is prohibited unless it is an intentional one-shot.

## 15.8 Melody Generation

Lead generation should use:

- Euclidean trigger positions
- Chord-tone preference on strong beats
- Scale passing tones on weaker events
- Short motif repetition
- One controlled variation every 4 or 8 bars
- Limited register movement
- Rest probability that decreases during Bloom

A motif should be 3–6 notes and transformed by octave, final note, or rhythm rotation rather than regenerated completely each phrase.

## 15.9 Bass Generation

Bass should:

- Prefer chord roots and fifths
- Use a subset of the Euclidean pulse pattern
- Sustain through rests
- Avoid rapid octave jumps
- Become rhythmically more active in Flow and Bloom

## 15.10 Percussion Generation

Required synthesized parts:

- Soft kick
- Wooden click or rim
- Brushed/noise hat
- Optional tiny shaker texture

Percussion should use Euclidean patterns with complementary rotations rather than random hit placement.

## 15.11 Musical Taste Guards

Generation must reject or correct outputs that violate rules such as:

- Excessive repeated notes without rhythmic purpose
- Lead outside instrument range
- More than two consecutive large leaps
- Bass collision with pad low voices
- Percussion density exceeding configured maximum
- Unresolved dissonant interval held across a chord boundary
- Full arrangement peak exceeding safe headroom

The generator should be procedural within a designed grammar, not an unbounded composition algorithm.

---

# 16. Runtime Synthesizer

## 16.1 General Rule

Implement purpose-built voices. Do not implement a general modular synthesizer.

## 16.2 Pad Voice

Suggested design:

- Two triangle or band-limited saw-like oscillators
- Mild detuning
- Slow attack and release
- One-pole low-pass filter
- Optional subtle pulse-width or phase drift
- Maximum simultaneous pad notes defined by chord voicing size

## 16.3 Bass Voice

Suggested design:

- Sine fundamental
- Quiet triangle harmonic
- Fast but soft attack
- Rounded decay/sustain
- Gentle saturation

## 16.4 Pluck/Lead Voice

Suggested design:

- Triangle or softened square oscillator
- Fast-decaying amplitude envelope
- Faster-decaying filter envelope
- Optional quiet sine transient

## 16.5 Bell Accent

If code budget allows, the lead may use a bell variant:

- Two sine oscillators at a curated inharmonic ratio
- Fast attack
- Medium decay
- No long sustain

This is a voice mode, not a fifth required stem.

## 16.6 Percussion Voices

### Kick

- Sine oscillator with rapid pitch drop
- Short amplitude envelope
- Soft clipping

### Wooden Click

- Short filtered impulse or triangle burst
- High-mid frequency
- Very short decay

### Brush/Hat

- Deterministic noise
- High-pass or simple difference filter
- Short envelope

## 16.7 Master Processing

Required:

- DC blocker
- Soft limiter or saturator
- Master gain
- One-pole low-pass reserved for damage/near-failure

Recommended if stable and small:

- Two short stereo feedback delays for gentle ambience

Avoid a large general reverb implementation unless it is clearly necessary and stable.

## 16.8 Audio Safety

- No output sample may become NaN or infinity.
- Master peak should normally remain below approximately -1 dBFS equivalent.
- Limiting should be subtle during normal play.
- Noise generation must use a dedicated deterministic state.
- Oscillator phases must remain bounded or periodically wrapped.

---

# 17. Audio Engine Architecture

## 17.1 Critical Path

The audio engine is the technical spine of the game and must be implemented before final gameplay or visual polish.

## 17.2 Output

Recommended configuration:

```text
sample rate: 48,000 Hz
sample format: 32-bit float
channels: stereo
single AudioStream
```

Use raylib’s audio stream callback mechanism or an equivalent single callback path.

## 17.3 Transport

Maintain a global 64-bit sample clock owned by the audio callback.

Derive musical event timestamps from absolute transport ticks, not from frame time and not by repeatedly adding a rounded samples-per-step value.

Recommended PPQN:

```text
96 ticks per quarter note
```

Conceptual timestamp:

```text
event_sample = floor(tick * sample_rate * 60 / (bpm * PPQN))
```

Use sufficiently wide integer arithmetic or fixed-point math to avoid drift.

## 17.4 Callback Responsibilities

The callback may:

- Advance the sample clock
- Trigger pre-generated scheduled events
- Allocate fixed voice slots
- Render voices
- Mix stems
- Apply gain ramps and master processing
- Write interleaved output samples

The callback must not:

- Allocate or free memory
- Read files
- Log to console
- Call rendering functions
- Regenerate the song
- Lock a mutex that may block
- Access mutable game state without a safe handoff mechanism

## 17.5 Event Scheduling

Pre-generate the complete song event list before play.

Events should include:

```text
sample timestamp
stem ID
voice type
note/frequency
velocity
length or envelope parameters
flags
```

Sort events by timestamp.

The callback should render in blocks split at event boundaries rather than checking the entire event list for every sample.

## 17.6 Game-to-Audio Commands

Use a fixed-size lock-free or single-producer/single-consumer command queue.

Supported commands:

- Request layer mask change at next downbeat
- Immediate duck request
- Damage low-pass target
- Recovery low-pass release
- Start run
- Stop/fade run
- Pause/resume where supported

Game code requests changes. The audio thread applies them at musically correct times.

## 17.7 Stem Gain Model

Each stem has:

- Current gain
- Target gain
- Ramp increment
- Structural enabled state
- Temporary damage suppression state

All pattern clocks continue regardless of gain. Re-enabling a stem must preserve phase.

## 17.8 Audio-to-Visual Synchronization

The render thread reads a safely published transport snapshot containing:

```text
sample clock
bar index
beat index
step index
phase within beat
phase within bar
```

Visuals derive positions from this snapshot. They must not maintain an independent musical clock.

## 17.9 Pause Behavior

Recommended behavior:

- Pause freezes gameplay.
- Audio quickly fades to a low sustained ambience or pauses the stream cleanly.
- Resume occurs with a short fade and no transport discontinuity visible to gameplay.

Implementation may fully stop transport during pause, provided the audio and visual clocks remain aligned after resume.

---

# 18. Visual Direction

## 18.1 Visual Identity

The visual style is:

> Warm generative album art living inside a damaged disk utility.

The game should combine:

- Circular disk geometry
- Euclidean flowers
- Fine vector lines
- Glowing particles
- Persistent motion trails
- Controlled symmetry
- Dark erased sectors
- Sparse retro-computer typography

## 18.2 Visual Grammar

The final visual system should prioritize three major elements:

1. Euclidean sector flowers
2. Player and note trails
3. Corruption that erases color and symmetry

Other mathematical curves may decorate the scene but must not be presented as core gameplay structure unless the mapping is legible.

## 18.3 Internal Resolution

Recommended internal render resolution:

```text
640 × 360
```

Scale to the window using integer scaling where possible and letterbox otherwise.

The game should remain legible at 1280×720 and 1920×1080 output.

## 18.4 Palette System

Store 8–12 handcrafted palettes as small RGB tables.

Each palette should contain:

- Background dark
- Background secondary
- Stable/recovered color
- Active pulse color
- Player color
- Accent color
- Corruption dark
- Corruption edge

The seed selects a palette and minor bounded variations.

Do not generate completely random RGB colors.

## 18.5 Visual Progression

### Boot

- Near-black background
- Thin ring outlines
- Small player glow
- Sparse particles

### Signal

- First active petals
- Gentle pad-driven breathing
- Short trails

### Pulse

- Beat rings
- Percussion flashes
- Corruption becomes visible

### Flow

- Bass deforms larger geometry
- Trails become longer
- More ring interactions

### Bloom

- Full palette range
- Rich particle activity
- Complete flowers
- Controlled symmetry and geometric overlays

### Final Read

- Recovered elements align into a coherent final form
- Full song produces the most expressive state
- Damage or low recovery leaves visible missing petals and dark sectors

## 18.6 Glow

Implement glow using repeated primitive draws and additive blending:

- Large low-alpha halo
- Medium halo
- Bright core
- Expanding beat ring

Do not require an external shader.

An embedded shader may be added only if:

- It materially improves the result
- It works on target systems
- A no-shader fallback exists
- It does not threaten size or schedule

## 18.7 Trails

Use fixed-size arrays of recent positions for:

- Player trail
- Note fragment trails
- Corruption ribbon

Trail lifetime and thickness increase with recovery and arrangement phase.

## 18.8 Decorative Mathematics

Allowed decorative systems:

- Lissajous curves
- Rose curves
- Damped harmonograph trails
- Simple sine flow deformation

Use no more than two decorative systems simultaneously during ordinary play. The screen must remain readable.

## 18.9 Damage Visuals

Damage should cause:

- Reduced saturation
- Shortened player trail
- Broken flower symmetry
- Dark sector fragments
- Mild temporal jitter or flutter
- Master geometry contraction

Avoid full-screen flashing or harsh glitch noise.

## 18.10 Camera

The camera remains centered on the disk.

Permitted motion:

- Very small beat breathing
- Tiny damage impulse
- Slow seed-derived rotation offset

No aggressive screen shake.

---

# 19. User Interface and Flow

## 19.1 Application States

```text
BOOT
TITLE
SEED_SELECT
MODE_SELECT
PLAYING
PAUSED
RESULTS
CREDITS
EXIT
```

SEED_SELECT and MODE_SELECT may share one screen if readability is better.

## 19.2 Boot Sequence

Suggested presentation:

```text
A:\> READ TRACK01.DAT
SCANNING...
BAD SECTORS DETECTED
SIGNAL FOUND
```

Keep total boot sequence under 3 seconds and allow skipping after first launch.

## 19.3 Title Screen

Required elements:

- Title: SONGS FROM BAD SECTORS
- Current/random seed
- Play
- Mode
- Controls hint
- Credits/exit in a secondary position

## 19.4 Seed Entry

- Six large character slots
- Keyboard typing supported
- Gamepad character cycling supported
- Randomize button
- Last seed option where available
- Display selected seed during results

## 19.5 In-Game HUD

Keep minimal:

- Recovery percentage
- Integrity arcs, except Bloom
- Seed in small type
- Pause indication

The world itself should communicate phrase progress and Pulse readiness.

## 19.6 Results Screen

Required:

```text
TRACK RECOVERED / SIGNAL LOST
SEED: XXXXXX
MODE: FLOW
RECOVERY: 87%
```

Actions:

- Replay same seed
- Change mode
- New random seed
- Enter seed
- Return to title

Clipboard copy may be supported if it adds negligible complexity, but visible seed text is sufficient.

## 19.7 Tutorial

No separate tutorial level.

Use:

- First phrase without damage
- A brief movement icon/text
- A Pulse prompt when the first cluster appears
- A visible corruption warning before the echo activates

Prompts disappear after use and may be disabled after the first completed run.

## 19.8 Credits

Include concise credits and acknowledgments, including raylib license notice, in the executable and README.

---

# 20. Sound and Visual Feedback Matrix

| Event | Audio | Visual | Gameplay |
|---|---|---|---|
| Node telegraphs | Quiet preview tick or harmonic shimmer | Expanding faint ring | None |
| Node recovered | Note accent or consonant chime | Petal locks into place, bright trail | Recovery increases |
| Resonant Pulse | Stronger transient, subtle stereo width | Larger halo and longer trail | Stronger impulse/repair |
| Layer enters | Stem fades in on downbeat | New visual family appears | Phase progression |
| Echo advances | Soft low pulse | Dark ribbon contracts/steps | Route changes |
| Player hit | Lead duck, filter closes | Desaturation, broken trail | Integrity/recovery loss |
| Integrity lost | Low warm impact | One arc extinguishes | Reduced remaining safety |
| Final success | Full cadence | Complete disk bloom | Results |
| Final failure | Layers subtract | World collapses to sparse signal | Results |

---

# 21. Technical Architecture

## 21.1 Language and Library

- C99
- raylib 6.0 pinned to an exact commit or release tag
- Static linking
- No external runtime dependencies

raylib is appropriate because it is written in C99, supports static/self-contained builds, includes graphics/input/audio functionality, and provides audio streaming callbacks.

## 21.2 Proposed Module Layout

```text
src/
  main.c
  app.c / app.h
  config.h
  seed.c / seed.h
  rng.c / rng.h
  euclid.c / euclid.h
  genome.c / genome.h
  music_gen.c / music_gen.h
  transport.c / transport.h
  synth.c / synth.h
  audio_engine.c / audio_engine.h
  game.c / game.h
  player.c / player.h
  nodes.c / nodes.h
  corruption.c / corruption.h
  run_director.c / run_director.h
  render.c / render.h
  ui.c / ui.h
  input.c / input.h
  save.c / save.h
```

For final size, modules may be unity-built into fewer translation units while preserving logical separation in source.

## 21.3 Fixed Memory Policy

After initialization, avoid heap allocation.

Use fixed arrays for:

- Audio voices
- Scheduled events
- Nodes
- Trail samples
- Corruption path
- Particles
- UI state

All fixed capacities must have overflow guards.

## 21.4 Suggested Capacity Limits

```text
scheduled music events: 8192
simultaneous synth voices: 64
phrase nodes: 16
visible historical nodes: 128
player trail samples: 256
corruption trail samples: 512
particles: 1024–2048, based on performance
command queue entries: 64
```

These are starting limits and may be reduced after profiling.

## 21.5 Core Structures

```c
typedef struct Pattern {
    uint16_t bits;
    uint8_t steps;
    uint8_t pulses;
    uint8_t rotation;
} Pattern;

typedef struct Phrase {
    int start_bar;
    int duration_bars;
    Pattern pattern;
    uint8_t ring_id;
    uint8_t stem_focus;
    uint8_t palette_emphasis;
    float pressure;
} Phrase;

typedef struct TransportSnapshot {
    uint64_t sample_clock;
    uint32_t bar;
    uint8_t beat;
    uint8_t step;
    float beat_phase;
    float bar_phase;
} TransportSnapshot;

typedef struct Node {
    Vector2 position;
    uint8_t step_index;
    uint8_t state;
    uint8_t weight;
    float pulse_phase;
} Node;

typedef struct RunState {
    SongGenome genome;
    int mode;
    int phase;
    int phrase_index;
    int integrity;
    int recovered_weight;
    int total_weight;
    bool run_ended;
} RunState;
```

## 21.6 Update Order

Recommended frame order:

1. Poll input
2. Read audio transport snapshot
3. Process application state
4. Update player using frame delta
5. Process beat/bar crossings from transport
6. Update nodes and phrase director
7. Update corruption path and collisions
8. Queue audio state commands
9. Update particles/trails
10. Render world
11. Render UI

Do not drive beat events solely from `GetFrameTime()`.

---

# 22. Run Director

## 22.1 Responsibility

The run director coordinates:

- Current bar and phrase
- Phrase activation
- Node spawning
- Layer targets
- Visual phase
- Corruption delay and width
- Final read behavior

## 22.2 Precomputation

At run initialization, generate:

- Full phrase list
- Pattern list
- Ring choices
- Node angular positions
- Music events
- Palette progression
- Mode parameter curve

Runtime should select and activate precomputed data rather than perform heavy generation during play.

## 22.3 Phase Transitions

Phase transitions occur on bar boundaries.

Visual and layer changes should be ramped, never abruptly snapped except for intentional pulse accents.

## 22.4 Final Read

The Final Read combines recognizable elements from earlier patterns.

Possible method:

- Use the union or rotated interleave of two earlier Euclidean patterns
- Place nodes across two rings
- Play full musical motif
- Allow a final recovery window

The final pattern must be tested for route viability under each mode.

---

# 23. Save Data

## 23.1 Required Persistence

Persist only small quality-of-life settings:

- Last seed
- Selected mode
- Master volume
- Fullscreen/window preference
- Tutorial seen flag

## 23.2 Optional Best Results

A small fixed table of recent seeds and best recovery may be stored if stable.

Maximum suggested records: 32.

Save failure must never prevent the game from launching or playing.

## 23.3 File Location

Prefer the executable directory for simplicity only if writable. Otherwise use an OS-appropriate local application data path.

Given the size and contest context, it is acceptable to run without persistence when no writable location is available.

---

# 24. Performance Requirements

## 24.1 Target

- 60 FPS at 640×360 internal resolution
- Stable audio callback with no underruns
- Windows 10 and 11 x64
- Functional on modest integrated graphics

## 24.2 Frame Independence

Gameplay movement and collision must use delta time or fixed-step simulation.

Musical timing must use the audio sample clock.

## 24.3 Graceful Degradation

If performance is low:

- Reduce decorative particle count
- Reduce trail sample rendering density
- Reduce glow layers

Never reduce audio scheduling accuracy or core node readability.

---

# 25. Build and Size Strategy

## 25.1 Release Build Goals

- Standalone Windows x64 executable
- Static raylib
- Static or otherwise self-contained C runtime as practical
- No external DLL files in submission
- Stripped symbols
- Link-time optimization
- Garbage-collected unused sections

## 25.2 Suggested Compiler Flags

Starting point for GCC/MinGW:

```text
-std=c99
-Os
-flto
-ffunction-sections
-fdata-sections
-fno-asynchronous-unwind-tables
-fno-unwind-tables
-s
-Wl,--gc-sections
```

Every flag must be compatibility-tested. Do not remove required exception/unwind or platform behavior blindly.

## 25.3 Custom raylib Build

Disable unused capabilities where supported:

- 3D models
- Unused image formats
- Unused audio decoders
- Camera systems not used
- Unused file-format loaders
- Unused gesture support

Retain only the modules and platform code needed for:

- Window/core
- Input
- 2D shapes and render textures if used
- Text or embedded bitmap font drawing
- Audio stream

## 25.4 Assets

No external content assets are required.

Compiled data may include:

- Palette tables
- Chord tables
- Scale tables
- Progression tables
- Tiny bitmap font table if raylib default font is not used
- Boot strings and UI text

## 25.5 Executable Compression

Do not depend on UPX or another packer to meet the limit.

A packer may be evaluated only after:

- The unpacked executable already has healthy margin
- Antivirus false positives are tested
- Startup and compatibility are verified
- Contest interpretation is confirmed

## 25.6 Automated Size Gate

The release script must:

1. Build release executable.
2. Create a clean staging directory.
3. Copy only submission files.
4. Calculate the sum of extracted file sizes.
5. Print used bytes, remaining bytes, and percentage.
6. Fail with nonzero exit code if over 1,474,560 bytes.
7. Warn if over 1,350,000 bytes.
8. Produce the final archive separately; archive size is informational, not the compliance measurement.

---

# 26. Functional Requirements

## 26.1 Application

- **FR-001 Required:** The game shall launch as a standalone native Windows x64 executable.
- **FR-002 Required:** The game shall run without network access.
- **FR-003 Required:** The game shall not require a browser.
- **FR-004 Required:** The game shall not require external DLLs or assets in the submission package.
- **FR-005 Required:** The game shall expose title, seed selection, mode selection, play, pause, results, credits, and exit flows.

## 26.2 Seed and Generation

- **FR-010 Required:** The game shall accept a six-character seed.
- **FR-011 Required:** The game shall generate a random valid seed.
- **FR-012 Required:** The same seed shall regenerate the same song, phrase sequence, palette, and node layout.
- **FR-013 Required:** Music, layout, gameplay, and visual RNG streams shall be independent.
- **FR-014 Required:** Generation shall complete before the run begins.

## 26.3 Audio

- **FR-020 Required:** All music and sound shall be synthesized at runtime.
- **FR-021 Required:** Musical timing shall be driven by a sample-accurate shared transport.
- **FR-022 Required:** The game shall provide pad, percussion, bass, and lead/pluck stems.
- **FR-023 Required:** Stem phase shall remain aligned even while stems are inaudible.
- **FR-024 Required:** Planned stem transitions shall occur on downbeats.
- **FR-025 Required:** Damage shall produce immediate but click-free audio feedback.
- **FR-026 Required:** The game shall generate a complete 56-bar arrangement.

## 26.4 Gameplay

- **FR-030 Required:** The player shall move freely within a circular arena.
- **FR-031 Required:** The player shall have one Pulse action that always functions.
- **FR-032 Required:** Pulse strength shall increase near musical beats.
- **FR-033 Required:** The game shall generate recoverable nodes from Euclidean patterns.
- **FR-034 Required:** Node positions and musical pulse positions shall share the same pattern.
- **FR-035 Required:** Recovery shall determine the result percentage.
- **FR-036 Required:** The corruption echo shall retrace the player’s delayed path.
- **FR-037 Required:** Corruption threat progression shall be synchronized to beat boundaries.
- **FR-038 Required:** Bloom, Flow, and Pulse presets shall be supported.
- **FR-039 Required:** Flow shall be the default mode.

## 26.5 Visuals

- **FR-040 Required:** The game shall render without shipped textures or sprite sheets.
- **FR-041 Required:** Recovered nodes shall form visible Euclidean flowers or sector patterns.
- **FR-042 Required:** Visual progression shall follow arrangement and recovery progression.
- **FR-043 Required:** Corruption shall be distinguished through shape, value, movement, and color.
- **FR-044 Required:** The player, notes, and corruption shall use readable trails.
- **FR-045 Required:** The final read shall produce a distinct audiovisual resolution.

## 26.6 Results and Replay

- **FR-050 Required:** Results shall show seed, mode, and recovery percentage.
- **FR-051 Required:** The player shall be able to replay the same seed immediately.
- **FR-052 Required:** The player shall be able to generate or enter a different seed.
- **FR-053 Required:** Restart time shall be under two seconds on target hardware.

## 26.7 Compliance

- **FR-060 Required:** The build pipeline shall enforce the exact 1,474,560-byte extracted size limit.
- **FR-061 Required:** The release package shall contain all required license notices.
- **FR-062 Required:** A clean target machine shall be able to run the submission package.

---

# 27. Nonfunctional Requirements

- **NFR-001:** Audio must not audibly drift from visuals during a complete run.
- **NFR-002:** The audio callback must not allocate memory or block on the game thread.
- **NFR-003:** No ordinary seed may generate NaN, infinity, buffer overflow, or invalid geometry.
- **NFR-004:** The first meaningful interaction must occur within ten seconds of starting a run.
- **NFR-005:** The default mode must create a routing decision at least once per phrase after corruption begins.
- **NFR-006:** The game must remain comprehensible without hearing audio, although audio materially improves anticipation.
- **NFR-007:** The game must remain playable for color-vision differences through non-color cues.
- **NFR-008:** The game must handle loss of focus and pause safely.
- **NFR-009:** Save failures must be nonfatal.
- **NFR-010:** Release builds must be reproducible from documented commands.

---

# 28. Balance Targets

These are initial tuning targets, not immutable constants.

## 28.1 Flow Completion

For a new player after one practice run:

- Run completion rate: 70–90%
- Typical recovery: 55–80%
- Typical integrity remaining: 1

For an experienced player:

- Typical recovery: 80–98%
- 100% recovery should be possible but uncommon

## 28.2 Bloom

- Nearly all players reach final bloom
- Typical recovery: 70–95%
- Corruption still changes routes but does not produce early failure

## 28.3 Pulse

- First-attempt completion: 35–60%
- Experienced completion: 70–90%
- 100% recovery should require seed familiarity or excellent routing

## 28.4 Music Quality

Across a broad seed test:

- No obviously broken or painfully dissonant seeds
- No silent seeds
- No seeds with missing required stems
- No seeds where percussion masks the warm tonal identity
- Variation should be noticeable within three random seeds

---

# 29. Testing Strategy

## 29.1 Unit Tests

Test:

- Seed normalization and hashing
- RNG stream independence
- Euclidean pattern counts and spacing
- Pattern rotation
- Tick-to-sample conversion
- Bar/beat/step boundaries
- Recovery percentage math
- Ring-buffer wraparound
- Corruption delay indexing
- Save-data validation
- Package size calculation

## 29.2 Determinism Tests

For a fixed list of seeds, snapshot:

- Genome values
- Phrase patterns
- First N music events
- Node positions
- Palette ID

CI must detect unintended changes.

## 29.3 Audio Tests

- Render selected seeds offline to a buffer where practical.
- Scan for NaN/infinity.
- Measure peaks.
- Verify event timestamps are monotonic.
- Run a 30-minute loop test to detect transport drift or callback instability.
- Toggle every layer repeatedly at downbeats and listen for clicks.
- Simulate damage duck and restoration.

## 29.4 Generation Fuzzing

Run at least 100,000 generated seeds in a headless validation tool or test mode.

Check:

- Array capacity
- Node overlap
- Valid note ranges
- Pattern density limits
- Final pattern route constraints
- Total event count
- Geometry bounds
- Audio peak estimates

## 29.5 Visual Review

Create a developer gallery mode that rapidly previews generated palettes and phrase patterns.

Manually review at least 200 seeds for:

- Legibility
- Excessive clutter
- Muddy palettes
- Invisible corruption
- Ugly or repetitive geometry
- Poor contrast

The gallery mode may be excluded from the release build through a compile flag.

## 29.6 Gameplay Testing

Test with:

- Non-rhythm players
- Rhythm-game players
- Gamepad and keyboard
- Audio on and muted
- Bloom, Flow, and Pulse
- First-time players with no verbal explanation

Key observation:

> Do players understand that their old path becomes the corruption hazard?

If not, improve telegraphing before adding more systems.

## 29.7 Compatibility Testing

Required clean-machine tests:

- Windows 10 x64
- Windows 11 x64
- Integrated Intel or AMD graphics
- NVIDIA/AMD discrete graphics where available
- 60 Hz and high-refresh displays
- Keyboard only
- Common XInput gamepad
- Audio device at common system configurations
- Windowed and fullscreen

## 29.8 Release Testing

- Extract final archive into a clean folder.
- Confirm extracted total byte count.
- Confirm game launches with network disabled.
- Confirm no missing DLL prompt.
- Complete one run in each mode.
- Enter and replay a known seed.
- Pause, lose focus, resume, and exit.
- Confirm README and license notice.

---

# 30. Developer and Debug Tools

Debug functionality should be compiled out of release builds.

Required debug capabilities:

- Toggle each stem gain
- Display sample clock, bar, beat, step, and phase
- Jump to arrangement phase
- Freeze corruption
- Show collision geometry
- Show node state and pattern bitset
- Adjust mode parameters live
- Force damage
- Force 100% recovery
- Regenerate seed
- Export or print seed genome
- Run automated seed sweep
- Display current binary/package size through build script

Debug keys must not leak into release behavior unexpectedly.

---

# 31. Implementation Passes

These passes define build order for the complete game. A pass is complete only when its exit criteria are met.

## Pass 0: Repository, Toolchain, and Size Gate

Deliver:

- C99 project structure
- Pinned raylib source or dependency
- Debug and release builds
- Static Windows x64 target
- Automated extracted-size checker
- Minimal window/audio initialization
- README with build commands

Exit criteria:

- Clean build succeeds.
- Minimal executable runs on a second machine.
- Size script reports exact bytes and fails correctly.

## Pass 1: Audio Transport and Callback

Deliver:

- Single 48 kHz stereo stream
- Sample clock
- PPQN transport
- Tick-to-sample scheduling
- Fixed voice pool
- Debug metronome
- Published transport snapshot
- Stem gain controls quantized to downbeats

Exit criteria:

- Metronome remains aligned with visual beat marker for at least 30 minutes.
- Debug stem toggles do not click or drift.
- Callback performs no allocation.

## Pass 2: Runtime Synth and Song Generator

Deliver:

- Seed system and independent RNG streams
- Pad, bass, pluck, and percussion voices
- Scales, progressions, voicings, motifs
- Euclidean pattern generation
- 56-bar event schedule
- Master processing and safe headroom

Exit criteria:

- At least 100 test seeds produce complete, audible songs.
- No NaN, buffer overflow, or unacceptable peak.
- Same seed reproduces the same event list.

## Pass 3: Euclidean Arena Visualizer

Deliver:

- Circular arena and rings
- Read sweep tied to transport
- Step anchors and active petals
- Node state rendering
- Seed palettes
- Basic additive glow

Exit criteria:

- The audible rhythm and visible active positions are clearly related.
- Visuals remain legible across all supported step counts.

## Pass 4: Player Movement and Pulse

Deliver:

- Keyboard and gamepad movement
- Acceleration, drag, boundaries
- Pulse cooldown and impulse
- Beat resonance scaling
- Player trail and feedback
- Node collection

Exit criteria:

- Moving and collecting nodes feels satisfying without corruption.
- Pulse always works and feels stronger near the beat.
- No framerate-dependent movement defects.

## Pass 5: Corruption Echo and Damage

Deliver:

- Musical-time path recording
- Delayed echo head
- Ribbon rendering and collision
- Mode-dependent delay and width
- Integrity
- Recovery corruption
- Damage audio and visual feedback

Exit criteria:

- The player clearly understands the echo is following the previous route.
- Flow mode creates routing decisions without unavoidable hits.
- Invulnerability prevents repeated contact damage.

## Pass 6: Full Run Director

Deliver:

- 56-bar phase plan
- Four-bar phrases
- Layer progression
- Pressure progression
- Final Read pattern
- Success and early failure resolution

Exit criteria:

- A complete run lasts two to three minutes.
- Each phase is perceptibly different.
- The song and visuals build together.
- Final Read works in all three modes.

## Pass 7: Menus, Seed Sharing, Modes, and Results

Deliver:

- Boot and title presentation
- Seed entry/randomization
- Mode selection
- Minimal tutorial prompts
- Pause/restart
- Results screen
- Replay same seed/new seed
- Settings persistence
- Credits and notice

Exit criteria:

- A new player can begin without developer explanation.
- Same-seed replay requires no retyping.
- All state transitions are stable.

## Pass 8: Complete Visual and Audio Polish

Deliver:

- Final palettes
- Layer-linked visual progression
- Controlled decorative curves
- Final bloom
- Failure collapse
- Balanced mix
- Ambience/delay if retained
- UI animation and typography polish

Exit criteria:

- The game looks intentional rather than randomly generated.
- Every palette maintains contrast.
- Audio remains warm and uncluttered.
- Effects do not obscure nodes or corruption.

## Pass 9: Balance, Fuzzing, and Compatibility

Deliver:

- 100,000-seed validation
- Determinism snapshots
- Gameplay tuning across modes
- Clean-machine Windows testing
- Performance fallback settings
- Save failure handling

Exit criteria:

- No critical seed-generation failures.
- Target completion/recovery ranges are approximately met.
- No audio underruns on target machines.

## Pass 10: Size Optimization and Submission Build

Deliver:

- Custom minimized raylib build
- Dead-code removal
- Release-only unity build if useful
- Final package staging
- Exact byte report
- README and license notice
- Submission archive

Exit criteria:

- Extracted package is no more than 1,474,560 bytes.
- Target package is preferably no more than 1,350,000 bytes.
- Submission runs from a clean extracted directory.
- Final archive and source tag are reproducible.

---

# 32. Acceptance Criteria for the Complete Game

The complete game is done when all of the following are true:

1. A clean Windows machine can launch the game with no installation.
2. The extracted submission package is within the exact contest limit.
3. A player can enter or randomize a six-character seed.
4. The seed deterministically creates a complete song and arena.
5. The game provides Bloom, Flow, and Pulse modes.
6. A complete run lasts approximately two to three minutes.
7. Euclidean rhythm is visibly and audibly expressed through notes, nodes, and petals.
8. The player can move, Pulse, recover sectors, and avoid the delayed corruption echo.
9. Rhythm improves action strength without invalidating off-beat input.
10. Musical layers enter and leave in response to visible game state.
11. The game produces a distinct final bloom or musical failure resolution.
12. Results display seed, mode, and Recovery Percentage.
13. The player can immediately replay the same seed.
14. Audio and visuals remain synchronized throughout the run.
15. No tested seed produces broken audio, invalid geometry, or impossible starting conditions.
16. The game is visually readable and aesthetically coherent across its palette set.
17. No external asset or runtime dependency is required.
18. Credits and raylib license obligations are satisfied.
19. Release build commands and size verification are documented.
20. The final game feels like an arcade experience, not merely a steerable screensaver.

---

# 33. Risk Register and Mitigations

## Risk: Procedural music frequently sounds mediocre

Mitigation:

- Use curated progressions and voicings.
- Restrict note ranges.
- Repeat and transform short motifs.
- Fuzz technically and review seeds by ear.
- Prefer fewer good possibilities over broad random variation.

## Risk: Visual generation becomes noise

Mitigation:

- Limit visual grammar to flowers, trails, and corruption.
- Use handcrafted palettes.
- Clamp every parameter.
- Show only one or two decorative systems at once.
- Build a gallery review mode.

## Risk: Game remains beautiful but shallow

Mitigation:

- Make delayed path corruption the central pressure mechanic.
- Ensure at least one routing decision per phrase.
- Tune Flow as the primary experience.
- Test the first minute repeatedly before polishing the finale.

## Risk: Audio callback bugs appear late

Mitigation:

- Build callback and transport first.
- Keep one stream and one clock.
- Pre-generate events.
- Use fixed memory.
- Test long-term alignment before gameplay expansion.

## Risk: Binary exceeds 1.44MB

Mitigation:

- Add size gate in Pass 0.
- Build a custom raylib configuration.
- Avoid external assets and broad format support.
- Maintain a 124KB or larger safety margin.
- Check size after every major pass.

## Risk: AI-generated code adds unnecessary abstractions

Mitigation:

- Prefer plain structs and functions.
- Ban general ECS, scripting runtime, reflection, serialization framework, plugin architecture, and general synth graph.
- Require each module to justify runtime and binary cost.
- Review release map file and unused symbols.

## Risk: Float behavior harms determinism

Mitigation:

- Use integer patterns and absolute tick/sample calculations.
- Use deterministic PRNGs.
- Treat exact particle paths as nonessential to deterministic sharing.
- Snapshot structural outputs rather than rendered pixels.

## Risk: Deadline ambiguity

Mitigation:

- Submit at least 24 hours before September 4, 2026 KST.
- Recheck the official form and rules before final packaging.
- Do not plan work against the latest listed minute.

---

# 34. AI Coder Instructions

The coding agent implementing this specification should follow these rules:

1. Do not substitute a different genre or core mechanic.
2. Do not turn Pulse into a precision rhythm judgment.
3. Do not add combat, shooting, upgrades, or enemies.
4. Do not use per-frame timing for music.
5. Do not add external assets to solve a procedural quality problem.
6. Do not add a general-purpose engine abstraction when a direct C function is sufficient.
7. Keep music, layout, gameplay, and visuals on independent deterministic RNG streams.
8. Maintain a continuously passing release size check.
9. Complete the exit criteria of each pass before proceeding.
10. Preserve debug tools behind compile-time flags.
11. Prefer testable, fixed-size data structures.
12. Record any intentional deviation in a short `DECISIONS.md` entry containing:
    - Original requirement
    - Reason for deviation
    - Replacement behavior
    - Size, quality, and schedule impact

---

# 35. Final Creative North Star

A strong run should begin as a nearly empty signal and end as a song the player feels they personally rescued.

The player should remember three things:

1. The glowing pattern they heard was the same pattern they played.
2. Their own beautiful trail eventually became the danger chasing them.
3. An entire musical world came from six characters and fit on a floppy disk.

That is the game.
