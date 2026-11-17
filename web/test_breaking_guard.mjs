// SPDX-License-Identifier: GPL-3.0-only
// The installer page's breaking-update guard (E72): the markup, the texts in both languages, the syntax of the page's script.
//   node web/test_breaking_guard.mjs
import fs from "fs";
import os from "os";
import path from "path";
import { execFileSync } from "child_process";
const html = fs.readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");
let fails = 0;
const check = (what, ok) => { console.log(`guard: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const script = html.match(/<script type="module">([\s\S]*)<\/script>/)[1];
const tmp = path.join(os.tmpdir(), `guard-${process.pid}.mjs`);
fs.writeFileSync(tmp, script.replace("/*LIB*/", "").replace("/*META*/", '{"breaking":true,"version":"1.0.0","pkg":"x","product":"FM-1_900"}'));
let syntax = true;
try { execFileSync(process.execPath, ["--check", tmp], { stdio: "pipe" }); } catch (e) { syntax = false; console.log(String(e.stderr || e)); }
fs.unlinkSync(tmp);
check("the page's script has valid syntax", syntax);
const text = new Function("return " + script.match(/const TEXT = (\{[\s\S]*?\n\});/)[1])();
const ja = Object.keys(text.ja).sort(), en = Object.keys(text.en).sort();
check("both languages have the same keys", JSON.stringify(ja) === JSON.stringify(en));
for (const k of ["brkTag", "brkLead", "brk1", "brk2", "brk3", "brk4", "brkUnderstand", "brkBackup", "brkConfirm", "brkNoAuto", "brkNoAutoConfirm"])
  check(`text ${k} exists in ja and en`, !!text.ja[k] && !!text.en[k]);
for (const id of ["breaking", "brk-understand", "brk-backup", "go"])
  check(`markup has #${id}`, html.includes(`id="${id}"`));
const used = [...html.matchAll(/data-t="([A-Za-z0-9]+)"/g)].map((m) => m[1]);
check("every data-t key of the markup has a text", used.every((k) => text.en[k] && text.ja[k]));
check("the install button is gated by the confirmation", script.includes("brkReady()") && script.includes('$("brk-understand").checked && $("brk-backup").checked'));
check("a backup is captured and a final confirm asked before install", script.includes("captureBackup") && script.includes('confirm(t("brkConfirm"))'));
console.log(fails ? `guard: ${fails} FAILED` : "guard: all passed");
process.exit(fails ? 1 : 0);
