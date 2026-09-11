# AFTERGLOW

A cozy third-person mystery in a harbor drawn in pastel chalk. Five deliveries,
five kitchen-table puzzles, and a lighthouse that needs a little kindness.
Everything is generated in code: town, characters, weather, effects and music.
The complete story plays offline. Optional local or cloud model voices use an
OpenAI-compatible endpoint; models never control the mystery.

Requires Node **22.12+** and npm. From this directory:

```sh
nvm use                 # if you use nvm; .nvmrc selects Node 22
npm ci
make run                # http://127.0.0.1:5199
make desktop            # build and open the Electron desktop game
make web                # standalone browser build in dist/
make check              # pure tests, no-assets check, production build
npx playwright install chromium
make playtest           # actual keyboard campaign + screenshots + resume
make package            # native package for the current OS in release/
make size               # report bytes; no size ceiling
```

Use `PW_CHANNEL=chrome make playtest` to use an already installed Chrome.
The browser build needs HTTP hosting: `npm run preview`, or serve `dist/` with
any static server. All runtime code is bundled; no CDN or remote assets.

The desktop build produces macOS DMG/ZIP, Windows portable EXE, and Linux
AppImage/tar.gz. Build each target on its own OS. These self-contained packages
include Chromium, so they are much larger than the older raylib entries.
The build pipeline deliberately has no floppy size gate.

| Action | Keyboard |
| --- | --- |
| Wander / choose a reply | Arrows, or WASD |
| Talk, delivery, puzzle action | Enter, or Space |
| Pause, route, notebook, hints | Esc |
| Camera nudge | Q / E; optional pointer drag |
| Puzzle hint shortcut | H; also fully reachable through Esc |

The entire campaign, including assisted puzzle completion, needs only arrows,
Enter and Esc. The title’s comfort menu has left/right one-hand presets,
individual remapping, L/XL text, contrast, reduced motion, relaxed/lively play,
volume and scenery reshuffling. Typing is only an optional conversation feature.

Deliver to the neighbor marked on your route. Help with three rounds; their
secret and an authored fact enter your notebook. Connect the fact to its holder
on the case board, then return to the depot to sleep. Night five asks for the
Lamplighter. The Lighting Run takes you through all districts and ends at the
harbor with the town, music and credits. You can keep exploring afterwards.

The square arcade has unlocked puzzle replays, nine holes of Harbor Putt,
Lantern Rhythm, and Rosie’s chalk collection. These never block the story.
Hints offer three levels, then **Show me**. Assisted completion counts fully.

Three save slots store location, puzzle state, facts, pins, weather chapter,
music layers, collectibles and side progress. Saves live in localStorage
(browser: per origin; desktop: Electron’s application data directory). Closing
or pausing saves too. Settings and the optional API key stay on this device;
keys never enter campaign saves, prompts or diagnostic logs. A browser’s private
mode or cleared site data can remove its local saves.

For model voices, open **Comfort → Optional model voices**. Supply the server
base URL, exact installed model ID, and optional key, then Test connection.
Ollama commonly uses `http://localhost:11434`; LM Studio uses port 1234.
`/v1` is appended if absent. llama.cpp and vLLM use their configured port.
The adapter accepts SSE chat-completions responses, retries once without JSON
mode when necessary, and falls back after an eight-second total deadline.

Browser requests need CORS from the endpoint. For Ollama, configure
`OLLAMA_ORIGINS` for the browser origin and restart the server. HTTPS pages may
block an HTTP endpoint; localhost hosting or the desktop app avoids that.
Desktop uses a sandboxed preload network bridge, without disabling web security.
No endpoint is contacted until you configure one. There is no telemetry.

Model output is quarantined until validation, then rendered as chalk handwriting.
This consciously prioritizes preventing transient spoilers over displaying raw,
unvalidated streamed tokens. Conversations always have authored fallbacks.
Model performance acceptance rates are not claimed without real model testing.

`.github/workflows/afterglow.yml` runs checks and a full keyboard campaign,
builds the browser game, and packages on Windows, macOS and Linux. Each job
uploads reviewable artifacts and reports size. Pushing an `afterglow-v*` tag
publishes those artifacts with checksums; ordinary pushes do not publish.
Packages are currently unsigned. Existing repository signing scripts can be
used as a separate release step when signing this game is desired.

Source organization:

- `src/story.js`: authoritative bible, cast cards, facts, 60 phrases, word banks,
  seeded filler and fixed district anchors.
- `src/puzzles.js`, `src/state.js`: deterministic puzzle and campaign logic.
- `src/llm.js`: knowledge prompts, response parsing, validation and fallbacks.
- `src/world.js`, `src/audio.js`: procedural 3D town and synthesized sound.
- `src/main.js`, `src/style.css`: keyboard UI, story wiring and side attractions.
- `docs/CONTRACTS.md`: grounding decisions and module boundaries.
- `docs/VERIFICATION.md`: measured results and remaining acceptance work.

The earlier v1–v8 entries informed procedural assets, deterministic simulation,
headless testing, forgiving failure paths and reproducible local/CI commands.
AFTERGLOW uses Three.js/Electron for one desktop/browser code path and direct
model integration; it does not replace their C/raylib builds.

Local builds from this session are in `release/`: a macOS Apple silicon app,
DMG and ZIP, plus `AFTERGLOW-web.zip` and checksums. The browser files total
about 641 KB; the macOS runtime package is about 110 MB. The complete mystery is
playable, but this is a compact first release: the GDD’s longer playtime,
reference-hardware targets, full model acceptance benchmarks and Mom playtests
are still open, as detailed in the verification ledger.
