# HUEDUNIT
### Requirements, Design & Gameplan — v1.0
*A cozy detective puzzle-adventure. The town of Prismbrook has lost its color, its Tinter, and its nerve. You're here to find all three.*

(Working title: whodunit + hue. Rename freely — but the color gimmick it names is load-bearing, see §2.2.)

---

# PART 0 — HOW TO USE THIS DOCUMENT (FOREMAN)

This is three documents in one:
- **Part I–II** are the REQUIREMENTS and DESIGN. Part II §2–§6 is canon. It gets extracted verbatim into `docs/CANON.md` in the repo during Wave 0 and every content agent must read it before writing a line.
- **Part III–IV** are the TECHNICAL ARCHITECTURE and CONTENT FORMATS. §7–§10 get extracted into `docs/API.md`. After Wave 0, `API.md` is **frozen**: no agent may change a public interface or content-format grammar without escalating to FOREMAN, which escalates to Forrester. Agents build against the contract, never around it.
- **Part V–VI** are the GAMEPLAN: wave structure, parallel job cards, ownership matrix, merge order, and definition of done. Dispatch jobs exactly as carded; each card lists owned files — an agent that needs to touch a file it doesn't own has found a design problem, not a permission problem. Stop and escalate.

Repo: default to a **new standalone repo** `huedunit/` (this is a full game with CI, not a jam entry; if Forrester prefers the games monorepo, it becomes a top-level folder there — confirm at kickoff, everything else is unchanged). There is **no size constraint** on this project. Quality, charm, and finish outrank bytes everywhere they conflict.

---

# PART I — REQUIREMENTS & VISION

## 1.1 What this game is
A single-player, fully offline, cozy detective adventure. The player is a traveling detective hired by a small town to solve a warm-hearted mystery: someone AND something have gone missing, and the only witness is hiding from everyone at the top of a sealed tower. Play is: explore town scenes, talk to characters, solve diegetic mini-game puzzles to earn trust and clues, and assemble those clues on deduction Case Boards that advance the story. No combat, no fail states, no timers, no grinding. "RPG" here means characters, relationships, a journal, and progression — not stats and battles.

## 1.2 Player promise (requirements, testable)
1. **Playable by Forrester's mom.** Concretely: mouse-only complete playthrough possible; no reflex demands anywhere in the critical path; every puzzle skippable after 3 honest attempts with zero punishment; readable large-text option; interactable-highlight toggle default ON; autosave on every scene change; a "Previously, in Prismbrook…" recap on every load.
2. **4–6 hours** first playthrough. Prologue ≤ 15 minutes and must land the hook (the town turning gray) inside the first 3 minutes.
3. **Fair mystery.** Every Case Board answer is deducible from evidence the player has already been given. Enforced by tooling, not vibes (§10.4 clue-closure linter).
4. **The player does the detecting.** The game never solves the mystery in a cutscene the player could have solved on a board. Dialogue reveals evidence; only the player connects it.
5. **Warmth.** No villain, no death, no cruelty. Stakes are emotional and communal. The ending must be earned, kind, and a little bit of a tearjerker.
6. Ships as a standalone desktop build (Windows x64 primary, Linux + macOS secondary), 60fps on integrated graphics.

## 1.3 Design pillars
1. **Cozy Layton, honest Golden Idol.** Layton's shape — a charming town, chatty townsfolk, puzzles woven into conversation, a hint economy — married to Golden Idol-style fill-in deduction boards where the PLAYER assembles the truth from collected words. The boards are the mystery; the mini-games are the town.
2. **Color is progress.** The world starts as ink on gray paper. Each solved chapter visibly restores one hue to the entire town. The player can SEE how far they've come from any screen. (This is also the procedural-art gimmick: see 2.2.)
3. **Everyone is a suspect; no one is a villain.** Red herrings resolve into sympathetic truths. Suspicion is the engine; forgiveness is the destination.
4. **Puzzles are people.** Every mini-game belongs to a character's trade and personality. No abstract puzzle popups from nowhere — the baker's problem is a baking problem.
5. **The engine serves the writers.** All narrative content is plain-text data (dialogue, cutscenes, boards) compiled at build time. Engine agents build interpreters; content agents write files. The two never edit each other's territory.

---

# PART II — GAME DESIGN (CANON — extract to docs/CANON.md)

## 2. World & Premise

### 2.1 The setup
**Prismbrook** is a seaside hill town famous for its color. At its heart stands the **Prismworks** — a lighthouse-like tower whose lantern room holds the **Prism**, an heirloom crystal that (so the town believes) gives Prismbrook its impossibly vivid hues. The Prism is tended by the town **Tinter, Iris Marlow** — painter, lamplighter of the Prism, quietly beloved, fiercely private.

One morning the town wakes **gray**. All of it — the sea, the tarts, the bunting. The Prism is gone from its cradle. So is Iris. The Prismworks door is locked from the inside, and the only living thing anyone's seen near it is **Pip**, Iris's magpie — who now hides in the tower heights and flees from everyone. Half the town thinks the "thieving bird" stole the Prism. The council has pooled its savings and hired you: **the Detective** (player-named, dialogue-choice personality, never voiced).

### 2.2 The gimmick (canon + art direction + tech, one decision)
The world is rendered as **living ink on paper** — every scene is programmatic 2D vector art (strokes, blobs, hatching, paper grain), zero image assets. Diegetically, Prismbrook without its Prism is literally a town drained to ink. Each chapter solved restores one **hue band** to the global palette: the art system tints its strokes and fills through a staged palette (§8.2). Gray → blue → +yellow → +red → +green → +violet → full spectrum finale. The gimmick makes procedural art the *point*, gives casual players a progress bar they can feel, and gives Wave 3's graphics round a clear target: making ink feel alive.

### 2.3 The truth (SPOILER CANON — the whole team must know it; the player must never be told it early)
- The Prism is real but not magic the way the town thinks: it is **charged by gathering**. Every year at the **Festival of Lanterns**, the whole town climbs Tower Green, lanterns are lit, and the Prism drinks the shared light. The festival has lapsed for three years (rain one year, a budget quarrel the next, apathy the third). The Prism has been quietly dimming — and Iris was the only one close enough to see it cracking.
- Too proud to alarm anyone (and privately heartbroken that no one else noticed), Iris borrowed the locksmith's picks, carried the Prism to the lantern room to re-anchor and mend it alone at night. The rotten upper stair collapsed behind her. She has been **trapped in the lantern room** the whole game — safe, stubborn, embarrassed, unheard over the harbor wind.
- **Pip is the only witness.** He's been trying to lead people to the tower ever since — swooping, stealing shiny things to get attention — which is exactly why the town decided he was the thief. The only clue really has been *hiding from the town, in the tower*: a scared bird everyone blames.
- Resolution: the player pieces this together, wins Pip's trust, reaches Iris, and — because a cracked Prism can't just be re-lit — rallies every character they've helped all game to hold the Festival of Lanterns that night. The town's gathered light mends the Prism, color floods back, Iris is welcomed down, Pip is publicly un-accused (and made "Deputy Detective"). Nobody done it. Everybody fixes it.

### 2.4 Red herrings (each must resolve warmly)
- **Mayor Aurelius Grand** dodges questions about the tower → he'd secretly cut the festival budget and is ashamed.
- **Otto Brine** (fishmonger) was seen at the tower at midnight → he's been leaving fish for the "cursed" magpie, because no one should go hungry, not even a thief.
- **Petra Ward's** lockpicks are missing and she reported nothing → Iris borrowed them with a promise of secrecy, and Petra keeps promises.
- **Nona Ember** (retired lamplighter) owns the only spare tower key and claims it's lost → she hid it years ago after the festival lapsed, out of grief, and can't bear to say so.

## 3. Structure & Progression

### 3.1 Chapter spine
Prologue + 5 chapters + finale. Each chapter opens one **district**, ~3 characters with puzzle chains, ends at the Prismworks with a **Case Board** on a newly unlocked tower floor, and restores one hue.

| Ch | District | Hue restored | Chapter question (its Case Board) | Key beat |
|----|----------|--------------|-----------------------------------|----------|
| P | Town gate & square (gray) | — | Tutorial board: "What happened this morning?" | Hired; meet town; see Pip flee |
| 1 | Harbor | Blue | Who was at the tower at midnight — and why? | Clears Otto; first Pip feather |
| 2 | Market Row | Yellow | How was the Prismworks opened without its key? | Petra's picks; Iris did it herself?! |
| 3 | Old Quarter | Red | What was wrong with the Prism before it vanished? | Cog's ledger: Iris ordered repair braces |
| 4 | The Gardens | Green | Why did the Festival of Lanterns stop? | Mayor's shame; Nona's hidden key |
| 5 | Tower Green | Violet | Where is Iris Marlow right now? | Key recovered; Pip trusts you; climb |
| F | Prismworks summit | Full + gold | Finale sequence (§3.4) | Rescue, Festival, mended Prism |

### 3.2 The chapter loop
Explore district scenes → talk (dialogue reveals problems) → solve a character's mini-game (earns trust + a **clue token** + often a physical lead) → optional pokes: inspect hotspots for flavor + **feathers** (hint currency) → when the chapter's clue set is complete, the tower floor opens → Case Board → cutscene: hue restored, next district unlocked. The journal (§5.3) tracks everything so a player returning after a week is never lost.

### 3.3 Case Boards (the deduction spine)
Modeled on the fill-in-the-blank school of deduction games, tuned for a casual player:
- A board is a short illustrated scroll of statements with blanks: "At midnight, ___ climbed Tower Green carrying ___, because ___." Blanks are filled from the player's collected **clue tokens** (names, objects, motives — nouns and short phrases, never free text).
- Questions are **clear and unambiguous, never leading, never vague** — each blank has exactly one defensible answer given the evidence.
- **Granular over grand:** each chapter's board is self-contained (5–9 blanks) and answers that chapter's question; a final board synthesizes. Small boards pace better than one monolith.
- **Anti-brute-force:** the board confirms correctness only when ALL blanks are committed via the "Deduce!" button, with the classic partial feedback: "3 of 7 are correct" — enough to keep momentum, not enough to guess through. Wrong-answer attempts are unlimited and unpunished.
- **Multiple roads to key answers:** every load-bearing deduction must be reachable from at least two independent evidence sources, so missing one nuance never hard-blocks a player.
- Chapter 1's board is deliberately easy (3 blanks) and diegetically tutorializes the mechanic via your journal margin notes.

### 3.4 Finale (not a board — a payoff)
The finale trades deduction for warmth: a guided sequence where the player, atop the tower with Iris and Pip, must get the festival lit. It's a "victory lap" chain — visit each district once, and each character you helped says yes in a way that pays off their arc (the baker bakes lantern-buns, the kids run the invitations, Nona lights the first wick). Ends in the game's biggest cutscene: lantern climb, Prism mend, full-spectrum flood, epilogue vignettes. This sequence is Wave 3's crown jewel and gets the largest cutscene budget.

## 4. Characters (roster canon)
15 townsfolk + Iris + Pip. Names, trades, puzzle, and secret per character. (Content agents: voices may sparkle, facts may not drift.)

**Harbor (Ch1):** Capt. Maribel Sorge, ferry captain — rope/knot routing puzzle; gruff, secretly sentimental. Otto Brine, fishmonger — weights & sorting puzzle; the midnight red herring. Tansy, 9, crab-catcher — shell memory game; first to say Pip "isn't bad, just sad."
**Market Row (Ch2):** Bruno Crumb, baker — tart-tiling (pentomino fit) puzzle; feeds everyone's feelings. Greta Spool, seamstress — thread-grid (nonogram-style) puzzle; town gossip hub, kind about it. Felix Route, postmaster — parcel-routing graph puzzle; knows who wrote to whom.
**Old Quarter (Ch3):** Edwina Cogg, clockmaker & Iris's aunt — gear-train puzzle; her ledger is the Ch3 linchpin. Bartleby Shelf, librarian — cipher/word puzzle; keeper of festival history. Petra Ward, locksmith — tumbler-logic puzzle; the promise-keeper.
**Gardens (Ch4):** Sage Fern, gardener — water-flow (pipes) puzzle; grows gray flowers and grieves them. Mo Hum, beekeeper — hex-path puzzle; bees "remember the festival dances." Dr. Poppy Bloom, apothecary — mixing-ratio puzzle; treats the town's low spirits with tea and bluntness.
**Tower Green (Ch5):** Mayor Aurelius Grand — stamp-order bureaucracy puzzle (played for comedy); the shame arc. Nona Ember, retired lamplighter — light & mirror reflection puzzle; the hidden key, the game's most emotional mid-arc. Wick, tower groundskeeper — mechanism/key puzzle; speaks mostly to the tower.
**Iris Marlow** — appears in flashback ink-vignettes and the finale. **Pip** — trust meter advanced by evidence of kindness (returning shiny things, Otto's fish, Tansy's testimony), not by a puzzle; the one "relationship" mechanic.

## 5. Systems

### 5.1 Mini-game puzzles (15 + variants)
Each is a self-contained plugin behind one interface (§7.4): 3–8 minutes, mouse-only, difficulty ramping within each chapter and across the game, always re-playable from the journal for fun. Every puzzle carries: intro dialogue framing, 3 hint tiers, a skip (after 3 attempts), and a solved payoff line that hands over the clue token. Variety mandate: across the 15, at least — 3 spatial/fit, 3 logic/deduction-lite, 3 routing/flow, 2 pattern/memory, 2 word/cipher (language-simple), 2 mechanical/simulation. No math beyond arithmetic; no trivia; no timing except one optional bee-waggle rhythm bonus (never on critical path).

### 5.2 Hints — the Feather system
Pip sheds **feathers** hidden in scenes (sparkle-highlighted like all interactables). Feathers buy hints: tier 1 nudge → tier 2 method → tier 3 near-answer. Deviation from the classic scarce-coin economy, on purpose: feathers are **plentiful** (soft cap ensures a player who explores casually can afford ~2 tiers on every puzzle). The scarcity model creates hoarding anxiety; mom-cozy wants generosity. The skip option needs no feathers at all. Collecting feathers also silently advances Pip's trust — the hint system IS the relationship system, which turns "I needed help" into "the bird likes me." Case Boards use the same feathers (tier hints per blank).

### 5.3 The Case Journal
One book UI, four tabs: **People** (portraits, what you know, updates as trust grows), **Clues** (every token with where/how you got it), **Boards** (past boards, current board), **Town** (map, district status, hue progress, feather count, puzzle replays). Auto-writing, charming margin doodles, zero management burden. The load-game recap reads from it.

### 5.4 Saves & settings
Autosave every scene transition + manual slots (3). Settings: text size (2 steps), highlight toggle, music/SFX volumes, fullscreen, reduce-motion (disables screen shake & fast pans — respect it in every cutscene command, §9.3).

## 6. Tone & writing rules (content agents memorize this)
Warm, wry, gentle. Humor from character, never meanness. Sentences short; vocabulary friendly; British-cozy flavor without dialect walls. No romance subplots, no politics, no peril to animals or children, no death (the stakes are loneliness and fading, and that is plenty). The Detective's dialogue choices are tone choices (kind/curious/dry), never moral forks — all roads reach the same warm ending. Every character gets exactly one moment of unexpected depth. Pip never talks.

---

# PART III — TECHNICAL ARCHITECTURE (extract §7–§10 to docs/API.md; FROZEN after Wave 0)

## 7. Engine & module contracts

### 7.1 Stack
C11 + raylib (window/input/render/audio device), desktop targets. All art programmatic (§8), all audio synthesized (§8.4), all narrative content plain-text compiled at build time (§9) by our own `bakery` tool (also C — one toolchain). Plain `Makefile`; `make`, `make debug`, `make check` (build + tests + content lint), `make bake` (content only).

### 7.2 Architecture rule of the whole project
**Interpreters, not implementations.** Engine code knows HOW to run a dialogue, a cutscene, a board, a puzzle. It never knows WHAT any particular one contains. All the WHAT lives in `content/` as text. This is the boundary that makes parallel agent work safe: systems agents and content agents cannot collide because they share no files.

### 7.3 Module map & public interfaces (signatures are the contract)
```
huedunit/
  Makefile
  docs/CANON.md  docs/API.md  docs/JOBS/        (job cards live here)
  tools/bakery/                                  (content compiler + linters)
  src/
    core/    main.c app.c        app state machine: BOOT/TITLE/TOWN/PUZZLE/BOARD/CUTSCENE/JOURNAL
    scene/   scene.c/.h          scene defs, hotspots, walk targets, feather spawns
    dlg/     dlg.c/.h            dialogue interpreter        API: dlg_start(id), dlg_update, dlg_draw
    flags/   flags.c/.h          world state                 API: flag_get/set(name), clue_grant(id), trust_add(npc, n)
    puzzle/  puzzle.c/.h         plugin host                 API: §7.4
    board/   board.c/.h          case-board interpreter      API: board_start(id) → BOARD_SOLVED
    cut/     cut.c/.h            cutscene interpreter        API: cut_play(id) → done
    art/     artkit.c/.h         ink&paper renderer          API: §8.1–8.3
    audio/   synth.c/.h          SFX/music synth             API: sfx_play(SFX_id), music_mood(MOOD_id)
    journal/ journal.c/.h        the book UI (reads flags/clues; writes nothing)
    save/    save.c/.h           versioned save blob of the flag store + settings
  content/
    dialogue/*.dlg   cutscenes/*.cut   boards/*.case   scenes/*.scn   strings/*.txt
  src/puzzles/p_<name>.c          one file per mini-game, self-registering (§7.4)
```
- **Auto-discovery, no registries.** `bakery` and the Makefile glob `content/**` and `src/puzzles/p_*.c`; there is no hand-edited index file anywhere. (Shared registry files are the classic multi-agent merge hotspot — we simply don't have one.)
- The flag store is the ONLY cross-system state: string-keyed ints, namespaced (`ch1.otto.trust`, `clue.midnight_lantern`). Dialogue, boards, cutscenes, and scenes read/write flags exclusively through the flags API. Saves serialize the flag store — meaning saves are trivially forward-compatible with content additions.

### 7.4 Puzzle plugin contract (one agent per puzzle, zero collisions)
```c
typedef enum { PZ_RUNNING, PZ_SOLVED, PZ_EXITED } pz_status;
typedef struct {
  const char* id;            // "tart_tiling"
  const char* clue_granted;  // clue token id on solve (may be NULL)
  void  (*init)(pz_ctx* ctx);        // ctx: rng seed, difficulty, artkit*, audio*
  pz_status (*update)(pz_ctx*, float dt);
  void  (*draw)(pz_ctx*);
  const char* (*hint)(pz_ctx*, int tier);   // tiers 1..3
  void  (*shutdown)(pz_ctx*);
} pz_def;
void pz_register(const pz_def*);   // called from p_<name>.c constructor
```
Host provides: standard frame (paper panel, title, hint button, skip flow, attempt counting), so puzzle agents build ONLY the puzzle. Puzzles draw exclusively through artkit (visual consistency for free) and may not touch flags directly — the host grants `clue_granted` on PZ_SOLVED.

## 8. Art & audio kit

### 8.1 Ink & paper renderer (artkit) — the vocabulary every visual is built from
`ink_stroke(pts, w, wobble)`, `ink_fill(poly, hatch_style)`, `ink_blob`, `paper_panel(rect, torn_edges)`, `paper_grain(intensity)`, `doodle(id, x, y, scale)` (a library of parametric doodles: lantern, fish, gear, teacup, feather…), `vignette`, `wind_lines`. All strokes have hand-drawn wobble from a per-frame-stable noise (nothing shimmers unless asked). Characters are **parametric paper-puppets**: `npc_draw(npc_id, pose, emote)` builds each townsperson from shape grammar (silhouette, hat, prop, palette slots) — 17 characters from one system, and Wave 3 can upgrade everyone at once by upgrading the grammar.

### 8.2 Palette & hue progression (the gimmick's engine)
One global palette object. `palette_set_stage(n)` (0=gray … 6=full+gold). Every artkit color is a semantic slot (`INK`, `PAPER`, `ACCENT_A/B`, `SKY`, `SEA`, `WARM`, `COOL`), resolved through the stage table — restoring a hue recolors the entire game with zero content edits. `palette_bloom(region, stage)` animates a hue physically flooding outward from the tower for the chapter-end cutscenes; this effect is a named Wave 3 deliverable and should be the most beautiful thing in the game.

### 8.3 Portraits & emotes
Dialogue portraits are the paper-puppets at bust framing with `emote` states: neutral, happy, sad, worried, shifty, laughing, moved. Emote changes are cutscene/dialogue commands. Minimum viable in Wave 1 (two emotes), full range in Wave 3.

### 8.4 Audio
All synthesized at startup (his usual recipe approach): UI ticks, page turns, ink scritches (dialogue text blips per-character pitched to the speaker), puzzle chimes, board "deduce" fanfare, feather sparkle, and the big Prism-mend swell. Music: generative cozy engine — 3-voice pattern system with per-district mood tables (`MOOD_HARBOR`, `MOOD_MARKET`, …) that gain one voice/brightness step per restored hue, so the soundtrack literally recolors with the town. Music volume auto-ducks under dialogue.

## 9. Content formats (grammar is contract)

### 9.1 Dialogue `.dlg` (interpreter: src/dlg)
```
@node otto_intro
  [if flag ch1.met_otto == 0]
  OTTO(worried): Cold morning, detective. Colder without the blue, if you ask me.
  YOU:
    * (kind) Everyone misses it, Mr. Brine.   -> otto_warm
    * (dry) The fish seem fine with it.       -> otto_chuckle
  set ch1.met_otto = 1
@node otto_warm
  OTTO(moved): ...Aye. Well. Ask your questions.
  grant clue.otto_alibi
  start_puzzle weights_sorting
```
Node graph, speaker(emote) lines, tone-tagged choices, flag conditions/sets, `grant`, `start_puzzle`, `play_cut`. `bakery` validates: every node reachable, every jump/flag/clue/puzzle id exists.

### 9.2 Case boards `.case`
```
board ch1_midnight
  requires clue.otto_alibi clue.tansy_saw clue.lantern_oil ...
  text "At midnight, {p1} climbed Tower Green carrying {p2},"
  text "not to steal, but to {p3}."
  blank p1 answer token.otto     pool people
  blank p2 answer token.fish_pail pool objects
  blank p3 answer token.feed_pip  pool motives
  feedback grouped        # "N of M correct" on Deduce!
  on_solved cut ch1_hue_blue
```

### 9.3 Cutscenes `.cut` (the Wave 3 workhorse)
Timeline commands, one per line: `BG scene_id` · `PAN x1 y1 x2 y2 t` · `ACTOR id AT x y` · `MOVE id x y t` · `EMOTE id state` · `SAY id "…"` · `WAIT t` · `MUSIC mood` · `SFX id` · `FADE in|out t` · `HUE stage t` · `BLOOM x y stage` · `DOODLE id x y` · `SHAKE amt t` (no-op under reduce-motion) · `TITLE "Chapter One" "The Harbor"`. Deliberately small verb set: if a cutscene needs a new verb, that's an artkit/cut engine job card, not an inline hack.

### 9.4 Scenes `.scn`
Per screen: background composition (artkit ops), hotspot list (inspect text / dialogue node / feather spawn / exit links), walk plane. Point-and-click; the Detective is a paper-puppet who strolls.

## 10. Quality gates (all run in `make check`; merge-blocking)
1. Build clean, `-Wall -Wextra -Werror`.
2. Unit tests: flags, save round-trip, dlg/board/cut interpreters against fixture scripts.
3. **Content lint (bakery):** unreachable nodes, dangling ids, undefined flags, orphan clues.
4. **Clue-closure check (the fairness enforcer):** for every board, prove each `requires` clue is grantable through some reachable path BEFORE that board unlocks. A mystery game that can dead-end an honest player is broken; this makes it structurally impossible.
5. Puzzle solvability: each `p_*.c` ships a `solve_replay` fixture the host runs headless to PZ_SOLVED.
6. Determinism where seeded (puzzle shuffles derive from save seed — re-entering a puzzle shows the same instance).

---

# PART V — GAMEPLAN (waves, lanes, job cards)

## 11. Method (why it's shaped this way)
Contract-first, scaffold-then-split: **Wave 0 freezes the interfaces and grammars before any parallel dispatch**, because agents building against a shifting contract is the primary multi-agent failure mode. Then: one agent per lane, each lane owning a disjoint file set (the ownership matrix IS the conflict-prevention system), each agent in its own worktree/branch, merging **sequentially in card order** into `develop` with `make check` green as the gate. Integration is a job, not a hope: every wave ends with an integration card. FOREMAN dispatches lanes to whichever agents it likes (Claude Code/Codex/Antigravity/OpenCode); every job prompt MUST begin with: read `docs/CANON.md`, `docs/API.md`, and your job card in `docs/JOBS/`.

## 12. Wave 0 — FOUNDATION (sequential; 1 agent + Forrester review; ~small)
**Card W0:** repo scaffold per §7.3; app state machine with stub screens; flags + save; artkit v0 (stroke/fill/panel/palette stages — ugly is fine, API complete); bakery v0 (parse all four grammars + linters 3–6); one stub of each content type flowing end-to-end (walk a gray square, talk to a test NPC, solve a stub puzzle, fill a 1-blank board, watch a 3-line cutscene turn the sky blue); CI running `make check`. Extract CANON.md + API.md from this doc. **Exit gate (hard):** Forrester plays the stub loop; API.md declared frozen.

## 13. Wave 1 — SYSTEMS (parallel, 5 lanes)
| Card | Lane | Owns | Deliverable |
|------|------|------|-------------|
| W1-A | Dialogue & Journal | src/dlg, src/journal | Full .dlg interpreter (choices, tones, conditions, portraits), journal book UI, recap-on-load |
| W1-B | Puzzle host + refs | src/puzzle, src/puzzles/p_ref_* | Host frame (hints/skip/attempts), TWO reference puzzles (tart_tiling, thread_grid) proving the contract, headless solve harness |
| W1-C | Artkit v1 | src/art | Full §8.1 vocabulary, paper-puppet grammar w/ 2 emotes, palette stages + basic bloom, 17 character silhouettes |
| W1-D | Audio | src/audio | Synth SFX set, generative mood engine, hue-brightening, ducking |
| W1-E | Boards & Cutscenes | src/board, src/cut | Both interpreters complete vs §9.2/9.3, grouped feedback, reduce-motion honored |
**W1-INT:** integration agent merges A→E in order, fixes seams, records a full stub-loop video for Forrester. Physics… sorry, wrong game — *interfaces* are frozen; anything a lane needed changed goes in a written API-change request for review here, not silently.

## 14. Wave 2 — CONTENT (parallel, the widest wave)
District cards **W2-D1…D5** (one agent per district): all `.scn` scenes, all `.dlg` for their 3 NPCs + chapter connective tissue, feather placement, their chapter's `.case` board, hotspot flavor text. Owns ONLY `content/**` files matching their chapter prefix (`ch1_*`, …). Canon-fidelity is the review bar.
Puzzle cards **W2-P1…P13** (one agent per remaining mini-game, batchable 2–3 per agent): one `p_<name>.c` each per §5.1 catalog + hints + solve fixture. Owns only their file.
**W2-S:** story lead agent — prologue + connective beats + Pip trust moments + `strings/` UI text, and a continuity read of all district dialogue against CANON (report, don't edit others' files).
**W2-INT:** merge (D-cards, then P-cards, then S), clue-closure across the whole game green, first full playthrough build → **Forrester plays Chapters P–2.** Mid-project checkpoint: tone, difficulty, pace adjustments carded before Wave 3.

## 15. Wave 3 — THE GRAPHICS & CUTSCENE ROUND (parallel; the round Forrester asked for by name)
Split into isolated, delegable beauty-jobs. Every card = read CANON + API + card; touch only owned files; produce a before/after capture.
| Card | Owns | Deliverable |
|------|------|-------------|
| W3-C1…C8 | one `.cut` each | The 8 majors: Opening Grayfall; 5 chapter hue-restorations (each themed to its district); Iris Found; the Festival Finale (double budget — this one gets two agents, script + any new artkit verbs it justifies via change requests) |
| W3-A1 | src/art (puppet grammar) | Full emote range, idle sway, walk bounce, blink |
| W3-A2 | src/art (world FX) | Weather & ambience: harbor mist, market bustle doodles, garden pollen, tower wind; all palette-aware, all reduce-motion-safe |
| W3-A3 | src/art (bloom) | The palette_bloom showpiece — hue flooding from the tower, per-district variations |
| W3-A4 | src/core (title) | Title screen: the gray town breathing under paper grain; menu; credits |
| W3-M | src/audio | Finale music: the one authored (non-generative) piece, built in the pattern engine, for the lantern climb |
**W3-INT:** merge in table order; full-game capture reel for Forrester.

## 16. Wave 4 — INTEGRATION, TUNING & THE MOM TEST (mostly sequential)
1. **Difficulty & pace pass:** hint text quality audit (every tier-2 must actually teach the method), puzzle attempt data from playthroughs, board blank-count tuning.
2. **Accessibility audit** vs §1.2.1 checklist, item by item.
3. **THE MOM TEST (release gate):** a real non-gamer (ideally the actual mom) plays Prologue–Ch2 unassisted. Pass = never stuck >5 min without an in-game path forward, never asks "what do I do now?", and voluntarily keeps playing past the required stop. Failures become cards; repeat until pass.
4. Performance & platform builds; save-compat check; credits; ship candidate.

## 17. Ownership matrix summary & standing rules
- src/core: W0 then W3-A4 then W4 only. tools/bakery: W0 then frozen except linter additions. docs/: FOREMAN only. Each src module: its W1 lane, then only via carded change requests. content/: chapter-prefix ownership per W2/W3 cards.
- Merge order within every wave = card order in this doc. `make check` green before any merge, no exceptions, including "it's just content."
- Any agent hitting a contract limitation writes `docs/JOBS/change-requests/CR-<n>.md` and continues on non-blocked work. FOREMAN batches CRs to Forrester.

## 18. Definition of Done (ship gate)
1. Full playthrough P→Finale on all three platforms; 4–6h; autosaves + recap verified.
2. `make check` green: all linters, clue-closure across all 6 boards, all 15 puzzle solve-fixtures.
3. Every §1.2 requirement demonstrably true; Mom Test passed.
4. All 8 cutscenes at Wave 3 quality; hue progression stage-perfect 0→6; finale lands.
5. Zero image/audio asset files in the repo or the build — everything programmatic, per the gimmick.
6. All content original (names, text, music, characters).

---
*One town, one bird, one tower, no villains. Make it kind, make it fair, make it ink.*
