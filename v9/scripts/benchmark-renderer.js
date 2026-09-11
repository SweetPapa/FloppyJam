import { chromium } from "@playwright/test";
import { newGame } from "../src/state.js";
const duration = Number(process.argv[2] || 300),
  s = newGame();
s.night = 2;
s.position = { x: 64, y: 0, z: 10 };
s.weather = { rain: 1, snow: 0 };
const browser = await chromium.launch({
  channel: process.env.PW_CHANNEL || undefined,
  args: ["--enable-webgl", "--ignore-gpu-blocklist"],
});
try {
  const page = await browser.newPage({
    viewport: { width: 1920, height: 1080 },
  });
  await page.addInitScript(
    (value) => localStorage.setItem("afterglow.slot.0", JSON.stringify(value)),
    s,
  );
  await page.goto("http://127.0.0.1:5199");
  await page.getByRole("button", { name: /Continue/ }).click();
  await page.getByRole("button", { name: /Slot 1/ }).click();
  await page.waitForTimeout(5000);
  const result = await page.evaluate(async (duration) => {
    const samples = [];
    let last = performance.now(),
      start = last;
    return await new Promise((resolve) => {
      function tick(now) {
        samples.push(now - last);
        last = now;
        if (now - start < duration * 1000) {
          requestAnimationFrame(tick);
          return;
        }
        samples.sort((a, b) => a - b);
        const canvas = document.querySelector("#world"),
          gl = canvas.getContext("webgl2"),
          ext = gl.getExtension("WEBGL_debug_renderer_info");
        resolve({
          seconds: (now - start) / 1000,
          frames: samples.length,
          meanFps: samples.length / ((now - start) / 1000),
          p95FrameMs: samples[Math.floor(samples.length * 0.95)],
          p99FrameMs: samples[Math.floor(samples.length * 0.99)],
          renderer: ext
            ? gl.getParameter(ext.UNMASKED_RENDERER_WEBGL)
            : "unknown",
          scene: window.__afterglow.layout(),
          viewport: [innerWidth, innerHeight],
        });
      }
      requestAnimationFrame(tick);
    });
  }, duration);
  await page.screenshot({ path: "/private/tmp/afterglow-rain-benchmark.png" });
  console.log(JSON.stringify(result, null, 2));
} finally {
  await browser.close();
}
