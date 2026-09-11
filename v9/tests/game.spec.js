import { test, expect } from "@playwright/test";
const snap = (page) => page.evaluate(() => window.__afterglow.snapshot());
async function screen(page, name) {
  await expect
    .poll(() => page.evaluate(() => window.__afterglow?.screen()))
    .toBe(name);
}
async function choose(page, action) {
  await expect(page.locator(`[data-action="${action}"]`)).toBeVisible();
  for (let i = 0; i < 40; i++) {
    const current = await page
      .locator("button.selected")
      .getAttribute("data-action")
      .catch(() => null);
    if (current === action) {
      await page.keyboard.press("Enter");
      return;
    }
    await page.keyboard.press("ArrowDown");
  }
  throw Error(`Could not choose ${action}`);
}
async function route(page, index) {
  await page.keyboard.press("Escape");
  await screen(page, "pause");
  await choose(page, "map");
  await choose(page, `travel:${index}`);
  await screen(page, "world");
  const d = [
    { x: 0, z: 4 },
    { x: 64, z: 10 },
    { x: 30, z: -58 },
    { x: -45, z: -54 },
    { x: -68, z: 21 },
  ][index];
  await expect
    .poll(
      async () => {
        const s = await snap(page);
        return Math.hypot(s.position.x - d.x, s.position.z - d.z);
      },
      { timeout: 15000 },
    )
    .toBeLessThan(0.2);
}
async function helpRound(page) {
  for (let n = 0; n < 3; n++) {
    if ((await snap(page)).puzzle.solved) break;
    await page.keyboard.press("Escape");
    await choose(page, "hint");
  }
  if (!(await snap(page)).puzzle.solved) {
    await page.keyboard.press("Escape");
    await choose(page, "solve");
  }
  await page.keyboard.press("Enter");
}
async function begin(page) {
  await page.goto("/");
  await page.waitForFunction(() => window.__afterglow);
  await choose(page, "new");
  await choose(page, "slot:0");
  await choose(page, "start");
  await screen(page, "world");
}
test("visual title, delivery, and puzzle smoke", async ({ page }) => {
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto("/");
  await page.waitForFunction(() => window.__afterglow);
  await page.waitForTimeout(1500);
  await page.screenshot({ path: "test-results/title.png" });
  await choose(page, "new");
  await choose(page, "slot:0");
  await choose(page, "start");
  await route(page, 0);
  await page.waitForTimeout(1000);
  await page.screenshot({ path: "test-results/harbor.png" });
  await page.keyboard.press("Enter");
  await screen(page, "dialogue");
  await choose(page, "puzzle");
  await page.screenshot({ path: "test-results/phrase.png" });
  expect(errors).toEqual([]);
});
test("whole campaign using only arrows, Enter, Esc with unreachable model", async ({
  page,
}) => {
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.addInitScript(() =>
    localStorage.setItem(
      "afterglow.settings",
      JSON.stringify({
        llm: {
          base: "http://127.0.0.1:1",
          model: "unreachable",
          key: "",
          temperature: 0.7,
          maxTokens: 160,
        },
      }),
    ),
  );
  await begin(page);
  for (let night = 0; night < 5; night++) {
    await route(page, night);
    await page.keyboard.press("Enter");
    await screen(page, "dialogue");
    await choose(page, "puzzle");
    for (let round = 0; round < 3; round++) {
      await screen(page, "puzzle");
      expect((await snap(page)).round).toBe(round);
      await helpRound(page);
    }
    await screen(page, "fact");
    await page.screenshot({ path: `test-results/night-${night + 1}.png` });
    await choose(page, "journal");
    const id = ["left-hand", "brass", "wrench", "testing", "ada"][night];
    await choose(page, `clue:${id}`);
    await choose(
      page,
      `pin:${["Odette", "Bo", "Pearl", "Idris", "Ada"][night]}`,
    );
    if (night < 4) {
      await screen(page, "journal");
      await choose(page, "home");
      await expect
        .poll(
          async () => {
            const s = await snap(page);
            return Math.hypot(s.position.x, s.position.z - 4);
          },
          { timeout: 15000 },
        )
        .toBeLessThan(0.2);
      await page.keyboard.press("Enter");
      await screen(page, "night-end");
      await choose(page, "sleep");
      await choose(page, "world");
    }
  }
  await screen(page, "finale-intro");
  await choose(page, "world");
  for (let i = 0; i < 5; i++) {
    await route(page, i);
    await page.keyboard.press("Enter");
    expect((await snap(page)).finale).toContain(i);
  }
  await route(page, 0);
  await page.keyboard.press("Enter");
  await screen(page, "credits");
  expect((await snap(page)).completed).toBe(true);
  await page.screenshot({ path: "test-results/credits.png" });
  expect(errors).toEqual([]);
});
test("save reload keeps in-progress puzzle and exact position", async ({
  page,
}) => {
  await begin(page);
  await route(page, 0);
  await page.keyboard.press("Enter");
  await choose(page, "puzzle");
  await page.keyboard.press("ArrowRight");
  await page.keyboard.press("Enter");
  const before = await snap(page);
  await page.reload();
  await page.waitForFunction(() => window.__afterglow);
  await choose(page, "load");
  await choose(page, "slot:0");
  await screen(page, "puzzle");
  const after = await snap(page);
  expect(after.puzzle).toEqual(before.puzzle);
  expect(after.position).toEqual(before.position);
});
import { stackMove } from "./solver.js";
async function cursorTo(page, index, width) {
  let p = (await snap(page)).puzzle;
  if (width) {
    while (Math.floor(p.cursor / width) !== Math.floor(index / width)) {
      await page.keyboard.press("ArrowDown");
      p = (await snap(page)).puzzle;
    }
    while (p.cursor % width !== index % width) {
      await page.keyboard.press("ArrowRight");
      p = (await snap(page)).puzzle;
    }
  } else {
    while (p.cursor !== index) {
      await page.keyboard.press("ArrowRight");
      p = (await snap(page)).puzzle;
    }
  }
}
async function solveNormally(page) {
  let p = (await snap(page)).puzzle;
  for (let n = 0; n < 250 && !p.solved; n++) {
    if (p.kind === "phrase") {
      const letter = [...p.phrase].find(
        (c) => c !== " " && !p.guessed.includes(c),
      );
      await cursorTo(page, letter.charCodeAt() - 65);
    }
    if (p.kind === "stack") {
      const move = stackMove(p);
      if (p.rotation !== move.rot) await page.keyboard.press("ArrowUp");
      let cursor = (await snap(page)).puzzle.cursor;
      while (cursor !== move.col) {
        await page.keyboard.press(
          cursor < move.col ? "ArrowRight" : "ArrowLeft",
        );
        cursor = (await snap(page)).puzzle.cursor;
      }
    }
    if (p.kind === "drill") await page.keyboard.press("ArrowDown");
    if (p.kind === "light") {
      const index = Number(
        Object.keys(p.solution).find((i) => p.solution[i] !== p.board[i].rot),
      );
      await cursorTo(page, index, 6);
    }
    if (p.kind === "signal") {
      const word = p.clue.targets.find(
        (w) => !p.revealed.includes(p.board.findIndex((c) => c.word === w)),
      );
      await cursorTo(
        page,
        p.board.findIndex((c) => c.word === word),
        5,
      );
    }
    await page.keyboard.press("Enter");
    p = (await snap(page)).puzzle;
  }
  expect(p.solved).toBe(true);
  expect(p.hints).toBe(0);
  await page.keyboard.press("Enter");
}
test("all fifteen rounds solved normally and side attractions played", async ({
  page,
}) => {
  test.setTimeout(300000);
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await begin(page);
  for (let night = 0; night < 5; night++) {
    await route(page, night);
    await page.keyboard.press("Enter");
    await choose(page, "puzzle");
    for (let r = 0; r < 3; r++) {
      if (r === 0)
        await page.screenshot({ path: `test-results/puzzle-${night + 1}.png` });
      await solveNormally(page);
    }
    await choose(page, "journal");
    await choose(
      page,
      `clue:${["left-hand", "brass", "wrench", "testing", "ada"][night]}`,
    );
    await choose(
      page,
      `pin:${["Odette", "Bo", "Pearl", "Idris", "Ada"][night]}`,
    );
    if (night < 4) {
      await choose(page, "home");
      await expect
        .poll(
          async () =>
            Math.hypot(
              (await snap(page)).position.x,
              (await snap(page)).position.z - 4,
            ),
          { timeout: 15000 },
        )
        .toBeLessThan(0.2);
      await page.keyboard.press("Enter");
      await choose(page, "sleep");
      await choose(page, "world");
    }
  }
  await choose(page, "world");
  await page.keyboard.press("Escape");
  await choose(page, "arcade");
  await choose(page, "putt");
  await page.keyboard.press("ArrowLeft");
  await page.keyboard.press("ArrowUp");
  await page.keyboard.press("Enter");
  await page.waitForTimeout(1200);
  expect((await snap(page)).putt.strokes).toBe(1);
  await page.screenshot({ path: "test-results/putt.png" });
  await page.keyboard.press("Escape");
  await choose(page, "arcade");
  await choose(page, "rhythm");
  for (let i = 0; i < 8; i++) await page.keyboard.press("Enter");
  expect((await snap(page)).rhythm.hits).toBe(8);
  await page.keyboard.press("Escape");
  await choose(page, "arcade");
  await choose(page, "murals");
  await page.screenshot({ path: "test-results/murals.png" });
  expect(errors).toEqual([]);
});
test("comfort settings work at XL and preserve the game when changing slots", async ({
  page,
}) => {
  await begin(page);
  await page.keyboard.press("Escape");
  await choose(page, "settings");
  await choose(page, "text");
  await expect(page.locator("html")).toHaveClass(/xl/);
  await choose(page, "toggle:contrast");
  await choose(page, "toggle:reduced");
  await choose(page, "new-town");
  await choose(page, "settings-back");
  await choose(page, "save-title");
  await choose(page, "new");
  await choose(page, "slot:0");
  await screen(page, "overwrite");
  const before = await page.evaluate(() =>
    localStorage.getItem("afterglow.slot.0"),
  );
  await page.waitForTimeout(6000);
  expect(
    await page.evaluate(() => localStorage.getItem("afterglow.slot.0")),
  ).toBe(before);
  await choose(page, "slots");
  await page.keyboard.press("Escape");
  await choose(page, "settings");
  await page.screenshot({ path: "test-results/comfort-xl.png" });
});
