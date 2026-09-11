import { readdirSync } from "node:fs";
import { join } from "node:path";
const forbidden =
  /\.(png|jpe?g|webp|gif|ico|icns|svg|wav|mp3|ogg|flac|ttf|otf|woff2?|mp4|glb|gltf|obj)$/i;
function walk(dir) {
  return readdirSync(dir, { withFileTypes: true }).flatMap((e) =>
    [
      "node_modules",
      "dist",
      "release",
      "test-results",
      "playwright-report",
      ".git",
    ].includes(e.name)
      ? []
      : e.isDirectory()
        ? walk(join(dir, e.name))
        : [join(dir, e.name)],
  );
}
const files = walk(".").filter((f) => forbidden.test(f));
if (files.length) {
  console.error("External assets are forbidden:", files);
  process.exit(1);
}
console.log("Asset gate passed: all game art and audio are generated in code.");
