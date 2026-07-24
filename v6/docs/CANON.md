# HUEDUNIT — CANON

Extracted from `HUEDUNIT_req_design_gameplan.md` Part II (§2–§6) at Wave 0.
Every content agent reads this before writing a line. Voices may sparkle;
facts may not drift.

---

## 2. World & premise

### 2.1 The setup
**Prismbrook** is a seaside hill town famous for its colour. At its heart
stands the **Prismworks** — a lighthouse-like tower whose lantern room holds
the **Prism**, an heirloom crystal that (so the town believes) gives
Prismbrook its impossibly vivid hues. The Prism is tended by the town
**Tinter, Iris Marlow** — painter, lamplighter of the Prism, quietly beloved,
fiercely private.

One morning the town wakes **gray**. All of it. The Prism is gone from its
cradle. So is Iris. The Prismworks door is locked from the inside, and the
only living thing anyone has seen near it is **Pip**, Iris's magpie — who now
hides in the tower heights and flees from everyone. Half the town thinks the
"thieving bird" stole the Prism. The council has pooled its savings and hired
you: **the Detective** (player-named, dialogue-choice personality, never
voiced).

### 2.2 The gimmick
The world is **living ink on paper** — every scene is programmatic 2D vector
art, zero image assets. Diegetically, Prismbrook without its Prism is a town
drained to ink. Each chapter solved restores one **hue band** to the global
palette: gray → blue → +yellow → +red → +green → +violet → full spectrum and
gold.

### 2.3 The truth (SPOILER CANON — the team knows it; the player is never told it early)
- The Prism is real but not magic the way the town thinks: it is **charged by
  gathering**. Every year at the **Festival of Lanterns** the whole town climbs
  Tower Green, lanterns are lit, and the Prism drinks the shared light. The
  festival has lapsed for three years (rain, then a budget quarrel, then
  apathy). The Prism has been quietly dimming — and Iris was the only one
  close enough to see it cracking.
- Too proud to alarm anyone (and privately heartbroken that nobody else
  noticed), Iris borrowed the locksmith's picks and carried the Prism to the
  lantern room to re-anchor and mend it alone at night. The rotten upper stair
  collapsed behind her. She has been **trapped in the lantern room** the whole
  game — safe, stubborn, embarrassed, unheard over the harbour wind.
- **Pip is the only witness.** He has been trying to lead people to the tower
  ever since — swooping, taking shiny things to get attention — which is
  exactly why the town decided he was the thief. The only clue really has been
  hiding from the town, in the tower: a scared bird everyone blames.
- **Resolution:** the player pieces this together, wins Pip's trust, reaches
  Iris, and — because a cracked Prism cannot simply be re-lit — rallies every
  character they have helped all game to hold the Festival of Lanterns that
  night. The town's gathered light mends the Prism, colour floods back, Iris is
  welcomed down, Pip is publicly un-accused and made "Deputy Detective".
  Nobody done it. Everybody fixes it.

### 2.4 Red herrings (each resolves warmly)
| Herring | Truth |
| --- | --- |
| **Mayor Aurelius Grand** dodges questions about the tower | He cut the festival budget and is ashamed |
| **Otto Brine** was seen at the tower at midnight | He leaves fish for the "cursed" magpie, because nobody should go hungry |
| **Petra Ward's** lockpicks are missing and unreported | Iris borrowed them with a promise of secrecy, and Petra keeps promises |
| **Nona Ember** owns the spare tower key and claims it is lost | She hid it years ago out of grief, and cannot bear to say so |

---

## 3. Structure & progression

### 3.1 Chapter spine
| Ch | District | Hue | Chapter question (its board) | Key beat |
| --- | --- | --- | --- | --- |
| P | Town gate & square | — | What happened this morning? | Hired; meet town; see Pip flee |
| 1 | Harbour | Blue | Who was at the tower at midnight, and why? | Clears Otto; first Pip feather |
| 2 | Market Row | Yellow | How was the Prismworks opened without its key? | Petra's picks; Iris did it herself |
| 3 | Old Quarter | Red | What was wrong with the Prism before it vanished? | Cogg's ledger: repair braces |
| 4 | The Gardens | Green | Why did the Festival of Lanterns stop? | The Mayor's shame surfaces |
| 5 | Tower Green | Violet | Where is Iris Marlow right now? | Key recovered; Pip trusts you; climb |
| F | Prismworks summit | Full + gold | Finale sequence (§3.4) | Rescue, Festival, mended Prism |

### 3.2 The chapter loop
Explore district scenes → talk (dialogue reveals problems) → solve a
character's mini-game (earns trust + a **clue token** + often a physical lead)
→ optional pokes: inspect hotspots for flavour and **feathers** → when the
chapter's clue set is complete, the tower floor opens → Case Board → cutscene:
hue restored, next district unlocked.

### 3.3 Case Boards
- A board is a short illustrated scroll of statements with blanks. Blanks are
  filled from collected **clue tokens** — never free text.
- Questions are clear and unambiguous, never leading, never vague: each blank
  has exactly one defensible answer given the evidence.
- **Granular over grand:** each chapter's board is self-contained (3–9 blanks).
- **Anti-brute-force:** the board confirms only when ALL blanks are committed
  via **Deduce!**, with grouped feedback ("3 of 7 are correct"). Wrong attempts
  are unlimited and unpunished.
- **Multiple roads:** every load-bearing deduction is reachable from at least
  two independent evidence sources. This is enforced by tooling, not vibes —
  see `docs/API.md` §9.2 and §10.4.
- The prologue board is deliberately easy (3 blanks) and tutorialises the
  mechanic.

### 3.4 Finale
Not a board — a payoff. Atop the tower with Iris and Pip, the player must get
the festival lit: a victory-lap chain where each character you helped says yes
in a way that pays off their arc. Ends in the biggest cutscene in the game:
lantern climb, Prism mend, full-spectrum flood, epilogue.

---

## 4. Characters (roster canon)

15 townsfolk + Iris + Pip, plus the Detective.

**Harbour (Ch1)** — Capt. Maribel Sorge, ferry captain (rope/knot routing;
gruff, secretly sentimental). Otto Brine, fishmonger (weights & sorting; the
midnight red herring). Tansy, 9, crab-catcher (shell memory; first to say Pip
"isn't bad, just sad").

**Market Row (Ch2)** — Bruno Crumb, baker (tart-tiling; feeds everyone's
feelings). Greta Spool, seamstress (thread-grid; gossip hub, kind about it).
Felix Route, postmaster (parcel-routing; knows who wrote to whom).

**Old Quarter (Ch3)** — Edwina Cogg, clockmaker & Iris's aunt (gear-train; her
ledger is the linchpin). Bartleby Shelf, librarian (cipher; keeper of festival
history). Petra Ward, locksmith (tumbler-logic; the promise-keeper).

**Gardens (Ch4)** — Sage Fern, gardener (water-flow; grows gray flowers and
grieves them). Mo Hum, beekeeper (hex-path; bees "remember the festival
dances"). Dr. Poppy Bloom, apothecary (mixing-ratio; treats low spirits with
tea and bluntness).

**Tower Green (Ch5)** — Mayor Aurelius Grand (stamp-order bureaucracy, played
for comedy; the shame arc). Nona Ember, retired lamplighter (light & mirror
reflection; the hidden key, the game's most emotional mid-arc). Wick,
groundskeeper (mechanism/word-wheel; speaks mostly to the tower).

**Iris Marlow** appears in the finale. **Pip** has a trust meter advanced by
evidence of kindness, not by a puzzle. **Pip never talks.**

---

## 5. Systems

### 5.1 Mini-games (15)
Self-contained plugins behind one interface. 3–8 minutes, mouse-only,
difficulty ramping. Every puzzle carries intro framing, 3 hint tiers, a skip
after 3 attempts, and a solved payoff line that hands over the clue token.

Variety mandate, and how it is met:

| Category | Required | Shipped |
| --- | --- | --- |
| spatial / fit | 3 | tart_tiling, thread_grid, mirror_light |
| logic / deduction-lite | 3 | weights_sorting, tumbler_logic, stamp_order |
| routing / flow | 3 | rope_knot, parcel_route, water_flow |
| pattern / memory | 2 | shell_memory, hex_path |
| word / cipher | 2 | cipher_shelf, mechanism_key |
| mechanical / simulation | 2 | gear_train, mix_ratio |

No maths beyond arithmetic. No trivia. No timing anywhere on the critical
path.

### 5.2 Hints — the Feather system
Pip sheds **feathers** hidden in scenes, sparkle-highlighted like all
interactables. Feathers buy hints: tier 1 nudge → tier 2 method → tier 3
near-answer. Feathers are deliberately **plentiful**: the classic scarce-coin
economy creates hoarding anxiety, and mom-cozy wants generosity. The skip needs
no feathers at all. Collecting feathers silently advances Pip's trust — the
hint system IS the relationship system. Case Boards spend the same feathers.

### 5.3 The Case Journal
One book, four tabs: **People**, **Clues**, **Boards**, **Town**.
Auto-writing, margin doodles, zero management burden. It reads the world and
writes nothing back.

### 5.4 Saves & settings
Autosave on every scene transition plus manual slots. Settings: text size (3
steps), interactable highlight (default ON), music/SFX volume, fullscreen,
reduce-motion (disables screen shake and fast pans everywhere, cutscenes
included).

---

## 6. Tone & writing rules

Warm, wry, gentle. Humour from character, never meanness. Sentences short;
vocabulary friendly; British-cozy flavour without dialect walls. No romance
subplots, no politics, no peril to animals or children, no death — the stakes
are loneliness and fading, and that is plenty. The Detective's dialogue choices
are tone choices (kind / curious / dry), never moral forks: all roads reach the
same warm ending. Every character gets exactly one moment of unexpected depth.
Pip never talks.
