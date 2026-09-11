import { CLUES, DISTRICTS, WEATHER } from "./story.js";
import { createPuzzle } from "./puzzles.js";
export const DEFAULT_SETTINGS = {
  lively: false,
  text: "L",
  contrast: false,
  reduced: false,
  volume: 0.35,
  preset: "arrows",
  bindings: {
    up: "ArrowUp",
    down: "ArrowDown",
    left: "ArrowLeft",
    right: "ArrowRight",
    confirm: "Enter",
    pause: "Escape",
  },
  llm: { base: "", model: "", key: "", temperature: 0.7, maxTokens: 160 },
  seed: 927,
};
export function newGame(slot = 0, seed = 927) {
  return {
    version: 1,
    slot,
    seed,
    weather: { rain: 0, snow: 0 },
    name: "Kit",
    night: 0,
    position: { x: 0, y: 0, z: 10 },
    facts: [],
    pins: [],
    round: 0,
    puzzle: null,
    stage: "delivery",
    warmth: {},
    memory: {},
    exchanges: {},
    chalk: [],
    murals: 0,
    trail: 0,
    lit: [],
    finale: [],
    putt: {
      hole: 0,
      strokes: 0,
      best: [],
      ball: { x: 0, z: 7 },
      angle: 0,
      power: 0.5,
    },
    rhythm: { hits: 0, best: 0 },
    seconds: 0,
    completed: false,
  };
}
export function objective(s) {
  if (s.completed)
    return "Brightwater is yours. Wander, play, and leave a little light.";
  if (s.stage === "finale")
    return s.finale.length < 5
      ? `The Lighting Run · ${s.finale.length}/5 lanterns · ${DISTRICTS.find((d, i) => !s.finale.includes(i))?.name}`
      : "Meet everyone in Harbor Square.";
  if (s.stage === "board") return "Open your notebook and pin tonight’s clue.";
  if (s.stage === "sleep")
    return "Your route is done. A warm bed awaits at the Courier Depot.";
  return `Deliver ${DISTRICTS[s.night].parcel} to ${DISTRICTS[s.night].npc}.`;
}
export function beginPuzzle(s, lively = false, endless = null) {
  if (endless !== null) {
    s.puzzle = createPuzzle(
      DISTRICTS[endless].puzzle,
      s.round % 3,
      s.seed + s.round * 19,
      lively,
    );
    s.puzzle.endless = true;
    s.puzzle.district = endless;
  } else
    s.puzzle = createPuzzle(DISTRICTS[s.night].puzzle, s.round, s.seed, lively);
  return s.puzzle;
}
export function finishRound(s) {
  if (!s.puzzle?.solved) return false;
  if (s.puzzle.endless) {
    s.puzzle = null;
    return "endless";
  }
  s.round++;
  s.puzzle = null;
  if (s.round === 3) {
    const clue = CLUES[s.night];
    if (!s.facts.includes(clue.id)) s.facts.push(clue.id);
    s.stage = "board";
    return "fact";
  }
  return "round";
}
export function pinClue(s, id, suspect) {
  const clue = CLUES.find((c) => c.id === id);
  if (!clue || !s.facts.includes(id) || clue.clear !== suspect) return false;
  if (!s.pins.includes(id)) s.pins.push(id);
  if (s.pins.includes(CLUES[s.night].id)) {
    s.stage = s.night === 4 ? "finale" : "sleep";
    if (!s.lit.includes(s.night)) s.lit.push(s.night);
  }
  return true;
}
export function sleepNight(s) {
  if (s.stage !== "sleep" || s.night >= 4) return false;
  s.night++;
  s.round = 0;
  s.stage = "delivery";
  s.position = { x: 0, y: 0, z: 10 };
  s.memory = {};
  s.exchanges = {};
  return true;
}
export function lightLantern(s, index) {
  if (s.stage !== "finale" || s.finale.includes(index)) return false;
  s.finale.push(index);
  return true;
}
export function completeGame(s) {
  if (s.stage === "finale" && s.finale.length === 5) {
    s.completed = true;
    s.stage = "complete";
    return true;
  }
  return false;
}
export function saveGame(storage, s) {
  try {
    storage.setItem(`afterglow.slot.${s.slot}`, JSON.stringify(s));
    return true;
  } catch {
    return false;
  }
}
export function loadGame(storage, slot) {
  try {
    const s = JSON.parse(storage.getItem(`afterglow.slot.${slot}`));
    if (
      !s ||
      s.version !== 1 ||
      s.slot !== slot ||
      !Number.isInteger(s.night) ||
      s.night < 0 ||
      s.night > 4 ||
      !Array.isArray(s.facts) ||
      s.facts.some((id) => !CLUES.some((c) => c.id === id)) ||
      !Array.isArray(s.pins) ||
      !Array.isArray(s.lit) ||
      !Array.isArray(s.finale) ||
      !Number.isFinite(s.position?.x) ||
      !Number.isFinite(s.position?.z) ||
      !Number.isFinite(s.position?.y) ||
      Math.abs(s.position.x) > 500 ||
      Math.abs(s.position.z) > 500 ||
      !["delivery", "board", "sleep", "finale", "complete"].includes(s.stage)
    )
      return null;
    if (!validSave(s)) return null;
    return s;
  } catch {
    return null;
  }
}
function validSave(s) {
  const base = newGame(s.slot);
  if (Object.keys(base).some((k) => !(k in s))) return false;
  if (
    !Number.isInteger(s.seed) ||
    !Number.isInteger(s.round) ||
    s.round < 0 ||
    s.round > 3 ||
    typeof s.name !== "string" ||
    s.name.length > 24 ||
    !Number.isFinite(s.seconds) ||
    s.seconds < 0
  )
    return false;
  if (
    !s.warmth ||
    !s.memory ||
    !s.exchanges ||
    !s.putt ||
    !s.rhythm ||
    !s.weather ||
    !Number.isFinite(s.weather.rain) ||
    !Number.isFinite(s.weather.snow)
  )
    return false;
  if (
    !Array.isArray(s.chalk) ||
    s.chalk.some((i) => !Number.isInteger(i) || i < 0 || i >= 15) ||
    !Number.isInteger(s.murals) ||
    s.murals < 0 ||
    s.murals > s.chalk.length ||
    !Number.isInteger(s.trail) ||
    s.trail < 0 ||
    s.trail > 5
  )
    return false;
  if (
    s.lit.some((i) => !Number.isInteger(i) || i < 0 || i > 4) ||
    s.finale.some((i) => !Number.isInteger(i) || i < 0 || i > 4) ||
    s.pins.some((id) => !s.facts.includes(id))
  )
    return false;
  if (
    !s.putt.ball ||
    !Number.isFinite(s.putt.ball.x) ||
    !Number.isFinite(s.putt.ball.z) ||
    !Number.isInteger(s.putt.hole) ||
    s.putt.hole < 0 ||
    s.putt.hole > 8 ||
    !Array.isArray(s.putt.best)
  )
    return false;
  const p = s.puzzle;
  if (!p) return true;
  if (
    !["phrase", "stack", "drill", "light", "signal"].includes(p.kind) ||
    !Number.isInteger(p.round) ||
    p.round < 0 ||
    !Number.isInteger(p.cursor) ||
    p.cursor < 0 ||
    !Number.isInteger(p.hints) ||
    p.hints < 0 ||
    p.hints > 3
  )
    return false;
  if (p.kind === "phrase")
    return (
      typeof p.phrase === "string" &&
      /^[A-Z ]+$/.test(p.phrase) &&
      p.phrase.length < 80 &&
      Array.isArray(p.guessed) &&
      p.cursor < 26
    );
  if (!Array.isArray(p.board)) return false;
  if (p.kind === "stack")
    return (
      p.board.length === 60 &&
      Array.isArray(p.pair) &&
      p.pair.length === 2 &&
      p.cursor < 6 &&
      p.board.every(
        (c) =>
          c === null ||
          (Number.isInteger(c.color) &&
            c.color >= 0 &&
            c.color < 5 &&
            typeof c.goal === "boolean"),
      )
    );
  if (p.kind === "drill")
    return (
      p.width === 7 &&
      Number.isInteger(p.height) &&
      p.height >= 12 &&
      p.height <= 20 &&
      p.board.length === 7 * p.height &&
      Number.isInteger(p.x) &&
      p.x >= 0 &&
      p.x < 7 &&
      Number.isInteger(p.y) &&
      p.y >= 0 &&
      p.y < p.height
    );
  if (p.kind === "light")
    return (
      p.board.length === 36 &&
      p.cursor < 36 &&
      Array.isArray(p.sources) &&
      Array.isArray(p.targets) &&
      p.solution &&
      p.board.every(
        (c) =>
          c && ["empty", "mirror", "splitter", "lens", "wall"].includes(c.type),
      )
    );
  return (
    p.board.length === 25 &&
    p.cursor < 25 &&
    Array.isArray(p.revealed) &&
    p.revealed.every((i) => Number.isInteger(i) && i >= 0 && i < 25) &&
    p.clue &&
    Array.isArray(p.clue.targets) &&
    p.board.every(
      (c) =>
        c &&
        typeof c.word === "string" &&
        ["beacon", "fog", "horn"].includes(c.type),
    )
  );
}
export function recap(s) {
  return `Previously, in Brightwater… ${s.facts.length ? s.facts.map((id) => CLUES.find((c) => c.id === id).text).join(" ") : "You arrived to be the new night courier."} Tonight: ${WEATHER[s.night].toLowerCase()}. ${objective(s)}`;
}
