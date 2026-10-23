// Kickoff time templates (SourceData/Journey/LFF_KickoffTemplates.json): which home game the tour shows, and the tour
// that game gets. LFF_GoldenPath.json is the 1:00 PM tour; a template changes only the steps whose times, copy or order
// differ for its kickoff time, and each game fills in its opponent and date. The admin's choice (Settings > Game day)
// is kept on the device; "Automatic" follows the season to the next home game.
import templatesJson from "../../../SourceData/Journey/LFF_KickoffTemplates.json";
import { GameMode } from "../config";
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

export const homeGames = (): HomeGame[] => FILE.Games;
export const kickoffTemplates = (): KickoffTemplate[] => FILE.Templates;
export const templateFor = (game: HomeGame): KickoffTemplate =>
    FILE.Templates.find((t) => t.Id === game.Template) ?? FILE.Templates[0];

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

const voicePath = (name: string) => `/Game/LFF/Audio/VO/Kickoff/${name}.${name}`;

/**
 * The game's tour. A card whose words differ from the 1:00 PM tour gets its own voice line: <base>__<template Id>, or
 * <base>__g<date> when it names the game (Tools/Audio/make_kickoff_vo.py makes the same names).
 */
export function resolveGameDay(game: HomeGame, base: Row[] = goldenPathRows()): GameDay {
    const template = templateFor(game);
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
            row.VoiceWave = voicePath(`${baseVoice}__${gameWords ? `g${game.Date.replaceAll("-", "")}` : template.Id}`);
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
        for (const step of resolveGameDay(game).steps) {
            if (!step.VoiceWave.includes("/Kickoff/")) continue;
            lines.set(soundName(step.VoiceWave), { title: step.Title, body: step.Body });
        }
    }
    return lines;
}

const STORAGE_KEY = "lff.gameDay";
export const AUTOMATIC = "auto";

/** "Short weekday, Mon DD": "Mon, Oct 26". */
export function shortDate(game: HomeGame): string {
    const [, mo, d] = game.Date.split("-").map(Number);
    const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    return `${game.Weekday.slice(0, 3)}, ${months[mo - 1]} ${d}`;
}

/** The game the tour is set to: ?game=YYYY-MM-DD, else the device's saved choice, else the next home game. */
export class GameDayController {
    private choice: string;
    private listeners: ((day: GameDay) => void)[] = [];
    current: GameDay;

    constructor(private readonly now: () => Date = () => new Date(), search = typeof location !== "undefined" ? location.search : "") {
        const fromUrl = new URLSearchParams(search).get("game");
        this.choice = this.valid(fromUrl) ? fromUrl! : this.valid(readStored()) ? readStored()! : AUTOMATIC;
        this.current = resolveGameDay(this.gameFor(this.choice));
        console.info(`Game day: ${this.describe()}${this.choice === AUTOMATIC ? " (automatic: the next home game)" : ""}.`);
    }

    private valid(choice: string | null): boolean {
        return choice === AUTOMATIC || homeGames().some((g) => g.Date === choice);
    }

    private gameFor(choice: string): HomeGame {
        return homeGames().find((g) => g.Date === choice) ?? nextHomeGame(this.now());
    }

    get isAutomatic(): boolean {
        return this.choice === AUTOMATIC;
    }

    onChanged(listener: (day: GameDay) => void): void {
        this.listeners.push(listener);
    }

    /** A game's date, or AUTOMATIC. Saved on this device. */
    select(choice: string): void {
        if (!this.valid(choice)) return;
        const before = this.current.game.Date;
        this.choice = choice;
        store(choice);
        this.current = resolveGameDay(this.gameFor(choice));
        console.info(`Game day: ${this.describe()}${choice === AUTOMATIC ? " (automatic)" : ""}.`);
        if (this.current.game.Date !== before) for (const listener of this.listeners) listener(this.current);
    }

    /** Picks a kickoff time: its next game from today (or its first). */
    selectTemplate(templateId: string): void {
        const today = easternToday(this.now());
        const games = homeGames().filter((g) => g.Template === templateId);
        const game = games.find((g) => g.Date >= today) ?? games[0];
        if (game) this.select(game.Date);
    }

    describe(day = this.current): string {
        return `${shortDate(day.game)} vs. ${day.game.OpponentShort}, ${day.template.Label}`;
    }
}

function readStored(): string | null {
    try {
        return window.localStorage.getItem(STORAGE_KEY);
    } catch {
        return null;
    }
}

function store(value: string): void {
    try {
        window.localStorage.setItem(STORAGE_KEY, value);
    } catch {
        /* private mode: the choice lasts for this visit */
    }
}
