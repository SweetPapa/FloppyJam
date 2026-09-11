# Verification ledger

This is a playable first release, with the whole authored mystery wired from
arrival through Lantern Night and post-credits free exploration. The original
GDD remains the target; passing software tests is not the same as completing
the human, hardware and model acceptance work below.

## Automated and hands-on evidence

`npm test`: 35 pure tests cover all fifteen rounds, ordinary drop/drill
solutions, assisted completion, progression and final deduction, exact
serialized save round trips across three slots, deterministic town generation,
JSON/SSE parsing, structured output compatibility, and unreachable endpoints.

The validator suite submits 200 fabricated foreign-name turns and forbidden
clue reveals, plus 100 invalid Signal Words clues for each of three boards.
None of these adversarial fixtures passes validation. These are synthetic
fixtures, not a claim of 200 real-model conversations or universal semantic
validation. An allowlist and content patterns are necessarily conservative.

Playwright runs the actual rendered game at 1440×900 in Chrome/Chromium:

- title → all five nights → fifteen rounds → correct pins → sleep → five
  finale lanterns → credits, exclusively arrows/Enter/Esc, with an unreachable
  endpoint configured;
- all fifteen puzzles solved through ordinary keyboard actions with zero hints;
- in-progress puzzle reload and position restoration;
- Harbor Putt shot/roll, Lantern Rhythm scoring, mural UI;
- XL text, contrast, reduced motion, scenery reshuffle, and protection of a save
  while the new-story overwrite screen is pending;
- screenshots of the title, town, every puzzle type, fact cards and credits.

The tests inspect read-only development snapshots to choose legal puzzle inputs.
They never invoke hidden progression functions or write campaign state in the
campaign test. Hints and Show me are selected through the same pause menu a
player uses. Renderer benchmarks use an explicitly seeded rain fixture instead.

The packaged macOS app was launched with Electron automation; title → new game
→ pause works, no renderer exception was observed, `window.require` is absent,
and the preload model bridge is present. Windows/Linux packages are configured
in the CI matrix; they have not been executed on this Mac.

## Real local model smoke tests (September 6, 2026)

The installed Ollama models were tested without downloading any model.

| Configuration | Dialogue | Signal Words | Observation |
| --- | --- | --- | --- |
| gemma4:e4b-it-q8_0, cold | 5/5 fallback | 5/5 fallback | Eight-second deadline expired during model load. |
| lfm2.5-2.6b:latest | 5/5 fallback | 5/5 fallback | Its thinking consumed the 160-token response ceiling. |
| gemma4:e4b-it-q8_0, preloaded, thinking disabled, first warm sample | 3/5 accepted | 2/5 accepted | Dialogue responses 2.2–5.7 s when accepted; timeouts/validation trigger fallback. |
| Same warm configuration, second sample | 5/5 accepted | 2/5 accepted | Accepted dialogue completed in 2.28–3.24 s. |

Loading the 4B model separately took 16.6 seconds. The adapter now requests
`reasoning_effort: none` on its first attempt and drops optional compatibility
fields on retry. The final prompt also asks for shorter dialogue to leave room
for the JSON envelope and suggested replies. The warm samples above preceded
that final brevity change; they are not a measured rate for the final prompt.

No invalid generated response in these samples was shipped. Signal validation
remains particularly strict; the current sample does not meet the GDD’s model
fallback target. These small samples do not establish V-3 or V-5. No 27B model
was installed, and no download was initiated. A repeatable benchmark is included:

```sh
node scripts/benchmark-models.js MODEL_ID 200 100
# Defaults to local Ollama; AFTERGLOW_MODEL_URL overrides the base URL.
```

Model startup is outside the turn deadline: preload your chosen model in its
server, then use Test connection. Short, non-thinking responses suit this game.
The in-app text uses validated typewriting; raw streamed content is deliberately
quarantined, so the GDD’s visible-first-token target is not claimed.

## Acceptance work that remains

- **V-3 / V-5:** full 200-turn 4B/27B and 100-clue real-model benchmarks; tune
  prompts/dictionary coverage until the stated fallback rates are met.
- **V-6:** results from a renderer soak on this host are recorded separately;
  the specified reference integrated-GPU laptop and 2018 MacBook Air still need
  runs. Hardware-specific performance cannot be inferred from a different Mac.
- **V-9:** both Mom playtests. No human preference result is invented here.
- **Pacing:** the campaign currently uses compact authored rounds and direct
  routes. It does not yet establish the GDD’s 4–6 hour relaxed playtime. Human
  testing should guide additional round complexity and conversation variety;
  there are no artificial waiting periods.
- **Presentation scope:** capsule cast and portraits, adjustable chalk strokes,
  bloom, particles, weather and reflections are implemented. Rich facial
  expressions, accumulating rooftop snow, authored cinematic vignettes and a
  fully sung Bo performance remain simpler procedural interpretations.
- **Traversal scope:** slopes, rail rides, spring bounces, wet momentum and safe
  water recovery work. Roof-to-roof authored set pieces and spinning-door
  launches are not a bespoke campaign course in this release.
- **Distribution:** macOS is built and launched here; Windows/Linux run through
  the workflow matrix. All packages are unsigned until a separate signing step
  is configured. Nothing was published or deployed during development.

V-7 is scoped to v9 game sources: older entries already contain binary artwork.
The asset gate excludes dependencies and generated build/test output. Runtime
Three.js is bundled into the game, so its unneeded package assets are excluded
from desktop packaging. V-8 is byte equality of the pure layout and puzzle data;
it does not promise pixel equality across graphics drivers.

## Final local artifacts

The final local `npm run package -- --publish never` completed with all 35 pure
tests passing, the asset gate passing, and a successful production build.
The five Playwright scenarios passed in 2.1 minutes. The packaged macOS smoke
check passed again after the final build in a fresh temporary profile.

| Artifact | Bytes |
| --- | ---: |
| Browser files, unpacked | 640,913 |
| AFTERGLOW-web.zip | 174,999 |
| macOS Apple silicon ZIP | 106,821,679 |
| macOS Apple silicon DMG | 110,429,759 |

The game’s ASAR is about 630 KB; the rest of the desktop distribution is the
Electron runtime. Local distribution files and SHA256SUMS.txt are in `release/`
and are deliberately gitignored. Generated screenshots remain in `test-results/`.
The package is unsigned and was not uploaded anywhere.

The final full browser run preceded only the last zero-temperature bug fix and
local-model help text change; the final package reran the pure tests, asset check
and build, and was then launched successfully. The renderer soak below uses the
final source. Earlier runs during active edits were discarded and rerun; they
are not used as final verification evidence.

## Five-minute rain soak, final source

At **1920×1080**, Cannery Row with rain active ran for **300.005 seconds**:
17,972 rendered frames, **59.91 FPS mean**, 16.8 ms at both the 95th and 99th
frame-time percentiles. The renderer reported **ANGLE Metal / Apple M4 Pro**.
The final rain screenshot was visually inspected. This meets the practical
60 Hz target on this host; it is not evidence for the specified older laptop.

```sh
PW_CHANNEL=chrome node scripts/benchmark-renderer.js 300
```

The development renderer's draw-call snapshot reports the final composer pass,
not the whole scene, so its `drawCalls` / `triangles` fields were not used for
performance conclusions. The benchmark measures raw requestAnimationFrame
timestamps independently of the simulation timestep cap.
