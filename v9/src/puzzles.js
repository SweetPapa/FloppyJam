import { PHRASES, SIGNAL_BANKS, rng, shuffle } from "./story.js";
export const PUZZLE_NAMES = {
  phrase: "Chalk Menu",
  stack: "The Stack",
  drill: "The Descent",
  light: "Light Routing",
  signal: "Signal Words",
};
export const RULES = {
  phrase:
    "Restore the bakery phrase. Choose a letter with the arrows, then press Enter. Wrong guesses only dim a lantern.",
  stack:
    "Move the hovering pair with Left / Right; Up rotates it. Enter drops it. Four connected crates of one color pop; clear the marked dock crates.",
  drill:
    "Arrows choose a neighboring block. Enter drills its whole color group. Clear a route to the parcel at the bottom; brick needs three taps.",
  light:
    "Move around the grid with the arrows. Enter turns a mirror or splitter. Route the live beams to every matching eyepiece.",
  signal:
    "Ada gives a clue and a number. Move with the arrows and choose words with Enter. Find nine beacons; fog and the foghorn only end a turn.",
};
export function createPuzzle(kind, round = 0, seed = 927, lively = false) {
  const p = {
    kind,
    round,
    seed,
    lively,
    cursor: 0,
    hints: 0,
    solved: false,
    moves: 0,
    elapsed: 0,
    message: "",
    score: 0,
  };
  const random = rng(seed + round * 73);
  if (kind === "phrase") {
    p.phrase = PHRASES[round % 3][Math.floor(random() * 20)];
    p.guessed = [];
    p.wrong = 0;
  }
  if (kind === "stack") {
    p.width = 6;
    p.height = 10;
    p.board = Array(60).fill(null);
    p.rotation = 0;
    p.cursor = 2;
    p.pairIndex = 0;
    p.cleared = 0;
    p.colors = round ? 5 : 4;
    for (let c = 0; c < 6; c++) {
      for (let k = 0; k < 2 + round * 2; k++) {
        const color = (c + Math.floor(k / 3)) % p.colors;
        p.board[(9 - k) * 6 + c] = { color, goal: true };
      }
    }
    p.remaining = p.board.filter(Boolean).length;
    p.pair = [0, 0];
  }
  if (kind === "drill") {
    p.width = 7;
    p.height = 12 + round * 4;
    p.x = 3;
    p.y = 0;
    p.cursor = 3;
    p.oil = 100;
    p.board = Array.from({ length: p.width * p.height }, (_, i) =>
      i < 7
        ? null
        : {
            color: Math.floor(random() * 4),
            hits: round === 2 && i % 13 === 0 ? 3 : 1,
            oil: i % 19 === 0,
          },
    );
    p.target = (p.height - 1) * 7 + 3;
    p.dir = "down";
  }
  if (kind === "light") Object.assign(p, lightBoard(round));
  if (kind === "signal") {
    const bank = SIGNAL_BANKS[round % 3];
    p.theme = bank.theme;
    p.board = shuffle(
      [
        ...bank.groups.flatMap((g) =>
          g.words.map((word) => ({ word, type: "beacon", group: g.clue })),
        ),
        ...bank.fog.map((word) => ({ word, type: "fog" })),
        { word: bank.horn, type: "horn" },
      ],
      random,
    );
    p.revealed = [];
    p.turn = 1;
    p.found = 0;
    p.clue = signalFallback(p);
    p.guessesLeft = p.clue.count;
  }
  return p;
}
// Mirrors reflect both ways; splitters continue straight and branch clockwise.
// Three independently colored lanes make the last board's color constraint explicit.
function lightBoard(round) {
  const width = 6,
    board = Array.from({ length: 36 }, () => ({ type: "empty", rot: 0 }));
  const targets = [];
  const sources = [];
  const solution = {};
  if (round === 0) {
    sources.push({ x: -1, y: 4, dx: 1, dy: 0, color: 0 });
    board[4 * 6 + 3] = { type: "mirror", rot: 1 };
    solution[27] = 0;
    targets.push({ x: 3, y: 0, color: 0 });
  } else if (round === 1) {
    sources.push({ x: -1, y: 3, dx: 1, dy: 0, color: 0 });
    board[3 * 6 + 2] = { type: "splitter", rot: 0 };
    solution[20] = 1;
    board[3 * 6 + 4] = { type: "mirror", rot: 1 };
    solution[22] = 0;
    targets.push({ x: 2, y: 0, color: 0 }, { x: 4, y: 0, color: 0 });
    board[5 * 6] = { type: "wall", rot: 0 };
  } else {
    for (let j = 0; j < 3; j++) {
      sources.push({ x: -1, y: j + 3, dx: 1, dy: 0, color: j });
      const x = 5 - j;
      const index = (j + 3) * 6 + x;
      board[index] = { type: "mirror", rot: 1 };
      solution[index] = 0;
      board[(j + 3) * 6 + 1] = { type: "lens", color: j, rot: 0 };
      targets.push({ x, y: 0, color: j });
    }
    board[2 * 6 + 1] = { type: "wall", rot: 0 };
  }
  return {
    width,
    board,
    targets,
    sources,
    solution,
    cursor: round === 0 ? 27 : round === 1 ? 20 : 23,
  };
}
export function traceLight(p) {
  const segments = [],
    hit = new Set(),
    visited = new Set();
  const queue = p.sources.map((x) => ({ ...x }));
  while (queue.length && segments.length < 250) {
    let ray = queue.shift();
    const x = ray.x + ray.dx,
      y = ray.y + ray.dy;
    if (x < 0 || y < 0 || x >= 6 || y >= 6) continue;
    const k = `${x},${y},${ray.dx},${ray.dy},${ray.color}`;
    if (visited.has(k)) continue;
    visited.add(k);
    segments.push({ x1: ray.x, y1: ray.y, x2: x, y2: y, color: ray.color });
    const target = p.targets.findIndex(
      (t) => t.x === x && t.y === y && t.color === ray.color,
    );
    if (target >= 0) hit.add(target);
    const tile = p.board[y * 6 + x];
    let dx = ray.dx,
      dy = ray.dy,
      color = ray.color;
    if (tile.type === "wall") continue;
    if (tile.type === "mirror") {
      if (tile.rot % 2 === 0) [dx, dy] = [-dy, -dx];
      else [dx, dy] = [dy, dx];
    }
    if (tile.type === "lens") color = tile.color;
    if (tile.type === "splitter") {
      const sign = tile.rot % 2 === 0 ? 1 : -1;
      queue.push({ x, y, dx: -dy * sign, dy: dx * sign, color });
    }
    queue.push({ x, y, dx, dy, color });
  }
  return { segments, hit: [...hit] };
}
export function signalFallback(p) {
  const b = SIGNAL_BANKS[p.round % 3];
  for (const group of b.groups) {
    const targets = group.words.filter(
      (w) => !p.revealed.includes(p.board.findIndex((c) => c.word === w)),
    );
    if (targets.length)
      return { clue: group.clue, count: targets.length, targets };
  }
  return { clue: "COMPLETE", count: 0, targets: [] };
}
function gravity(p) {
  for (let x = 0; x < 6; x++) {
    let write = 9;
    for (let y = 9; y >= 0; y--)
      if (p.board[y * 6 + x]) {
        const tile = p.board[y * 6 + x];
        p.board[y * 6 + x] = null;
        p.board[write-- * 6 + x] = tile;
      }
  }
}
function settleStack(p) {
  let chain = 0;
  gravity(p);
  for (let iter = 0; iter < 20; iter++) {
    const seen = new Set(),
      pop = [];
    for (let i = 0; i < 60; i++) {
      if (!p.board[i] || seen.has(i)) continue;
      const group = [],
        q = [i],
        color = p.board[i].color;
      seen.add(i);
      while (q.length) {
        const j = q.pop();
        group.push(j);
        for (const n of neighbors(j, 6, 10)) {
          if (!seen.has(n) && p.board[n]?.color === color) {
            seen.add(n);
            q.push(n);
          }
        }
      }
      if (group.length >= 4) pop.push(...group);
    }
    if (!pop.length) break;
    chain++;
    for (const i of pop) {
      if (p.board[i].goal) p.cleared++;
      p.board[i] = null;
    }
    p.score += pop.length * 10 * chain;
    gravity(p);
  }
  p.remaining = p.board.filter((c) => c?.goal).length;
  p.solved = p.remaining === 0;
  p.message = chain
    ? `${chain > 1 ? `${chain}-step chain! ` : ""}A little more room on the dock.`
    : "The crates settle. Try a matching color.";
}
function neighbors(i, w, h) {
  const out = [];
  if (i % w > 0) out.push(i - 1);
  if (i % w < w - 1) out.push(i + 1);
  if (i >= w) out.push(i - w);
  if (i < w * (h - 1)) out.push(i + w);
  return out;
}
function dropStack(p) {
  const cols =
    p.rotation % 2
      ? [p.cursor, Math.min(5, p.cursor + 1)]
      : [p.cursor, p.cursor];
  for (let j = 0; j < 2; j++) {
    let y = 9;
    while (y >= 0 && p.board[y * 6 + cols[j]]) y--;
    if (y < 0) {
      p.message = "Bo makes a little room. No crates lost.";
      for (let row = 0; row < 9; row++)
        if (!p.board[row * 6 + cols[j]]?.goal)
          p.board[row * 6 + cols[j]] = null;
      y = 0;
      while (p.board[y * 6 + cols[j]] && y < 9) y++;
      if (p.board[y * 6 + cols[j]]) continue;
    }
    p.board[y * 6 + cols[j]] = { color: p.pair[j], goal: false };
  }
  settleStack(p);
  p.pairIndex++;
  const c = p.pairIndex % p.colors;
  p.pair = [c, c];
}
function drill(p) {
  let dx = p.dir === "left" ? -1 : p.dir === "right" ? 1 : 0,
    dy = p.dir === "up" ? -1 : p.dir === "down" ? 1 : 0;
  const x = p.x + dx,
    y = p.y + dy;
  if (x < 0 || x >= 7 || y < 0 || y >= p.height) return;
  const i = y * 7 + x,
    tile = p.board[i];
  if (tile?.hits > 1) {
    tile.hits--;
    p.message = `Brick: ${tile.hits} more taps.`;
    return;
  }
  if (tile) {
    const q = [i],
      seen = new Set(q),
      color = tile.color;
    while (q.length) {
      const j = q.pop(),
        t = p.board[j];
      if (t?.oil) p.oil = Math.min(100, p.oil + 25);
      p.board[j] = null;
      p.score += 5;
      for (const n of neighbors(j, 7, p.height))
        if (
          !seen.has(n) &&
          p.board[n]?.color === color &&
          p.board[n].hits === 1
        ) {
          seen.add(n);
          q.push(n);
        }
    }
  }
  p.x = x;
  p.y = y;
  while (p.y < p.height - 1 && !p.board[(p.y + 1) * 7 + p.x]) p.y++;
  // Settle unsupported blocks above the player. A block meeting the lantern slides
  // to a free neighbor; if both sides are full Pearl's cart catches it safely.
  for (let col = 0; col < 7; col++)
    for (let row = p.y - 1; row >= 1; row--) {
      const from = row * 7 + col;
      if (!p.board[from]) continue;
      let to = row;
      while (
        to + 1 < p.height &&
        !p.board[(to + 1) * 7 + col] &&
        !(col === p.x && to + 1 >= p.y)
      )
        to++;
      if (to !== row) {
        p.board[to * 7 + col] = p.board[from];
        p.board[from] = null;
      }
    }
  p.solved = p.y === p.height - 1;
  p.cursor = Math.max(0, Math.min(p.board.length - 1, (p.y + 1) * 7 + p.x));
  p.message = p.solved
    ? "Parcel recovered! Pearl’s cart is waiting."
    : "Connected colors clear together. Keep heading down.";
}
export function inputPuzzle(p, action) {
  if (p.solved) return;
  p.moves += action === "confirm" ? 1 : 0;
  const d = { left: -1, right: 1, up: -1, down: 1 }[action];
  if (p.kind === "phrase") {
    if (d) p.cursor = (p.cursor + d + 26) % 26;
    if (action === "confirm") {
      const c = String.fromCharCode(65 + p.cursor);
      if (!p.guessed.includes(c)) {
        p.guessed.push(c);
        if (!p.phrase.includes(c)) p.wrong++;
      }
      p.solved = [...p.phrase].every((c) => c === " " || p.guessed.includes(c));
      p.message = p.phrase.includes(c)
        ? "A little chalk, a little clearer."
        : "That letter can rest. There is no hurry.";
    }
  }
  if (p.kind === "stack") {
    if (action === "left" || action === "right")
      p.cursor = Math.max(0, Math.min(p.rotation % 2 ? 4 : 5, p.cursor + d));
    if (action === "up" || action === "down") {
      p.rotation = 1 - p.rotation;
      p.cursor = Math.min(p.cursor, p.rotation ? 4 : 5);
    }
    if (action === "confirm") dropStack(p);
  }
  if (p.kind === "drill") {
    if (d) {
      p.dir = action;
      const dx = action === "left" ? -1 : action === "right" ? 1 : 0,
        dy = action === "up" ? -1 : action === "down" ? 1 : 0;
      p.cursor = Math.max(
        0,
        Math.min(
          p.board.length - 1,
          (p.y + dy) * 7 + Math.max(0, Math.min(6, p.x + dx)),
        ),
      );
    }
    if (action === "confirm") drill(p);
  }
  if (p.kind === "light" || p.kind === "signal") {
    const w = p.kind === "light" ? 6 : 5;
    if (d) {
      const x = p.cursor % w,
        y = Math.floor(p.cursor / w);
      p.cursor =
        action === "left"
          ? y * w + ((x + w - 1) % w)
          : action === "right"
            ? y * w + ((x + 1) % w)
            : action === "up"
              ? ((y + w - 1) % w) * w + x
              : ((y + 1) % w) * w + x;
    }
    if (action === "confirm" && p.kind === "light") {
      const t = p.board[p.cursor];
      if (t.type === "mirror" || t.type === "splitter") t.rot = 1 - t.rot;
      p.solved = traceLight(p).hit.length === p.targets.length;
      p.message = p.solved
        ? "Every eyepiece is glowing."
        : "Follow the beam; a quarter turn changes its path.";
    }
    if (
      action === "confirm" &&
      p.kind === "signal" &&
      !p.revealed.includes(p.cursor)
    ) {
      p.revealed.push(p.cursor);
      const c = p.board[p.cursor];
      if (c.type === "beacon") {
        p.found++;
        p.guessesLeft--;
        p.message = "A beacon! We make a good team.";
      } else {
        p.guessesLeft = 0;
        p.message =
          c.type === "horn"
            ? "Poooot! Ada smiles. Just the foghorn. Another clue?"
            : "A patch of fog. No harm done; let us try another clue.";
      }
      p.solved = p.found === 9;
      if (
        p.guessesLeft <= 0 ||
        !p.clue.targets.some(
          (w) => !p.revealed.includes(p.board.findIndex((c) => c.word === w)),
        )
      ) {
        p.turn++;
        p.clue = signalFallback(p);
        p.guessesLeft = p.clue.count;
      }
    }
  }
}
export function tickPuzzle(p, dt) {
  if (!p.lively || p.solved) return;
  p.elapsed += dt;
  if (p.kind === "stack" && p.elapsed >= 6) {
    p.elapsed = 0;
    dropStack(p);
  }
  if (p.kind === "drill") {
    p.oil = Math.max(0, p.oil - dt * 0.6);
    if (!p.oil) {
      p.oil = 100;
      p.message = "Pearl tops up your lantern. Back to it.";
      p.score = Math.max(0, p.score - 10);
    }
  }
}
export function hintPuzzle(p) {
  p.hints = Math.min(3, p.hints + 1);
  const hints = {
    phrase: [
      "Start with vowels.",
      "Look for E, A, and the short words.",
      "I will chalk in one missing letter.",
    ],
    stack: [
      "Connect four crates of the same color.",
      "Try dropping the pair beside a matching stack.",
      "Bo will sort one color group for you.",
    ],
    drill: [
      "Head down toward the parcel.",
      "Large color groups make more room.",
      "Pearl will clear the next step below you.",
    ],
    light: [
      "Follow the beam from its source.",
      "A slash mirror turns a rightward beam upward.",
      "Idris will turn one misplaced tile for you.",
    ],
    signal: [
      "Think about what the clue words have in common.",
      "Ada’s clue points only to beacons.",
      "Ada will point out one beacon for you.",
    ],
  };
  p.message = hints[p.kind][p.hints - 1];
  if (p.hints === 3) assistStep(p);
  return p.message;
}
export function assistStep(p) {
  if (p.kind === "phrase") {
    const c = [...p.phrase].find((c) => c !== " " && !p.guessed.includes(c));
    if (c) {
      p.cursor = c.charCodeAt() - 65;
      inputPuzzle(p, "confirm");
    }
  }
  if (p.kind === "stack") {
    const c = p.board.find((t) => t?.goal)?.color;
    if (c !== undefined) {
      for (let i = 0; i < 60; i++)
        if (p.board[i]?.color === c) {
          p.cleared += p.board[i].goal ? 1 : 0;
          p.board[i] = null;
        }
      settleStack(p);
    }
  }
  if (p.kind === "drill") {
    p.dir = "down";
    const i = (p.y + 1) * 7 + p.x;
    if (p.board[i]) p.board[i].hits = 1;
    drill(p);
  }
  if (p.kind === "light") {
    const key = Object.keys(p.solution).find(
      (i) => p.board[i].rot !== p.solution[i],
    );
    if (key !== undefined) {
      p.cursor = Number(key);
      inputPuzzle(p, "confirm");
    }
  }
  if (p.kind === "signal") {
    p.cursor = p.board.findIndex(
      (c, i) => c.type === "beacon" && !p.revealed.includes(i),
    );
    if (p.cursor >= 0) inputPuzzle(p, "confirm");
  }
}
export function autosolvePuzzle(p) {
  let guard = 0;
  while (!p.solved && guard++ < 150) assistStep(p);
  if (!p.solved) throw new Error(`Autosolve failed: ${p.kind}`);
  p.message = "Done together. That counts just as much.";
}
