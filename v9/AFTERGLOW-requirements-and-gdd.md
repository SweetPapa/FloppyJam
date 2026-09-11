# AFTERGLOW (working title)
### Requirements Document + Creative Game Design Document
**Sweet Papa Technologies — v0.1 — Sept 5, 2026**

> A cozy 3D night-time mystery in a chalk-drawn harbor town, where the characters are played by a local LLM and the puzzles are the kind you'd do at the kitchen table.

---

## Part 1 — Overview

### 1.1 One-line pitch
You're the new night courier in **Brightwater**, a small harbor town drawn in glowing pastel chalk on a black sky. Five nights before the town's Lantern Night festival, the lighthouse has gone dark — and someone calling themselves *the Lamplighter* has been secretly borrowing, fixing, and returning things all over town. Walk the rooftops and rails, talk to everyone, solve their puzzles, and figure out who the Lamplighter is before the festival.

### 1.2 What this game is
- A **third-person 3D adventure** with light, joyful traversal (Sonic Adventure hub-world energy: slopes, rails, springs — never precision platforming).
- A **cozy mystery** with a fixed, authored solution and five suspects, each cleared by learning a harmless, sweet secret.
- A **puzzle collection** built from the genres Mom already likes: phrase guessing, color-stacking, drilling, light-routing, and a Codenames-style word game played *with* an AI partner.
- An **LLM-powered cast**: every character is voiced by any OpenAI-compatible model (local Ollama/LM Studio/llama.cpp/vLLM, or cloud), improvising within a hard-authored story bible.

### 1.3 What this game is not
- Not an RPG. No stats, inventory management, leveling, combat, or dialogue-tree grinding.
- No magic, mythology, ghosts, monsters, villains, or peril. Nobody is in danger. The worst thing that can happen is a rained-on chalk menu.
- Not a typing game. Typing is *optional* in every conversation.
- Not an asset-based game. Zero external art, audio, fonts, or text files. Everything is generated in code or by the LLM at runtime.

### 1.4 Design pillars
1. **Kitchen-table puzzles in a beautiful world.** Every puzzle is one Mom would recognize from a magazine, a board game, or a Genesis cartridge.
2. **One hand, no hurry.** Arrow keys + Enter finishes the whole game. Nothing is timed by default. Nothing is missable.
3. **Deterministic mystery, improvised voices.** The LLM makes people feel alive; it never decides what's true.
4. **Every night ends warmer than it started.** Each chapter resolves with someone's kindness revealed, and the town gets a little brighter (literally — lights and music layers turn on).
5. **Chalk on black.** One consistent, striking look that's cheap to render and gorgeous in rain and snow.

### 1.5 Target players
| | **Mom** | **FoFo** |
|---|---|---|
| Wants | Word & physical puzzles, a story she can follow, no stress, pretty | Movement that feels good, music, a little speed, a system to admire |
| Avoids | Menus, jargon, fast reflexes, mouse-look, anything spooky | Long cutscenes, RPG busywork |
| Design answer | Relaxed mode (default), suggested replies, hint tiers, auto-camera | Lively mode toggle, rails/slopes, rhythm side-activity, layered synth score |

### 1.6 Predecessors and what changes
- **COLLAPSE** proved LLM NPCs with trust-gated knowledge, and taught the hard lesson: an NPC that invents a person or place breaks a mystery. AFTERGLOW inherits **entity validation + knowledge gates** as non-negotiable requirements, but simplifies trust into a light "warmth" model.
- **The floppy games** (THE GAUNTLET, MagLava 3D, the minigolf×pool game, FoFoTetros Cubes, Phrasey) proved procedural music and puzzle feel. Several are reprised here as puzzles or side-attractions.
- Size cap is lifted. Asset ban stays.

---

## Part 2 — Creative Game Design

### 2.1 Setting: Brightwater
A harbor town on a hill, drawn as pastel-neon chalk lines on a black night. It is always night in AFTERGLOW; the town *is* the light. Buildings glow at their edges, windows are warm rectangles, lampposts throw soft cones, and wet streets reflect it all. The weather is part of the story — each night has its own sky.

**Districts** (each is a chapter, arranged around the central hub):

| District | Feel | Anchor building | Chapter |
|---|---|---|---|
| **Harbor Square** (hub) | Café tables, string lights, the courier depot | Odette's Bakery, Courier Depot, Town Hall | Night 1 |
| **Cannery Row** | Docks, cranes, stacked crates, boats bobbing | Bo's Dock Office, the Boatyard | Night 2 |
| **The Undercroft** | Old brick service tunnels under the hill, lantern-lit | Pearl's Tunnel Workshop | Night 3 |
| **Hilltop Gardens & Observatory** | Terraced gardens, greenhouse, a domed observatory | The Observatory | Night 4 |
| **The Point** | A windy spit of land, the dark lighthouse at the end | The Lighthouse | Night 5 + Finale |

Traversal spokes (rails, slides, springs) connect the districts back to the hub so getting anywhere takes under a minute and is fun on the way.

### 2.2 The cast
Every character has a **public face**, a **harmless secret** (revealed in their chapter), and a **clue** they hold toward the real mystery. Names are short and distinct on purpose.

| Character | Role | Public face | Secret (revealed) | Clue toward the Lamplighter |
|---|---|---|---|---|
| **Winnie Tallow** | Mayor | Cheerful, over-committed, wants Lantern Night saved | Has been quietly paying for the festival out of pocket for years | Sets the mystery; gives the case board |
| **Ferris** | Courier dispatch / "Night Owl Radio" host | Warm radio voice heard around town; your boss | None — he's the narrator/hint voice | Hints, recaps, weather |
| **Odette Marchetti** | Baker, Harbor Square | Chatty, kind gossip, knows everyone | Writing a cookbook of the town's family recipes as a surprise | The chalk lanterns are always drawn by a **left-handed** person (smudge direction) |
| **Bo Kalani** | Dockmaster, Cannery Row | Big, calm, few words | Secretly taking singing lessons to perform at Lantern Night | The "missing" crates were **brass fittings addressed to the boatyard** |
| **Pearl Okafor** | Tunnel engineer, the Undercroft | Precise, funny, loves her tunnels | Building a model train of the whole town down there | Someone has been **moving heavy things through the tunnel at night**; a dropped **boatyard-stamped wrench** |
| **Prof. Idris Haddad** | Astronomer, Observatory | Absent-minded, delighted by everything | Hiding boxes of telescope parts for a surprise kids' star party | Through the telescope: **light flickering in the lighthouse lamp room at 3 a.m.** — someone testing |
| **Gus Fenwick** | Retired lighthouse keeper, the Point | Gruff, sweet underneath; believes Lantern Night is over since the lens cracked | He's been sad, not stubborn | Knows the lens is "beyond fixing" — which is the point |
| **Ada Fenwick** | Boatyard mechanic, Gus's granddaughter | Shy, capable, left-handed, always has grease on her hands | **She is the Lamplighter.** She's been secretly rebuilding the cracked lighthouse lens from borrowed parts to surprise Gus, and fixing everyone's things as thanks for what she borrowed | — |
| **Rosie** | Kid, runs the Chalk Mural Club | Bossy, enthusiastic | None | Side quests: collect chalk, fill murals |

**The player** names themself (default: "Kit"). No backstory beyond: you moved to Brightwater last week to take the night courier job.

### 2.3 Story summary
**Setup.** Lantern Night is the one night a year the whole town lights the harbor and the lighthouse beam sweeps the bay. This year the lens is cracked and Gus has given up. Meanwhile, objects around town vanish at night and reappear days later repaired, each with a small chalk lantern drawn nearby. Winnie asks the new courier — who's out at night anyway — to find the Lamplighter, because whoever it is might be able to save the lens too.

**Nights 1–4.** Each night, Ferris gives you a delivery route into a new district. Delivering the parcel means meeting that district's character, who needs help with a puzzle (their thing is broken/scrambled/blocked because the Lamplighter borrowed something). Solving it earns their trust; they share their secret (clearing them as a suspect) and one real clue. At the end of the night you pin the clue on the **case board**.

**Night 5.** The clues point to the boatyard and the lighthouse. At the Point you find Ada in the lamp room with a half-rebuilt lens. She's been keeping notes using Gus's old signal-word game. You play it with her (the Codenames-style puzzle), she confesses, and you convince her the town would rather help than be surprised.

**Finale — Lantern Night.** Everyone brings a piece: Bo's brass, Pearl's tunnel cart, Idris's spare prism, Odette's… bread. Gus turns the crank. The beam comes on. You do **the Lighting Run** — a joyful rooftop-and-rail run through every district lighting lanterns, each district adding a layer to the music, ending at the harbor with the whole town lit and Bo singing. Credits roll as chalk drawings.

**Why this works for Mom:** there's no bad guy, every suspect turns out to be lovely, the mystery is solvable from clues she literally pins on a board, and the ending is a party.

### 2.4 Chapter breakdown

| Night | Weather | Delivery | Character | Signature puzzle | Traversal beat | Clue | Music layer added |
|---|---|---|---|---|---|---|---|
| 1 | Clear, still | Flour to Odette | Odette | **Chalk Menu** (phrase guessing) | Tutorial: slopes, first rail, springs | Left-handed | Bass + pads |
| 2 | Drizzle | Manifest to Bo | Bo | **The Stack** (color-match crate stacking) | Crane rail over the docks, wet-slide | Brass fittings → boatyard | Drums |
| 3 | Steady rain | Lamp oil to Pearl | Pearl | **The Descent** (drilling) | Tunnel slides, minecart rail | Wrench, night tunnel traffic | Arps |
| 4 | Snow | Package to Idris | Idris | **Light Routing** (mirror grid) | Garden terraces, greenhouse spring tower | 3 a.m. light in the lamp room | Lead melody |
| 5 | Clearing, stars | "Return to sender" — to Ada | Ada, Gus | **Signal Words** (Codenames with an AI partner) | The Point's cliff rail | Confession | Choir/bells |
| Finale | Clear, starlit | — | Everyone | **The Lighting Run** | All spokes, back-to-back | — | Everything |

Target play time: 4–6 hours relaxed, ~3 lively. Each night is a natural stopping point with an auto-save.

### 2.5 Core loops

**Night loop (30–60 min):** Radio intro → walk/rail to the district → deliver → talk → puzzle (3 rounds, rising difficulty) → secret + clue → case board → lights come on in that district → optional wander/side stuff → sleep.

**Conversation loop (1–3 min):** Approach a character → they speak (LLM, streamed as chalk handwriting) → you answer by picking one of **three suggested replies** with arrows + Enter, or optionally typing → repeat. Conversations naturally end when the character has nothing new; the journal records anything important as a **fact card** (authored text, not LLM text).

**Puzzle loop (5–15 min):** Rules explained by the character in two sentences → Round 1 (easy) → Round 2 → Round 3 → celebration. Hints are always one keypress away.

**Deduction loop (2 min, end of each night):** The case board shows suspects and today's clue. Drag-free: pick a clue, pick a suspect, Enter. Wrong pins get a gentle nudge from Ferris ("Hmm, check Odette's fact card again"). On Night 5 you're asked outright: *Who is the Lamplighter?*

### 2.6 Puzzle catalog

All puzzles share these rules:
- **Input:** arrows to move a cursor/piece, Enter to confirm/rotate/drop, Esc to pause. That's it. Mouse works too but is never needed.
- **Relaxed mode (default):** no timers, no lives, pieces wait for you. **Lively mode:** gravity, timers, score.
- **Hint tiers:** H (1) nudge, H again (2) point at the spot, H again (3) do one step for you. After tier 3, a **"Show me"** button appears that auto-solves the round. Nobody gets stuck. The character delivers hints in their own voice (LLM phrasing over an authored hint fact).
- **Rounds:** three per puzzle, then optional **Endless** mode from the town square arcade after the chapter.

**P1 — Chalk Menu** (Night 1, Odette) — *Phrasey / hangman family*
Odette's menu board got rained on. A hidden phrase shows as chalk blanks with a category ("Pastry", "Town saying"). A letter wheel at the bottom; arrows spin it, Enter guesses. Correct letters chalk themselves in with a satisfying scratch; wrong letters dim one of five lanterns (relaxed: lanterns just dim, no fail). Rounds: 2-word → 3-word → 5-word with a pun. Phrases are authored (~60) and tagged by round; Endless mode may use LLM-generated phrases *only* after dictionary + length + profanity validation, with authored fallback.

**P2 — The Stack** (Night 2, Bo) — *Mean Bean Machine / FoFoTetros family*
A dock lane pre-filled with tangled colored crates (4 pastel colors). Pairs of crates descend; arrows move/rotate, Enter drops. Four or more same-color connected crates pop with a chalk-dust burst; chains cascade. Goal per round: clear the lane so Bo's boat can dock. Relaxed: pieces hover until dropped. Rounds add a fifth color and a taller starting pile.

**P3 — The Descent** (Night 3, Pearl) — *Mr. Driller family*
Drill straight down through a shaft of colored blocks to reach a fallen crate at the bottom. Drilling a block clears every connected block of the same color; unsupported blocks fall and can pin you (relaxed: they just push you sideways, no crush). Your lantern oil is the air meter; oil cans are scattered through the shaft (relaxed: oil never runs out, cans just sparkle). Depth increases per round; round 3 adds "brick" blocks that need 3 hits.

**P4 — Light Routing** (Night 4, Idris) — *pipe-dream / mirror-puzzle family*
A grid of tiles: mirrors, splitters, lenses, walls. A beam enters from one edge; rotate tiles (arrows move cursor, Enter rotates) so the beam reaches every telescope eyepiece. Beam renders live as you rotate, so it's exploratory rather than punishing. Rounds: 1 target → 2 targets with a splitter → 3 targets on a 6×6 with colored beams that must reach matching eyepieces.

**P5 — Signal Words** (Night 5, Ada) — *Codenames family, solo with an AI partner*
Gus's old signal-lamp game: 25 words on a 5×5 board. Nine are "beacons," one is the "foghorn," the rest are "fog." Ada (the LLM) is your spymaster: she gives a one-word clue and a number ("HARBOR, 2"). You pick words with arrows + Enter, one at a time; a beacon lights up, fog ends your turn, the foghorn just makes Ada wince and costs a turn (never a loss). Win when all nine beacons are lit. Three boards, each with a themed word set (sea, kitchen, sky) that references things you've seen in town.
LLM contract: the game sends Ada the full key (beacons/fog/foghorn) and asks for `{clue, count, targets[]}`. Validation is deterministic: clue must be a single dictionary word, not on the board, not a substring/superstring of a board word, and `targets` must be beacons; otherwise regenerate (max 2) then fall back to an authored clue for that board state. Small models do fine here because the task is narrow.

**S1 — Harbor Putt** (optional side attraction, Harbor Square, unlocks Night 2) — the minigolf×pool game reprised in the chalk style: aim with arrows, power with Enter hold-and-release (or two taps in relaxed mode), nine holes on the pier.

**S2 — Lantern Rhythm** (optional, every district) — lanterns light in time with the music; in relaxed mode any press near the beat counts, in lively mode timing windows tighten and a combo adds a music layer. This is the FoFo-facing candy; it never gates story.

**S3 — Chalk Murals** (optional, Rosie) — find chalk sticks hidden around town (traversal collectibles); each district has a blank wall that fills in a procedural mural as you turn in chalk. Cosmetic reward: your lantern trail color.

### 2.7 Traversal design ("Sonic Adventure, but kind")
- **Auto-run** at a brisk, pleasant speed. No sprint button.
- **Jump** exists (Enter/Space) but is never required: gaps on the story path are bridged by **assist hops** (auto-jump when you run off a marked ledge) and every high place has a spring or rail.
- **Rails** (ziplines, crane cables, tunnel minecart, cliff rail): run into one to ride it; balance is automatic; you can press left/right to lean for a spark shower (cosmetic).
- **Slopes and wet-slides**: momentum carries you; on rain nights, streets shimmer and you slide a little further. Nothing is lost by sliding wrong.
- **Springs, bounce awnings, spinning-door launches**: single-input, forgiving.
- **No fall damage, no drowning.** Fall in the harbor → a rowboat fishes you out with a Ferris one-liner and drops you on the nearest dock.
- **Camera**: automatic follow with cinematic set-pieces on rails. Q/E nudge the camera; the mouse can orbit but is never required.
- **Waypoint**: the delivery destination glows on the skyline and a chalk arrow on the ground points the way when you stand still for 3 seconds. Nobody gets lost.

### 2.8 Conversation design (player-facing)
- Characters walk up and greet you when you're near; a chalk speech bubble streams their line letter by letter (the chalk-scratch sound is the "voice").
- **Three suggested replies** appear as chalk cards: typically one *warm*, one *curious*, one *about the case*. Arrows + Enter. A fourth, dimmer card — "Say something else…" — opens a text field for anyone who wants to type. Typing is never required or advantaged.
- Characters remember what you've said within the night (short summary carried per character), and warmth builds with kindness. Warmth is invisible to the player; it changes tone and how soon a character volunteers their clue, never whether they do.
- Important information is **never only in LLM prose**: the moment a clue is revealed, an authored **fact card** slides into the journal with a chalk-stamp sound. The LLM said it; the card makes it canon.
- Ferris on the radio fills the town with ambient chatter — weather, a joke, a recap when you load a save ("Previously, in Brightwater…"). Recaps are LLM-written from the authored fact list, so they're fresh but always true.
- Every character has a distinct two-note synth "mumble" and speech cadence so Mom can tell who's talking without reading a name.

### 2.9 Art direction
- **Canvas:** pure black. Nothing is ever drawn "lit"; things are drawn *glowing*.
- **Palette (6 pastels + white):** mint, peach, lavender, butter, sky, rose; chalk white for the player and UI. Each district and each character owns one accent color. Fog and rain are desaturated versions.
- **Chalk rendering:** geometry drawn as edge strokes with slight jitter and variable width; fills as faint hatching that brightens with proximity; soft bloom on everything; visible "chalk dust" particles on movement, pops, and reveals. Strokes have a subtle hand-drawn wobble that re-samples every few frames so the world feels alive, not vibrating.
- **Weather** (procedural, per night): clear (stars drift, lantern halos), drizzle (short streaks, tiny ripples), rain (longer streaks, puddle reflections, gutters running), snow (soft dots with sine wobble, accumulating rims on rooftops and rails, footprints). Weather changes are story beats and get a slow crossfade.
- **Characters:** simple capsule bodies, line-drawn faces with 4–6 expressions, one signature accessory (Odette's apron, Bo's cap, Pearl's headlamp, Idris's scarf, Ada's tool belt). Idle animations are tiny loops. The player is chalk-white with a lantern whose trail color is the cosmetic reward.
- **Procedural town:** a fixed campaign seed generates the layout; district templates place the authored anchor buildings; everything else (houses, boats, lampposts, trees, crates, string lights) is generated. "New Town" in settings reshuffles the filler only; anchors and spokes never move.
- **UI:** chalk on black, big type, no icons without labels. Journal is a spiral notebook; case board is a corkboard with string.

### 2.10 Audio direction (all synthesized)
- Lo-fi, warm, slow synth score; each district lit adds a layer (see chapter table); layers persist for the rest of the game. Finale plays everything.
- Rain/snow/wind ambience generated from filtered noise and synced to weather intensity.
- Chalk scratch for text, chalk-stamp for fact cards, soft dust-pop for puzzle clears, rail hum that rises with speed.
- Character mumbles: two-note synth phrases per character with per-syllable pitch variation.
- No TTS required (a TTS endpoint hook is an optional later extension).

### 2.11 Accessibility & comfort (a design layer, not a menu)
- Whole game completable with **arrow keys + Enter + Esc** (or WASD + Space + Esc). One-hand presets for left and right hand. Fully remappable.
- No holds longer than 0.5 s required; no double-taps; no simultaneous presses; no timed inputs in relaxed mode.
- Text size L/XL; high-contrast option raises stroke width; motion-reduce option disables camera swoops and stroke wobble.
- Auto-save at every fact card and puzzle round; resume anywhere.
- Skip any puzzle round after hint tier 3; skip any cutscene with Esc.

---

## Part 3 — Requirements

### 3.1 Functional requirements

**World & traversal**
- FR-1 Third-person 3D town, five districts + hub, generated from a seed with authored anchors; fully walkable with no loading screens between districts.
- FR-2 Auto-run movement, assist hops, rails, springs, slides as specified in 2.7; no death/fail state in traversal.
- FR-3 Automatic camera with keyboard nudge; optional mouse orbit.
- FR-4 Waypoint system: skyline marker + ground arrow after 3 s idle.
- FR-5 Per-night weather states (clear, drizzle, rain, snow) with crossfade.

**Story & progression**
- FR-6 Five nights + finale, gated only by puzzle completion (skippable via hints) and case-board pins.
- FR-7 Journal with authored fact cards; case board with suspects and clue pinning; final "Who is the Lamplighter?" deduction.
- FR-8 Radio recap on load, generated from authored facts.
- FR-9 Side content (Harbor Putt, Lantern Rhythm, Chalk Murals) never gates story.

**Puzzles**
- FR-10 Puzzles P1–P5 implemented against a shared `PuzzleHost` interface (start round, input, hint(tier), autosolve, on-complete) so they can be built and tested in isolation.
- FR-11 Relaxed/Lively mode per 2.6; hint tiers 1–3 + "Show me."
- FR-12 Endless mode from the hub arcade after each chapter.

**Conversation & LLM**
- FR-13 LLM adapter speaks the OpenAI chat-completions API (`/v1/chat/completions`, streaming); settings UI for base URL, model, optional API key, temperature, max tokens, and a **Test connection** button. Works with Ollama, LM Studio, llama.cpp server, vLLM, and cloud providers.
- FR-14 Character responses are structured: `{say, mood, reveal_clue_ids[], suggested_replies[3], end_conversation}`. JSON mode / response-format used where the server supports it; otherwise a tolerant parser with fallback.
- FR-15 **Knowledge gates:** each character card lists knowledge tiers (public / after warmth / after chapter puzzle / never). The prompt is assembled per turn from only the tiers currently unlocked; the solution is in no card except Ada's, which unlocks at Night 5.
- FR-16 **Entity validation:** every proper noun in `say` is checked against the story bible's entity list (characters, places, objects, events). Unknown entities → regenerate once → fallback authored line. Clue reveals are accepted only for IDs the character is allowed to reveal at that moment.
- FR-17 **Fallback script:** every conversation beat has authored lines and authored suggested replies. If the LLM is unreachable, times out (8 s), or fails validation twice, the authored line plays. The game must be completable start to finish with no LLM configured ("Offline mode," offered on first launch).
- FR-18 Per-character short-term memory: a rolling summary (≤120 words) of what the player said this night, regenerated by the LLM and validated the same way.
- FR-19 Signal Words spymaster contract per 2.6 (P5), with deterministic clue validation and authored fallback clues.
- FR-20 Content guard in the system prompt and post-check: no violence, romance, magic/supernatural, profanity, real-world brands or people. Violations → fallback line.

**Meta**
- FR-21 Settings: input presets, remapping, text size, contrast, motion-reduce, relaxed/lively, LLM config, New Town reshuffle.
- FR-22 Auto-save at fact cards and puzzle rounds; three save slots; resume to the exact spot.
- FR-23 Credits rendered as chalk drawings of the cast, plus the model name that voiced them.

### 3.2 Non-functional requirements
- NFR-1 **No external assets.** No image, audio, font, or text data files in the repo or build; all content is code, authored data tables (JSON/TS/whatever the stack prefers), or runtime-generated. Authored data is text in source — that's allowed; binary blobs are not.
- NFR-2 **Performance:** 60 fps at 1080p on an integrated-GPU laptop class machine with rain active; 30 fps floor on a 2018 MacBook Air. Cold start < 5 s.
- NFR-3 **LLM latency:** first streamed token visible < 1.5 s on a local 27B-class model; hard timeout 8 s to fallback; total tokens per turn ≤ 160.
- NFR-4 **Model breadth:** playable and coherent with a 4B-class local model; polished with 12B–30B; cloud models must not be required.
- NFR-5 **Privacy:** no network calls except to the user-configured LLM endpoint. No telemetry.
- NFR-6 **Determinism:** same seed → same town layout, same puzzle boards, same clue order; LLM output never affects world state except through validated `reveal_clue_ids` and `end_conversation`.
- NFR-7 **Build targets:** desktop (Windows/macOS/Linux) required; browser/WASM build strongly desired for sharing (note: a browser build calling a local LLM needs the endpoint to allow CORS — Ollama supports `OLLAMA_ORIGINS`; document this in-app on the Test-connection screen).
- NFR-8 **Size:** no cap, but stay lean out of habit; report build size in CI.
- NFR-9 **Way of the FoFo compliance:** deterministic-first; security not delegated (API key stored locally only, never logged); documentation-driven (story bible and contracts precede code); count what survived (see 3.5).

### 3.3 Content & data model (the grounding docs FOREMAN agents work from)
- **Story bible** — `bible.json`: districts, anchors, entities (every nameable thing with a one-line description), timeline of the five nights, the solution, the finale beats.
- **Character cards** — one per character: voice description, three sample lines, speech cadence, accent color, mumble notes, knowledge tiers with clue IDs, forbidden topics, secret (with unlock condition), authored fallback lines per conversation beat, authored suggested replies.
- **Clue table** — `clues.json`: id, holder, unlock condition, fact-card text (authored), which suspect it clears/implicates, case-board position.
- **Puzzle specs** — one per puzzle: rules, round definitions (boards/phrases/shafts/grids), hint text per tier per round, autosolve procedure, lively-mode parameters.
- **Phrase bank / word banks** — authored lists for P1 and P5 with tags; validation dictionary (can be a compact generated word list).
- **Prompt templates** — system prompt skeleton, per-turn assembly rules, JSON schema, the content guard, the recap prompt, the spymaster prompt.
- **World gen spec** — district templates, anchor placement rules, spoke definitions (rail start/end anchors), decoration budgets, weather parameters per night.

All of these are text in the repo, versioned, and are the single source of truth. Agents may not invent entities, clues, or story beats outside them.

### 3.4 LLM integration detail

**Per-turn prompt assembly (deterministic):**
1. System: game frame + content guard + output schema.
2. Character card: identity, voice, samples, current mood.
3. Unlocked knowledge tiers only (facts as short bullets, with clue IDs the character may reveal *now*).
4. Tonight's world state summary (authored template filled from state: weather, what's been fixed, which districts are lit).
5. Rolling per-character memory summary.
6. Last 6 exchanges verbatim.
7. Player's chosen/typed line.

**Output handling:** stream `say` for display; on completion parse JSON; run entity validation, clue-permission check, content guard; apply `reveal_clue_ids` → fact cards; render `suggested_replies` (validated: ≤ 12 words each, no forbidden content, dedupe against last turn; if invalid, use authored replies for this beat).

**Failure ladder:** invalid → regenerate at lower temperature (once) → authored fallback line + authored replies. Log every fallback with reason to a local debug log (for tuning, never uploaded).

**Model settings defaults:** temperature 0.7 (0.3 for spymaster/recap), max_tokens 160, stop on `}`. Ship a curated "known-good models" list on the settings screen with one-line notes, plus a "works with anything OpenAI-compatible" line.

### 3.5 Verification (falsifiable — if it can't fail, it isn't a test)
- V-1 A tester completes the full campaign using only arrow keys, Enter, and Esc. Any moment that requires another key fails.
- V-2 A tester completes the full campaign with the LLM endpoint pointed at an unreachable address. Any progression block fails.
- V-3 Over 200 scripted conversations against a 4B and a 27B model, the rate of turns falling back to authored lines is < 15% (4B) and < 5% (27B); the rate of *shipped* (post-validation) turns naming a non-bible entity is 0.
- V-4 Every clue reveal in those 200 conversations references a clue ID the character was permitted to reveal at that moment; any exception fails.
- V-5 Signal Words: over 100 generated clues, 0 invalid clues reach the player (validation catches them), and the authored fallback is used < 10% of the time on the 27B model.
- V-6 60 fps sustained for 5 minutes in Cannery Row during rain on the reference integrated-GPU laptop.
- V-7 `git ls-files` contains no binary assets; a CI check fails the build if any file matches image/audio/font types.
- V-8 Same seed produces byte-identical town layout and puzzle boards across two fresh installs.
- V-9 **Mom playtest:** she reaches the end of Night 2 in one sitting without asking how to do something more than twice, and says she wants to play Night 3. Anything else is a design bug, not a Mom bug.
- V-10 Save/resume from every fact card and puzzle round restores position, weather, journal, and music layers exactly.

### 3.6 Phases and parallel work packages (for FOREMAN)
Each package has a clear interface so agents can build in parallel against the grounding docs.

**Phase 0 — Grounding (sequential, first):** story bible, character cards, clue table, puzzle specs, prompt templates, world-gen spec, `PuzzleHost` and `LLMAdapter` interface definitions, coding conventions. Nothing else starts until this is committed. (Rule: triple-check everything is committed before any agent session.)

**Phase 1 — Foundations (parallel):**
- WP-1 Chalk renderer (stroke shader, bloom, dust particles, palette)
- WP-2 World generator (districts, anchors, spokes, decoration)
- WP-3 Player controller + camera + traversal elements (rails, springs, slides, assist hops)
- WP-4 Synth audio engine (layers, ambience, SFX, mumbles)
- WP-5 LLM adapter + settings UI + Test connection + failure ladder
- WP-6 Weather system

**Phase 2 — Systems (parallel):**
- WP-7 Conversation system (prompt assembly, validation, suggested replies, fact cards, memory summaries)
- WP-8 Journal + case board + deduction
- WP-9 Save/load + settings/remapping + accessibility options
- WP-10–14 Puzzles P1–P5, one agent each, against `PuzzleHost`, each with its own test harness and Endless mode
- WP-15 Side attractions S1–S3

**Phase 3 — Story wiring (sequential-ish):** night scripts, deliveries, cutscenes as chalk vignettes, radio recaps, finale Lighting Run, credits.

**Phase 4 — Verification & polish:** run V-1…V-10, tune prompts per model, Mom playtest, second Mom playtest after fixes.

### 3.7 Risks
| Risk | Mitigation |
|---|---|
| Small local models drift out of character or JSON | Narrow tasks, short outputs, schema + tolerant parser, authored fallback on every beat; test on 4B from day one |
| LLM latency makes conversations feel slow | Streaming chalk handwriting *is* the pacing; start generation when the player approaches, not when they press Enter |
| Puzzles too hard for Mom / too easy for FoFo | Relaxed vs lively, three rounds, hint tiers, Endless mode |
| Chalk look is cheap-looking instead of striking | Bloom + dust + wobble + weather are the whole show; budget real polish time in WP-1 and WP-6, and review on a dark room screen |
| Browser build vs local LLM CORS friction | In-app instructions; desktop build is the primary target |
| Agents invent story content | Bible is source of truth; entity validation runs in tests, not just at runtime |

### 3.8 Open decisions
1. Working title — **AFTERGLOW** is a placeholder; alternatives: *Lantern Night*, *Brightwater*, *Chalk Harbor*.
2. Player name entry: typed once at start (fine) vs. picked from a list (zero typing).
3. Whether Harbor Putt reuses the earlier minigolf×pool code path or gets rebuilt in the chalk style from its spec.
4. Whether to include an optional TTS hook for character voices in a later version.
5. Browser build: required for v1 or a v1.1 target?
