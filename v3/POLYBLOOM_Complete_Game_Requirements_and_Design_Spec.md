# POLYBLOOM
## Complete Requirements and Game Design Specification

**Document type:** Full game requirements, technical design, level design, and implementation plan  
**Target:** 1.44 MB Game Development Contest  
**Language:** C99  
**Primary target platform:** Windows x64 standalone executable  
**Working title:** **POLYBLOOM**  
**Revision:** 1.0  
**Date:** July 21, 2026

---

# 1. Executive Summary

**POLYBLOOM** is a compact, original 3D platformer inspired by the broad pleasures of classic character platformers:

- Precise, readable jumping and secret routes
- Fast charging, gliding, and collectible-driven exploration
- Forward-moving obstacle gauntlets and satisfying breakables
- A charming hero with forgiving aerial control

The game must not copy characters, levels, enemies, names, art, sounds, animations, or signature layouts from any existing franchise. Those games are references for *feel and design values only*.

The finished game contains exactly **two complete, polished levels**. A first-time successful run should take approximately **60–90 seconds per level**. Skilled players may finish faster, while new players may take up to two minutes because of exploration, mistakes, or collectible hunting.

Everything visible is created from generated geometric meshes, mathematical animation, vertex colors, procedural particles, and code-defined level recipes. Music and sound effects are synthesized at runtime. The final submission should preferably be a single executable with no external assets.

The player controls **Mote**, a small geometric creature that can:

1. Run with responsive acceleration and turning
2. Jump with variable height
3. Hold jump to open a petal-like **Bloom Glide**
4. Use a fast **Burst** move to break objects, defeat enemies, cross gaps, and maintain momentum

The two levels are deliberately authored, not randomly assembled:

- **Level 1 — Petalstep Gardens:** A bright, semi-open introduction combining flowing jumps, a glide route, breakable structures, a small secret, and a joyful final descent.
- **Level 2 — Prismrush Cascade:** A faster, mostly linear obstacle run with moving geometry, collapsing paths, a pursuing geometric storm, and a climactic downhill finish.

The game should feel colorful, playful, fast, forgiving, and visually surprising. It is not a technology demo, an infinite procedural generator, or a miniature open world. It is a tiny but complete arcade platformer.

---

# 2. Feasibility Verdict

## 2.1 Is this possible within 1.44 MB?

**Yes, provided the project is designed around the constraint from the first build.**

The official contest limit is **1,474,560 bytes after extraction**, including the executable, runtime, libraries, and all other submitted files. The game must run as a standalone application and cannot be browser-based. The official judging criteria prioritize completion, staying under the size cap, and fun.

A two-level 3D platformer is feasible because:

- Level layouts can be stored as compact arrays of transforms and gameplay commands.
- All meshes can be generated from a small library of primitives.
- Animation can use transforms, squash-and-stretch, and simple procedural rigs.
- Vertex colors and a tiny shader can replace textures and materials.
- Runtime synthesis can replace music and sound files.
- A kinematic character controller is much smaller than a general physics engine.
- The package can contain one executable and a tiny text file only if required.

The main risks are not raw bytes. They are:

- Camera quality
- Collision reliability
- Movement tuning
- Level design iteration
- Visual composition
- Final executable size if a general-purpose framework is included carelessly

## 2.2 Mandatory architecture decision

The project must perform a **size feasibility spike before full production**.

Preferred implementation path:

- C99
- A custom-configured, statically linked raylib build
- Only the modules and features actually used
- All unnecessary codecs, model loaders, image formats, logging, examples, and optional systems disabled
- Release compilation with size optimization and symbol stripping

Fallback implementation path if the measured package cannot remain comfortably under budget:

- Win32 window/input layer
- OpenGL compatibility renderer
- `waveOut` or similarly small Windows audio output
- XInput plus keyboard
- Custom game code unchanged above the platform layer

The project must not reach late production before proving that a representative 3D scene, input, audio callback, and release build fit beneath the size budget.

---

# 3. Contest Compliance Requirements

## 3.1 Hard requirements

| ID | Requirement |
|---|---|
| CON-001 | The extracted submission package must be no larger than 1,474,560 bytes. |
| CON-002 | The game must be playable as a standalone executable. |
| CON-003 | The game must not require a browser, network connection, installer, or downloaded content. |
| CON-004 | The game and all included content must be original and created after the contest announcement. |
| CON-005 | The package must include every runtime dependency needed on the chosen target system. |
| CON-006 | Internal compression is permitted, but the final extracted package remains subject to the hard limit. |
| CON-007 | Internal target is submission at least 48 hours before September 4, 2026 because the official page contains conflicting minute values. |
| CON-008 | The build pipeline must fail automatically when package size exceeds the configured safe threshold. |

## 3.2 Internal size targets

| Item | Target |
|---|---:|
| Main executable | 1,150,000 bytes or less |
| Optional README / controls text | 12,000 bytes or less |
| Optional license notices | 20,000 bytes or less |
| Reserved safety margin | At least 200,000 bytes |
| Preferred extracted total | 1,250,000 bytes or less |
| Absolute extracted total | 1,474,560 bytes or less |

Any build larger than 1,300,000 bytes is considered **at risk** and requires immediate investigation.

---

# 4. Product Vision

## 4.1 One-sentence pitch

> A joyful miniature 3D platformer where a tiny geometric creature runs, bursts, bounces, and blooms through two handcrafted worlds generated entirely from code.

## 4.2 Player fantasy

The player should feel like a small, buoyant creature moving through a toy universe that is:

- Soft but geometric
- Fast but forgiving
- Simple but expressive
- Tiny in storage but unexpectedly rich on screen

The game should repeatedly create the feeling:

> “I cannot believe all of this fits on a floppy disk.”

## 4.3 Design pillars

### Pillar 1 — Movement is the main reward

Running, jumping, gliding, and bursting must feel satisfying before collectibles, enemies, or scenery are added.

### Pillar 2 — Authored paths, procedural beauty

The playable collision layout is intentionally designed. Procedural systems generate meshes, decoration, color variation, animation, particles, and audio without compromising level quality.

### Pillar 3 — Fast readability

A judge should understand the controls and objective within ten seconds.

### Pillar 4 — Forgiving mastery

A new player can finish. A skilled player can chain movement, discover shortcuts, collect everything, and improve their time.

### Pillar 5 — Every byte appears expensive

The game uses code to produce visual density and polish rather than shipping large assets.

---

# 5. Scope

## 5.1 Complete final-game scope

The finished game includes:

- One original playable character
- Four core movement actions
- Three health petals
- Two complete 3D levels
- One compact tutorial sequence integrated into Level 1
- Three enemy types
- Thirteen reusable gameplay object types
- Two synthesized music tracks
- A synthesized sound-effect set
- Procedural geometric scenery
- Title screen
- Level select
- Pause menu
- Results screen
- Collectible and best-time persistence
- Keyboard support
- Controller support
- Adjustable camera sensitivity
- Optional camera inversion
- Immediate restart
- Full completion sequence

## 5.2 Explicit non-goals

The finished game does **not** include:

- A third level
- A hub world
- Boss fights
- Dialogue
- Cutscenes longer than five seconds
- Inventory
- Skill tree
- Character transformations
- Copy abilities
- Online features
- Multiplayer
- Randomly generated collision layouts
- A level editor
- Skeletal animation
- Texture maps
- Imported 3D models
- Voice acting
- Full dynamic shadows
- General-purpose physics
- Procedural story generation
- Multiple playable characters

These are prohibited scope expansions unless every acceptance criterion in this document is already complete and package size remains safe.

---

# 6. Original World and Character

## 6.1 Hero: Mote

**Mote** is a small creature assembled from rounded geometric plates.

### Silhouette

- A low, rounded central body
- Two small feet
- Two expressive eyes
- A thin ribbon-like tail
- Five folded geometric petals around the back
- Petals unfold during gliding

Mote must not be a sphere with Kirby-like proportions, a dragon, a bandicoot, a plumber, or a recognizable imitation of an existing character.

### Geometry

Mote is generated from:

- One rounded dodecahedron-like body mesh
- Two half-sphere eyes
- Two wedge feet
- Five thin petal prisms
- One segmented ribbon trail

No texture is required. Facial expression uses eye scale, eye separation, eyelid angle, and body squash.

## 6.2 World premise

The world is a garden of living geometry. Paths grow as ribbons. Trees are layered polyhedra. Flowers are rotating prisms. Clouds are clustered soft shapes. Ancient gates rearrange themselves into traversable courses.

The story is communicated only through visual progression:

- Mote enters a dormant region.
- Moving through it wakes color and motion.
- Completing the course causes the level’s final structure to bloom.

No explanatory text is required beyond controls and level names.

---

# 7. Core Game Loop

1. Select a level.
2. Enter the course immediately.
3. Run, jump, glide, and Burst through obstacles.
4. Collect Glints and three optional Bloom Seeds.
5. Avoid hazards or recover quickly after mistakes.
6. Reach the level’s Bloom Gate.
7. Trigger a short visual completion bloom.
8. View time, Glints, Seeds, and best result.
9. Retry instantly or continue to the other level.

A level should have no loading screen after selection beyond a brief generated transition.

---

# 8. Controls

## 8.1 Controller

| Action | Default |
|---|---|
| Move | Left stick |
| Camera | Right stick |
| Jump / Bloom Glide | South face button |
| Burst | West face button |
| Camera recenter | Right shoulder |
| Pause | Menu / Start |
| Restart level | Hold both shoulder buttons for 0.5 seconds |

## 8.2 Keyboard

| Action | Default |
|---|---|
| Move | WASD |
| Camera | Arrow keys or mouse movement |
| Jump / Bloom Glide | Space |
| Burst | Left Shift or J |
| Camera recenter | R |
| Pause | Escape |
| Restart | Backspace held for 0.5 seconds |

## 8.3 Input requirements

| ID | Requirement |
|---|---|
| INP-001 | Keyboard-only play must be fully supported. |
| INP-002 | XInput-compatible controllers must be supported on Windows. |
| INP-003 | Input prompts must switch to the most recently used input family. |
| INP-004 | Jump input buffering must be implemented. |
| INP-005 | Coyote time must be implemented. |
| INP-006 | Holding restart must prevent accidental resets. |
| INP-007 | The game must remain playable without mouse input. |

---

# 9. Player Movement Model

Movement quality is the highest-priority feature.

## 9.1 Movement states

```c
typedef enum PlayerState {
    PLAYER_GROUNDED,
    PLAYER_RISING,
    PLAYER_FALLING,
    PLAYER_GLIDING,
    PLAYER_BURST,
    PLAYER_BOUNCED,
    PLAYER_HURT
} PlayerState;
```

## 9.2 Baseline tuning targets

All values are starting points and must be tuned by feel.

| Parameter | Initial target |
|---|---:|
| Maximum run speed | 8.5 m/s |
| Ground acceleration | 42 m/s² |
| Ground deceleration | 55 m/s² |
| Air acceleration | 20 m/s² |
| Turn responsiveness | 11–16 rad/s |
| Gravity | 25 m/s² |
| Jump launch speed | 10.5 m/s |
| Minimum jump cut velocity | 4.5 m/s |
| Coyote time | 0.12 s |
| Jump buffer | 0.14 s |
| Ground snap distance | 0.20 m |
| Step height | 0.32 m |
| Maximum walkable slope | 48 degrees |
| Glide fall speed | 2.7 m/s |
| Glide forward bonus | 1.8 m/s |
| Burst speed | 15 m/s |
| Burst duration | 0.28 s |
| Burst recovery | 0.18 s |
| Burst cooldown | 0.45 s |
| Hurt invulnerability | 1.0 s |

## 9.3 Run

Run movement must:

- Accelerate quickly without feeling digital.
- Preserve some momentum through turns.
- Allow fast 180-degree correction at low speed.
- Lean the character into turns.
- Generate a short ribbon trail above 70% maximum speed.
- Produce subtle footstep puffs.

## 9.4 Jump

Jump behavior must include:

- Buffered jump input before landing
- Coyote time after leaving an edge
- Variable jump height
- Strong initial rise
- Rounded apex
- Slightly faster fall than rise
- Automatic body squash on takeoff and landing
- Optional tiny landing pause of no more than 30 ms for strong impacts

The player must never be required to use a pixel-perfect edge jump.

## 9.5 Bloom Glide

Holding Jump while falling unfolds Mote’s petals.

Effects:

- Vertical fall speed is capped.
- Forward control becomes smoother and slightly faster.
- Camera pulls back slightly.
- Petals rotate slowly.
- A translucent geometric wake appears.
- Glide rings grant a brief lift impulse.
- Releasing Jump folds the petals and restores normal falling.

Restrictions:

- Glide cannot begin during Burst recovery.
- Glide does not gain altitude by itself.
- Glide cannot stall the player indefinitely against a wall.
- Maximum uninterrupted glide time should not require a meter; level geometry limits its use.

## 9.6 Burst

Burst is a fast directed movement used for both traversal and interaction.

Ground Burst:

- Launches Mote along intended movement direction.
- Breaks brittle geometry.
- Defeats vulnerable enemies.
- Preserves speed when transitioning onto ramps.
- Can cross short gaps.

Air Burst:

- Travels mostly horizontally with a small upward bias if the stick points upward.
- Can be used once before landing or touching a recharge ring.
- Does not fully cancel downward momentum unless performed near the jump apex.
- Can strike Burst targets and bounce from them.

Burst feedback:

- Brief character stretch
- Petal collapse
- Bright ribbon trail
- Short synthesized “fwip”
- Small camera impulse
- Chromatic color flash achieved through geometry or shader parameters, not textures

## 9.7 Bounce

Bounce surfaces and defeated enemies can launch the player.

A high bounce:

- Recharges air Burst.
- Places the player into `PLAYER_BOUNCED`.
- Supports immediate steering.
- May transition directly into Glide.

## 9.8 Forgiveness systems

- Ledge nudge: if Mote’s feet miss a platform lip by a very small margin while descending, gently correct inward.
- Hazard grace: newly spawned or newly activated hazards cannot damage the player during their first readable telegraph.
- Recovery platform: after a fall, respawn at the latest checkpoint with forward orientation.
- No finite lives.
- A fall costs one health petal but does not reset collectibles.
- At zero petals, respawn at the checkpoint with all petals restored and add a time penalty.

---

# 10. Character Controller and Collision

## 10.1 Physics philosophy

Use a custom kinematic controller. Do not integrate a general physics engine.

Player representation:

- Vertical capsule or sphere-stack approximation
- Separate broad-phase bounding sphere
- Ground probe below the capsule
- Optional shoulder probe for narrow passages

## 10.2 Required collision shapes

- Axis-aligned boxes
- Oriented boxes
- Planes
- Ramps
- Capsules
- Spheres
- Simple convex wedges
- Trigger volumes

Arbitrary triangle-mesh collision is not required for gameplay. Generated visual meshes should sit on top of simpler collision shapes.

## 10.3 Collision algorithm

Recommended approach:

1. Integrate intended horizontal velocity.
2. Sweep capsule against nearby static shapes.
3. Resolve earliest collision.
4. Project remaining velocity along the collision plane.
5. Attempt step-up for low obstacles.
6. Integrate vertical movement.
7. Ground snap when descending near a walkable surface.
8. Resolve moving-platform displacement.
9. Update final grounded state.

## 10.4 Spatial lookup

Each level uses a fixed-size uniform grid or compact sector list for broad-phase collision and object lookup.

No dynamic allocation is permitted during normal gameplay.

## 10.5 Collision acceptance criteria

- No tunneling through standard obstacles at maximum Burst speed.
- No visible jitter while standing on moving platforms.
- No camera-induced movement direction reversals.
- No permanent wall sticking.
- No slope launch when crossing a seam.
- No missed ground detection at normal frame rates.
- Behavior must remain stable from 30 to 144 rendered frames per second using fixed-step simulation.

---

# 11. Camera Design

Camera quality is a release blocker.

## 11.1 Camera modes

### Follow Camera

Used for most of Level 1.

- Third-person camera behind and above Mote
- Smooth target tracking
- Look-ahead based on player velocity
- Automatic recenter during forward movement
- Limited manual orbit
- Collision shortening to prevent wall clipping

### Course Camera

Used for selected Level 2 sequences.

- Camera follows a designed spline or zone
- Player retains normal 3D movement
- Framing emphasizes upcoming obstacles
- Transition between camera modes is blended

### Completion Camera

- Briefly orbits the final Bloom Gate
- Maximum duration: 3.5 seconds
- Skippable with Jump or Burst

## 11.2 Camera requirements

| ID | Requirement |
|---|---|
| CAM-001 | The camera must frame the landing zone of ordinary jumps. |
| CAM-002 | The camera must not rotate sharply without player input or a clearly signaled course transition. |
| CAM-003 | Camera collision must preserve visibility of Mote. |
| CAM-004 | Movement input must remain intuitive during camera blends. |
| CAM-005 | A recenter control must always be available. |
| CAM-006 | Sensitivity and vertical inversion must be configurable. |
| CAM-007 | Camera shake must be subtle and disableable through the reduced-effects setting. |

## 11.3 Camera collision

Use a ray or swept sphere from target to desired camera position.

If blocked:

- Pull camera inward.
- Apply temporal smoothing.
- Prefer raising slightly before moving extremely close.
- Never place camera inside the player.
- Restore distance gradually after obstruction clears.

---

# 12. Health, Failure, and Checkpoints

## 12.1 Health

Mote has three visible petals representing health.

Damage:

- Removes one petal.
- Knocks Mote away from the hazard.
- Grants temporary invulnerability.
- Does not remove collected Bloom Seeds.
- May scatter up to three recently collected Glints, which can be recollected.

Health pickups:

- Small Heart Bloom restores one petal.
- At least one appears before the hardest sequence in each level.

## 12.2 Falls

Falling below the level floor:

- Fades quickly to the level palette’s dark color.
- Respawns at the latest checkpoint.
- Removes one health petal.
- Adds a 3-second result penalty.
- Preserves permanent collectibles.

## 12.3 Zero health

At zero health:

- Respawn at checkpoint.
- Restore all petals.
- Add a 7-second result penalty.
- Do not show a lengthy game-over screen.

## 12.4 Checkpoints

- Level 1 has one midpoint checkpoint.
- Level 2 has two compact checkpoints because of the chase sequence.
- Checkpoints are glowing geometric arches.
- Activation is automatic.
- Respawn orientation is authored.
- Respawn must place the player in a safe, readable state.

---

# 13. Collectibles and Results

## 13.1 Glints

Each level contains **30 Glints**.

Properties:

- Small floating geometric sparks
- Arranged to suggest good movement lines
- Emit a warm chime whose pitch rises within short collection chains
- Not required for level completion
- All 30 should be collectible in one run
- Collection state during the run is preserved through falls

## 13.2 Bloom Seeds

Each level contains **three Bloom Seeds**.

Purpose:

- Reward exploration and mastery
- One on the primary route but requiring execution
- One on a clearly visible alternate route
- One hidden behind a secret or challenge

Collecting a Seed:

- Adds one petal layer to the final Bloom Gate
- Plays a distinctive chord
- Is permanently recorded after level completion
- Does not alter player abilities

## 13.3 Results

Results show:

- Completion time
- Best time
- Glints collected out of 30
- Bloom Seeds collected out of 3
- Number of falls
- “Full Bloom” badge if all collectibles were obtained
- “Flow Run” badge if completed without damage or falls

No letter grade is required. The presentation should remain friendly rather than punitive.

---

# 14. Gameplay Objects

## 14.1 Required reusable objects

| Object | Function |
|---|---|
| Solid platform | Standard collision surface |
| Moving platform | Loops along a compact authored path |
| Bounce Bloom | Launches player and recharges air Burst |
| Glide Ring | Gives forward lift and aligns the next route |
| Brittle Prism | Breakable only by Burst |
| Burst Target | Airborne target that bounces player onward |
| Checkpoint Arch | Saves respawn position |
| Bloom Gate | Level endpoint |
| Falling Tile | Telegraphs, drops, and resets |
| Rotating Bar | Slow readable hazard |
| Heart Bloom | Restores health |
| Glint | Standard collectible |
| Bloom Seed | Major collectible |

## 14.2 Object animation

All animation uses:

- Translation
- Rotation
- Scale
- Color interpolation
- Procedural vertex displacement when necessary
- Particles

No skeletal animation system is permitted.

---

# 15. Enemies

Enemies exist to create movement decisions, not combat depth.

## 15.1 Pogo Bud

A small three-legged geometric bud.

Behavior:

- Idles with visible squash.
- Hops at a fixed interval.
- Turns toward the player only when nearby.
- Can be jumped on or Burst through.
- Defeat creates a useful bounce.

Purpose:

- Introduce enemy interaction.
- Provide optional vertical route assistance.

## 15.2 Rollfin

A faceted rolling creature.

Behavior:

- Patrols a short authored spline.
- Accelerates downhill.
- Telegraphs direction with body tilt.
- Can be defeated by Burst from the side or above.
- Contact causes knockback.

Purpose:

- Create timing decisions on narrow paths.
- Reinforce momentum play.

## 15.3 Prism Wisp

A hovering cluster of rotating shards.

Behavior:

- Floats around a fixed anchor.
- Charges for 0.7 seconds.
- Sends a slow, visible pulse along the ground or through a lane.
- Becomes vulnerable after firing.
- Burst destroys it.

Purpose:

- Encourage reading and lane changes in Level 2.

## 15.4 Enemy requirements

- Maximum active enemies: 10.
- No pathfinding.
- No enemy may require combat to finish a level.
- Every attack has a visual telegraph.
- Enemy reset behavior must be deterministic.
- Defeated enemies remain defeated until restart or full checkpoint reset, according to per-object flags.

---

# 16. Procedural Art System

## 16.1 Core principle

**Procedural does not mean random level design.**

The level’s collision, pacing, major landmarks, obstacle order, and intended routes are authored. Procedural systems produce their visual realization.

## 16.2 Generated mesh library

The engine must generate and cache these meshes at startup:

- Cube
- Beveled cube
- Rounded box approximation
- Wedge
- Triangular prism
- Low-poly sphere
- Low-poly capsule
- Cylinder
- Cone
- Ring
- Arch
- Ribbon segment
- Petal
- Leaf cluster
- Cloud cluster
- Starburst
- Fractured prism pieces

Meshes may share vertex/index buffers and use per-instance transforms and colors.

## 16.3 Procedural scenery grammar

### Trees

- Trunk: tapered prism
- Crown: 2–4 overlapping polyhedra
- Seed controls rotation and scale
- Sway is a low-frequency sine motion

### Flowers

- Center sphere
- 4–8 petal prisms
- Petal angle and size generated within constrained ranges
- Open slightly when Mote passes

### Grass and reeds

- Sparse triangle or ribbon clusters
- Placed only in authored decoration zones
- Density limited by performance budget

### Clouds

- Clusters of low-poly spheres
- Slow drift
- No collision
- Generated from level seed

### Distant terrain

- Layered simplified ribbons or low-poly hills
- Fogged heavily
- No gameplay collision

### Floating shards

- Decorative low-poly objects
- Rotate slowly
- React to nearby Burst events

## 16.4 Controlled randomness

Every level has a fixed seed.

Separate random streams must be used for:

- Decoration placement
- Mesh variation
- Color variation
- Particle variation
- Audio ornamentation

Gameplay collision and critical timings do not consume random values.

```c
typedef struct RandomStreams {
    uint32_t decor;
    uint32_t mesh;
    uint32_t color;
    uint32_t particles;
    uint32_t audio;
} RandomStreams;
```

Changing particle count must never move a platform or alter an enemy.

---

# 17. Rendering and Visual Style

## 17.1 Visual target

The visual identity is:

- Low-poly
- Pastel but saturated
- Softly lit
- Highly geometric
- Clear silhouettes
- Layered atmospheric depth
- Abundant but controlled particles
- No texture noise

## 17.2 Render resolution

Recommended:

- Internal render target: 640×360
- Window default: 1280×720
- Integer or clean filtered upscale
- Optional 960×540 internal mode if performance permits
- 16:9 layout
- Windowed mode required
- Borderless fullscreen optional only if size cost is negligible

## 17.3 Lighting

Use one compact lighting model:

- One directional key light
- Ambient hemisphere term
- Rim-light term
- Distance fog
- Vertex color
- Optional height-based color variation

No real-time point lights are required.

## 17.4 Shadows

Use blob shadows:

- Flattened translucent ellipse or projected disk
- Scales with height
- Fades with distance from ground
- Used for Mote, enemies, and important moving objects

Do not implement shadow maps.

## 17.5 Color palettes

Each level uses one authored palette with constrained variation.

### Level 1 palette

- Warm cream sky
- Mint and teal terrain
- Coral and golden highlights
- Lavender shadows
- White-blue Glints

### Level 2 palette

- Deep violet sky
- Cyan and blue terrain
- Magenta hazard accents
- Warm orange path guidance
- Pale green safe objects

Color variation must preserve gameplay readability:

- Safe traversable surfaces remain within the level’s safe color family.
- Hazards use the hazard accent family.
- Breakables use a consistent bright fracture pattern.
- Bounce objects use a consistent luminous center.

## 17.6 Effects

Required effects:

- Mote movement ribbon
- Burst trail
- Landing puff
- Glint collection spark
- Seed collection bloom
- Brittle Prism fragments
- Enemy defeat burst
- Checkpoint activation ring
- Falling Tile warning pulse
- Bloom Gate finale
- Chase distortion and wind lines

Optional only if size and performance permit:

- One-pass bloom-like glow
- Screen-space vignette
- Subtle color grading

## 17.7 Visual-effects limit

Maximum particle count: 1,024 active particles.

Use fixed arrays and pool recycling. Reduced-effects mode lowers this to approximately 400.

---

# 18. User Interface

## 18.1 Font

Use one of:

- A tiny embedded bitmap font
- A code-defined 5×7 or 6×8 glyph set
- Raylib’s default font only if it is already inside the measured final binary

Do not ship a TTF file.

## 18.2 Title screen

Displays:

- POLYBLOOM logo built from geometric lettering
- Mote running in a small looping scene
- Play
- Options
- Quit

Pressing the primary action from the title should reach level select in one input.

## 18.3 Level select

Two large geometric portals:

1. Petalstep Gardens
2. Prismrush Cascade

Display:

- Best time
- Glints
- Bloom Seeds
- Completion state

Level 2 is visible from the start. It does not need to be locked.

## 18.4 HUD

During gameplay show only:

- Three health petals
- Glint count
- Seed icons
- Small timer

HUD opacity may fade when unchanged.

## 18.5 Pause menu

- Resume
- Restart Level
- Options
- Return to Level Select

## 18.6 Options

- Master volume
- Music volume
- Sound volume
- Camera sensitivity
- Invert vertical camera
- Reduced effects
- Screen mode if supported

---

# 19. Audio Design

## 19.1 Architecture

All audio is synthesized at runtime.

Required architecture:

- One audio callback
- One sample clock
- Fixed-size voice pool
- No allocation in callback
- No file access in callback
- Music events scheduled by sample position
- SFX voices mixed into the same output
- Master limiter or soft clipper
- Short gain ramps to avoid clicks

## 19.2 Music

Each level has a distinct 60–90 second looping or through-composed pattern.

Music must be cheerful, warm, and compact rather than aggressively retro.

### Voice set

- Soft pulse bass
- Triangle or sine lead
- Warm two-oscillator pad
- Wooden pluck
- Rounded kick
- Tick or shaker noise
- Bell voice for collectibles

### Level 1 music

- 110–118 BPM
- Major or Lydian-like color
- Light syncopation
- Melody opens as player reaches the midpoint
- Final bloom adds bell harmony

### Level 2 music

- 124–132 BPM
- Minor-to-major energy
- Stronger percussion
- Repeating ascending motif
- Chase section adds a low pulse
- Finale resolves brightly

Music patterns are authored as compact note and event tables. Ornamentation may use deterministic procedural variation, but harmonic structure is constrained and tested.

## 19.3 SFX list

- Jump
- Glide open
- Glide ring
- Burst
- Bounce
- Landing
- Glint
- Bloom Seed
- Checkpoint
- Damage
- Fall
- Enemy defeat
- Brittle Prism break
- Falling Tile warning
- Gate activation
- Menu move
- Menu confirm
- Level complete

Each effect is generated from simple oscillator, envelope, and noise parameters.

## 19.4 Audio requirements

- No audible clicks at layer or SFX boundaries.
- Music timing must not depend on frame rate.
- SFX must not starve the music voices.
- The game must remain playable with music muted.
- The completion chord must align with the Bloom Gate animation.

---

# 20. Level Authoring Format

## 20.1 Philosophy

Levels are stored as compact code-defined recipes, not imported model files.

A level is composed from:

- Sections
- Collision primitives
- Render instances
- Gameplay objects
- Enemy spawns
- Camera zones
- Decoration zones
- Checkpoints
- Trigger volumes

## 20.2 Suggested structures

```c
typedef struct Transform {
    Vec3 position;
    Vec3 scale;
    Vec3 rotation;
} Transform;

typedef enum ShapeType {
    SHAPE_BOX,
    SHAPE_WEDGE,
    SHAPE_CYLINDER,
    SHAPE_RING,
    SHAPE_ARCH,
    SHAPE_RIBBON
} ShapeType;

typedef struct LevelPiece {
    uint8_t shape;
    uint8_t material;
    uint8_t collision_type;
    uint8_t flags;
    int16_t px, py, pz;
    int16_t sx, sy, sz;
    int16_t rx, ry, rz;
} LevelPiece;

typedef struct ObjectSpawn {
    uint8_t type;
    uint8_t variant;
    uint16_t flags;
    int16_t px, py, pz;
    int16_t a, b, c;
} ObjectSpawn;
```

Quantized integers should be converted to floats at load time. This reduces static data size and improves deterministic authoring.

## 20.3 Section commands

The AI coder may implement helper macros or functions such as:

```c
ADD_PLATFORM(x, y, z, sx, sy, sz, material);
ADD_RAMP(x, y, z, sx, sy, sz, yaw, material);
ADD_GLINT_LINE(start, end, count);
ADD_BOUNCE(x, y, z, strength);
ADD_RING(x, y, z, radius, yaw, pitch);
ADD_ENEMY(type, x, y, z, variant);
ADD_CAMERA_ZONE(bounds, mode, target, distance);
ADD_DECOR_ZONE(bounds, seed, density, style);
```

These calls must compile into compact static arrays or execute once during level initialization.

---

# 21. Level 1 — Petalstep Gardens

## 21.1 Purpose

Level 1 teaches the full movement set without explicit tutorial screens and demonstrates the game’s beauty immediately.

## 21.2 Target timings

| Player | Target |
|---|---:|
| Skilled direct route | 45–55 seconds |
| Typical first clear | 70–90 seconds |
| Full collectible route | 95–125 seconds |

## 21.3 Structure

The level is a compact semi-open path around a floating garden basin. The critical path bends in a broad S, allowing the player to see future landmarks.

### Beat 1 — Awakening Terrace: 0–10 seconds

- Mote starts facing the first Glint trail.
- Flat space teaches run and camera.
- Three Glints lead to a small step.
- A short gap teaches Jump.
- A nearby sign uses only icons: Move, Jump, Burst.
- No enemy.

**Goal:** The player is moving within one second.

### Beat 2 — Prism Orchard: 10–25 seconds

- A line of low platforms introduces variable jump height.
- Two brittle prism stacks block the fastest line.
- Burst prompt appears near the first stack.
- One Pogo Bud provides a safe enemy interaction.
- Five Glints arc over the defeated-enemy bounce.

**Bloom Seed 1:** Above the fast route, reached by bouncing from the Pogo Bud and gliding.

### Beat 3 — Ringwater Gap: 25–42 seconds

- The player reaches a wide scenic gap.
- A Bounce Bloom launches toward three Glide Rings.
- Holding Jump naturally opens petals.
- Rings create a gentle curved route.
- Missing a ring still lands on a lower recovery path.
- Lower path takes several seconds longer but is safe.

**Checkpoint 1:** On the landing island.

### Beat 4 — Turning Grove: 42–58 seconds

- Camera opens into a small garden with two routes.
- Primary route: moving platform, Rollfin, short ramp.
- Alternate route: narrow elevated ribbon requiring Burst.
- Routes reconnect within 12 seconds.

**Bloom Seed 2:** Visible on the alternate route.

### Beat 5 — Sunpetal Climb: 58–72 seconds

- Three ascending Bounce Blooms.
- One Burst Target between the second and third bloom.
- Player can Jump → Burst Target → Bounce → Glide.
- Falling returns to the base quickly rather than causing a death plane.

**Bloom Seed 3:** Hidden inside a cracked side prism at the base of the climb. Breaking it reveals a compact bounce route back upward.

### Beat 6 — Garden Runoff: 72–90 seconds

- A gentle downhill curve.
- Glints form a high-speed line.
- Two breakable arches reward continuous Burst.
- Final jump passes through a large ring into the Bloom Gate.
- The garden flowers open in sequence behind Mote.

## 21.4 Hazards

- One Rollfin
- One rotating bar near a wide platform
- Falling is recoverable in most sections
- No high-pressure chase

## 21.5 Visual landmark

A giant layered geometric flower sits behind the Bloom Gate and gradually opens as Seeds are collected.

## 21.6 Completion animation

- Mote lands in the Gate.
- Petals flare outward.
- Garden colors brighten.
- Collected Seeds add visible outer rings.
- Camera arcs for no more than 3.5 seconds.
- Results appear immediately.

---

# 22. Level 2 — Prismrush Cascade

## 22.1 Purpose

Level 2 is the more energetic, forward-focused course. It tests chaining, lane choice, and recovery under pressure.

## 22.2 Target timings

| Player | Target |
|---|---:|
| Skilled direct route | 50–60 seconds |
| Typical first clear | 75–95 seconds |
| Full collectible route | 100–130 seconds |

## 22.3 Structure

The level descends through a crystalline mountain channel. A geometric storm called the **Shatterwake** pursues the player during the middle section. It is an environmental hazard, not a character or boss.

### Beat 1 — Cascade Entry: 0–12 seconds

- Immediate downhill runway.
- Glints form a curving slalom.
- One brittle wall rewards Burst.
- A low tunnel teaches releasing Glide and staying grounded.
- Camera uses a wider follow angle.

### Beat 2 — Split Falls: 12–28 seconds

- Course splits into three lanes.
- Center is safest but has a rotating bar.
- Left contains extra Glints and one Rollfin.
- Right is a faster ramp requiring Burst.
- Lanes reconnect at a Bounce Bloom.

**Bloom Seed 1:** Above the right ramp, requiring Burst into Glide.

### Beat 3 — Falling Steps: 28–42 seconds

- Large tiles begin pulsing when touched.
- They fall after 0.8 seconds.
- Player must keep moving.
- One Prism Wisp fires a readable lane pulse.
- A lower recovery path prevents repeated death.

**Checkpoint 1:** After the falling steps.

### Beat 4 — Shatterwake Chase: 42–67 seconds

The Shatterwake forms behind the player as a wall of disassembling prisms.

Rules:

- It advances along the course spline.
- It is slower than normal running speed.
- Colliding with it causes immediate checkpoint respawn and a time penalty.
- It does not use dynamic physics.
- It cannot overtake a player who continues moving and performs ordinary jumps.
- Glints clearly mark the intended line.
- Camera moves to a course-guided chase framing.

Obstacle sequence:

1. Burst through brittle gate.
2. Jump across two moving platforms.
3. Glide through a wide ring.
4. Land on a ramp.
5. Burst under a closing arch.
6. Bounce over a broken bridge.

**Bloom Seed 2:** On a short side platform during the chase. Collecting it costs approximately two seconds but remains safe with competent movement.

**Checkpoint 2:** Immediately after escaping through a sealing prism door.

### Beat 5 — Corkscrew Canopy: 67–82 seconds

- Camera returns to follow mode.
- Course wraps around a central crystal.
- Three Burst Targets form an ascending arc.
- Missing one lands on a slower inner path.
- A Prism Wisp adds one timed pulse.

**Bloom Seed 3:** At the top of the complete Burst Target chain.

### Beat 6 — Final Rush: 82–100 seconds

- Steep downhill ribbon.
- Moving platforms rotate into place before the player arrives.
- Continuous Glint chain.
- Two Rollfins cross the route but can be Burst through.
- Final long jump uses Jump → air Burst → Glide.
- Bloom Gate opens like an iris.

## 22.4 Shatterwake visual design

The Shatterwake is created from:

- Reused prism fragments
- Darkened level colors
- Forward-moving fog plane
- Wind-line particles
- Low synthesized pulse
- Slight camera FOV increase

It must remain readable and must not create intense flashing.

## 22.5 Completion animation

The storm reaches the gate and dissolves into colorful petals. The central mountain crystal unfolds into a giant pinwheel form.

---

# 23. Level Design Quality Rules

## 23.1 Route rules

- The primary route is always readable through Glints, shape language, and camera framing.
- Optional routes reconnect quickly.
- Secrets must be visible or hinted at before they become unreachable.
- A player should never wait more than one second for a moving platform.
- No mandatory blind jumps.
- No jump should depend on camera manipulation during takeoff.
- No obstacle should require a control introduced less than three seconds earlier.
- Failure recovery should take less than five seconds.
- Each level must contain at least three moments where movement actions chain naturally.

## 23.2 Difficulty rules

- Level 1 introduces; Level 2 combines.
- Hazards remove health but do not produce long interruptions.
- A first-time player should finish each level in no more than five attempts.
- A skilled player should be able to complete both without slowing down.
- Full collectible completion may be meaningfully harder than simply reaching the end.

## 23.3 Playtest questions

For every section ask:

1. Is the destination visible?
2. Is the intended action obvious?
3. Does success feel good even without a collectible?
4. Does failure teach the player something?
5. Is recovery fast?
6. Does the camera help?
7. Is there a faster or more expressive line for skilled play?
8. Is the section visually distinct from the preceding one?

---

# 24. Game Flow and State Machine

```c
typedef enum GameMode {
    MODE_BOOT,
    MODE_TITLE,
    MODE_LEVEL_SELECT,
    MODE_LEVEL_INTRO,
    MODE_PLAYING,
    MODE_LEVEL_COMPLETE,
    MODE_RESULTS,
    MODE_PAUSED
} GameMode;
```

Transitions:

- Boot → Title
- Title → Level Select
- Level Select → Level Intro
- Level Intro → Playing
- Playing → Paused
- Playing → Level Complete
- Level Complete → Results
- Results → Retry / Level Select / Other Level

Level Intro lasts no more than 1.5 seconds and may simply display the level name over a camera view.

---

# 25. Save Data

Store:

- Magic/version
- Completion bit for each level
- Best time per level
- Best Glint count per level
- Bloom Seed bitmask per level
- Options
- Checksum

```c
typedef struct SaveData {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t best_time_ms[2];
    uint8_t best_glints[2];
    uint8_t seed_masks[2];
    uint8_t volume_master;
    uint8_t volume_music;
    uint8_t volume_sfx;
    uint8_t camera_sensitivity;
    uint8_t option_flags;
    uint32_t checksum;
} SaveData;
```

Failure to read save data must fall back safely to defaults.

---

# 26. Technical Architecture

## 26.1 Module layout

Recommended source structure:

```text
src/
  main.c
  game.c
  game.h
  platform.h
  platform_raylib.c
  platform_win32.c
  math3d.c
  math3d.h
  render.c
  render.h
  meshgen.c
  meshgen.h
  audio.c
  audio.h
  synth.c
  synth.h
  input.c
  input.h
  player.c
  player.h
  camera.c
  camera.h
  collision.c
  collision.h
  objects.c
  objects.h
  enemies.c
  enemies.h
  particles.c
  particles.h
  levels.c
  levels.h
  level_petalgarden.c
  level_prismrush.c
  ui.c
  ui.h
  save.c
  save.h
  debug.c
  debug.h
```

The final release may use unity compilation to improve optimization and reduce binary overhead.

## 26.2 Fixed-step simulation

- Simulation step: 1/120 second preferred, 1/60 acceptable after testing.
- Rendering interpolates between simulation states.
- Audio runs independently in the callback.
- Clamp accumulated simulation time after long stalls.
- Game behavior must not depend on monitor refresh rate.

## 26.3 Memory

- Fixed-capacity arrays for objects, enemies, particles, collision shapes, and audio voices.
- Arena allocator permitted during level initialization.
- No allocation during player update, rendering, collision, or audio callback.
- Debug builds must detect pool overflow.
- Release build may omit diagnostic strings.

## 26.4 Suggested capacities

| Resource | Maximum |
|---|---:|
| Render instances | 2,048 |
| Collision shapes | 512 |
| Gameplay objects | 256 |
| Enemies | 32 |
| Particles | 1,024 |
| Audio voices | 32 |
| Dynamic lights | 0 |
| Camera zones | 32 |
| Trigger volumes | 64 |

## 26.5 Rendering batches

Batch by:

- Mesh type
- Material or palette index
- Transparency class

Avoid one draw call per decorative object where practical.

---

# 27. Build Configuration and Size Control

## 27.1 Development build

- Full assertions
- Debug overlay
- Collision visualization
- Camera visualization
- Section warp controls
- Object counters
- Current player state
- Frame and simulation timing
- Package-size report
- Seed and level info

## 27.2 Release build

- `-Os` or compiler equivalent
- Link-time optimization
- Function and data section garbage collection
- Strip symbols
- Disable logging
- Remove debug menus and text
- Disable unused library features
- Unity build if it reduces size
- Avoid exception or C++ runtime dependencies
- Prefer one executable
- Verify on a clean target machine

## 27.3 Automated size report

Every release build must print:

```text
Executable bytes:
Additional package bytes:
Extracted total:
Bytes remaining:
PASS / FAIL:
```

The build must fail when extracted total exceeds 1,474,560 bytes.

## 27.4 Dependency audit

The final build must be tested with:

- Dependency inspection tool
- Clean Windows environment
- No developer SDK installed
- No raylib DLL beside executable unless included in size
- No hidden runtime download
- Static C runtime where practical and measured

---

# 28. Performance Requirements

| ID | Requirement |
|---|---|
| PERF-001 | Maintain 60 FPS at 1280×720 on a modest integrated GPU representative of common contest hardware. |
| PERF-002 | Fixed simulation must remain stable during temporary frame drops. |
| PERF-003 | Startup to title screen must take less than two seconds. |
| PERF-004 | Level restart must take less than 0.5 seconds. |
| PERF-005 | Shader failure must fall back to a basic material rather than a blank screen. |
| PERF-006 | Audio callback underruns must not occur under normal load. |
| PERF-007 | Maximum normal memory usage should remain below 128 MB, with a stretch target below 64 MB. |

---

# 29. Accessibility and Comfort

- Camera sensitivity control
- Vertical inversion
- Reduced effects mode
- No mandatory audio timing
- No rapid full-screen flashes
- Color and shape both communicate hazards
- Generous input buffering
- Controller and keyboard parity
- Pause at any time outside the completion animation
- Completion animation skippable
- Text kept large enough for 720p
- Chase remains beatable without perfect movement

---

# 30. Functional Requirements

## 30.1 Movement

| ID | Requirement |
|---|---|
| MOV-001 | Player can run, jump, glide, Burst, and bounce. |
| MOV-002 | Jump supports coyote time, buffering, and variable height. |
| MOV-003 | Glide caps fall speed and improves forward control. |
| MOV-004 | Air Burst is available once per airtime and recharges on landing or approved objects. |
| MOV-005 | Burst breaks brittle objects and defeats vulnerable enemies. |
| MOV-006 | Movement remains deterministic at fixed simulation step. |
| MOV-007 | Player can recover from small platform-edge misses. |

## 30.2 Levels

| ID | Requirement |
|---|---|
| LVL-001 | Game contains exactly two finished levels. |
| LVL-002 | Typical first completion time is 60–90 seconds per level. |
| LVL-003 | Each level contains 30 Glints and 3 Bloom Seeds. |
| LVL-004 | Each level has a unique palette, music track, landmark, and pacing profile. |
| LVL-005 | Level 1 teaches all required controls through play. |
| LVL-006 | Level 2 includes the Shatterwake chase. |
| LVL-007 | Levels are authored as compact code data. |
| LVL-008 | Procedural decoration cannot alter critical collision or route timing. |

## 30.3 Presentation

| ID | Requirement |
|---|---|
| VIS-001 | No external textures or imported models are required. |
| VIS-002 | Character, enemies, scenery, and effects are generated from geometric primitives. |
| VIS-003 | Each gameplay object has consistent shape and color language. |
| VIS-004 | Mote has readable anticipation, action, and recovery poses. |
| VIS-005 | Final Bloom Gate sequence reflects Bloom Seeds collected. |

## 30.4 Audio

| ID | Requirement |
|---|---|
| AUD-001 | Music and SFX are synthesized at runtime. |
| AUD-002 | Two distinct level tracks are included. |
| AUD-003 | Audio scheduling is independent of frame rate. |
| AUD-004 | Volume categories are configurable. |
| AUD-005 | No audio files are required in the final package. |

## 30.5 Progression

| ID | Requirement |
|---|---|
| PROG-001 | Best results persist across launches. |
| PROG-002 | Both levels are selectable from the beginning. |
| PROG-003 | Retry from results requires one input. |
| PROG-004 | Full Bloom and Flow Run accomplishments are displayed. |

---

# 31. Debug and Authoring Tools

Debug-only tools are mandatory because the levels are coded rather than edited visually.

## 31.1 Debug overlay

Show:

- FPS
- Simulation time
- Player position and velocity
- Grounded state
- Camera mode
- Collision count
- Object count
- Particle count
- Audio voice count
- Current level section
- Current checkpoint
- Current package-size estimate if available at build time

## 31.2 Section warp

Developer keys move the player to:

- Each named level beat
- Each checkpoint
- Each Bloom Seed
- Final gate

## 31.3 Free camera

Debug-only free camera for inspecting:

- Collision alignment
- Decorative placement
- Camera zones
- Occlusion
- Shortcuts

## 31.4 Recording test runs

A compact input recording and replay system is recommended.

Uses:

- Regression testing
- Camera comparison
- Performance profiling
- Ensuring authored routes remain viable after tuning
- Capturing benchmark runs

It must be removable from the final release if size is material.

---

# 32. Testing Strategy

## 32.1 Automated tests

Where practical, test:

- Vector and matrix math
- Seed determinism
- Quantized level-data decoding
- Save checksum
- Player state transitions
- Jump buffer and coyote windows
- Burst recharge
- Collision sweeps against boxes and ramps
- Results-time penalty calculation
- Package size

## 32.2 Collision test room

Create a debug-only test level containing:

- Flat ground
- Steps
- Ramps
- Narrow ledges
- Moving platform
- Low ceiling
- Wall corner
- Burst-speed wall
- Bounce surface
- Falling platform

The character controller is not accepted until it behaves correctly here.

## 32.3 Playtest groups

At minimum:

- Developer or expert runs
- Platformer-experienced player
- Player unfamiliar with 3D platformers
- Keyboard-only player
- Controller player

## 32.4 Required playtest metrics

For each tester record:

- Time to first movement
- Time to understand Burst
- Level completion time
- Falls
- Damage events
- Camera complaints
- Missed route cues
- Glints collected
- Seeds noticed and collected
- Whether they voluntarily replayed
- Favorite and least favorite section

## 32.5 Success thresholds

- 90% of testers begin moving within three seconds.
- 80% understand Jump and Burst without reading prose.
- 80% finish Level 1 within three attempts.
- 70% finish Level 2 within five attempts.
- Fewer than 10% of failures are blamed on camera or unclear collision.
- At least half of testers voluntarily retry one level to improve a result or find a Seed.

---

# 33. AI-Coder Implementation Passes

These passes sequence the work. They do not redefine or reduce the final scope. Each pass must end in a runnable build, a size report, and a short status note.

## Pass 0 — Constraint and platform spike

### Deliverables

- C99 project builds on Windows.
- Opens a window.
- Draws a representative 3D scene.
- Accepts keyboard and controller input.
- Runs one audio callback with a synthesized tone.
- Produces stripped release executable.
- Reports exact package size.
- Includes framework feature-pruning configuration.

### Exit criteria

- Representative build is comfortably below 900 KB, or the team switches immediately to the fallback platform layer.
- No external runtime file is unexpectedly required.
- Clean-machine launch is demonstrated.

## Pass 1 — Math, renderer, and mesh generation

### Deliverables

- Vector, matrix, and quaternion math
- Camera projection
- Mesh cache
- Generated primitive library
- Vertex-color material system
- Directional lighting, rim term, and fog
- Internal render target and upscale
- Blob shadows
- Debug free camera

### Exit criteria

- Scene displays Level 1 palette, trees, flowers, platforms, Mote placeholder, and particles.
- Stable 60 FPS.
- No textures or model files.
- Size remains within internal budget.

## Pass 2 — Character controller and follow camera

### Deliverables

- Capsule controller
- Fixed-step update
- Run
- Jump
- Variable height
- Coyote time
- Input buffer
- Glide
- Burst
- Bounce
- Moving platform support
- Camera follow, recenter, and collision
- Collision test room

### Exit criteria

- Movement is enjoyable for at least five minutes in the test room.
- No known critical collision or camera bugs.
- Representative chain Jump → Burst → Glide → Land works reliably.
- Keyboard and controller both tested.

## Pass 3 — Level system and Level 1 blockout

### Deliverables

- Compact level-data format
- Collision and render instances
- Triggers
- Camera zones
- Respawn and checkpoints
- All six Level 1 beats in blockout
- 30 Glint positions
- 3 Seed positions
- Bloom Gate placeholder

### Exit criteria

- Level 1 is completable from start to finish.
- Typical internal completion time is near target.
- All routes and recovery paths work.
- No decorative art is required for acceptance at this pass.

## Pass 4 — Objects, enemies, health, and results

### Deliverables

- Gameplay object set
- Three enemy types
- Health petals
- Damage and invulnerability
- Fall recovery
- Collectibles
- Result calculation
- Basic HUD
- Save data

### Exit criteria

- Level 1 includes its intended enemies and objects.
- All collectibles can be obtained.
- Restart and checkpoint behavior are reliable.
- Best results persist.

## Pass 5 — Level 2 complete blockout

### Deliverables

- All six Level 2 beats
- Course camera zones
- Falling tiles
- Lane split
- Shatterwake chase
- Checkpoints
- 30 Glints
- 3 Seeds
- Final gate

### Exit criteria

- Level 2 is complete and beatable without debug features.
- Chase is readable and fair.
- Both direct and full-collectible paths are valid.
- Typical completion time is near target.

## Pass 6 — Character, world, animation, and effects

### Deliverables

- Final Mote geometry
- Procedural poses and squash/stretch
- Petal glide animation
- Final enemy meshes
- Scenery grammar
- Level palettes
- Particles
- Burst and break effects
- Final Bloom Gate sequences
- Reduced-effects mode

### Exit criteria

- Screenshots from both levels look intentionally composed.
- Gameplay silhouettes remain readable.
- Effects do not obscure hazards or landing surfaces.
- Performance target remains satisfied.

## Pass 7 — Complete synthesized audio

### Deliverables

- Fixed audio engine
- Voice pool
- Two level tracks
- Full SFX list
- Volume controls
- Chase-layer behavior
- Completion synchronization

### Exit criteria

- No callback clicks or underruns.
- Music survives restarts and pauses correctly.
- SFX remain audible without clipping.
- Audio consumes no external assets.

## Pass 8 — Menus, UX, accessibility, and final flow

### Deliverables

- Title
- Level select
- Intro
- Pause
- Results
- Options
- Input-prompt switching
- Completion badges
- Save validation
- Skippable completion camera

### Exit criteria

- Player can launch, select either level, finish, view results, retry, and quit without debug controls.
- All options persist.
- All text fits at 720p.
- The entire game flow is clean on keyboard and controller.

## Pass 9 — Tuning, regression, size, and submission

### Deliverables

- Movement tuning
- Camera tuning
- Level pacing revisions
- Collectible-route tuning
- Novice recovery tuning
- Performance profiling
- Release configuration cleanup
- Clean-machine testing
- Final package
- Submission screenshots
- Control instructions

### Exit criteria

- Every acceptance criterion in Section 35 passes.
- Package is below preferred size target.
- No known crash, softlock, missing dependency, or severe camera defect.
- Submission is prepared at least 48 hours before deadline.

---

# 34. Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| Framework binary too large | Critical | Pass 0 size spike; prune modules; fallback platform layer |
| Movement feels generic | Critical | Finish and tune controller before content production |
| Camera causes failures | Critical | Zone-guided camera, collision, look-ahead, dedicated playtesting |
| Fully procedural layout becomes poor | High | Author collision and pacing; proceduralize presentation only |
| Two levels exceed schedule | High | Reuse systems; fixed level scope; no hub or boss |
| Visuals look noisy | High | Strict palettes, density limits, authored decor zones |
| Chase feels unfair | High | Slow pursuit speed, clear line, checkpoint before chase |
| Audio engine clicks | Medium | Sample-clock scheduling, fixed voices, gain ramps |
| Collision bugs at Burst speed | High | Swept controller and dedicated test room |
| Coder adds engine systems not needed | High | Explicit non-goals and pass exit criteria |
| Package margin disappears late | Critical | Size report every release build |
| Originality concerns | High | Original character, naming, geometry, level layouts, and audio |

---

# 35. Complete-Game Acceptance Criteria

The game is ready only when all of the following are true.

## 35.1 Compliance

- [ ] Extracted package is at or below 1,474,560 bytes.
- [ ] Preferred internal package target is met or deviation is documented.
- [ ] Game runs standalone on a clean Windows machine.
- [ ] No browser or network is required.
- [ ] All content is original.
- [ ] All required dependency and license obligations are satisfied.

## 35.2 Content

- [ ] Title screen is complete.
- [ ] Level select is complete.
- [ ] Petalstep Gardens is complete.
- [ ] Prismrush Cascade is complete.
- [ ] Each level has 30 Glints.
- [ ] Each level has 3 Bloom Seeds.
- [ ] Each level has a completion sequence.
- [ ] Both music tracks and all required SFX are present.
- [ ] Results and persistence work.

## 35.3 Gameplay

- [ ] Run feels responsive.
- [ ] Jump buffering and coyote time work.
- [ ] Glide is useful and readable.
- [ ] Burst works on ground and in air.
- [ ] Burst correctly breaks objects and defeats enemies.
- [ ] Bounce chains work.
- [ ] Checkpoints and falls work.
- [ ] Health and damage work.
- [ ] Level 1 teaches required controls.
- [ ] Level 2 chase is fair.
- [ ] No mandatory blind jumps.
- [ ] Both levels meet approximate duration targets.

## 35.4 Camera and collision

- [ ] No repeatable camera clipping that hides the player.
- [ ] Camera input and recenter work.
- [ ] Course-camera transitions are smooth.
- [ ] No known collision tunneling at maximum speed.
- [ ] Moving platforms are stable.
- [ ] Slopes and steps behave consistently.
- [ ] No known softlocks.

## 35.5 Presentation

- [ ] Mote has a unique and readable silhouette.
- [ ] Level palettes are distinct.
- [ ] Gameplay objects use consistent visual language.
- [ ] Effects are attractive without obscuring play.
- [ ] Reduced-effects mode works.
- [ ] HUD is legible.
- [ ] Completion animations are skippable.

## 35.6 Technical

- [ ] Stable fixed-step simulation.
- [ ] Stable 60 FPS on target hardware.
- [ ] No audio underruns.
- [ ] Startup is under two seconds.
- [ ] Restart is under 0.5 seconds.
- [ ] Save corruption fails safely.
- [ ] Release build contains no debug-only systems or strings that materially waste size.
- [ ] Final release package has been opened and tested after packaging.

---

# 36. Instructions to the AI Coding Agent

1. Treat this document as the source of truth.
2. Implement the complete game in the ordered passes.
3. Do not quietly replace the game with a 2D platformer, endless runner, tech demo, or randomly generated obstacle course.
4. Do not add a third level, hub, boss, dialogue, inventory, or copy-ability system.
5. Keep a playable build at the end of every pass.
6. Report exact release executable and package sizes after every pass.
7. Build and validate movement, collision, camera, and binary size before spending time on decorative systems.
8. Preserve deterministic level behavior.
9. Use fixed-size pools and avoid runtime allocation in gameplay and audio paths.
10. Do not import art, music, models, textures, or fonts from existing franchises.
11. Favor small, legible systems over general engine architecture.
12. Add debug authoring tools in debug builds, but guarantee they can be removed from release.
13. When a requirement conflicts with size, performance, or schedule, preserve in this order:
    - Fun movement
    - Two complete levels
    - Camera and collision reliability
    - Standalone execution
    - Package-size compliance
    - Visual polish
    - Optional effects
14. Never sacrifice completion for a speculative rendering feature.
15. Do not declare the project finished until every item in Section 35 has been evaluated.

---

# 37. Final Product Summary

**POLYBLOOM** is a deliberately small but complete 3D platformer.

It combines:

- Responsive jumping
- A fast Burst
- A forgiving flower-like glide
- Momentum-focused routes
- Optional collectible challenges
- A bright exploratory first level
- A faster obstacle-driven second level
- A wholly geometric procedural art style
- Runtime-generated music and sound
- A compact native C implementation

Its procedural systems create abundance on screen, while its level layouts remain human-designed and carefully paced. The game should feel like a forgotten 3D mascot platformer from an impossible tiny console—playful, polished, immediately readable, and astonishingly small.

---

# 38. Verified External Basis

This specification was prepared against the official 2P Game Arcade contest page as available on July 21, 2026. The official page states:

- Maximum extracted package size: 1,474,560 bytes
- Engine choice is unrestricted, but executable and runtime count toward the limit
- The game must be standalone
- Browser games are prohibited
- Internal compression is allowed
- Judging emphasizes completion, size compliance, and fun
- Published deadline: September 4, 2026, with conflicting page text showing 11:59 PM and 23:39; this specification therefore requires early submission

The technical recommendation uses raylib only as a measured and aggressively pruned option. raylib is written in C, supports 3D and OpenGL, includes its dependencies, and exposes build-time configuration for removing unused functionality. It is not assumed to fit automatically; Pass 0 must prove the actual release size.
