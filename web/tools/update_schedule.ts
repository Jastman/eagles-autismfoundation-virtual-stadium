// Brings the bundled home schedule (SourceData/Journey/LFF_KickoffTemplates.json) up to date with the league's schedule feed:
// opponents, dates, kickoff times, broadcasts, and a tour template for any kickoff time the file doesn't have yet. The app does the
// same check on its own while it runs (journey/schedule.ts); this keeps what ships with the app, and the Unreal build's copy, right.
// It follows the same season rule: the season stays until the league's season is officially over.
//
//   npx vite-node tools/update_schedule.ts            # write the file
//   npx vite-node tools/update_schedule.ts --check    # show what would change, write nothing
//
// Then, for cards whose words changed (a game moved to another time): npx vite-node tools/kickoff_vo_lines.ts, record the new
// lines (Tools/Audio/make_kickoff_vo.py), and npx vite-node tools/kickoff_vo_lines.ts --recorded. Until then those cards are silent.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bundledGames, deriveTemplate } from "../src/journey/gameday";
import type { HomeGame, KickoffTemplate } from "../src/journey/gameday";
import { describeChanges, easternParts, fetchHomeSchedule } from "../src/journey/schedule";

const file = join(resolve(dirname(fileURLToPath(import.meta.url)), "..", ".."), "SourceData", "Journey", "LFF_KickoffTemplates.json");
const check = process.argv.includes("--check");
const now = new Date();

const { season, games } = await fetchHomeSchedule(now, bundledGames());
if (games.length === 0) {
    console.log(`The ${season} schedule has no home games yet; the file is unchanged.`);
    process.exit(0);
}

const original = readFileSync(file, "utf8");
const data = JSON.parse(original) as { Season: string; Games: HomeGame[]; Templates: KickoffTemplate[] };
const changes = Number(data.Season) < season
    ? [`The ${season} season's home schedule replaces ${data.Season}'s.`]
    : describeChanges(data.Games, games, easternParts(now.getTime()).date);
for (const line of changes) console.log(`- ${line}`);
if (changes.length === 0) console.log("No changes to the schedule.");

// A template for every kickoff time that has none.
const added: KickoffTemplate[] = [];
for (const game of games) {
    if (data.Templates.some((t) => t.Id === game.Template) || added.some((t) => t.Id === game.Template)) continue;
    added.push(deriveTemplate(Number(game.Template.slice(0, 2)) * 60 + Number(game.Template.slice(2))));
}
if (added.length) console.log(`New tour templates (estimated) for: ${added.map((t) => t.Label).join(", ")}`);

// The file is kept by hand (one line per game), so only the season, check time, games and new templates are rewritten in place.
const gameLines = games
    .map((g) => `    { ${Object.entries(g).map(([k, v]) => `${JSON.stringify(k)}: ${JSON.stringify(v)}`).join(", ")} }`)
    .join(",\n");
let text = original
    .replace(/\n  "ScheduleChecked": "[^"]*",/, "")
    .replace(/\n  "Season": "[^"]*",/, `\n  "Season": "${season}",\n  "ScheduleChecked": "${now.toISOString()}",`)
    .replace(/"Games": \[[\s\S]*?\n  \],/, () => `"Games": [\n${gameLines}\n  ],`);
if (added.length) {
    const blocks = added.map((t) => JSON.stringify(t, null, 2).replace(/^/gm, "    ")).join(",\n");
    text = text.replace(/\n  \]\n\}\s*$/, () => `,\n${blocks}\n  ]\n}\n`);
}
if (check) {
    console.log("(--check: nothing written)");
} else {
    writeFileSync(file, text);
    console.log(`${games.length} home games for ${season} -> ${file}`);
}
