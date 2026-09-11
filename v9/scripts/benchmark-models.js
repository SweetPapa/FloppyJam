import {
  requestDialogue,
  requestSignal,
  diagnostics,
  validateDialogue,
} from "../src/llm.js";
import { newGame } from "../src/state.js";
import { createPuzzle, signalFallback } from "../src/puzzles.js";
import { CLUES } from "../src/story.js";
const model = process.argv[2],
  count = Number(process.argv[3] || 10),
  base = process.env.AFTERGLOW_MODEL_URL || "http://127.0.0.1:11434";
if (!model) {
  console.error(
    "Usage: node scripts/benchmark-models.js MODEL [dialogue count, default 10]",
  );
  process.exit(1);
}
const config = { model, base, temperature: 0.7, maxTokens: 160 };
let fallback = 0,
  invalid = 0;
const latencies = [];
for (let i = 0; i < count; i++) {
  const s = newGame();
  s.night = i % 5;
  const name = CLUES[s.night].holder;
  if (i % 2) s.facts = CLUES.slice(0, s.night + 1).map((c) => c.id);
  const start = performance.now(),
    reply = await requestDialogue(
      name,
      s,
      [
        "Hello. How is your work going?",
        "What have you noticed about the chalk lanterns?",
        "I am happy to help.",
      ][i % 3],
      config,
    );
  latencies.push(Math.round(performance.now() - start));
  if (reply.source === "authored fallback") fallback++;
  else if (!validateDialogue(reply, name, s)) invalid++;
  console.log(
    JSON.stringify({
      turn: i + 1,
      name,
      source: reply.source,
      ms: latencies.at(-1),
    }),
  );
}
const signalCount = Number(process.argv[4] || Math.min(count, 10));
let signalFallbacks = 0;
for (let i = 0; i < signalCount; i++) {
  const p = createPuzzle("signal", i % 3),
    reply = await requestSignal(p, config);
  if (!reply) signalFallbacks++;
  console.log(
    JSON.stringify({
      signal: i + 1,
      clue: (reply || signalFallback(p)).clue,
      authored: !reply,
    }),
  );
}
console.log(
  JSON.stringify(
    {
      model,
      turns: count,
      fallbacks: fallback,
      fallbackRate: fallback / count,
      invalidShipped: invalid,
      latencyMs: latencies,
      signalFallbacks,
      signalCount,
      reasons: diagnostics,
    },
    null,
    2,
  ),
);
