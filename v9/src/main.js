import "./style.css";
import { World, terrain } from "./world.js";
import { Synth } from "./audio.js";
import {
  CAST,
  CLUES,
  COLORS,
  DISTRICTS,
  WEATHER,
  REPLIES,
  fallbackDialogue,
} from "./story.js";
import {
  DEFAULT_SETTINGS,
  newGame,
  objective,
  beginPuzzle,
  finishRound,
  pinClue,
  sleepNight,
  lightLantern,
  completeGame,
  saveGame,
  loadGame,
  recap,
} from "./state.js";
import {
  PUZZLE_NAMES,
  RULES,
  inputPuzzle,
  hintPuzzle,
  autosolvePuzzle,
  tickPuzzle,
  traceLight,
  signalFallback,
} from "./puzzles.js";
import {
  requestDialogue,
  requestSignal,
  remember,
  testConnection,
  diagnostics,
  summarizeMemory,
  requestRecap,
} from "./llm.js";
const ui = document.querySelector("#ui"),
  toastEl = document.querySelector("#toast");
let settings = structuredClone(DEFAULT_SETTINGS);
try {
  const loaded = JSON.parse(localStorage.getItem("afterglow.settings"));
  if (loaded)
    settings = {
      ...settings,
      ...loaded,
      llm: { ...settings.llm, ...loaded.llm },
      bindings: { ...settings.bindings, ...loaded.bindings },
    };
} catch {}
let playing = false;
let state = newGame(0, settings.seed),
  screen = "title",
  selected = 0,
  pauseFrom = "world",
  settingsFrom = "title",
  dialogueName = "",
  dialogue = null,
  dialogueToken = 0,
  dialogueBusy = false,
  typedText = "",
  boardClue = "",
  status = "",
  slotMode = "new",
  lastSave = 0,
  saveOk = true,
  remapping = null,
  signalToken = 0,
  signalTurn = -1,
  endlessRound = 0,
  puttPhase = "aim",
  rhythmCombo = 0;
const keys = new Set(),
  audio = new Synth();
let world;
try {
  world = new World(document.querySelector("#world"), settings);
} catch (error) {
  ui.innerHTML =
    '<div class="modal-wrap"><div class="panel"><h2>A little graphics help</h2><p>AFTERGLOW needs WebGL 2. Enable hardware acceleration in your browser or open the desktop app.</p></div></div>';
  throw error;
}
world.onRescue = () =>
  toast("Ferris: The rowboat has you. Back on dry boards.");
world.onChalk = () => {
  toast(`A chalk stick! ${state.chalk.length} found. Rosie will be delighted.`);
  audio.effect("chalk");
  save();
};
function esc(value) {
  return String(value ?? "").replace(
    /[&<>"']/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        c
      ],
  );
}
function button(label, action, extra = "", primary = false) {
  return `<button class="btn${primary ? " primary" : ""}" data-action="${action}" ${extra}>${label}</button>`;
}
function portrait(name) {
  return `<div class="portrait" style="--accent:${CAST[name]?.color || COLORS[0]}"><div class="head"></div><div class="body"></div></div>`;
}
function modal(content, wide = false) {
  return `<div class="modal-wrap"><section class="panel interactive${wide ? " wide" : ""}">${content}</section></div>`;
}
function toast(message) {
  toastEl.textContent = message;
  toastEl.classList.add("visible");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => toastEl.classList.remove("visible"), 4200);
}
function save() {
  if (!playing) return;
  saveOk = saveGame(localStorage, state);
  lastSave = state.seconds;
  if (!saveOk)
    toast(
      "This browser could not save. Free some local storage before leaving.",
    );
}
function saveSettings() {
  try {
    localStorage.setItem("afterglow.settings", JSON.stringify(settings));
  } catch {
    toast("Settings could not be saved on this device.");
  }
  world.settings = settings;
  applySettings();
}
function applySettings() {
  document.documentElement.classList.toggle("xl", settings.text === "XL");
  document.documentElement.classList.toggle("high-contrast", settings.contrast);
  document.documentElement.classList.toggle("reduced", settings.reduced);
}
function go(next) {
  screen = next;
  selected = 0;
  keys.clear();
  render();
}
function menuButtons() {
  return [...ui.querySelectorAll("button[data-action]:not(:disabled)")];
}
function select(index) {
  const buttons = menuButtons();
  if (!buttons.length) return;
  selected = (index + buttons.length) % buttons.length;
  buttons.forEach((b, i) => b.classList.toggle("selected", i === selected));
  buttons[selected]?.scrollIntoView({ block: "nearest" });
}
function hud() {
  return `<header class="hud"><div><div class="hud-title">AFTERGLOW</div><p>${esc(objective(state))}</p></div><div class="hud-right"><div class="eyebrow">NIGHT ${state.night + 1} / 5 · ${esc(WEATHER[state.night])}</div><div class="night-dots">${Array.from({ length: 5 }, (_, i) => (state.lit.includes(i) ? "●" : "○")).join("")}</div><div class="save-status">${saveOk ? "AUTOSAVED · SLOT " + (state.slot + 1) : "SAVE UNAVAILABLE"}</div></div></header>`;
}
function title() {
  return `<section class="title-screen interactive"><div class="brand"><div class="tiny-lantern"></div><div class="eyebrow">Sweet Papa presents</div></div><div class="title-body"><div class="eyebrow">A little mystery. A lot of light.</div><h1>AFTERGLOW</h1><div class="subtitle">Five nights in Brightwater</div><p>The lighthouse has gone quiet.<br>Someone is leaving kindness in chalk.<br>There’s a delivery with your name on it.</p><div class="actions">${button("Begin your story <small>OFFLINE & UNHURRIED</small>", "new", "", true)}${button("Continue <small>THREE LITTLE SAVE SLOTS</small>", "load")}</div><div class="actions" style="margin-top:12px">${button("Make yourself comfortable", "settings")}${button("About Brightwater", "about")}</div></div><footer class="title-footer"><span>ARROWS TO CHOOSE · ENTER TO BEGIN</span><span>ALL THE TIME IN THE WORLD</span></footer></section>`;
}
function render() {
  applySettings();
  let html = screen === "title" ? title() : hud();
  if (screen === "world") {
    const near = world.nearest(state.position);
    let label = near ? `Talk to ${near}` : "Your route & notebook";
    if (
      state.stage === "sleep" &&
      Math.hypot(state.position.x, state.position.z - 10) < 12
    )
      label = "Turn in for the night";
    if (state.stage === "finale")
      label =
        state.finale.length === 5
          ? "Meet the town at the harbor"
          : "Light the district lantern";
    html += `<footer class="world-footer"><p><span class="key">↑ ↓ ← →</span> Wander <span class="key">Enter</span> Talk / route <span class="key">Esc</span> Notebook & comfort</p><button class="interact-hint" data-action="interact"><span class="key">Enter</span> ${label}</button></footer>`;
  }
  if (screen === "slots")
    html += modal(
      `<div class="eyebrow">A place to leave your lantern</div><h2>${slotMode === "new" ? "Begin a new story" : "Welcome back"}</h2><p>Three stories, saved on this device. Every puzzle round and fact card is kept safe.</p><div class="actions column">${[
        0, 1, 2,
      ]
        .map((i) => {
          const s = loadGame(localStorage, i);
          return button(
            `Slot ${i + 1} · ${s ? (s.completed ? "Lantern Night complete" : `Night ${s.night + 1} · ${s.facts.length} facts`) : "An empty notebook"}`,
            `slot:${i}`,
            !s && slotMode === "load" ? "disabled" : "",
          );
        })
        .join("")}${button("Back", "title")}</div>`,
    );
  if (screen === "overwrite")
    html += modal(
      `<div class="eyebrow">This notebook already has a story</div><h2>Start again in slot ${state.slot + 1}?</h2><p>The existing story in this slot will be replaced when you begin.</p><div class="actions">${button("Keep the existing story", "slots", "", true)}${button("Start a fresh notebook", "intro")}</div>`,
    );
  if (screen === "intro")
    html += modal(
      `<div class="eyebrow">Night Owl Radio · First broadcast</div><h2>Welcome to Brightwater, ${esc(state.name)}.</h2><p>You moved here last week. Tonight is your first route. The lighthouse lens is cracked, Lantern Night is five nights away, and someone is borrowing things… then returning them repaired.</p><div class="fact-card"><h3>Winnie left you a notebook</h3><p>“Find the person drawing those little chalk lanterns. I think they might know how to bring our light back.”</p></div><p class="help">Use the arrows to wander. Enter talks, delivers, and opens your route. Esc opens your notebook and comfort settings. There are no lives to lose. Hints can finish any puzzle with you.</p><div class="actions">${button("Take my first delivery", "start", "", true)}${button(`My name is ${esc(state.name)} · change`, "name")}${button("Optional model voices", "llm")}</div><p class="help">Offline voices are ready. An AI model is optional; the whole story works without one.</p>`,
    );
  if (screen === "name")
    html += modal(
      `<div class="eyebrow">Your courier badge</div><h2>What should we call you?</h2><div class="actions column">${["Kit", "Robin", "Alex", "Sam", "Jo"].map((n) => button(n, `name:${n}`)).join("")}${button("Back", "intro")}</div>`,
    );
  if (screen === "pause")
    html += modal(
      `<div class="eyebrow">A little breathing room</div><h2>Stay a while.</h2><p>${esc(objective(state))}</p><div class="actions column">${button("Keep going", "resume", "", true)}${state.puzzle ? button("A little hint", "hint") + (state.puzzle.hints === 3 ? button("Show me this round", "solve") : "") : ""}${button("Delivery route & town map", "map")}${button("Case notebook", "journal")}${button("Comfort & settings", "settings")}${button("Arcade & little diversions", "arcade")}${button("Save and return to title", "save-title")}</div>`,
    );
  if (screen === "map") {
    html += modal(
      `<div class="eyebrow">Courier route · Night ${state.night + 1}</div><h2>Every path leads home.</h2><p>Choose a destination and take the lantern rail. Or close the map and follow the glowing marker on foot.</p><div class="map-art">${DISTRICTS.map((d, i) => `<div class="map-point" style="left:${47 + d.x * 0.48}%;top:${55 + d.z * 0.6}%">${esc(d.short)} ${state.lit.includes(i) ? "✧" : ""}</div>`).join("")}</div><div class="route-list">${DISTRICTS.map((d, i) => button(`${esc(d.name)} <small>${i === state.night && state.stage === "delivery" ? "YOUR DELIVERY · " : ""}${i > state.night ? "OPEN FOR A WANDER" : state.lit.includes(i) ? "LANTERNS LIT" : d.anchor.toUpperCase()}</small>`, `travel:${i}`, "", i === state.night)).join("")}${button("Courier Depot <small>HOME, TEA & A WARM BED</small>", "home")}</div><div class="actions">${button("Back to the town", "world")}</div>`,
      true,
    );
  }
  if (screen === "dialogue" && dialogue) {
    const name = dialogueName,
      chapter =
        DISTRICTS[state.night].npc === name && state.stage === "delivery";
    html += modal(
      `<div class="panel-header"><div><div class="eyebrow">${esc(CAST[name].role)}</div><h2>${esc(CAST[name].full)}</h2></div><span class="badge">${dialogueBusy ? "Listening…" : "A friendly face"}</span></div><div class="dialogue-layout">${portrait(name)}<div><p class="dialogue" id="speech">${esc(dialogue.say.replaceAll("Kit", state.name))}</p><div class="dialogue-status" id="dialogue-status">${esc(status)}</div></div></div><div class="reply-list">${chapter ? button(state.round === 0 ? "Here’s your parcel. Let’s help." : `Let’s try round ${state.round + 1}.`, "puzzle", "", true) : ""}${dialogue.end_conversation ? "" : dialogue.suggested_replies.map((r, i) => button(esc(r), `reply:${i}`)).join("")}${name === "Rosie" ? button(`Share my chalk · ${state.chalk.length - state.murals} sticks`, "mural") : ""}${name === "Ferris" ? button("Read my route back to me", "recap") : ""}${button("Say something else…", "type")}${button("See you around", "world")}</div>`,
    );
  }
  if (screen === "type")
    html += modal(
      `<div class="eyebrow">A word with ${esc(dialogueName)}</div><h2>What’s on your mind?</h2><div class="setting-row"><label for="say">Your words</label><input id="say" maxlength="500" value="${esc(typedText)}" autocomplete="off"></div><div class="actions">${button("Say it", "send-typed", "", true)}${button("Back to suggested replies", "dialogue")}</div><p class="help">Typing is always optional. The suggested replies tell the same story.</p>`,
    );
  if (screen === "puzzle") html += puzzleView();
  if (screen === "fact") {
    const c = CLUES[state.night];
    html += modal(
      `<div class="eyebrow">A kindness uncovered · Fact ${state.night + 1} of 5</div><h2>${esc(c.title)}</h2><p>${esc(c.secret)}</p><div class="fact-card"><h3>Pinned in your notebook</h3><p>${esc(c.text)}</p></div><p>${state.night === 4 ? "Ada lowers her tools. “I wanted to surprise him.” You tell her the town would rather help." : "Another neighbor with a lovely secret. The mystery is a little clearer."}</p><div class="actions">${button("Open the case board", "journal", "", true)}${button("Talk a little longer", `talk:${c.holder}`)}</div>`,
    );
  }
  if (screen === "journal") {
    html += modal(
      `<div class="eyebrow">Kit’s case notebook · ${state.facts.length} / 5 facts</div><h2>Little things, connected.</h2><p>${state.facts.length ? "Choose a fact to connect it to a neighbor. The first four facts clear their holders; the last identifies the Lamplighter." : "Your pages are waiting. Deliver to Odette and help with her chalk menu to discover your first fact."}</p><div class="notebook">${state.facts
        .map((id) => {
          const c = CLUES.find((c) => c.id === id);
          return button(
            `<span style="color:var(--accent)">${esc(c.title)}</span><small>${state.pins.includes(id) ? "CONNECTED · " : ""}${esc(c.text)}</small>`,
            `clue:${id}`,
            'style="width:100%;margin-bottom:10px"',
          );
        })
        .join(
          "",
        )}</div><div class="actions">${button("Back to the town", "world", "", true)}${state.stage === "sleep" ? button("Turn in at the depot", "home") : ""}</div>`,
      true,
    );
  }
  if (screen === "board") {
    const c = CLUES.find((c) => c.id === boardClue);
    html += modal(
      `<div class="eyebrow">The case board</div><h2>${c.id === "ada" ? "Who is the Lamplighter?" : "Whose secret explains their odd behavior?"}</h2><div class="fact-card"><h3>${esc(c.title)}</h3><p>${esc(c.text)}</p><p class="muted">${esc(c.secret)}</p></div><div class="suspects">${["Odette", "Bo", "Pearl", "Idris", "Ada"].map((n) => button(`${n}<small>${state.pins.some((id) => CLUES.find((c) => c.id === id).clear === n) ? "CONNECTED" : "CONNECT THREAD"}`, `pin:${n}`)).join("")}</div><p class="puzzle-message">${esc(status)}</p><div class="actions">${button("Back to notebook", "journal")}</div>`,
      true,
    );
  }
  if (screen === "night-end")
    html += modal(
      `<div class="eyebrow">Night ${state.night + 1} · Route complete</div><h2>A little brighter than before.</h2><p>${esc(CLUES[state.night].secret)} The lamps in ${esc(DISTRICTS[state.night].name)} come on. A new part joins the music. Your notebook is safe.</p><div class="actions">${button("Sleep until tomorrow night", "sleep", "", true)}${button("A little more wandering", "world")}</div>`,
    );
  if (screen === "morning")
    html += modal(
      `<div class="eyebrow">Night Owl Radio · Night ${state.night + 1}</div><h2>${esc(WEATHER[state.night])}.</h2><p>“Evening, ${esc(state.name)}. ${["", "There is drizzle on the dock rails. Bo has a space waiting for his boat.", "The rain is settling in. Pearl keeps a warm lantern below the hill.", "Snow in the gardens! Idris says every flake has excellent geometry.", "The stars are coming back. One last parcel, marked Return to sender."][state.night]} Tonight, take ${esc(DISTRICTS[state.night].parcel)} to ${esc(DISTRICTS[state.night].npc)}.”</p><div class="actions">${button("Pick up my delivery", "world", "", true)}${button("Show me the route", "map")}</div>`,
    );
  if (screen === "finale-intro")
    html += modal(
      `<div class="eyebrow">Lantern Night</div><h2>It takes a whole town.</h2><p>Bo brings the brass. Pearl rolls in her tunnel cart. Idris carries a spare prism. Odette brings enough bread for absolutely everyone.</p><p>Ada fits the last piece. Gus rests his hand on the crank.</p><div class="fact-card"><h3>“Well. That is something.”</h3><p>The lighthouse beam turns across the bay. It’s time for the Lighting Run: visit each district and press Enter at its lantern, then return to Harbor Square.</p></div><div class="actions">${button("Carry the first light", "world", "", true)}</div>`,
    );
  if (screen === "credits")
    html += modal(
      `<div class="eyebrow">Lantern Night · Brightwater, together</div><h2>You left the town brighter.</h2><p>Bo sings his first song on the harbor steps. Gus and Ada watch the beam sweep the water. Odette hands you a warm bun. Ferris signs off: “Good night, night owl. You’re home.”</p><div class="credits-cast">${["Odette", "Bo", "Pearl", "Idris", "Ada"].map((n) => `<div>${portrait(n)}<p>${n}</p></div>`).join("")}</div><p class="help">AFTERGLOW · Sweet Papa Technologies<br>All geometry, light, weather, and music made in code.<br>Voices: ${esc(settings.llm.model || "The authored Brightwater cast")} · Mystery: the Brightwater story bible.<br>Thank you for taking your time.</p><div class="actions">${button("Stay in Brightwater", "world", "", true)}${button("Save and return to title", "save-title")}</div>`,
      true,
    );
  if (screen === "settings") html += settingsView();
  if (screen === "llm") html += llmView();
  if (screen === "about")
    html += modal(
      `<div class="eyebrow">A kitchen-table mystery in a 3D town</div><h2>Nothing lost. Kindness found.</h2><p>Five deliveries, five puzzles, and a lighthouse that could use a little help. Take rails across the harbor, learn your neighbors’ secrets, and bring the town together for Lantern Night.</p><p class="help">Relaxed mode has no deadlines. In every puzzle, Esc opens hints. Three hints reveal “Show me,” which completes the round together. The same story awaits whichever way you play.</p><p class="help">No art or audio files. No telemetry. Network requests happen only when you configure model voices. Save slots and settings stay on this device.</p><div class="actions">${button("Back", "title", "", true)}</div>`,
    );
  if (screen === "arcade")
    html += modal(
      `<div class="eyebrow">The square arcade</div><h2>A little while longer.</h2><p>Revisit a puzzle after lighting its district. Nothing here is needed for your deliveries.</p><div class="route-list">${DISTRICTS.map((d, i) => button(`${PUZZLE_NAMES[d.puzzle]} <small>${state.lit.includes(i) ? "ENDLESS ROUNDS" : "LIGHT THIS DISTRICT FIRST"}</small>`, `endless:${i}`, state.lit.includes(i) ? "" : "disabled")).join("")}${button(`Harbor Putt <small>NINE HOLES ON THE PIER</small>`, "putt", state.night >= 1 ? "" : "disabled")}${button("Lantern Rhythm <small>A LITTLE MUSIC WITH FERRIS</small>", "rhythm")}${button("Chalk Mural Club <small>FIND ROSIE IN THE SQUARE</small>", "murals")}</div><div class="actions">${button("Back to the town", "world")}</div>`,
    );
  if (screen === "putt")
    html += modal(
      `<div class="eyebrow">Harbor Putt · Hole ${state.putt.hole + 1} / 9</div><h2>Aim for a little peace.</h2><p class="help">Left / Right aim. Up / Down choose power. Enter takes the shot. Banks bounce; a gentle shot settles into the cup. Esc pauses.</p><canvas class="putt-canvas" id="putt" width="750" height="350"></canvas><p class="puzzle-message" id="putt-status">${esc(status || `${state.putt.strokes} strokes · Power ${Math.round(state.putt.power * 100)}%`)}</p><div class="actions">${button("Take the shot", "putt-shot", "", true)}${button("Aim toward the cup", "putt-aim")}${button("Next hole", "putt-next")}${button("Back to the town", "world")}</div>`,
    );
  if (screen === "rhythm")
    html += modal(
      `<div class="eyebrow">Lantern Rhythm · Just for the joy of it</div><h2>Leave a light on the beat.</h2><p>${settings.lively ? "Tap Enter as the center lantern brightens. A close beat builds your combo." : "Tap Enter whenever you feel the music. Every tap adds a little light."}</p><div class="rhythm-lanterns">${[0, 1, 2, 3, 4].map(() => "<span></span>").join("")}</div><p class="puzzle-message" id="rhythm-status">${state.rhythm.hits} little lights · Best combo ${state.rhythm.best}</p><div class="actions">${button("Light a beat", "rhythm-hit", "", true)}${button("Back to the town", "world")}</div>`,
    );
  if (screen === "murals")
    html += modal(
      `<div class="eyebrow">Rosie’s Chalk Mural Club</div><h2>A town drawn together.</h2><p>You’ve found ${state.chalk.length} of 15 chalk sticks. ${state.murals} strokes have joined the walls. Look for sparkling chalk by the paths in each district.</p><div style="font-size:55px;letter-spacing:12px;color:${COLORS[state.trail]};padding:25px 0">${"⌂ ≋ ✧ ♡ ☼".slice(0, Math.max(1, state.murals * 2))}</div><div class="actions">${button(`Share ${state.chalk.length - state.murals} new sticks`, "mural", "", true)}${button("Back to the town", "world")}</div>`,
    );
  ui.innerHTML = html;
  if (!["world", "puzzle", "putt", "rhythm"].includes(screen)) select(selected);
  if (screen === "putt") drawPutt();
}
function puzzleView() {
  const p = state.puzzle;
  if (!p) return "";
  if (p.solved)
    return modal(
      `<div class="round-done"><div class="eyebrow">${PUZZLE_NAMES[p.kind]} · Round ${(p.round % 3) + 1} complete</div><div style="font-size:50px;color:var(--accent)">✧</div><h2>A lovely bit of work.</h2><p>${esc(p.message)} ${p.endless ? "There is always another round at the arcade." : state.round < 2 ? "Shall we try the next one?" : "Your neighbor has something to share with you."}</p><div class="actions">${button(p.endless ? "Back to the arcade" : state.round < 2 ? "Next round" : "Hear their story", "round-done", "", true)}</div></div>`,
    );
  let board = "";
  if (p.kind === "phrase")
    board = `<div class="eyebrow">${["Something from the bakery", "A town saying", "A five-word bakery pun"][p.round % 3]}</div><div class="lanterns">${Array.from({ length: 5 }, (_, i) => (i < 5 - p.wrong ? "♧" : "·")).join("")}</div><div class="phrase">${p.phrase
      .split(" ")
      .map(
        (w) =>
          `<span style="display:inline-block">${[...w].map((c) => (p.guessed.includes(c) ? c : "_")).join("")}</span>`,
      )
      .join(
        " ",
      )}</div><div class="letter-wheel">${Array.from({ length: 26 }, (_, i) => `<button data-cell="${i}" class="tile${p.cursor === i ? " selected" : ""}${p.guessed.includes(String.fromCharCode(65 + i)) ? " used" : ""}">${String.fromCharCode(65 + i)}</button>`).join("")}</div>`;
  if (p.kind === "stack") {
    board = `<div class="eyebrow">${p.remaining} dock crates to clear · ${p.score} points</div><div style="display:grid;grid-template-columns:repeat(6,1fr);width:250px;margin:14px 0 5px">${Array.from({ length: 6 }, (_, i) => `<span style="height:38px;text-align:center;color:${COLORS[p.pair[0]]}">${i === p.cursor ? (p.rotation ? "■ → ■" : "■<br>■") : ""}</span>`).join("")}</div><div class="grid-board stack-board">${p.board.map((c, i) => `<button class="tile${i % 6 === p.cursor ? " selected" : ""}" data-cell="${i}" style="background:${c ? COLORS[c.color] + "44" : "#0a1820"};color:${c ? COLORS[c.color] : "#1c3540"}">${c ? (c.goal ? "✦" : "■") : "·"}</button>`).join("")}</div>`;
  }
  if (p.kind === "drill") {
    const from = Math.max(0, Math.min(p.height - 10, p.y - 3)),
      to = Math.min(p.height, from + 10);
    board = `<div class="eyebrow">Depth ${p.y + 1} / ${p.height} · ${p.lively ? `Oil ${Math.round(p.oil)}%` : "Lantern full"}</div><div class="grid-board drill-board" style="margin-top:16px">${p.board
      .slice(from * 7, to * 7)
      .map((c, j) => {
        const i = j + from * 7;
        return `<button data-cell="${i}" class="tile${p.cursor === i ? " selected" : ""}" style="background:${c ? COLORS[c.color] + "44" : "#0a1820"};color:${c ? COLORS[c.color] : "#f3ebd6"}">${i === p.y * 7 + p.x ? "♙" : c ? (c.hits > 1 ? "▥" + c.hits : c.oil ? "✧" : "■") : i >= p.board.length - 7 ? "▣" : "·"}</button>`;
      })
      .join("")}</div>`;
  }
  if (p.kind === "light") {
    const light = traceLight(p);
    board = `<div class="eyebrow">${light.hit.length} / ${p.targets.length} eyepieces lit</div><div class="light-wrap" style="margin-top:24px"><div class="grid-board light-board">${p.board
      .map((c, i) => {
        const target = p.targets.findIndex((t) => t.y * 6 + t.x === i);
        return `<button data-cell="${i}" class="tile${i === p.cursor ? " selected" : ""}" style="color:${target >= 0 ? COLORS[p.targets[target].color] : c.type === "lens" ? COLORS[c.color] : "#c9ddcf"}">${target >= 0 ? (light.hit.includes(target) ? "◉" : "◎") : c.type === "mirror" ? (c.rot ? "╲" : "╱") : c.type === "splitter" ? (c.rot ? "┴" : "┬") : c.type === "wall" ? "▧" : c.type === "lens" ? "◇" : "·"}</button>`;
      })
      .join(
        "",
      )}</div><svg class="beams" viewBox="0 0 600 600">${light.segments.map((s) => `<line x1="${s.x1 * 100 + 50}" y1="${s.y1 * 100 + 50}" x2="${s.x2 * 100 + 50}" y2="${s.y2 * 100 + 50}" stroke="${COLORS[s.color]}" stroke-width="5" opacity=".7"/>`).join("")}</svg></div>`;
  }
  if (p.kind === "signal")
    board = `<div class="eyebrow">${p.theme} · ${p.found} / 9 beacons</div><div class="signal-clue">Ada’s clue: ${p.clue.clue}, ${p.clue.count}<div class="help">${p.guessesLeft} guesses in this turn</div></div><div class="grid-board signal">${p.board.map((c, i) => `<button data-cell="${i}" class="tile${i === p.cursor ? " selected" : ""} ${p.revealed.includes(i) ? c.type : ""}">${c.word}${p.revealed.includes(i) && c.type === "beacon" ? " ✧" : ""}</button>`).join("")}</div>`;
  return modal(
    `<div class="panel-header"><div><div class="eyebrow">${p.endless ? "The square arcade" : DISTRICTS[state.night].npc + "’s puzzle"}</div><h2>${PUZZLE_NAMES[p.kind]}</h2></div><span class="badge">ROUND ${(p.round % 3) + 1} / 3 · ${p.lively ? "LIVELY" : "NO HURRY"}</span></div><div class="puzzle-layout"><div class="puzzle-main">${board}</div><aside class="puzzle-aside"><div class="eyebrow">A little how-to</div><p>${RULES[p.kind]}</p><div class="puzzle-message">${esc(p.message)}</div><div class="actions column">${button(`A little hint · ${p.hints}/3`, "hint")}${p.hints === 3 ? button("Show me this round", "solve") : ""}${button("Pause & notebook", "pause")}</div><p class="help" style="margin-top:20px">Arrows move · Enter acts<br>Esc for hints and a break<br>Nothing you do can lose the story.</p></aside></div>`,
    true,
  );
}
function settingsView() {
  return modal(
    `<div class="eyebrow">Make yourself comfortable</div><h2>Just your pace.</h2><div class="setting-row"><label>Play style</label>${button(settings.lively ? "Lively · moving pieces" : "Relaxed · no hurry", "toggle:lively")}</div><div class="setting-row"><label>Text size</label>${button(settings.text === "XL" ? "Extra large" : "Large", "text")}</div><div class="setting-row"><label>High contrast</label>${button(settings.contrast ? "On" : "Off", "toggle:contrast")}</div><div class="setting-row"><label>Reduce motion</label>${button(settings.reduced ? "On" : "Off", "toggle:reduced")}</div><div class="setting-row"><label>Music & sound</label>${button(`${Math.round(settings.volume * 100)}%`, "volume")}</div><div class="setting-row"><label>One-hand input</label>${button(settings.preset === "arrows" ? "Right hand · arrows" : settings.preset === "wasd" ? "Left hand · WASD" : "Custom keys", "preset")}</div><div class="actions">${button("Remap a key", "remap")}${button("Optional model voices", "llm")}${button("New Town · reshuffle scenery", "new-town")}</div><p class="help">${remapping ? `Press a key for ${remapping}. Esc cancels.` : "Anchors, routes and story stay in place when you reshuffle the town."}</p><div class="actions">${button("All comfortable", "settings-back", "", true)}</div>`,
  );
}
function llmView() {
  return modal(
    `<div class="eyebrow">Optional voices · Local or cloud</div><h2>Your town, your model.</h2><p class="help">Works with OpenAI-compatible chat-completions servers. Leave the address blank for offline play. Your key is saved locally on this device only.</p>${[
      ["base", "Base URL", "http://localhost:11434"],
      ["model", "Model name", "e.g. your installed model"],
      ["key", "API key (optional)", ""],
    ]
      .map(
        ([key, label, placeholder]) =>
          `<div class="setting-row"><label for="llm-${key}">${label} ${button("Edit", `edit-llm:${key}`)}</label><input id="llm-${key}" data-setting="${key}" ${key === "key" ? 'type="password"' : ""} value="${esc(settings.llm[key])}" placeholder="${placeholder}" autocomplete="off" spellcheck="false"></div>`,
      )
      .join(
        "",
      )}<div class="setting-row"><label>Temperature</label>${button(settings.llm.temperature.toFixed(1), "temperature")}</div><div class="setting-row"><label>Token ceiling</label>${button(String(settings.llm.maxTokens), "tokens")}</div><div class="actions">${button("Test connection", "test-llm", "", true)}${button("Use offline voices", "offline")}${button("Done", "llm-back")}</div><p class="puzzle-message" id="connection-status">${esc(status)}</p><p class="help">Ollama: port 11434 · LM Studio: port 1234 · llama.cpp / vLLM: use the port shown by your server. A 4B model can start small; 12B–30B models have more room for personality. No model is downloaded by the game. Preload your model in its server before playing: a cold model load can take longer than eight seconds. The game requests thinking off for quick replies when supported.</p><p class="help">Browser connection help: your server must allow this page’s origin (CORS). With Ollama, set OLLAMA_ORIGINS to this page’s origin and restart Ollama. An HTTPS page may block an HTTP server; use the desktop app or serve this build on localhost. Desktop requests use the app’s network bridge.</p><p class="help">A reply gets 8 seconds. Invalid names, locked clues and unsuitable content fall back to authored dialogue. ${diagnostics.length} recent fallback events; no prompts or keys are logged.</p>`,
    true,
  );
}
async function talk(name, line = "", beat = "hello") {
  dialogueName = name;
  dialogue = fallbackDialogue(name, state, beat);
  status = settings.llm.base
    ? "Waiting for a friendly voice…"
    : "Offline voice · the story is ready";
  dialogueBusy = true;
  const token = ++dialogueToken;
  go("dialogue");
  const result = await requestDialogue(
    name,
    state,
    line || `Hello, ${name}.`,
    settings.llm,
    state.memory[name],
    (s) => {
      if (token === dialogueToken && screen === "dialogue") {
        const el = document.querySelector("#dialogue-status");
        if (el) el.textContent = s;
      }
    },
    beat,
  );
  if (token !== dialogueToken || screen !== "dialogue") return;
  dialogue = result;
  dialogueBusy = false;
  status =
    result.source === "authored"
      ? ""
      : result.source === "authored fallback"
        ? "The offline voice has you covered."
        : "";
  remember(state, name, line || "Hello.", result);
  if ((state.exchanges[name]?.length || 0) >= 6) {
    const memoryState = state,
      memoryNight = state.night;
    summarizeMemory(name, state, settings.llm).then((summary) => {
      if (state === memoryState && state.night === memoryNight) {
        state.memory[name] = summary;
        save();
      }
    });
  }
  render();
  audio.effect("talk", Object.keys(CAST).indexOf(name));
  const el = document.querySelector("#speech"),
    text = result.say.replaceAll("Kit", state.name);
  if (!settings.reduced && el) {
    el.textContent = "";
    let count = 0;
    const interval = setInterval(() => {
      if (!el.isConnected) {
        clearInterval(interval);
        return;
      }
      count += 3;
      el.textContent = text.slice(0, count);
      if (count >= text.length) clearInterval(interval);
    }, 18);
  }
  save();
}
async function signalClue() {
  const p = state.puzzle;
  if (!p || p.kind !== "signal" || p.solved || signalTurn === p.turn) return;
  signalTurn = p.turn;
  const token = ++signalToken,
    turn = p.turn;
  const result = await requestSignal(structuredClone(p), settings.llm);
  if (
    result &&
    state.puzzle === p &&
    p.turn === turn &&
    token === signalToken &&
    !p.solved
  ) {
    p.clue = result;
    p.guessesLeft = result.count;
    if (screen === "puzzle") render();
  }
}
function doPuzzle(action) {
  const p = state.puzzle;
  if (!p) return;
  if (p.solved) {
    if (action === "confirm") act("round-done");
    return;
  }
  inputPuzzle(p, action);
  audio.effect(action === "confirm" ? "chalk" : "tap");
  save();
  if (p.solved) audio.effect("win");
  render();
  signalClue();
}
function interact() {
  if (state.stage === "finale") {
    const index = DISTRICTS.findIndex(
      (d) => Math.hypot(state.position.x - d.x, state.position.z - d.z) < 14,
    );
    if (index >= 0 && state.finale.length < 5) {
      if (lightLantern(state, index)) {
        audio.effect("win");
        toast(
          `${DISTRICTS[index].name} is glowing. ${state.finale.length}/5 lanterns lit.`,
        );
        save();
        render();
        return;
      }
    }
    if (
      state.finale.length === 5 &&
      Math.hypot(state.position.x, state.position.z) < 18
    ) {
      completeGame(state);
      save();
      audio.effect("win");
      go("credits");
      return;
    }
  }
  if (
    state.stage === "sleep" &&
    Math.hypot(state.position.x, state.position.z - 10) < 12
  ) {
    go("night-end");
    return;
  }
  const near = world.nearest(state.position);
  if (near) {
    talk(near);
    return;
  }
  go("map");
}
function startSlot(slot) {
  const previous = loadGame(localStorage, slot);
  if (slotMode === "load") {
    if (!previous) return;
    playing = true;
    state = previous;
    world.travel = null;
    world.velocity.set(0, 0);
    toast(recap(state));
    const loadedState = state;
    requestRecap(state, settings.llm, recap(state)).then((text) => {
      if (state === loadedState && screen === "world") toast(text);
    });
    audio.start();
    go(state.puzzle ? "puzzle" : "world");
    signalTurn = -1;
    signalClue();
    return;
  }
  playing = false;
  state = newGame(slot, settings.seed);
  if (previous) go("overwrite");
  else go("intro");
}
function act(action) {
  audio.start();
  const [verb, arg] = action.split(":");
  if (verb === "new") {
    slotMode = "new";
    go("slots");
    return;
  }
  if (verb === "load") {
    slotMode = "load";
    go("slots");
    return;
  }
  if (verb === "slot") {
    startSlot(Number(arg));
    return;
  }
  if (verb === "start") {
    playing = true;
    save();
    go("world");
    toast(
      "Ferris: Odette is just ahead. Use the arrows to approach her, or Enter for your route.",
    );
    return;
  }
  if (verb === "name") {
    if (arg) {
      state.name = arg;
      go("intro");
    } else go("name");
    return;
  }
  if (verb === "interact") {
    interact();
    return;
  }
  if (verb === "talk") {
    talk(arg);
    return;
  }
  if (verb === "travel") {
    go("world");
    world.rideTo(Number(arg), state.position);
    toast(
      `Lantern rail to ${DISTRICTS[Number(arg)].name}. Lean with the arrows, just for fun.`,
    );
    return;
  }
  if (verb === "home") {
    go("world");
    world.rideTo(0, state.position);
    if (state.stage === "sleep")
      toast("Your shift is done. Press Enter at the depot to sleep.");
    return;
  }
  if (verb === "puzzle") {
    dialogueToken++;
    if (!state.puzzle) beginPuzzle(state, settings.lively);
    save();
    signalTurn = -1;
    go("puzzle");
    signalClue();
    return;
  }
  if (verb === "hint") {
    if (state.puzzle) {
      hintPuzzle(state.puzzle);
      save();
      go("puzzle");
      if (state.puzzle.solved) audio.effect("win");
    }
    return;
  }
  if (verb === "solve") {
    if (state.puzzle?.hints === 3) {
      autosolvePuzzle(state.puzzle);
      save();
      audio.effect("win");
      go("puzzle");
    }
    return;
  }
  if (verb === "round-done") {
    const p = state.puzzle;
    if (!p?.solved) return;
    const result = finishRound(state);
    save();
    if (result === "fact") {
      audio.effect("win");
      go("fact");
    } else if (result === "endless") go("arcade");
    else if (result === "round") {
      beginPuzzle(state, settings.lively);
      save();
      signalTurn = -1;
      go("puzzle");
      signalClue();
    }
    return;
  }
  if (verb === "reply") {
    const i = Number(arg),
      line = dialogue?.suggested_replies[i] || REPLIES[i];
    state.warmth[dialogueName] = Math.min(
      3,
      (state.warmth[dialogueName] || 0) + (i === 0 ? 1 : 0),
    );
    talk(dialogueName, line, ["warm", "work", "case"][i]);
    return;
  }
  if (verb === "type") {
    typedText = "";
    go("type");
    document.querySelector("#say")?.focus();
    return;
  }
  if (verb === "send-typed") {
    typedText = document.querySelector("#say")?.value || "";
    talk(dialogueName, typedText, "warm");
    return;
  }
  if (verb === "recap") {
    dialogue = { ...fallbackDialogue("Ferris", state), say: recap(state) };
    status = "Your authored facts, safely kept.";
    go("dialogue");
    return;
  }
  if (verb === "clue") {
    boardClue = arg;
    status = "";
    go("board");
    return;
  }
  if (verb === "pin") {
    if (pinClue(state, boardClue, arg)) {
      save();
      audio.effect("win");
      if (state.stage === "finale") go("finale-intro");
      else {
        toast("A good connection. Another neighbor understood.");
        go("journal");
      }
    } else {
      status =
        "Ferris: Have another look at the secret on this fact card. We can try again.";
      render();
    }
    return;
  }
  if (verb === "sleep") {
    if (sleepNight(state)) {
      save();
      go("morning");
    }
    return;
  }
  if (verb === "pause") {
    pauseFrom = screen;
    save();
    go("pause");
    return;
  }
  if (verb === "resume") {
    go(
      state.puzzle
        ? "puzzle"
        : ["putt", "rhythm"].includes(pauseFrom)
          ? pauseFrom
          : "world",
    );
    return;
  }
  if (verb === "save-title") {
    save();
    playing = false;
    dialogueToken++;
    signalToken++;
    world.travel = null;
    go("title");
    return;
  }
  if (verb === "settings") {
    settingsFrom = screen;
    go("settings");
    return;
  }
  if (verb === "settings-back") {
    saveSettings();
    go(settingsFrom);
    return;
  }
  if (verb === "toggle") {
    settings[arg] = !settings[arg];
    if (arg === "lively" && state.puzzle) state.puzzle.lively = settings.lively;
    saveSettings();
    render();
    return;
  }
  if (verb === "text") {
    settings.text = settings.text === "L" ? "XL" : "L";
    saveSettings();
    render();
    return;
  }
  if (verb === "volume") {
    settings.volume = ((Math.round(settings.volume * 100) + 15) % 105) / 100;
    saveSettings();
    render();
    return;
  }
  if (verb === "preset") {
    settings.preset = settings.preset === "arrows" ? "wasd" : "arrows";
    settings.bindings =
      settings.preset === "wasd"
        ? {
            up: "KeyW",
            down: "KeyS",
            left: "KeyA",
            right: "KeyD",
            confirm: "Space",
            pause: "Escape",
          }
        : structuredClone(DEFAULT_SETTINGS.bindings);
    saveSettings();
    render();
    return;
  }
  if (verb === "remap") {
    remapping = ["up", "down", "left", "right", "confirm", "pause"][0];
    render();
    return;
  }
  if (verb === "new-town") {
    settings.seed = (settings.seed + 104729) >>> 0;
    state.seed = settings.seed;
    saveSettings();
    if (screen !== "title" && settingsFrom !== "title") save();
    render();
    toast("Fresh rooftops. The same familiar paths.");
    return;
  }
  if (verb === "llm") {
    settings.llmReturn = screen;
    status = "";
    go("llm");
    return;
  }
  if (verb === "llm-back") {
    readLlm();
    saveSettings();
    go(settings.llmReturn || "settings");
    return;
  }
  if (verb === "offline") {
    settings.llm.base = "";
    settings.llm.model = "";
    settings.llm.key = "";
    saveSettings();
    status = "Offline voices are ready. No connection needed.";
    render();
    return;
  }
  if (verb === "temperature") {
    readLlm();
    settings.llm.temperature = Number(
      ((settings.llm.temperature + 0.1) % 1.1).toFixed(1),
    );
    saveSettings();
    render();
    return;
  }
  if (verb === "tokens") {
    readLlm();
    settings.llm.maxTokens = settings.llm.maxTokens === 160 ? 96 : 160;
    saveSettings();
    render();
    return;
  }
  if (verb === "edit-llm") {
    document.querySelector(`#llm-${arg}`)?.focus();
    return;
  }
  if (verb === "test-llm") {
    readLlm();
    saveSettings();
    status = "Testing… up to eight seconds.";
    render();
    const el = document.querySelector('[data-action="test-llm"]');
    if (el) el.disabled = true;
    testConnection(settings.llm).then((result) => {
      status = result;
      if (screen === "llm") render();
    });
    return;
  }
  if (verb === "endless") {
    const i = Number(arg);
    if (!state.lit.includes(i)) return;
    if (state.puzzle && !state.puzzle.endless) {
      toast(
        "Your delivery puzzle is waiting. Finish it before starting an arcade round.",
      );
      go("puzzle");
      return;
    }
    const round = state.round;
    state.round = endlessRound++;
    beginPuzzle(state, settings.lively, i);
    state.round = round;
    signalTurn = -1;
    save();
    go("puzzle");
    signalClue();
    return;
  }
  if (verb === "mural") {
    if (state.chalk.length > state.murals) {
      state.murals = state.chalk.length;
      state.trail = state.murals % 6;
      save();
      audio.effect("win");
      toast(
        "Rosie: A boat, a star, a little light. You are officially in the club!",
      );
    } else
      toast("Rosie: There is more chalk sparkling beside the district paths.");
    go("murals");
    return;
  }
  if (verb === "putt") {
    puttPhase = "aim";
    status = "";
    go("putt");
    return;
  }
  if (verb === "putt-shot") {
    shootPutt();
    return;
  }
  if (verb === "putt-aim") {
    const cup = puttCup();
    state.putt.angle = Math.atan2(
      cup.x - state.putt.ball.x,
      state.putt.ball.z - cup.z,
    );
    state.putt.power = Math.min(
      1,
      Math.hypot(cup.x - state.putt.ball.x, cup.z - state.putt.ball.z) / 26,
    );
    drawPutt();
    return;
  }
  if (verb === "putt-next") {
    state.putt.hole = (state.putt.hole + 1) % 9;
    state.putt.strokes = 0;
    state.putt.ball = { x: 0, z: 7 };
    state.putt.angle = 0;
    state.putt.power = 0.5;
    puttPhase = "aim";
    status = "A fresh stretch of pier.";
    save();
    render();
    return;
  }
  if (verb === "rhythm-hit") {
    rhythmHit();
    return;
  }
  if (verb === "world") {
    dialogueToken++;
    save();
    go("world");
    return;
  }
  go(verb);
}
function readLlm() {
  for (const key of ["base", "model", "key"]) {
    const el = document.querySelector(`#llm-${key}`);
    if (el) settings.llm[key] = el.value.trim();
  }
}
ui.addEventListener("click", (event) => {
  const cell = event.target.closest("[data-cell]");
  if (cell && state.puzzle) {
    const p = state.puzzle,
      index = Number(cell.dataset.cell);
    if (p.kind === "stack") p.cursor = Math.min(p.rotation ? 4 : 5, index % 6);
    else if (p.kind === "drill") {
      const x = index % 7,
        y = Math.floor(index / 7);
      if (Math.abs(x - p.x) + Math.abs(y - p.y) !== 1) return;
      p.dir = x < p.x ? "left" : x > p.x ? "right" : y < p.y ? "up" : "down";
      p.cursor = index;
    } else p.cursor = index;
    doPuzzle("confirm");
    return;
  }
  const b = event.target.closest("[data-action]");
  if (b && !b.disabled) act(b.dataset.action);
});
function actionFor(event) {
  for (const [action, key] of Object.entries(settings.bindings))
    if (event.code === key) return action;
  const defaults = {
    ArrowUp: "up",
    ArrowDown: "down",
    ArrowLeft: "left",
    ArrowRight: "right",
    Enter: "confirm",
    Escape: "pause",
    KeyW: "up",
    KeyS: "down",
    KeyA: "left",
    KeyD: "right",
    Space: "confirm",
  };
  return defaults[event.code];
}
window.addEventListener("keydown", (event) => {
  audio.start();
  if (remapping) {
    event.preventDefault();
    if (event.code === "Escape") {
      remapping = null;
      render();
      return;
    }
    if (
      Object.entries(settings.bindings).some(
        ([a, k]) => a !== remapping && k === event.code,
      )
    ) {
      toast("That key is already assigned. Choose another.");
      return;
    }
    settings.bindings[remapping] = event.code;
    settings.preset = "custom";
    const order = ["up", "down", "left", "right", "confirm", "pause"];
    remapping = order[order.indexOf(remapping) + 1] || null;
    saveSettings();
    render();
    return;
  }
  if (event.target.matches("input")) {
    if (event.code === "Escape") {
      event.target.blur();
      event.preventDefault();
    } else if (event.code === "Enter" && screen === "llm") {
      readLlm();
      saveSettings();
      event.target.blur();
      event.preventDefault();
    } else if (event.code === "Enter" && screen === "type") {
      event.preventDefault();
      act("send-typed");
    }
    return;
  }
  const action = actionFor(event);
  if (!action) {
    if (screen === "world" && (event.code === "KeyQ" || event.code === "KeyE"))
      world.orbit += event.code === "KeyQ" ? -0.15 : 0.15;
    if (screen === "puzzle" && event.code === "KeyH") act("hint");
    return;
  }
  event.preventDefault();
  if (action === "pause" && !event.repeat) {
    if (screen === "title") return;
    if (
      screen === "puzzle" ||
      screen === "world" ||
      screen === "putt" ||
      screen === "rhythm"
    )
      act("pause");
    else if (screen === "pause") act("resume");
    else if (screen === "settings") act("settings-back");
    else if (screen === "llm") act("llm-back");
    else if (screen === "intro" || screen === "slots" || screen === "about")
      go("title");
    else if (screen === "fact") go("journal");
    else if (screen === "finale-intro") go("world");
    else if (screen === "credits") go("world");
    else if (screen === "type") go("dialogue");
    else go("world");
    return;
  }
  if (screen === "world") {
    if (action === "confirm" && !event.repeat) interact();
    else if (action !== "confirm") keys.add(action);
    return;
  }
  if (screen === "puzzle") {
    doPuzzle(action);
    return;
  }
  if (screen === "putt") {
    if (action === "confirm" && !event.repeat) shootPutt();
    else if (puttPhase === "aim") {
      if (action === "left") state.putt.angle -= 0.07;
      if (action === "right") state.putt.angle += 0.07;
      if (action === "up")
        state.putt.power = Math.min(1, state.putt.power + 0.05);
      if (action === "down")
        state.putt.power = Math.max(0.1, state.putt.power - 0.05);
      drawPutt();
    }
    return;
  }
  if (screen === "rhythm") {
    if (action === "confirm" && !event.repeat) rhythmHit();
    return;
  }
  if (["up", "left"].includes(action)) select(selected - 1);
  if (["down", "right"].includes(action)) select(selected + 1);
  if (action === "confirm" && !event.repeat) {
    const b = menuButtons()[selected];
    if (b) act(b.dataset.action);
  }
});
window.addEventListener("keyup", (event) => keys.delete(actionFor(event)));
window.addEventListener("blur", () => {
  keys.clear();
  if (screen !== "title") save();
});
window.addEventListener("beforeunload", () => {
  if (screen !== "title") save();
});
document.addEventListener("visibilitychange", () => {
  if (document.hidden) {
    keys.clear();
    if (screen === "world" || screen === "puzzle") act("pause");
  }
});
let drag = null;
document.querySelector("#world").addEventListener("pointerdown", (e) => {
  if (screen === "world") drag = e.clientX;
});
window.addEventListener("pointermove", (e) => {
  if (drag !== null) {
    world.orbit += (e.clientX - drag) * 0.007;
    drag = e.clientX;
  }
});
window.addEventListener("pointerup", () => (drag = null));
function puttCup() {
  return { x: Math.sin(state.putt.hole * 1.7) * 3, z: -6 };
}
let puttVelocity = { x: 0, z: 0 };
function shootPutt() {
  if (puttPhase === "sunk") {
    act("putt-next");
    return;
  }
  if (puttPhase !== "aim") return;
  const p = state.putt;
  puttVelocity = {
    x: Math.sin(p.angle) * p.power * 18,
    z: -Math.cos(p.angle) * p.power * 18,
  };
  p.strokes++;
  puttPhase = "rolling";
  audio.effect("tap");
}
function updatePutt(dt) {
  if (puttPhase !== "rolling") return;
  const p = state.putt,
    b = p.ball,
    cup = puttCup();
  b.x += puttVelocity.x * dt;
  b.z += puttVelocity.z * dt;
  if (Math.abs(b.x) > 4.6) {
    b.x = Math.sign(b.x) * 4.6;
    puttVelocity.x *= -0.8;
  }
  if (Math.abs(b.z) > 7.7) {
    b.z = Math.sign(b.z) * 7.7;
    puttVelocity.z *= -0.8;
  }
  const bumper = { x: Math.sin(p.hole) * 2, z: 0 };
  if (p.hole > 0 && Math.hypot(b.x - bumper.x, b.z) < 1) {
    const nx = b.x - bumper.x,
      nz = b.z,
      len = Math.hypot(nx, nz) || 1,
      proj = (puttVelocity.x * nx + puttVelocity.z * nz) / len;
    puttVelocity.x -= (2 * proj * nx) / len;
    puttVelocity.z -= (2 * proj * nz) / len;
    b.x = bumper.x + (nx / len) * 1.02;
    b.z = (nz / len) * 1.02;
  }
  const damping = Math.exp(-dt * 0.7);
  puttVelocity.x *= damping;
  puttVelocity.z *= damping;
  if (
    Math.hypot(b.x - cup.x, b.z - cup.z) < 0.65 &&
    Math.hypot(puttVelocity.x, puttVelocity.z) < 6
  ) {
    b.x = cup.x;
    b.z = cup.z;
    puttPhase = "sunk";
    p.best[p.hole] = Math.min(p.best[p.hole] || 999, p.strokes);
    status = `In the cup! ${p.strokes} strokes. Press Enter for the next hole.`;
    audio.effect("win");
    save();
  } else if (Math.hypot(puttVelocity.x, puttVelocity.z) < 0.12) {
    puttPhase = "aim";
    save();
  }
  drawPutt();
}
function drawPutt() {
  const canvas = document.querySelector("#putt");
  if (!canvas) return;
  const ctx = canvas.getContext("2d"),
    p = state.putt,
    cup = puttCup(),
    sx = (x) => 375 + x * 35,
    sz = (z) => 175 + z * 19;
  ctx.clearRect(0, 0, 750, 350);
  ctx.strokeStyle = "#a6eed5";
  ctx.lineWidth = 2;
  ctx.strokeRect(sx(-5), sz(-8), 350, 304);
  for (let i = -7; i < 8; i++) {
    ctx.strokeStyle = "#a6eed518";
    ctx.beginPath();
    ctx.moveTo(sx(-5), sz(i));
    ctx.lineTo(sx(5), sz(i));
    ctx.stroke();
  }
  ctx.strokeStyle = "#f5dda0";
  ctx.beginPath();
  ctx.arc(sx(cup.x), sz(cup.z), 10, 0, Math.PI * 2);
  ctx.stroke();
  ctx.fillStyle = "#f5dda0";
  ctx.font = "16px Georgia";
  ctx.fillText("⚑", sx(cup.x) + 10, sz(cup.z) - 7);
  if (p.hole > 0) {
    ctx.strokeStyle = "#c5b4f4";
    ctx.beginPath();
    ctx.ellipse(sx(Math.sin(p.hole) * 2), sz(0), 30, 17, 0, 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.fillStyle = "#fff3d6";
  ctx.shadowColor = "#f5dda0";
  ctx.shadowBlur = 12;
  ctx.beginPath();
  ctx.arc(sx(p.ball.x), sz(p.ball.z), 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;
  if (puttPhase === "aim") {
    ctx.setLineDash([5, 7]);
    ctx.strokeStyle = "#f5dda088";
    ctx.beginPath();
    ctx.moveTo(sx(p.ball.x), sz(p.ball.z));
    ctx.lineTo(
      sx(p.ball.x + Math.sin(p.angle) * p.power * 6),
      sz(p.ball.z - Math.cos(p.angle) * p.power * 6),
    );
    ctx.stroke();
    ctx.setLineDash([]);
  }
  const el = document.querySelector("#putt-status");
  if (el)
    el.textContent =
      puttPhase === "sunk"
        ? status
        : `${p.strokes} strokes · Power ${Math.round(p.power * 100)}% · ${puttPhase === "rolling" ? "A gentle roll…" : "Enter to shoot"}`;
}
function rhythmHit() {
  const phase = ((audio.context?.currentTime || 0) / (60 / 76)) % 1,
    close = Math.min(phase, 1 - phase) < (settings.lively ? 0.13 : 0.5);
  rhythmCombo = close ? rhythmCombo + 1 : 0;
  state.rhythm.hits++;
  state.rhythm.best = Math.max(state.rhythm.best, rhythmCombo);
  audio.effect("talk", rhythmCombo % 5);
  const el = document.querySelector("#rhythm-status");
  if (el)
    el.textContent = `${state.rhythm.hits} little lights · Combo ${rhythmCombo} · Best ${state.rhythm.best}`;
  save();
}
let last = performance.now(),
  hudTimer = 0,
  frames = 0,
  frameTime = 0;
function frame(now) {
  const rawDt = (now - last) / 1000,
    dt = Math.min(0.05, rawDt);
  last = now;
  if (screen !== "title") {
    state.seconds += dt;
    audio.setState(state, settings);
  }
  world.update(
    dt,
    state,
    keys,
    screen === "world",
    screen === "title" ||
      ["slots", "intro", "about", "overwrite", "name"].includes(screen),
  );
  if (screen === "puzzle" && state.puzzle?.lively) {
    const before = state.puzzle.moves,
      elapsed = state.puzzle.elapsed;
    tickPuzzle(state.puzzle, dt);
    if (state.puzzle.elapsed < elapsed || state.puzzle.moves !== before)
      render();
  }
  if (screen === "putt") updatePutt(dt);
  if (screen === "rhythm") {
    const t = audio.context?.currentTime || 0;
    ui.querySelectorAll(".rhythm-lanterns span").forEach(
      (el, i) =>
        (el.style.opacity = String(
          0.25 +
            0.75 *
              Math.max(0, Math.cos((t / (60 / 76)) * Math.PI * 2 - i * 0.5)),
        )),
    );
  }
  hudTimer += dt;
  if (hudTimer > 1 && screen === "world") {
    render();
    hudTimer = 0;
  }
  if (state.seconds - lastSave > 5 && screen !== "title") save();
  frames++;
  frameTime += rawDt;
  if (frameTime >= 1) {
    document.body.dataset.fps = String(Math.round(frames / frameTime));
    frames = 0;
    frameTime = 0;
  }
  requestAnimationFrame(frame);
}
applySettings();
render();
requestAnimationFrame(frame);
// Read-only diagnostics are available only in development or explicit test builds.
if (import.meta.env.DEV || import.meta.env.MODE === "test")
  window.__afterglow = {
    snapshot: () => structuredClone(state),
    screen: () => screen,
    layout: () => ({
      drawCalls: world.renderer.info.render.calls,
      triangles: world.renderer.info.render.triangles,
    }),
    settings: () => ({
      ...settings,
      llm: { ...settings.llm, key: "[redacted]" },
    }),
  };
