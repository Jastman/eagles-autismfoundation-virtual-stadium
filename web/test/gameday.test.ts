// Kickoff templates (journey/gameday.ts) and the time-of-day lighting's sun (scene/solar.ts). Web additions, Docs/WEB_PARITY.md
// W21-W22: no Unreal test to port.
import { describe, expect, it } from "vitest";
import { goldenPath } from "../src/journey/data";
import {
    CHAPTER_START_STEPS, easternOffsetHours, easternToUtcMs, homeGames, kickoffTemplates, kickoffVoiceLines, nextHomeGame, parseClock,
    resolveGameDay,
} from "../src/journey/gameday";
import { daylightLevels, sunPosition } from "../src/scene/solar";

const game = (date: string) => homeGames().find((g) => g.Date === date)!;
const LFF = { lat: 39.9008036, lon: -75.167479 };

describe("Kickoff templates: the 2026 home schedule", () => {
    it("has nine home games, each on a known template", () => {
        const ids = kickoffTemplates().map((t) => t.Id);
        expect(homeGames()).toHaveLength(9);
        for (const g of homeGames()) expect(ids).toContain(g.Template);
    });

    it("picks the next home game from today, Eastern time", () => {
        // Friday before Monday Night Football against Dallas.
        expect(nextHomeGame(new Date("2026-10-23T16:00:00Z")).Date).toBe("2026-10-26");
        // Late on game night in Philadelphia it is still that game's day.
        expect(nextHomeGame(new Date("2026-10-27T03:30:00Z")).Date).toBe("2026-10-26");
        expect(nextHomeGame(new Date("2026-10-27T12:00:00Z")).Date).toBe("2026-11-08");
        // After the season: the last game.
        expect(nextHomeGame(new Date("2027-02-01T12:00:00Z")).Date).toBe("2026-12-24");
    });

    it("reads clock text", () => {
        expect(parseClock("~4:15 PM")).toBe(16 * 60 + 15);
        expect(parseClock("12:50 PM")).toBe(12 * 60 + 50);
        expect(parseClock("12:15 AM")).toBe(15);
        expect(parseClock("20:15")).toBe(20 * 60 + 15);
        expect(parseClock("")).toBeNull();
    });

    it("knows daylight time", () => {
        expect(easternOffsetHours("2026-10-26")).toBe(-4);
        expect(easternOffsetHours("2026-11-01")).toBe(-5);
        expect(easternOffsetHours("2026-11-08")).toBe(-5);
        expect(easternOffsetHours("2026-03-08")).toBe(-4);
        expect(easternOffsetHours("2026-03-07")).toBe(-5);
        expect(new Date(easternToUtcMs("2026-10-26", 20 * 60 + 15)).toISOString()).toBe("2026-10-27T00:15:00.000Z");
    });
});

describe("Kickoff templates: Monday Night Football, 8:15 PM (the Eagles' visual schedule for October 26)", () => {
    const day = resolveGameDay(game("2026-10-26"));
    const step = (id: string) => day.steps.find((s) => s.StepId === id)!;

    it("matches the schedule's times", () => {
        expect(step("intro").Title).toBe("Today I am going to the Eagles vs. Cowboys game at Lincoln Financial Field.");
        expect(step("intro").Body).toContain("Dallas Cowboys on Monday, October 26. Kickoff is at 8:15 PM.");
        expect(step("plaza-gates").Title).toBe("The Pepsi Plaza gates open at 5:00 PM.");
        expect(step("club-gates").Title).toBe("The club suites and inner gates open at 6:00 PM.");
        expect(step("gate-flares").Title).toBe("There will be gate flares around 6:45 PM, 7:00 PM, and 7:15 PM.");
        expect(step("light-show").Title).toBe("There will be a cell phone light show around 8:00 PM.");
        expect(step("anthem").Title).toBe("Fireworks and the national anthem happen around 8:10 PM.");
        expect(step("kickoff").Title).toBe("The game begins at about 8:15 PM.");
        expect(step("kickoff").Body).toBe("The Eagles are playing the Dallas Cowboys.");
    });

    it("joins the club and inner gates and adds the light show before the anthem", () => {
        const ids = day.steps.map((s) => s.StepId);
        expect(ids).not.toContain("inner-gates");
        expect(ids.indexOf("light-show")).toBe(ids.indexOf("bright-lights") + 1);
        expect(ids.indexOf("anthem")).toBe(ids.indexOf("light-show") + 1);
        expect(step("club-gates").SceneCues).toEqual(["GateClub", "GateInner"]);
        expect(day.steps).toHaveLength(goldenPath().length);
    });

    it("starts each chapter on the same step as the 1:00 PM tour", () => {
        expect(day.tourFirstSteps.map((i) => day.steps[i].StepId)).toEqual(CHAPTER_START_STEPS);
    });

    it("lights each step at its time, through sunset into night", () => {
        const at = (id: string) => day.sceneMinutes[day.steps.findIndex((s) => s.StepId === id)];
        expect(at("intro")).toBe(16 * 60 + 15);
        expect(at("clear-bags")).toBe(17 * 60);
        expect(at("walk-to-seats")).toBe(19 * 60 + 20);
        expect(at("kickoff")).toBe(20 * 60 + 15);
        expect(at("food")).toBe(20 * 60 + 45);
        // Never goes back in time.
        for (let i = 1; i < day.sceneMinutes.length; ++i) expect(day.sceneMinutes[i]).toBeGreaterThanOrEqual(day.sceneMinutes[i - 1]);
    });

    it("gives changed cards their own voice lines and keeps the rest", () => {
        expect(step("plaza-gates").VoiceWave).toContain("VO_06_PlazaGates__2015");
        expect(step("intro").VoiceWave).toContain("VO_02_Intro__g20261026");
        expect(step("kickoff").VoiceWave).toContain("VO_19_Kickoff__g20261026");
        expect(step("light-show").VoiceWave).toContain("VO_25_LightShow__2015");
        // Only the clock changed: the 1:00 PM recording still fits.
        expect(step("security").VoiceWave).toBe(goldenPath().find((s) => s.StepId === "security")!.VoiceWave);
    });
});

describe("Kickoff templates: the 1:00 PM tour", () => {
    it("is the golden path, with the game's own opponent", () => {
        const day = resolveGameDay(game("2026-11-08"));
        const base = goldenPath();
        expect(day.steps.map((s) => s.StepId)).toEqual(base.map((s) => s.StepId));
        expect(day.steps.find((s) => s.StepId === "plaza-gates")!.Title).toBe(base.find((s) => s.StepId === "plaza-gates")!.Title);
        expect(day.steps.find((s) => s.StepId === "kickoff")!.Body).toBe("The Eagles are playing the New York Giants.");
        expect(day.tourFirstSteps).toEqual([0, 4, 7, 10, 14]);
    });

    it("needs one voice line per changed card, all named", () => {
        const lines = kickoffVoiceLines();
        expect(lines.size).toBeGreaterThan(20);
        for (const [name, words] of lines) {
            expect(name).toMatch(/^VO_\d\d_[A-Za-z]+__(g\d{8}|\d{4})$/);
            expect(words.title).not.toContain("{");
            expect(words.body).not.toContain("{");
        }
    });
});

describe("Time of day: the sun over Lincoln Financial Field", () => {
    it("is high at 1:00 PM in October and sets about 6:03 PM on October 26", () => {
        const noon = sunPosition(easternToUtcMs("2026-10-18", 13 * 60), LFF.lat, LFF.lon);
        expect(noon.altitude).toBeGreaterThan(35);
        expect(noon.azimuth).toBeGreaterThan(160);
        expect(noon.azimuth).toBeLessThan(220);
        const sunset = sunPosition(easternToUtcMs("2026-10-26", 18 * 60 + 3), LFF.lat, LFF.lon);
        expect(Math.abs(sunset.altitude)).toBeLessThan(1.2);
        expect(sunset.azimuth).toBeGreaterThan(245);
        expect(sunset.azimuth).toBeLessThan(260);
    });

    it("is night by the 6:45 PM gate flares and at kickoff", () => {
        const flares = sunPosition(easternToUtcMs("2026-10-26", 18 * 60 + 45), LFF.lat, LFF.lon);
        expect(daylightLevels(flares.altitude).night).toBeGreaterThan(0.95);
        const kickoff = sunPosition(easternToUtcMs("2026-10-26", 20 * 60 + 15), LFF.lat, LFF.lon);
        expect(kickoff.altitude).toBeLessThan(-18);
        expect(daylightLevels(kickoff.altitude)).toEqual({ night: 1, golden: 0 });
        // Daylight, with low warm sun, at the 5:00 PM plaza gates.
        const gates = daylightLevels(sunPosition(easternToUtcMs("2026-10-26", 17 * 60), LFF.lat, LFF.lon).altitude);
        expect(gates.night).toBe(0);
        expect(gates.golden).toBeGreaterThan(0.3);
    });
});
