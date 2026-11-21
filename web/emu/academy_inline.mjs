// Inline the tutorial data module into the emulator page (build step for the public site, whose emulator directory is a closed file list).
// usage: node academy_inline.mjs <build/emu directory>
import { readFileSync, writeFileSync, rmSync, existsSync } from "node:fs";
const dir = process.argv[2];
const htmlPath = `${dir}/index.html`, dataPath = `${dir}/tutorial_data.mjs`;
if (!existsSync(dataPath)) { console.log("no tutorial_data.mjs: nothing to inline"); process.exit(0); }
const html = readFileSync(htmlPath, "utf8"), data = readFileSync(dataPath, "utf8").replace(/^export\s+/gm, "");
const re = /^import \{[^}]*\} from "\.\/tutorial_data\.mjs";\n/m;
if (!re.test(html)) throw new Error("tutorial_data import not found in index.html");
writeFileSync(htmlPath, html.replace(re, () => data + "\n"));
rmSync(dataPath);
console.log("inlined tutorial data into index.html (" + data.length + " bytes) and removed tutorial_data.mjs");
