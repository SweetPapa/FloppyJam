import { readdirSync, statSync, existsSync } from "node:fs";
import { join } from "node:path";
function size(dir) {
  return readdirSync(dir).reduce((sum, name) => {
    const p = join(dir, name);
    return sum + (statSync(p).isDirectory() ? size(p) : statSync(p).size);
  }, 0);
}
if (existsSync("dist"))
  console.log(
    `Browser build: ${size("dist").toLocaleString()} bytes (no size cap)`,
  );
if (existsSync("release"))
  for (const name of readdirSync("release"))
    if (/\.(dmg|zip|exe|AppImage|tar\.gz)$/.test(name))
      console.log(
        `${name}: ${statSync(join("release", name)).size.toLocaleString()} bytes`,
      );
