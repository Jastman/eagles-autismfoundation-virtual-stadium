// Kickoff time templates (SourceData/Journey/LFF_KickoffTemplates.json): which home game the tour shows, and the tour
// that game gets. LFF_GoldenPath.json is the 1:00 PM tour; a template changes only the steps whose times, copy or order
// differ for its kickoff time, and each game fills in its opponent and date. The admin's choice (Settings > Game day)
// is kept on the device; "Automatic" follows the season to the next home game.
import templatesJson from "../../../SourceData/Journey/LFF_KickoffTemplates.json";
import recordedVoiceJson from "../../../SourceData/Audio/VO/Kickoff/KICKOFF_VO_RECORDED.json";
import { GameMode } from "../config";
import { sunPosition } from "../scene/solar";
import { goldenPathRows, parseSteps, soundName } from "./data";
import type { Row } from "./data";
import type { JourneyStep } from "./types";

export interface HomeGame {
    /** Eastern date, YYYY-MM-DD; also the game's id. */
    Date: string;
    Weekday: string;
    DateText: string;
    Opponent: string;
    OpponentShort: string;
    /** The scoreboard's three letters ("DAL"). */
    OpponentCode: string;
    /** KickoffTemplate.Id. */
    Template: string;
    Broadcast: string;
    /** A sentence (with its trailing space) for the intro card when it will be dark, or "". */
    DarkNote: string;
}

type StepOverride = Row & { SceneTime?: string };

export interface KickoffTemplate {
    Id: string;
    /** "8:15 PM". */
    Label: string;
    Name: string;
    /** 24-hour Eastern, "20:15". */
    Kickoff: string;
    Source: string;
    /** True when the times are worked out from another schedule rather than taken from the Eagles' own. */
    Estimated: boolean;
    Steps: Record<string, StepOverride>;
    RemoveSteps: string[];
    InsertSteps: { After: string; Step: Row }[];
}

interface TemplatesFile {
    Season: string;
    /** When tools/update_schedule.ts last checked the Eagles' schedule (ISO); a newer saved check on a device wins over the bundle. */
    ScheduleChecked?: string;
    GameSteps: Record<string, Row>;
    Games: HomeGame[];
    Templates: KickoffTemplate[];
}

const FILE = templatesJson as unknown as TemplatesFile;

/** The step each tour chapter (Arrival, Gates, Plaza, To my seat, Game) starts on. */
export const CHAPTER_START_STEPS = ["intro", "plaza-gates", "plaza", "club-gates", "loud-sounds"];

/** The tour as one game gets it, with the time of day each step is shown at. */
export interface GameDay {
    game: HomeGame;
    template: KickoffTemplate;
    steps: JourneyStep[];
    /** First step (0-based) of each tour chapter. */
    tourFirstSteps: number[];
    /** Per step: minutes after midnight, Eastern, that the scene is lit for. */
    sceneMinutes: number[];
    kickoffMinutes: number;
    /** Hours from UTC on the game's date: -4 (EDT) or -5 (EST). */
    utcOffsetHours: number;
}

/** The bundled schedule, as shipped (journey/schedule.ts replaces the live one with a newer check). */
export const bundledSeason = (): string => FILE.Season;
export const bundledChecked = (): string => FILE.ScheduleChecked ?? "";
export const bundledGames = (): HomeGame[] => FILE.Games;

let games: HomeGame[] = FILE.Games;
const derived = new Map<string, KickoffTemplate>();

/** The home games the app is showing: the bundled schedule until a newer check (journey/schedule.ts) replaces it. */
export const homeGames = (): HomeGame[] => games;
export function setHomeGames(next: HomeGame[]): void {
    games = next;
}

/** Every kickoff time the tour can be shown for: the bundled templates plus one for each other time a game is played at. */
export function kickoffTemplates(): KickoffTemplate[] {
    const all = new Map<string, KickoffTemplate>(FILE.Templates.map((t) => [t.Id, t]));
    for (const g of games) {
        const t = templateById(g.Template);
        if (t) all.set(t.Id, t);
    }
    return [...all.values()].sort((a, b) => (parseClock(a.Kickoff) ?? 0) - (parseClock(b.Kickoff) ?? 0));
}

/** A template by Id: bundled, else worked out from the Id's kickoff time ("2020" is 8:20 PM). */
export function templateById(id: string): KickoffTemplate | null {
    const bundled = FILE.Templates.find((t) => t.Id === id);
    if (bundled) return bundled;
    const m = /^(\d{2})(\d{2})$/.exec(id);
    if (!m || Number(m[1]) > 23 || Number(m[2]) > 59) return null;
    let template = derived.get(id);
    if (!template) {
        template = deriveTemplate(Number(m[1]) * 60 + Number(m[2]));
        derived.set(id, template);
    }
    return template;
}

export const templateFor = (game: HomeGame): KickoffTemplate => templateById(game.Template) ?? FILE.Templates[0];

/** "8:20 PM" for minutes after midnight. */
export function clockLabel(minutes: number): string {
    const m = ((Math.round(minutes) % 1440) + 1440) % 1440;
    const h = Math.floor(m / 60);
    return `${h % 12 || 12}:${String(m % 60).padStart(2, "0")} ${h < 12 ? "AM" : "PM"}`;
}

/** A template Id for a kickoff time: 20:20 -> "2020". */
export function templateIdFor(minutes: number): string {
    const m = ((Math.round(minutes) % 1440) + 1440) % 1440;
    return `${String(Math.floor(m / 60)).padStart(2, "0")}${String(m % 60).padStart(2, "0")}`;
}

const shiftTime = (text: string, delta: number): string =>
    text
        .replace(/(\d{1,2}:\d{2})\s*([AaPp][Mm])/g, (_all, t: string, half: string) => clockLabel((parseClock(`${t} ${half}`) ?? 0) + delta))
        .replace(/\b(a|an) (\d{1,2}):(\d{2}) ([AP]M)/g, (_all, _a: string, h: string, m: string, half: string) =>
            `${h === "8" || h === "11" ? "an" : "a"} ${h}:${m} ${half}`);

function shiftRow<T extends Row>(row: T, delta: number): T {
    const out: Row = {};
    for (const [key, value] of Object.entries(row)) {
        if (key === "SceneTime" && typeof value === "string") out[key] = clockLabel((parseClock(value) ?? 0) + delta);
        else out[key] = typeof value === "string" ? shiftTime(value, delta) : value;
    }
    return out as T;
}

/**
 * The tour for a kickoff time no bundled template covers (a game the league moved or flexed): the closest bundled tour with every
 * time moved by the difference. 8:00 PM or later follows the Eagles' own 8:15 PM schedule (light show, one inner gate card), earlier
 * follows the estimated 4:25 PM one. These are estimates, and say so.
 */
export function deriveTemplate(kickoffMinutes: number): KickoffTemplate {
    const ref = FILE.Templates.find((t) => t.Id === (kickoffMinutes >= 18 * 60 ? "2015" : "1625"))!;
    const delta = kickoffMinutes - (parseClock(ref.Kickoff) ?? 0);
    const label = clockLabel(kickoffMinutes);
    const name = kickoffMinutes < 12 * 60 ? "Morning" : kickoffMinutes < 16 * 60 ? "Afternoon" : kickoffMinutes < 18 * 60 ? "Late afternoon"
        : kickoffMinutes < 19.5 * 60 ? "Evening" : "Night game";
    return {
        Id: templateIdFor(kickoffMinutes),
        Label: label,
        Name: name,
        Kickoff: templateIdFor(kickoffMinutes).replace(/^(\d{2})(\d{2})$/, "$1:$2"),
        Source: shiftTime(`Estimated: the ${ref.Label} schedule's gate and show offsets moved to a ${label} kickoff. Check against the Eagles gameday visual schedule for the game.`, 0),
        Estimated: true,
        Steps: Object.fromEntries(Object.entries(ref.Steps).map(([name, row]) => [name, shiftRow(row, delta)])),
        RemoveSteps: [...ref.RemoveSteps],
        InsertSteps: ref.InsertSteps.map((i) => ({ After: i.After, Step: shiftRow(i.Step, delta) })),
    };
}

/** The sentence the intro card adds about daylight, worked out from the sun over Philadelphia (a game is about 3 hours long). */
export function darkNoteFor(isoDate: string, kickoffMinutes: number): string {
    const utcAt = (minutes: number) => easternToUtcMs(isoDate, minutes);
    let sunset: number | null = null;
    for (let m = 12 * 60; m < 23 * 60; m += 5) {
        if (sunPosition(utcAt(m), 39.9008, -75.1675).altitude < -0.833) {
            sunset = m;
            break;
        }
    }
    if (sunset === null) return "";
    if (kickoffMinutes >= 19 * 60) return "It is a night game, so it will be dark outside. ";
    if (kickoffMinutes >= sunset) return "It will be dark outside during the game. ";
    if (kickoffMinutes + 150 >= sunset) return "It will get dark during the game. ";
    if (kickoffMinutes + 195 >= sunset) return "It may get dark near the end of the game. ";
    return "";
}

/** "~4:15 PM", "8:15 PM", "13:45" -> minutes after midnight; null if the text holds no time. */
export function parseClock(text: string | undefined): number | null {
    if (!text) return null;
    const m = /(\d{1,2}):(\d{2})\s*([AaPp][Mm])?/.exec(text);
    if (!m) return null;
    let hours = Number(m[1]) % 24;
    const minutes = Number(m[2]);
    const half = m[3]?.toUpperCase();
    if (half === "PM" && hours < 12) hours += 12;
    if (half === "AM" && hours === 12) hours = 0;
    return hours * 60 + minutes;
}

/** US Eastern time's offset from UTC on a date: daylight time from the second Sunday of March to the first of November. */
export function easternOffsetHours(isoDate: string): number {
    const [y, mo, d] = isoDate.split("-").map(Number);
    const nthSunday = (month: number, n: number) => {
        const first = new Date(Date.UTC(y, month - 1, 1)).getUTCDay();
        return 1 + ((7 - first) % 7) + (n - 1) * 7;
    };
    const day = mo * 100 + d;
    const dstStart = 300 + nthSunday(3, 2);
    const dstEnd = 1100 + nthSunday(11, 1);
    return day >= dstStart && day < dstEnd ? -4 : -5;
}

/** A game's local (Eastern) time as a UTC timestamp in milliseconds. */
export function easternToUtcMs(isoDate: string, minutes: number): number {
    const [y, mo, d] = isoDate.split("-").map(Number);
    return Date.UTC(y, mo - 1, d) + (minutes - easternOffsetHours(isoDate) * 60) * 60000;
}

/** Today's date in Philadelphia, YYYY-MM-DD. */
export function easternToday(now: Date): string {
    const utc = now.toISOString().slice(0, 10);
    return new Date(now.getTime() + easternOffsetHours(utc) * 3600000).toISOString().slice(0, 10);
}

/** The next home game on or after today (the last one once the season is over). */
export function nextHomeGame(now: Date, games = homeGames()): HomeGame {
    const today = easternToday(now);
    return games.find((g) => g.Date >= today) ?? games[games.length - 1];
}

function fill(text: string, game: HomeGame, template: KickoffTemplate): string {
    return text
        .replaceAll("{OpponentShort}", game.OpponentShort)
        .replaceAll("{Opponent}", game.Opponent)
        .replaceAll("{Weekday}", game.Weekday)
        .replaceAll("{DateText}", game.DateText)
        .replaceAll("{Kickoff}", template.Label)
        .replaceAll("{DarkNote}", game.DarkNote);
}

const RECORDED = recordedVoiceJson as unknown as Record<string, { title: string; body: string }>;
/** True when the line <name> was recorded with exactly these words (SourceData/Audio/VO/Kickoff/KICKOFF_VO_RECORDED.json). */
export const isRecorded = (name: string, title: string, body: string): boolean => {
    const line = RECORDED[name];
    return !!line && line.title === title && line.body === body;
};

const voicePath = (name: string) => `/Game/LFF/Audio/VO/Kickoff/${name}.${name}`;

/**
 * The game's tour. A card whose words differ from the 1:00 PM tour gets its own voice line: <base>__<template Id>, or
 * <base>__g<date> when it names the game (Tools/Audio/make_kickoff_vo.py makes the same names).
 */
export function resolveGameDay(game: HomeGame, base: Row[] = goldenPathRows(), timeId: string | null = null, guardVoice = true): GameDay {
    const timeTemplate = timeId ? templateById(timeId) : null;
    const template = timeTemplate ?? templateFor(game);
    // The tour for another kickoff time than the game's own (Settings > Game day) says so on the intro card, and the daylight
    // note follows that time.
    if (timeTemplate && timeTemplate.Id !== game.Template) game = { ...game, DarkNote: darkNoteFor(game.Date, parseClock(timeTemplate.Kickoff) ?? 13 * 60) };
    let rows = base.map((r) => ({ ...r })).filter((r) => !template.RemoveSteps.includes(r.Name as string));
    const inserted = new Set<string>();
    for (const insert of template.InsertSteps) {
        const at = rows.findIndex((r) => r.Name === insert.After);
        rows.splice(at < 0 ? rows.length : at + 1, 0, { ...insert.Step });
        inserted.add(insert.Step.Name as string);
    }
    const kickoffMinutes = parseClock(template.Kickoff) ?? 13 * 60;
    const sceneMinutes: (number | null)[] = [];
    let last: number | null = null;
    let afterKickoff = false;
    rows = rows.map((row) => {
        const name = row.Name as string;
        const override = template.Steps[name] ?? {};
        const gameStep = FILE.GameSteps[name] ?? {};
        let ownWords = inserted.has(name);
        let gameWords = false;
        for (const [key, value] of Object.entries(override)) {
            if (key === "SceneTime") continue;
            if ((key === "Title" || key === "Body") && value !== row[key]) ownWords = true;
            row[key] = value;
        }
        for (const [key, value] of Object.entries(gameStep)) {
            const text = fill(String(value), game, template);
            if ((key === "Title" || key === "Body") && text !== row[key]) gameWords = true;
            row[key] = text;
        }
        const baseVoice = soundName(String(row.VoiceWave ?? ""));
        if (baseVoice && (gameWords || ownWords)) {
            const voice = `${baseVoice}__${gameWords ? `g${game.Date.replaceAll("-", "")}` : template.Id}`;
            // A recording of other words (the game moved to another time, say) would read the wrong card aloud: leave the
            // card silent until Tools/Audio/make_kickoff_vo.py has recorded this one.
            row.VoiceWave = !guardVoice || isRecorded(voice, String(row.Title ?? ""), String(row.Body ?? "")) ? voicePath(voice) : "";
        }
        // The scene's time: the step's own (SceneTime, then its clock), else the step before's; after kickoff, the game.
        const own = parseClock(override.SceneTime) ?? parseClock(row.ClockText as string);
        if (own !== null) last = own;
        else if (afterKickoff) last = Math.max(last ?? kickoffMinutes, kickoffMinutes + 30);
        sceneMinutes.push(last);
        if (name === "kickoff") afterKickoff = true;
        return row;
    });
    // Steps before the first time take it.
    const first = sceneMinutes.find((m) => m !== null) ?? kickoffMinutes - 240;
    const steps = parseSteps(rows);
    const ids = steps.map((s) => s.StepId);
    const tourFirstSteps = CHAPTER_START_STEPS.map((id, i) => {
        const at = ids.indexOf(id);
        return at >= 0 ? at : GameMode.TourStepFirstSteps[i];
    });
    return { game, template, steps, tourFirstSteps, sceneMinutes: sceneMinutes.map((m) => m ?? first), kickoffMinutes,
        utcOffsetHours: easternOffsetHours(game.Date) };
}

/** Every voice line the templates need beyond the 1:00 PM tour's: name -> the card's words (title, then body). */
export function kickoffVoiceLines(): Map<string, { title: string; body: string }> {
    const lines = new Map<string, { title: string; body: string }>();
    for (const game of homeGames()) {
        for (const step of resolveGameDay(game, undefined, null, false).steps) {
            if (!step.VoiceWave.includes("/Kickoff/")) continue;
            lines.set(soundName(step.VoiceWave), { title: step.Title, body: step.Body });
        }
    }
    return lines;
}

const STORAGE_KEY = "lff.gameDay";
const TIME_KEY = "lff.gameDayTime";
export const AUTOMATIC = "auto";

/** "Short weekday, Mon DD": "Mon, Oct 26". */
export function shortDate(game: HomeGame): string {
    const [, mo, d] = game.Date.split("-").map(Number);
    const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    return `${game.Weekday.slice(0, 3)}, ${months[mo - 1]} ${d}`;
}

/**
 * The game the tour is set to: ?game=YYYY-MM-DD, else the device's saved choice, else the next home game. A kickoff time picked
 * in Settings changes the tour's times for that game; it never changes the opponent.
 */
export class GameDayController {
    private choice: string;
    private timeChoice: string | null;
    private listeners: ((day: GameDay) => void)[] = [];
    current: GameDay;

    constructor(private readonly now: () => Date = () => new Date(), search = typeof location !== "undefined" ? location.search : "") {
        const fromUrl = new URLSearchParams(search).get("game");
        this.choice = this.valid(fromUrl) ? fromUrl! : this.valid(readStored()) ? readStored()! : AUTOMATIC;
        const time = readStored(TIME_KEY);
        this.timeChoice = time && templateById(time) ? time : null;
        this.current = this.resolve();
        console.info(`Game day: ${this.describe()}${this.choice === AUTOMATIC ? " (automatic: the next home game)" : ""}.`);
    }

    private valid(choice: string | null): boolean {
        return choice === AUTOMATIC || homeGames().some((g) => g.Date === choice);
    }

    private gameFor(choice: string): HomeGame {
        return homeGames().find((g) => g.Date === choice) ?? nextHomeGame(this.now());
    }

    private resolve(): GameDay {
        const game = this.gameFor(this.choice);
        // A saved time that is the game's own time is no override at all.
        if (this.timeChoice === game.Template) this.timeChoice = null;
        return resolveGameDay(game, undefined, this.timeChoice);
    }

    get isAutomatic(): boolean {
        return this.choice === AUTOMATIC;
    }

    /** True when the tour shows another kickoff time than the one the schedule has for this game. */
    get isTimeOverridden(): boolean {
        return this.timeChoice !== null && this.timeChoice !== this.current.game.Template;
    }

    /** The kickoff template the schedule gives the current game. */
    get scheduledTemplate(): KickoffTemplate {
        return templateFor(this.current.game);
    }

    onChanged(listener: (day: GameDay) => void): void {
        this.listeners.push(listener);
    }

    private apply(before: string): void {
        this.current = this.resolve();
        if (fingerprint(this.current) !== before) for (const listener of this.listeners) listener(this.current);
    }

    /** A game's date, or AUTOMATIC. Saved on this device. The game plays at its own kickoff time. */
    select(choice: string): void {
        if (!this.valid(choice)) return;
        const before = fingerprint(this.current);
        this.choice = choice;
        this.timeChoice = null;
        store(choice);
        store("", TIME_KEY);
        this.apply(before);
        console.info(`Game day: ${this.describe()}${choice === AUTOMATIC ? " (automatic)" : ""}.`);
    }

    /** Shows the current game's tour at another kickoff time (its own time clears the choice). The opponent stays. */
    selectTemplate(templateId: string): void {
        if (!templateById(templateId)) return;
        const before = fingerprint(this.current);
        this.timeChoice = templateId === this.current.game.Template ? null : templateId;
        store(this.timeChoice ?? "", TIME_KEY);
        this.apply(before);
    }

    /** The schedule changed (journey/schedule.ts): the game the choice points at may have a new time, or be gone. */
    reload(): void {
        if (this.choice !== AUTOMATIC && !homeGames().some((g) => g.Date === this.choice)) this.choice = AUTOMATIC;
        if (this.timeChoice && !templateById(this.timeChoice)) this.timeChoice = null;
        this.apply(fingerprint(this.current));
    }

    describe(day = this.current): string {
        return `${shortDate(day.game)} vs. ${day.game.OpponentShort}, ${day.template.Label}`;
    }
}

/** What a viewer would notice changing: the game, its time, and every card's words and voice. */
const fingerprint = (day: GameDay): string =>
    JSON.stringify([day.game, day.template.Id, day.steps.map((s) => [s.StepId, s.Title, s.Body, s.ClockText, s.TimelineLabel, s.VoiceWave])]);

function readStored(key = STORAGE_KEY): string | null {
    try {
        return window.localStorage.getItem(key) || null;
    } catch {
        return null;
    }
}

function store(value: string, key = STORAGE_KEY): void {
    try {
        if (value) window.localStorage.setItem(key, value);
        else window.localStorage.removeItem(key);
    } catch {
        /* private mode: the choice lasts for this visit */
    }
}
