// Schedule refresh (journey/schedule.ts) and the kickoff-time choice (journey/gameday.ts). Web additions, Docs/WEB_PARITY.md W25:
// no Unreal test to port.
import { afterEach, describe, expect, it } from "vitest";
import {
    GameDayController, bundledChecked, bundledGames, darkNoteFor, deriveTemplate, homeGames, kickoffTemplates, kickoffVoiceLines, parseClock, resolveGameDay,
    setHomeGames, templateById,
} from "../src/journey/gameday";
import type { HomeGame } from "../src/journey/gameday";
import { ScheduleUpdater, activeSeason, broadcastText, describeChanges, easternParts, parseEspnSchedule } from "../src/journey/schedule";
import type { EspnSchedule } from "../src/journey/schedule";

afterEach(() => setHomeGames(bundledGames()));

const team = (abbreviation: string, displayName: string, shortDisplayName: string) => ({ abbreviation, displayName, shortDisplayName });
const PHI = team("PHI", "Philadelphia Eagles", "Eagles");
const event = (date: string, home: ReturnType<typeof team>, away: ReturnType<typeof team>, extra: Record<string, unknown> = {}, nets = ["CBS"]) => ({
    date,
    competitions: [{
        competitors: [{ homeAway: "home", team: home }, { homeAway: "away", team: away }],
        broadcasts: nets.map((shortName) => ({ media: { shortName } })),
        status: { type: { name: "STATUS_SCHEDULED" } },
        timeValid: true,
        ...extra,
    }],
});
const STEELERS = team("PIT", "Pittsburgh Steelers", "Steelers");

describe("Eastern time from the feed's UTC moments", () => {
    it("knows daylight time to the hour it changes", () => {
        expect(easternParts(Date.parse("2026-09-13T20:25:00Z"))).toEqual({ date: "2026-09-13", minutes: 16 * 60 + 25 });
        // 00:15 UTC on October 27 is 8:15 PM the evening before.
        expect(easternParts(Date.parse("2026-10-27T00:15:00Z"))).toEqual({ date: "2026-10-26", minutes: 20 * 60 + 15 });
        // Saturday night before clocks fall back (Nov 1, 2026): still daylight time at 7:15 PM.
        expect(easternParts(Date.parse("2026-11-01T00:15:00Z"))).toEqual({ date: "2026-10-31", minutes: 20 * 60 + 15 });
        expect(easternParts(Date.parse("2026-11-22T18:00:00Z"))).toEqual({ date: "2026-11-22", minutes: 13 * 60 });
    });
});

describe("Reading the league's schedule", () => {
    const feed: EspnSchedule = {
        events: [
            event("2026-11-22T18:00Z", PHI, STEELERS),
            event("2026-11-26T21:30Z", team("DAL", "Dallas Cowboys", "Cowboys"), PHI),
            event("2026-10-11T13:30Z", PHI, team("JAX", "Jacksonville Jaguars", "Jaguars"), { neutralSite: true }),
            event("2026-09-13T20:25Z", PHI, team("WSH", "Washington Commanders", "Commanders"), {}, ["Fox"]),
            event("2026-12-25T01:15Z", PHI, team("HOU", "Houston Texans", "Texans"), {}, ["Prime Video"]),
            event("2026-12-30T01:15Z", PHI, team("NYG", "New York Giants", "Giants"), { status: { type: { name: "STATUS_CANCELED" } } }),
        ],
    };

    it("keeps the Eagles' home games, in date order, with their kickoff time as a template", () => {
        const games = parseEspnSchedule([feed]);
        expect(games.map((g) => [g.Date, g.OpponentShort, g.Template])).toEqual([
            ["2026-09-13", "Commanders", "1625"],
            ["2026-11-22", "Steelers", "1300"],
            ["2026-12-24", "Texans", "2015"],
        ]);
        const steelers = games[1];
        expect(steelers).toMatchObject({ Weekday: "Sunday", DateText: "November 22", Opponent: "Pittsburgh Steelers", OpponentCode: "PIT", Broadcast: "CBS", DarkNote: "" });
        expect(games[0].OpponentCode).toBe("WAS");
        expect(games[0].Broadcast).toBe("FOX");
        expect(games[2]).toMatchObject({ Weekday: "Thursday", Broadcast: "Thursday Night Football, Prime Video" });
    });

    it("keeps the time it had when the league hasn't set one", () => {
        const tbd = { events: [event("2026-12-13T05:00Z", PHI, team("IND", "Indianapolis Colts", "Colts"), { timeValid: false })] };
        // The league's placeholder hour is 12:00 AM Eastern on the 13th. The game we already had (the same opponent) keeps its time...
        const known = parseEspnSchedule([tbd], [{ ...bundledGames()[6], Date: "2026-12-12", Template: "1700" }]);
        expect(known[0]).toMatchObject({ Date: "2026-12-13", Template: "1700" });
        // ...and with no such game it is the usual 1:00 PM.
        expect(parseEspnSchedule([tbd])[0]).toMatchObject({ Date: "2026-12-13", Template: "1300" });
    });

    it("names the broadcast the way the scoreboard says it", () => {
        expect(broadcastText("Monday", 20 * 60 + 15, ["ESPN", "ABC"])).toBe("Monday Night Football, ESPN");
        expect(broadcastText("Sunday", 20 * 60 + 20, ["NBC"])).toBe("Sunday Night Football, NBC");
        expect(broadcastText("Sunday", 13 * 60, ["NBC"])).toBe("NBC");
        expect(broadcastText("Sunday", 13 * 60, [])).toBe("");
    });
});

describe("Which season is shown", () => {
    const info = { year: 2026, endDate: "2027-02-16T07:59Z" };
    it("stays on the current season until it is officially over", () => {
        expect(activeSeason(info, new Date("2027-01-20T12:00:00Z"))).toBe(2026);
        // Super Bowl Sunday itself: not over yet.
        expect(activeSeason(info, new Date("2027-02-14T23:00:00Z"))).toBe(2026);
        expect(activeSeason(info, new Date("2027-02-16T07:58:00Z"))).toBe(2026);
        expect(activeSeason(info, new Date("2027-02-16T08:00:00Z"))).toBe(2027);
        expect(activeSeason(info, new Date("2027-05-01T12:00:00Z"))).toBe(2027);
    });
    it("falls back to the league year (March 1) when the feed can't say, and never goes back before the bundled season", () => {
        expect(activeSeason(null, new Date("2027-02-10T12:00:00Z"))).toBe(2026);
        expect(activeSeason(null, new Date("2027-03-02T12:00:00Z"))).toBe(2027);
        expect(activeSeason(null, new Date("2026-02-10T12:00:00Z"))).toBe(2026);
    });
});

describe("The daylight note is worked out, not typed", () => {
    it("agrees with the notes written by hand for every bundled game", () => {
        for (const g of bundledGames()) {
            expect(darkNoteFor(g.Date, parseClock(templateById(g.Template)!.Kickoff)!), g.Date).toBe(g.DarkNote);
        }
    });
});

describe("Tours for kickoff times the file has no template for", () => {
    const stripScene = (steps: Record<string, Record<string, unknown>>) =>
        Object.fromEntries(Object.entries(steps).map(([k, row]) => [k, { ...row, ...(row.SceneTime ? { SceneTime: parseClock(String(row.SceneTime)) } : {}) }]));

    it("moves every time by the difference, as the estimated 5:00 PM template was made", () => {
        const bundled = templateById("1700")!;
        const made = deriveTemplate(17 * 60);
        expect(stripScene(made.Steps)).toEqual(stripScene(bundled.Steps));
        expect(made.Estimated).toBe(true);
    });

    it("follows the Eagles' own night schedule from 8:00 PM on, with its light show", () => {
        const t = templateById("2020")!;
        expect(t.Label).toBe("8:20 PM");
        expect(t.Steps["parking"].Body).toBe("The parking lots open in the afternoon for an 8:20 PM game. We can take our time.");
        expect(t.Steps["plaza-gates"].Title).toBe("The Pepsi Plaza gates open at 5:05 PM.");
        expect(t.InsertSteps[0].Step.Title).toBe("There will be a cell phone light show around 8:05 PM.");
        expect(t.RemoveSteps).toEqual(["inner-gates"]);
        expect(templateById("2020")).toBe(t);
        expect(templateById("9999")).toBeNull();
    });

    it("gets a tour whose card times move with the kickoff", () => {
        const game: HomeGame = { ...bundledGames()[0], Date: "2026-12-06", Template: "1830" };
        const day = resolveGameDay(game);
        expect(day.template.Label).toBe("6:30 PM");
        expect(day.steps.find((s) => s.StepId === "kickoff")!.Title).toBe("The game begins at about 6:30 PM.");
        expect(day.kickoffMinutes).toBe(18 * 60 + 30);
    });
});

describe("Choosing a kickoff time in Settings", () => {
    it("keeps the opponent and changes only the times", () => {
        const controller = new GameDayController(() => new Date("2026-11-17T15:00:00Z"), "?game=2026-11-22");
        expect(controller.current.game.OpponentShort).toBe("Steelers");
        expect(controller.current.template.Label).toBe("1:00 PM");
        const seen: string[] = [];
        controller.onChanged((day) => seen.push(`${day.game.OpponentShort} ${day.template.Label}`));
        controller.selectTemplate("2015");
        expect(controller.current.game.OpponentShort).toBe("Steelers");
        expect(controller.current.template.Label).toBe("8:15 PM");
        expect(controller.isTimeOverridden).toBe(true);
        expect(controller.current.steps.find((s) => s.StepId === "intro")!.Body).toContain("Pittsburgh Steelers on Sunday, November 22. Kickoff is at 8:15 PM.");
        // Back to the game's own time.
        controller.selectTemplate("1300");
        expect(controller.isTimeOverridden).toBe(false);
        expect(seen).toEqual(["Steelers 8:15 PM", "Steelers 1:00 PM"]);
        // Picking a game plays it at its own time.
        controller.selectTemplate("1700");
        controller.select("2026-12-13");
        expect(controller.current.game.OpponentShort).toBe("Colts");
        expect(controller.isTimeOverridden).toBe(false);
    });

    it("lists a chip for every kickoff time a game is played at", () => {
        const ids = kickoffTemplates().map((t) => t.Id);
        expect(ids).toEqual(["1300", "1625", "1700", "2015"]);
        setHomeGames([...homeGames(), { ...bundledGames()[0], Date: "2027-01-03", Template: "2020" }]);
        expect(kickoffTemplates().map((t) => t.Id)).toEqual(["1300", "1625", "1700", "2015", "2020"]);
    });
});

describe("Voice lines follow their words", () => {
    it("plays a recording only when the card still says what was recorded", () => {
        const colts = resolveGameDay(bundledGames().find((g) => g.Date === "2026-12-13")!);
        expect(colts.steps.find((s) => s.StepId === "intro")!.VoiceWave).toContain("VO_02_Intro__g20261213");
        // The Steelers moved from 4:25 PM to 1:00 PM and their intro and kickoff lines were re-recorded for it.
        const steelers = resolveGameDay(bundledGames().find((g) => g.Date === "2026-11-22")!);
        expect(steelers.steps.find((s) => s.StepId === "intro")!.VoiceWave).toContain("VO_02_Intro__g20261122");
        expect(steelers.steps.find((s) => s.StepId === "kickoff")!.VoiceWave).toContain("VO_19_Kickoff__g20261122");
        // The same game shown at another kickoff time says other words than any recording: the card stays silent.
        const other = resolveGameDay(bundledGames().find((g) => g.Date === "2026-11-22")!, undefined, "2015");
        expect(other.steps.find((s) => s.StepId === "intro")!.VoiceWave).toBe("");
        expect(other.steps.find((s) => s.StepId === "kickoff")!.VoiceWave).toBe("");
        // The rest of the 1:00 PM tour is the main recording.
        expect(steelers.steps.find((s) => s.StepId === "plaza-gates")!.VoiceWave).not.toContain("/Kickoff/");
        // The list of lines to record still has them.
        const lines = kickoffVoiceLines();
        expect(lines.has("VO_02_Intro__g20261122")).toBe(true);
        expect(lines.get("VO_02_Intro__g20261122")!.body).toContain("Kickoff is at 1:00 PM.");
    });
});

describe("Comparing schedules", () => {
    it("says what moved, in plain words, for games not yet played", () => {
        const before = bundledGames().map((g) => (g.Date === "2026-11-22" ? { ...g, Template: "1625" } : g));
        const after = bundledGames();
        expect(describeChanges(before, after, "2026-11-17")).toEqual(["Steelers game is now at 1:00 PM (was 4:25 PM)."]);
        // Games already played are not news.
        expect(describeChanges(before, after, "2026-12-01")).toEqual([]);
        const moved = after.map((g) => (g.Date === "2026-12-13" ? { ...g, Date: "2026-12-14", Weekday: "Monday", DateText: "December 14" } : g));
        expect(describeChanges(after, moved, "2026-11-17")).toEqual(["Colts game moved to Monday, December 14 at 1:00 PM."]);
        expect(describeChanges(after, after.filter((g) => g.OpponentCode !== "HOU"), "2026-11-17")).toEqual(["Texans game is no longer on the schedule."]);
    });
});

describe("Checking the schedule", () => {
    const board = (year: number, endDate: string) => ({ leagues: [{ season: { year, endDate } }] });
    /** A feed that answers the scoreboard and the team schedule; remembers the URLs asked. */
    const feedFor = (schedule: EspnSchedule, scoreboard: unknown = board(2026, "2027-02-16T07:59Z")) => {
        const asked: string[] = [];
        const impl = (async (url: string) => {
            asked.push(url);
            if (url.includes("/scoreboard")) return { ok: true, json: async () => scoreboard };
            if (url.includes("seasontype=3")) return { ok: true, json: async () => ({ events: [] }) };
            return { ok: true, json: async () => schedule };
        }) as unknown as typeof fetch;
        return { asked, impl };
    };
    const memory = () => {
        const data = new Map<string, string>();
        return { getItem: (k: string) => data.get(k) ?? null, setItem: (k: string, v: string) => void data.set(k, v) };
    };
    const now = () => new Date("2026-11-17T16:00:00Z");
    // The real schedule as of today: everything as bundled, except the Steelers at 1:00 PM and the Colts moved to a Monday night.
    const live: EspnSchedule = {
        events: bundledGames().map((g) => {
            const kickoff = g.Date === "2026-12-13" ? "2026-12-15T01:20Z" : `${g.Date}T${g.Template === "1300" ? "18:00" : g.Template === "1625" ? "20:25" : g.Template === "1700" ? "22:00" : "01:15"}Z`;
            return event(g.Template === "2015" ? `${new Date(Date.parse(`${g.Date}T12:00:00Z`) + 86400000).toISOString().slice(0, 10)}T01:15Z` : kickoff,
                PHI, team(g.OpponentCode, g.Opponent, g.OpponentShort), {}, [g.Broadcast.split(", ").pop()!]);
        }),
    };

    it("puts a changed schedule in place, tells the listeners, and keeps it on the device", async () => {
        const { impl, asked } = feedFor(live);
        const storage = memory();
        const updater = new ScheduleUpdater(now, impl, storage);
        const told: [string[], boolean][] = [];
        updater.onChanged((changes, asked) => told.push([changes, asked]));
        const changes = await updater.refresh(true);
        expect(changes).toEqual(["Colts game moved to Monday, December 14 at 8:20 PM."]);
        expect(told).toEqual([[changes, true]]);
        expect(asked.some((u) => u.includes("teams/phi/schedule?season=2026&seasontype=2"))).toBe(true);
        const colts = homeGames().find((g) => g.OpponentShort === "Colts")!;
        expect(colts).toMatchObject({ Date: "2026-12-14", Template: "2020" });
        expect(updater.status.message).toContain("Schedule updated.");
        // A second device session reads it back before anything is fetched.
        setHomeGames(bundledGames());
        // (Saved after the bundled file's own check, as it would be on a device that has been online since the last build.)
        const saved = JSON.parse(storage.getItem("lff.schedule.v1")!);
        storage.setItem("lff.schedule.v1", JSON.stringify({ ...saved, checkedAt: Date.parse(bundledChecked()) + 1000 }));
        const next = new ScheduleUpdater(now, impl, storage);
        next.loadSaved();
        expect(homeGames().find((g) => g.OpponentShort === "Colts")!.Template).toBe("2020");
    });

    it("says so when nothing changed, and waits half an hour between automatic checks", async () => {
        const { impl, asked } = feedFor({ events: [] });
        const updater = new ScheduleUpdater(now, impl, memory());
        expect(await updater.refresh(true)).toEqual([]);
        // No home games in the feed: the schedule stays.
        expect(homeGames()).toEqual(bundledGames());
        expect(updater.status.message).toBe("No newer schedule has been released yet.");
        const count = asked.length;
        await updater.refresh();
        expect(asked.length).toBe(count);
        await updater.refresh(true);
        expect(asked.length).toBeGreaterThan(count);
    });

    it("keeps the schedule it has when the feed can't be reached", async () => {
        const updater = new ScheduleUpdater(now, (async () => { throw new Error("offline"); }) as unknown as typeof fetch, memory());
        expect(await updater.refresh(true)).toEqual([]);
        expect(homeGames()).toEqual(bundledGames());
        expect(updater.status.message).toContain("Could not reach the schedule");
        expect(updater.status.checking).toBe(false);
    });

    it("moves to the next season only after the Super Bowl period is over", async () => {
        const during = feedFor({ events: [] }, board(2026, "2027-02-16T07:59Z"));
        await new ScheduleUpdater(() => new Date("2027-02-10T12:00:00Z"), during.impl, memory()).refresh(true);
        expect(during.asked.filter((u) => u.includes("teams/phi")).every((u) => u.includes("season=2026"))).toBe(true);
        const after = feedFor({ events: [] }, board(2026, "2027-02-16T07:59Z"));
        await new ScheduleUpdater(() => new Date("2027-02-20T12:00:00Z"), after.impl, memory()).refresh(true);
        expect(after.asked.filter((u) => u.includes("teams/phi")).every((u) => u.includes("season=2027"))).toBe(true);
    });

    it("loads the new season's games when the league has released them", async () => {
        const next: EspnSchedule = { events: [event("2027-09-12T17:00Z", PHI, team("DAL", "Dallas Cowboys", "Cowboys"), {}, ["Fox"])] };
        const { impl } = feedFor(next, board(2026, "2027-02-16T07:59Z"));
        const updater = new ScheduleUpdater(() => new Date("2027-05-20T12:00:00Z"), impl, memory());
        expect(await updater.refresh(true)).toEqual(["The 2027 season's home schedule is loaded."]);
        expect(homeGames()).toHaveLength(1);
        expect(homeGames()[0]).toMatchObject({ Date: "2027-09-12", OpponentShort: "Cowboys", Template: "1300", Broadcast: "FOX" });
    });
});
