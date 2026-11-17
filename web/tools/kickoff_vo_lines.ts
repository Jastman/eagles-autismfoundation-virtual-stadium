// Writes the voice lines the kickoff templates need (journey/gameday.ts names them) to
// SourceData/Audio/VO/Kickoff/KICKOFF_VO_LINES.json, for Tools/Audio/make_kickoff_vo.py to record and align.
//
//   npx vite-node tools/kickoff_vo_lines.ts [--recorded]
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { kickoffVoiceLines } from "../src/journey/gameday";

const out = join(resolve(dirname(fileURLToPath(import.meta.url)), "..", ".."), "SourceData", "Audio", "VO", "Kickoff");
mkdirSync(out, { recursive: true });
const lines = Object.fromEntries([...kickoffVoiceLines()].sort(([a], [b]) => a.localeCompare(b)));
// --recorded: the lines in KICKOFF_VO_LINES.json now exist as recordings with exactly these words (journey/gameday.ts keeps a
// card silent when its words no longer match its recording). Run it after Tools/Audio/make_kickoff_vo.py has mastered them.
if (process.argv.includes("--recorded")) {
    writeFileSync(join(out, "KICKOFF_VO_RECORDED.json"), `${JSON.stringify(lines, null, 2)}\n`);
    console.log(`${Object.keys(lines).length} lines marked as recorded`);
}
writeFileSync(join(out, "KICKOFF_VO_LINES.json"), `${JSON.stringify(lines, null, 2)}\n`);
console.log(`${Object.keys(lines).length} kickoff voice lines -> ${out}`);
