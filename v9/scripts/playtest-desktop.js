import { _electron as electron } from "@playwright/test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
const profile = mkdtempSync(join(tmpdir(), "afterglow-desktop-playtest-"));
const executablePath = process.argv[2];
if (!executablePath)
  throw new Error("Pass the packaged application executable.");
const app = await electron.launch({
  executablePath,
  args: [`--user-data-dir=${profile}`],
  timeout: 30000,
});
try {
  const page = await app.firstWindow();
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.getByRole("button", { name: /Begin your story/ }).waitFor();
  await page.keyboard.press("Enter");
  await page.keyboard.press("Enter");
  await page.keyboard.press("Enter");
  await page.getByText("Your route & notebook", { exact: false }).count();
  await page.keyboard.press("Escape");
  await page.getByRole("button", { name: "Keep going", exact: true }).waitFor();
  const isolation = await page.evaluate(() => ({
    require: typeof window.require,
    bridge: typeof window.afterglowDesktop?.request,
  }));
  if (isolation.require !== "undefined" || isolation.bridge !== "function")
    throw new Error("Desktop bridge isolation failed");
  await page.screenshot({ path: "/private/tmp/afterglow-desktop.png" });
  if (errors.length) throw new Error(errors.join("\n"));
  console.log(
    JSON.stringify({
      packagedApp: "launched",
      keyboard: "title to game to pause",
      isolation,
      errors,
    }),
  );
} finally {
  await app.close();
}
