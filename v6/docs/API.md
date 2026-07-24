# HUEDUNIT — API & CONTENT FORMATS

Extracted from `HUEDUNIT_req_design_gameplan.md` Part III (§7–§10) at Wave 0
and **frozen**. No agent may change a public interface or a content-format
grammar without escalating. Build against the contract, never around it.

Where this document differs from the source doc, the difference is marked
**[W0]** and was decided during Wave 0, before the freeze.

---

## 7. Engine & module contracts

### 7.1 Stack
C11 + raylib, desktop targets. All art programmatic (§8), all audio
synthesised (§8.4), all narrative content plain text compiled at build time by
our own `bakery` tool. Plain `Makefile`:

| Target | Does |
| --- | --- |
| `make` | release build, `-Os -flto`, stripped |
| `make debug` | `-O1 -g`, no LTO |
| `make run` | build and play |
| `make bake` | content only: bakery + linters |
| `make check` | bake + unit tests + build + size gate |
| `make size` | shipped bytes against the ceiling |
| `make windows` | cross-compile Windows x64 via MinGW-w64 |

**[W0] Size ceiling.** The source doc says there is no size constraint. This
build ships under the FloppyJam rule shared with v1–v5: the executable must be
**≤ 1,474,560 bytes** (one 1.44 MB floppy), enforced by `make size` and
therefore by `make check`. This is compatible with §18.5 either way, since the
game has no asset files at all.

### 7.2 Architecture rule of the whole project
**Interpreters, not implementations.** Engine code knows HOW to run a dialogue,
a cutscene, a board, a puzzle. It never knows WHAT any particular one contains.
All the WHAT lives in `content/` as text. This is the boundary that makes
parallel agent work safe.

### 7.3 Module map
```
v6/
  Makefile
  docs/CANON.md  docs/API.md
  tools/bakery/                       content compiler + linters
  src/
    core/    main.c app.c             app state machine
    scene/   scene.c/.h               scenes, hotspots, walk plane, feathers
    dlg/     dlg.c/.h                 dialogue interpreter
    flags/   flags.c/.h               world state
    puzzle/  puzzle.c/.h              plugin host
    board/   board.c/.h               case-board interpreter
    cut/     cut.c/.h                 cutscene interpreter
    art/     artkit.c npc.c artkit.h  ink & paper renderer
    audio/   synth.c/.h               SFX + generative music
    journal/ journal.c/.h             the book UI (reads; writes nothing)
    save/    save.c/.h                versioned save blob
    content/ content.c/.h             the baked blob and its readers
    generated/content_data.h          (built; never edited)
    puzzles/ p_<name>.c               one file per mini-game, self-registering
  content/
    dialogue/*.dlg  cutscenes/*.cut  boards/*.case  scenes/*.scn  strings/*.txt
  tests/
```

**Auto-discovery, no registries.** The Makefile globs `src/*/*.c` and bakery
globs `content/*/*` and `src/puzzles/p_*.c`. There is no hand-edited index file
anywhere, because a shared registry file is the classic multi-agent merge
hotspot.

**The flag store is the only cross-system state**: string-keyed ints,
namespaced (`ch1.otto.trust`, `clue.midnight_lantern`). Dialogue, boards,
cutscenes and scenes read and write flags exclusively through the flags API.
Saves serialise the flag store by name, so a save written by an older build
loads in a newer one with more content in it.

```c
void flags_reset(void);
int  flag_get(const char *name);
void flag_set(const char *name, int value);
void flag_add(const char *name, int delta);
void clue_grant(const char *id);
bool clue_has(const char *id);
int  clue_count(void);
const char *clue_at(int index);      /* discovery order, for the journal */
void trust_add(const char *npc, int n);
int  trust_get(const char *npc);
```

### 7.4 Puzzle plugin contract
```c
typedef enum { PZ_RUNNING, PZ_SOLVED, PZ_EXITED } pz_status;

typedef struct {
    unsigned  seed;        /* from the save: re-entry shows the same instance */
    int       difficulty;  /* 0..2 */
    Rectangle area;        /* the play area the host hands over */
    int       attempts;    /* 3 honest attempts unlock the skip */
    int       hint_tier;   /* 0..3, spent from feathers by the host */
    float     t;
} pz_ctx;

typedef struct {
    const char *id;                 /* "tart_tiling"     */
    const char *title;              /* shown in the frame */
    const char *clue_granted;       /* clue token on solve, may be NULL */
    void      (*init)(pz_ctx *);
    pz_status (*update)(pz_ctx *, float dt);
    void      (*draw)(pz_ctx *);
    const char *(*hint)(pz_ctx *, int tier);
    void      (*shutdown)(pz_ctx *);
    bool      (*solve_replay)(pz_ctx *);   /* [W0] the §10.5 fixture */
} pz_def;

void pz_register(const pz_def *);
#define PZ_REGISTER(sym)   /* constructor; called from p_<name>.c */
```

The host provides the standard frame — paper panel, title, hint button, skip
flow, attempt counting — so puzzle agents build ONLY the puzzle. Puzzles draw
exclusively through artkit and **may not touch flags**; the host grants
`clue_granted` on `PZ_SOLVED`.

**[W0]** `solve_replay` is a field on `pz_def` rather than a loose convention,
so §10.5 is a compile-time contract: bakery fails the build if a `p_*.c` has no
`solve_replay`, and `tests/test_puzzles.c` runs every fixture at every
difficulty across six seeds.

**[W0]** Walking out of a puzzle unsolved does not pay out the conversation
that started it; the payoff lines live on the far side of solving or skipping
it. Re-opening the conversation offers the puzzle again, so nothing can lock.

---

## 8. Art & audio kit

### 8.1 Ink & paper renderer
```c
void ink_stroke(const Vector2 *pts, int n, float w, float wobble, int seed, Color c);
void ink_line(...);  void ink_rect(...);  void ink_circle(...);
void ink_fill(const Vector2 *pts, int n, int hatch, int seed, Color c);
void ink_blob(float x, float y, float r, float squash, int seed, Color c);
void paper_panel(Rectangle r, float torn, int seed);
void paper_grain(float intensity);
void vignette(float amt);
void wind_lines(float t, float amt, Color c);
void doodle(int id, float x, float y, float scale, float rot, Color c);
float art_noise(int seed, int i);       /* stable: same inputs, same wobble */
```
All strokes have hand-drawn wobble from per-frame-stable noise: nothing
shimmers unless it asks to. Hatch styles: `HATCH_NONE / SOLID / DIAG / CROSS /
DOT / VERT`. Doodles are parametric, never sprites: lantern, fish, gear,
teacup, feather, key, prism, bird, flower, bread, book, clock, spool, bee,
star, wave, house, tree, cloud, envelope, anchor, lock.

Characters are **parametric paper-puppets**: `npc_draw(id, x, y, scale, pose,
emote, t)` builds each townsperson from a shape grammar (height, girth,
headwear, prop, stoop, coat, trim, hair). Eighteen characters from one system;
upgrading the grammar upgrades the whole cast at once.

Everything draws in a **1280×720 virtual canvas**, letterboxed to the window.
`art_mouse()` returns virtual coordinates, so no other system ever thinks about
window size.

### 8.2 Palette & hue progression
```c
void  palette_set_stage(int n);           /* 0 gray .. 6 full spectrum + gold */
int   palette_stage(void);
void  palette_bloom(float x, float y, int stage);   /* the flood */
Color pal_at(Color c, float x, float y);
```
**[W0]** Rather than a fixed table of semantic slots, colour resolution is
automatic: content authors pick honest full-spectrum colours, and `pal_at`
classifies each colour into a hue band (blue / yellow / red / green / violet /
gold, plus a neutral band for ink and paper) and drains it to luminance if the
player has not earned that band back. The consequence is the property the
gimmick needs: restoring a hue recolours the entire game, including content
written after the palette was designed, with zero content edits.

`palette_bloom` resolves per-shape by position, so the new hue physically
floods outward from the tower across a scene.

### 8.3 Portraits & emotes
Dialogue portraits are the paper-puppets at bust framing.
`neutral / happy / sad / worried / shifty / laughing / moved`, plus blink.
Emote changes are cutscene and dialogue commands.

### 8.4 Audio
Everything synthesised at startup. SFX: UI ticks, page turns, ink scritches
(dialogue blips pitched per speaker), puzzle chimes, the board fanfare, feather
sparkle, and the Prism-mend swell. Music is a three-voice pattern engine with a
mood table per district; it gains a voice and a brightness step per restored
hue, so the soundtrack recolours with the town. Music auto-ducks under
dialogue.

```c
void audio_init(void);  void audio_update(float dt);
void sfx_play(int id);  void sfx_blip(int npc_id);
void music_mood(int mood);  void music_duck(bool ducked);
```

---

## 9. Content formats (grammar is contract)

Common to all four: `#` starts a comment line, block headers are at column
zero, block bodies are indented.

**[W0] Line wrapping.** Content files wrap at eighty columns because that is
how you read a diff. A wrapped line is not a new beat: in `.dlg`, a line that
is not a verb, condition, choice, speaker or node header is appended to the
line above it, unless the line above finished its sentence and this one starts
with a capital. Paragraph = beat; line breaks are the writer's, not the
reader's.

**[W0] No string is ever cut off.** `TEXT_MAX` (512, in `content/content.h`)
is the longest single authored string the engine carries — one spoken beat,
one hotspot's flavour text, one board line — and every buffer that holds
prose is that size. Anything longer fails the build (§10.8) rather than
stopping mid-sentence on screen, which is the one content bug with no
symptom other than a character who trails off.

On screen the rule is the same. Dialogue and cutscene lines that outgrow
their panel **page** — a chevron appears, a click turns it — and the
surfaces that must show everything at once (a deduction scroll, the recap, a
hint note, a button label) step their type down until it fits. Nothing
clips, at any of the three text sizes.

### 9.1 Dialogue `.dlg`
```
@node otto_intro
  [if flag ch1.met_otto == 0]
  OTTO(worried): Cold morning, detective. Colder without the blue.
  YOU:
    * (kind) Everyone misses it, Mr. Brine.   -> otto_warm
    * (dry) The fish seem to be coping.       -> otto_chuckle
  [else]
  goto otto_after
  [end]

@node otto_warm
  OTTO(moved): ...Aye. Well. Ask your questions.
  trust otto 1
  grant clue.otto_alibi
  start_puzzle weights_sorting
```
Verbs: `grant <clue>...`, `set <flag> = <n>`, `add <flag> <n>`,
`trust <npc> <n>`, `start_puzzle <id>`, `play_cut <id>`, `open_board <id>`,
`goto <node>`, `end`.
Conditions: `[if <cond>]` / `[else]` / `[end]`, nestable to depth 8.
Speaker lines are `NAME(emote): text`; anything unrecognised is narration.
Choices are tone-tagged and may jump or fall through.

### 9.2 Case boards `.case`
```
board ch1_midnight
  title "The first floor: who was at the tower at midnight?"
  requires clue.otto_alibi clue.tansy_saw clue.ferry_log
  text "At midnight, {p1} climbed Tower Green"
  text "carrying {p2}."
  pool people  token.otto token.tansy token.iris
  pool objects token.fish_pail token.picks token.lantern
  blank p1 answer token.otto      pool people  from clue.otto_alibi clue.midnight_figure
  blank p2 answer token.fish_pail pool objects from clue.fish_pail clue.otto_alibi
  feedback grouped
  on_solved cut ch1_hue_blue          # or: on_solved goto <node>
```
**[W0] `pool` and `from` are additions.** `pool` makes each board's evidence
set self-contained and readable in one file. `from` names the independent
evidence sources behind each blank, which turns CANON §3.3's "multiple roads to
key answers" from a review note into the machine-checkable gate in §10.4 — and
gives the tier-2 blank hint something true to say.

Token and clue labels live in `content/strings/ui.txt` as `key = English`.

### 9.3 Cutscenes `.cut`
```
cut ch1_hue_blue
  BG ch1_dock
  TITLE "Chapter One" "The Harbour"
  ACTOR otto 560 534
  EMOTE otto worried
  SAY otto "So that is it, then. I am not the one."
  MOVE you 640 524 1.4
  WAIT 0.8
  BLOOM 1090 300 1
  EXIT
```
The complete verb set: `BG` `PAN` `CAM` `ACTOR` `MOVE` `EMOTE` `SAY` `WAIT`
`MUSIC` `SFX` `FADE` `HUE` `BLOOM` `DOODLE` `SHAKE` `TITLE` `EXIT`.
`WAIT` and `SAY` block; everything else is a tween or an instant. If a cutscene
needs a new verb, that is a cut-engine job card, not an inline hack — bakery
rejects unknown verbs. `SHAKE` and `PAN` collapse under reduce-motion, and
skipping a cutscene (`ESC`) still applies its `HUE`, `BLOOM` and `MUSIC`
changes, so a skipped scene can never leave the world half-changed.

### 9.4 Scenes `.scn`
```
scene ch1_dock
  title "Prismbrook Harbour — the Dock"
  bg harbor                       # backdrop composition
  music MOOD_HARBOR
  walk 110 454 1170 530           # walk plane, x1 y1 x2 y2
  spawn 180 524
  on_enter ch1_dock_open          # dialogue node, once
  on_enter_cut p_grayfall         # cutscene, once, before on_enter
  actor otto 470 524 stand
  hot 412 364 120 180 talk otto_intro "Otto Brine, fishmonger"
  hot 640 304 120 90  look "A slate by the fish stall..."
  hot 40  424 90  150 exit p_square "Back to the square"
  hot 500 464 280 90  board p_morning "The ground floor" if all clue.a clue.b
  feather 1010 284 f_ch1a
```
Hotspot kinds: `talk` `look` `exit` `board` `puzzle` `cut`. Any hotspot may
carry a trailing `if <cond>`.

### 9.5 Conditions (shared by `.dlg` and `.scn`)
```
flag <name> [<op> <n>]      clue <id>      trust <npc> <op> <n>
all <name>...               any <name>...          # [W0]
```
optionally prefixed with `not`. Operators: `== != >= <= > <`. A bare name is
true when non-zero. **[W0] `all`/`any`** exist because a chapter gate is
exactly "the whole clue set for this chapter is in the journal" (CANON §3.2),
and expressing that as six nested conditions in every scene file would be a
copy-paste hazard.

---

## 10. Quality gates (all run in `make check`; merge-blocking)

1. **Build clean** — `-Wall -Wextra -Werror`.
2. **Unit tests** — flags and the clue journal (`test_flags`); save round-trip
   including forward compatibility with flags this build has never heard of;
   the four grammars read back through the interpreters' own reader
   (`test_content`).
3. **Content lint (bakery)** — dangling ids (jump, cutscene, puzzle, scene,
   board), unreachable dialogue nodes, duplicate ids, unknown cutscene verbs,
   unknown hotspot kinds, orphan clues (granted but never used by any board or
   condition).
4. **Clue closure — the fairness enforcer.** For every board, every clue in
   `requires` and every clue in a blank's `from` must (a) be granted by some
   dialogue node or puzzle, and (b) be grantable in a chapter at or before that
   board's. Every blank must cite **at least two** independent sources, and
   every blank must actually appear in the board's text. A mystery game that
   can dead-end an honest player is broken; this makes it structurally
   impossible.
5. **Puzzle solvability** — every `p_*.c` ships a `solve_replay` fixture, and
   the harness runs all of them headless to `PZ_SOLVED` at difficulties 0–2
   across six seeds. Every hint tier must return a non-empty string.
6. **Determinism where seeded** — the same seed must produce the same puzzle
   instance, so re-entering a puzzle shows the instance you left.
7. **Size** — the shipped binary against the 1,474,560-byte ceiling.
8. **No truncation** — no authored string exceeds `TEXT_MAX`, measured both
   per line by bakery and per *joined beat* by the tests, because the
   interpreter joins wrapped lines and it is the joined paragraph that has to
   fit. The test prints the longest string in the game against the limit, so
   the headroom is visible rather than assumed.
