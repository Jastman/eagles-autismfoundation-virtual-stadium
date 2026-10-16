// Keeps the Unreal project and the web build from drifting apart (Docs/WEB_PARITY.md).
//
// Unreal -> web: every Unreal file the web build ports by hand is listed in web/parity/sources.json with the hash it
// had when the web port last matched it. A changed hash means the web counterparts need the same change.
// Web -> Unreal: every row in Docs/WEB_PARITY.md whose Unreal status is "Pending" is a web change Unreal hasn't taken.
//
// Files the web reads directly at build time (SourceData/Journey, SourceData/Audio, Content/LFF/Crowd/*.json) need no
// entry: a change there reaches both builds on its own.
//
//   node tools/check_parity.mjs            report; exit 1 if anything is outstanding
//   node tools/check_parity.mjs --accept   record the current Unreal hashes (after porting the change to the web)
//   node tools/check_parity.mjs --accept Source/VirtualStadium/Private/UI/LFFHUDWidget.cpp   accept one file
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const web = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repo = resolve(web, "..");
const sourcesPath = join(web, "parity", "sources.json");
const ledgerPath = join(repo, "Docs", "WEB_PARITY.md");

const sha = (path) => createHash("sha256").update(readFileSync(path)).digest("hex").slice(0, 16);
const sources = JSON.parse(readFileSync(sourcesPath, "utf8"));
const args = process.argv.slice(2);

if (args[0] === "--accept") {
    const only = new Set(args.slice(1));
    for (const [path, entry] of Object.entries(sources.files)) {
        if (only.size > 0 && !only.has(path)) continue;
        const full = join(repo, path);
        entry.sha = existsSync(full) ? sha(full) : null;
    }
    sources.updated = new Date().toISOString().slice(0, 10);
    writeFileSync(sourcesPath, JSON.stringify(sources, null, 2) + "\n");
    console.log(`check_parity: accepted ${only.size > 0 ? [...only].join(", ") : "all files"}.`);
    process.exit(0);
}

let outstanding = 0;
const changed = [];
for (const [path, entry] of Object.entries(sources.files)) {
    const full = join(repo, path);
    const now = existsSync(full) ? sha(full) : null;
    if (now !== entry.sha) changed.push({ path, entry, now });
}
if (changed.length > 0) {
    console.log(`Unreal -> web: ${changed.length} Unreal file(s) changed since the web port last matched them:`);
    for (const { path, entry, now } of changed) {
        console.log(`  ${path}${now === null ? " (deleted)" : ""}`);
        console.log(`      port to: ${entry.web.join(", ")}${entry.how ? `  (${entry.how})` : ""}`);
    }
    console.log("  When the web matches again: node tools/check_parity.mjs --accept <file>...");
    outstanding += changed.length;
} else {
    console.log("Unreal -> web: up to date.");
}

// Ledger rows: | ID | ... | Unreal status | ... ; a row is pending when a cell reads "Pending".
const pending = existsSync(ledgerPath)
    ? readFileSync(ledgerPath, "utf8").split("\n").filter((line) => /^\|\s*W\d+/.test(line) && /\|\s*Pending\b/i.test(line))
    : [];
if (pending.length > 0) {
    console.log(`Web -> Unreal: ${pending.length} web change(s) not yet in the Unreal build (Docs/WEB_PARITY.md):`);
    for (const line of pending) {
        const cells = line.split("|").map((c) => c.trim());
        console.log(`  ${cells[1]}  ${cells[2]}`);
    }
    outstanding += pending.length;
} else {
    console.log("Web -> Unreal: nothing pending.");
}
process.exit(outstanding > 0 && !args.includes("--report-only") ? 1 : 0);
