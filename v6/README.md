# HUEDUNIT (v6)

**A cozy detective puzzle-adventure. The town of Prismbrook has lost its
colour, its Tinter, and its nerve. You're here to find all three.**

One morning Prismbrook wakes the colour of a wet newspaper. The Prism is gone
from the lantern room, the Tinter is gone with it, the tower is locked from the
inside, and the only creature anyone has seen near it is a magpie the whole
town has decided is a thief.

Nobody done it. Everybody fixes it.

Single native executable, **264 KB**, no asset files of any kind. Every stroke
of the world is drawn from a vector vocabulary at runtime, every sound is
synthesised at startup, and the entire script — dialogue, scenes, boards,
cutscenes — is compiled into the binary at build time by our own tool.

## Build

```sh
make            # release: -Os -flto, stripped
make run        # build and play
make debug      # -O1 -g, warnings, no LTO
make check      # bake + tests + build + size gate
make size       # prints the shipped bytes and fails over 1,474,560
make clean
```

Needs a C11 compiler and raylib 5.x/6.x (`brew install raylib`, or your distro
package). The Makefile finds raylib through `pkg-config` and falls back to
`-lraylib`. macOS and Linux build out of the box.

Windows x64 is the primary ship target. Cross-compile with MinGW-w64 against a
MinGW raylib:

```sh
make windows RAYLIB_WIN=/path/to/mingw-raylib     # -> huedunit.exe
```

That link line is `-static -static-libgcc -mwindows`, so the result runs on a
stock Windows 10/11 machine with no DLLs beside it. The current macOS release
build is **264 KB** against a 1,474,560-byte ceiling.

## Playing

Mouse only, all the way through. Click to walk, click a person to talk, click
the sparkles because they are always something.

| Action | Input |
| --- | --- |
| Walk / talk / inspect | left click |
| Advance dialogue | click, `SPACE` or `ENTER` |
| Case Journal | `J` or `TAB` |
| Settings / menu | `ESC` |
| Skip a cutscene | `ESC` |
| Turn a puzzle piece | `R` (tart tray only) |
| Screenshot | `F12` |

Nothing in the game is timed, nothing has a fail state, and no wrong answer
ever takes anything away from you.

## The colour is the progress bar

The world is ink on gray paper. Every chapter you close restores one hue band
to the *entire* game — blue, then yellow, then red, then green, then violet,
then the full spectrum and gold — and the restoration is a flood that spreads
outward from the tower, shape by shape.

That is one mechanism, not six. Content authors pick honest colours; the
palette classifies each one into a hue band and drains it to luminance until
the player has earned that band back. So the harbour goes blue, Otto's coat
goes blue, and a scene written next month goes blue too, with no edits.

The music does the same thing. The three-voice pattern engine gains a voice
and a brightness step per restored hue, so the soundtrack recolours with the
town.

## The mystery is fair, and that is enforced

Every Case Board answer is deducible from evidence the player already has —
not as an aspiration, as a build failure.

`tools/bakery` compiles the content and proves, for every board, that every
clue it requires is grantable, that it is grantable in a chapter at or before
that board's, and that **every blank cites at least two independent evidence
sources**. It also proves no dialogue node is unreachable, no jump dangles, and
no clue is granted that nothing ever uses. `make check` runs it and 1,343 more
content assertions through the interpreters' own reader, plus 270 headless
puzzle solves.

A mystery game that can dead-end an honest player is broken. This one can't be.

## Layout

```
v6/
  Makefile
  docs/CANON.md          the world, the truth, the cast — frozen at Wave 0
  docs/API.md            module contracts and the four grammars — frozen
  tools/bakery/          content compiler, linters, clue-closure gate
  src/
    core/     main.c app.c        state machine, title, HUD, capture harness
    scene/    scene.c/.h          point-and-click screens, backdrops, walking
    dlg/      dlg.c/.h            dialogue interpreter
    board/    board.c/.h          Case Board interpreter
    cut/      cut.c/.h            cutscene interpreter
    puzzle/   puzzle.c/.h         plugin host: frame, hints, skip, attempts
    puzzles/  p_*.c               fifteen mini-games, one file each
    art/      artkit.c npc.c      ink vocabulary, palette, paper puppets
    audio/    synth.c/.h          every sound in the game, made at startup
    journal/  journal.c/.h        the book: People, Clues, Boards, Town
    flags/    flags.c/.h          the only cross-system state
    save/     save.c/.h           saves by name, forward-compatible forever
    content/  content.c/.h        the baked blob and its readers
  content/
    dialogue/*.dlg  scenes/*.scn  boards/*.case  cutscenes/*.cut  strings/*.txt
  tests/
```

Engine agents build interpreters; content agents write files; the two never
edit each other's territory. There is no registry file anywhere — the build
globs `src/*/*.c`, `src/puzzles/p_*.c` and `content/*/*`, so adding a mini-game
is exactly the act of adding one file.

## Tests

```sh
make check
```

- `test_flags` — the flag store: namespacing, clue idempotence and discovery
  order, trust, complete iteration (a save is "every non-zero flag by name", so
  incomplete iteration would silently eat a chapter), and refusing to wrap when
  full.
- `test_content` — 1,343 assertions over the real baked town: clue closure and
  the two-sources rule for all six boards, every blank present in its scroll
  and its answer present in its pool, every scene exit/talk/board target
  resolving, no dead-end screens, the closed cutscene verb set, every dialogue
  jump landing, every clue having English to show the player, the condition
  evaluator, and a save round-trip including a flag from a build that does not
  exist yet.
- `test_puzzles` — all 15 mini-games driven to solved with no window and no
  mouse, at three difficulties across six seeds (270 runs), plus every hint
  tier non-empty and every seeded puzzle proven deterministic.

## Captures

The integration cards want before/after pictures, so the game can photograph
itself:

```sh
./huedunit --shot title                 shot.png
./huedunit --shot scene:ch1_dock        dock.png
./huedunit --shot board:ch1_midnight    board.png
./huedunit --shot puzzle:mirror_light   nona.png
./huedunit --shot cut:f_festival        finale.png
```

It jumps straight to that screen, fabricates exactly enough world state to make
it honest, waits for the animation to settle, writes the PNG and exits.

## Scope note

`HUEDUNIT_req_design_gameplan.md` §0 says there is no size constraint on this
project. This build ships under the 1.44 MB floppy rule shared with v1–v5
anyway; the ceiling and the game's own "zero asset files" rule want the same
things, and there is 1.17 MB of headroom.

All names, text, characters, art and music are original.
