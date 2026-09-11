import test from "node:test";
import assert from "node:assert/strict";
import {
  createPuzzle,
  inputPuzzle,
  autosolvePuzzle,
  hintPuzzle,
  traceLight,
  tickPuzzle,
  signalFallback,
} from "../src/puzzles.js";
import {
  newGame,
  beginPuzzle,
  finishRound,
  pinClue,
  sleepNight,
  saveGame,
  loadGame,
  lightLantern,
  completeGame,
} from "../src/state.js";
import {
  CLUES,
  DISTRICTS,
  decorations,
  unlockedKnowledge,
} from "../src/story.js";
import {
  validateDialogue,
  validateSignal,
  assemblePrompt,
  parseObject,
  parseSSE,
  endpoint,
  requestDialogue,
} from "../src/llm.js";
const kinds = ["phrase", "stack", "drill", "light", "signal"];
for (const kind of kinds)
  for (let round = 0; round < 3; round++)
    test(`${kind} round ${round + 1}: solvable, serializable and deterministic`, () => {
      const p = createPuzzle(kind, round, 927);
      assert.deepEqual(p, createPuzzle(kind, round, 927));
      assert.deepEqual(JSON.parse(JSON.stringify(p)), p);
      autosolvePuzzle(p);
      assert.equal(p.solved, true);
    });
test("all five nights require three rounds and a correct case pin", () => {
  const s = newGame();
  for (let night = 0; night < 5; night++) {
    assert.equal(s.night, night);
    assert.equal(sleepNight(s), false);
    assert.equal(pinClue(s, CLUES[night].id, CLUES[night].clear), false);
    for (let round = 0; round < 3; round++) {
      beginPuzzle(s);
      assert.equal(finishRound(s), false);
      autosolvePuzzle(s.puzzle);
      assert.equal(finishRound(s), round === 2 ? "fact" : "round");
    }
    assert.equal(s.stage, "board");
    assert.equal(pinClue(s, CLUES[night].id, "Ferris"), false);
    assert.equal(pinClue(s, CLUES[night].id, CLUES[night].clear), true);
    if (night < 4) assert.equal(sleepNight(s), true);
  }
  assert.equal(s.stage, "finale");
  assert.equal(completeGame(s), false);
  for (let i = 0; i < 5; i++) {
    assert.equal(lightLantern(s, i), true);
    assert.equal(lightLantern(s, i), false);
  }
  assert.equal(completeGame(s), true);
  assert.equal(s.completed, true);
});
test("all puzzle rounds and fact cards survive exact save/resume", () => {
  const map = new Map(),
    storage = { setItem: (k, v) => map.set(k, v), getItem: (k) => map.get(k) };
  for (let slot = 0; slot < 3; slot++) {
    const s = newGame(slot);
    for (let n = 0; n < 5; n++) {
      s.position = { x: 12.25 + n, y: 1.3, z: -20.1 };
      for (let r = 0; r < 3; r++) {
        beginPuzzle(s);
        inputPuzzle(s.puzzle, "right");
        hintPuzzle(s.puzzle);
        saveGame(storage, s);
        assert.deepEqual(loadGame(storage, slot), s);
        autosolvePuzzle(s.puzzle);
        finishRound(s);
        saveGame(storage, s);
        assert.deepEqual(loadGame(storage, slot), s);
      }
      pinClue(s, CLUES[n].id, CLUES[n].clear);
      sleepNight(s);
    }
  }
});
test("malformed saves are rejected and quota failure is reported", () => {
  assert.equal(loadGame({ getItem: () => "{broken" }, 0), null);
  assert.equal(
    loadGame({ getItem: () => JSON.stringify({ ...newGame(), night: 99 }) }, 0),
    null,
  );
  assert.equal(
    saveGame(
      {
        setItem: () => {
          throw Error();
        },
      },
      newGame(),
    ),
    false,
  );
});
test("town seed changes filler only; two fresh layouts match byte for byte", () => {
  assert.equal(
    JSON.stringify(decorations(927)),
    JSON.stringify(decorations(927)),
  );
  assert.notDeepEqual(decorations(927), decorations(928));
  assert.equal(DISTRICTS[4].anchor, "The Lighthouse");
});
test("phrase wrong letters cannot fail the story", () => {
  const p = createPuzzle("phrase");
  for (let i = 0; i < 26; i++) {
    p.cursor = i;
    inputPuzzle(p, "confirm");
  }
  assert.equal(p.solved, true);
});
test("stack connected fours pop and other dock groups remain", () => {
  const p = createPuzzle("stack");
  p.cursor = 0;
  inputPuzzle(p, "confirm");
  assert.ok(p.cleared >= 2);
  assert.ok(p.score > 0);
});
test("drill brick takes three hits and relaxed oil never drains", () => {
  const p = createPuzzle("drill", 2);
  p.board[10] = { color: 0, hits: 3 };
  p.dir = "down";
  inputPuzzle(p, "confirm");
  assert.equal(p.board[10].hits, 2);
  inputPuzzle(p, "confirm");
  assert.equal(p.board[10].hits, 1);
  inputPuzzle(p, "confirm");
  assert.ok(p.y > 0);
  tickPuzzle(p, 10000);
  assert.equal(p.oil, 100);
});
test("light boards are initially unsolved and require all matching targets", () => {
  for (let r = 0; r < 3; r++) {
    const p = createPuzzle("light", r);
    assert.notEqual(traceLight(p).hit.length, p.targets.length);
    autosolvePuzzle(p);
    assert.equal(traceLight(p).hit.length, r + 1);
  }
  const p = createPuzzle("light", 2);
  autosolvePuzzle(p);
  p.targets[0].color = 2;
  assert.equal(traceLight(p).hit.length, 2);
});
test("signal foghorn ends a turn without losing or revealing a beacon", () => {
  const p = createPuzzle("signal");
  p.cursor = p.board.findIndex((c) => c.type === "horn");
  inputPuzzle(p, "confirm");
  assert.equal(p.turn, 2);
  assert.equal(p.found, 0);
  assert.equal(p.solved, false);
  autosolvePuzzle(p);
  assert.equal(p.found, 9);
});
test("three hints unlock assistance; autosolve uses legal progression", () => {
  for (const k of kinds) {
    const p = createPuzzle(k);
    for (let i = 0; i < 3; i++) hintPuzzle(p);
    assert.equal(p.hints, 3);
    autosolvePuzzle(p);
    assert.equal(p.solved, true);
  }
});
test("locked knowledge never enters character prompts", () => {
  const s = newGame();
  for (const d of DISTRICTS) {
    const prompt = JSON.stringify(assemblePrompt(d.npc, s, "Hello"));
    assert.ok(!prompt.includes("Ada is the Lamplighter"));
    assert.ok(!prompt.includes("model train"));
  }
  s.facts = ["left-hand"];
  assert.ok(unlockedKnowledge("Odette", s).join(" ").includes("cookbook"));
  assert.ok(!unlockedKnowledge("Ada", s).join(" ").includes("Lamplighter"));
  s.night = 4;
  s.facts.push("ada");
  assert.ok(
    unlockedKnowledge("Ada", s).join(" ").includes("Ada is the Lamplighter"),
  );
});
const valid = {
  say: "Hello, Kit. Welcome to Brightwater.",
  mood: "warm",
  reveal_clue_ids: [],
  suggested_replies: ["Thank you.", "How are you?", "Tell me about your work."],
  end_conversation: false,
};
test("200 invalid model turns: no foreign entities or unauthorized clue IDs ship", () => {
  const s = newGame();
  assert.ok(validateDialogue(valid, "Odette", s));
  for (let i = 0; i < 200; i++) {
    assert.equal(
      validateDialogue(
        {
          ...valid,
          say: `Hello from Zorbland${String.fromCharCode(65 + (i % 26))}.`,
        },
        "Odette",
        s,
      ),
      null,
    );
    assert.equal(
      validateDialogue(
        { ...valid, reveal_clue_ids: [CLUES[i % 5].id] },
        "Odette",
        s,
      ),
      null,
    );
  }
  assert.equal(
    validateDialogue({ ...valid, say: "Ada is the Lamplighter." }, "Ada", s),
    null,
  );
  assert.equal(
    validateDialogue({ ...valid, say: "A ghost is here." }, "Ada", s),
    null,
  );
});
test("only the holder can return an unlocked clue", () => {
  const s = newGame();
  s.facts = ["left-hand"];
  assert.ok(
    validateDialogue({ ...valid, reveal_clue_ids: ["left-hand"] }, "Odette", s),
  );
  assert.equal(
    validateDialogue({ ...valid, reveal_clue_ids: ["left-hand"] }, "Bo", s),
    null,
  );
});
test("signal validator blocks every illegal target and board-word clue", () => {
  for (let r = 0; r < 3; r++) {
    const p = createPuzzle("signal", r);
    assert.ok(validateSignal(signalFallback(p), p));
    for (let n = 0; n < 100; n++) {
      assert.equal(
        validateSignal(
          {
            clue: p.board[n % 25].word,
            count: 1,
            targets: [p.board.find((c) => c.type === "beacon").word],
          },
          p,
        ),
        null,
      );
      assert.equal(
        validateSignal(
          {
            clue: "VESSEL",
            count: 1,
            targets: [p.board.find((c) => c.type === "fog").word],
          },
          p,
        ),
        null,
      );
    }
    assert.equal(
      validateSignal({ clue: "MADEUPWORD", count: 1, targets: ["BOAT"] }, p),
      null,
    );
  }
});
test("tolerant JSON and SSE parsing preserve response content", () => {
  assert.deepEqual(parseObject('```json\n{"say":"Hello"}\n```'), {
    say: "Hello",
  });
  assert.equal(parseObject("oops"), null);
  assert.equal(
    parseSSE(
      'data: {"choices":[{"delta":{"content":"Hel"}}]}\n\ndata: {"choices":[{"delta":{"content":"lo"}}]}\n\ndata: [DONE]\n',
    ),
    "Hello",
  );
});
test("adapter validates endpoints and falls back on refused connections", async () => {
  assert.equal(
    endpoint("http://localhost:11434"),
    "http://localhost:11434/v1/chat/completions",
  );
  assert.equal(
    endpoint("https://example.com/v1/"),
    "https://example.com/v1/chat/completions",
  );
  assert.throws(() => endpoint("file:///etc/passwd"));
  const before = Date.now();
  const reply = await requestDialogue("Odette", newGame(), "Hello", {
    base: "http://127.0.0.1:1",
    model: "missing",
  });
  assert.equal(reply.source, "authored fallback");
  assert.ok(Date.now() - before < 8500);
});
import { stackMove } from "./solver.js";
test("all crate lanes can be solved by normal drops without hints", () => {
  for (let round = 0; round < 3; round++) {
    const p = createPuzzle("stack", round);
    let n = 0;
    while (!p.solved && n++ < 200) {
      const move = stackMove(p);
      p.cursor = move.col;
      p.rotation = move.rot;
      inputPuzzle(p, "confirm");
    }
    assert.ok(
      p.solved,
      `round ${round}, ${p.remaining} remain after ${n} drops`,
    );
  }
});
test("all drilling rounds reachable with ordinary downward inputs", () => {
  for (let round = 0; round < 3; round++) {
    const p = createPuzzle("drill", round);
    for (let n = 0; n < 100 && !p.solved; n++) {
      inputPuzzle(p, "down");
      inputPuzzle(p, "confirm");
    }
    assert.ok(p.solved);
  }
});
test("adapter consumes SSE and retries invalid entities before accepting a valid line", async () => {
  const original = globalThis.fetch;
  let requests = 0;
  globalThis.fetch = async () => {
    requests++;
    const value =
      requests === 1 ? { ...valid, say: "Welcome to Zorbland." } : valid;
    const content = JSON.stringify(value);
    return new Response(
      `data: ${JSON.stringify({ choices: [{ delta: { content: content.slice(0, 20) } }] })}\n\ndata: ${JSON.stringify({ choices: [{ delta: { content: content.slice(20) } }] })}\n\ndata: [DONE]\n`,
      { headers: { "Content-Type": "text/event-stream" } },
    );
  };
  try {
    const r = await requestDialogue("Odette", newGame(), "Hello", {
      base: "http://localhost:11434",
      model: "mock",
    });
    assert.equal(requests, 2);
    assert.equal(r.say, valid.say);
    assert.equal(r.source, "mock");
  } finally {
    globalThis.fetch = original;
  }
});
test("adapter retries without JSON mode for servers that reject response_format", async () => {
  const original = globalThis.fetch;
  let calls = 0;
  globalThis.fetch = async (_url, options) => {
    calls++;
    if (JSON.parse(options.body).response_format)
      return new Response("", { status: 400 });
    return new Response(
      JSON.stringify({
        choices: [{ message: { content: JSON.stringify(valid) } }],
      }),
      { headers: { "Content-Type": "application/json" } },
    );
  };
  try {
    assert.equal(
      (
        await requestDialogue("Odette", newGame(), "Hello", {
          base: "http://localhost:11434",
          model: "mock",
        })
      ).source,
      "mock",
    );
    assert.equal(calls, 2);
  } finally {
    globalThis.fetch = original;
  }
});
