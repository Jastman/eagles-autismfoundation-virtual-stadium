// The Eagles' home schedule, kept current (web addition, Docs/WEB_PARITY.md W25). The bundled schedule
// (SourceData/Journey/LFF_KickoffTemplates.json) is what the app ships with; this checks the league's schedule feed (ESPN's public
// team schedule, which is where flexed kickoff times and moved games show up first) and replaces the bundled home games with what
// it finds. A check runs when the app opens, after the app has been away for a while, and from Settings > Game day > Check for
// schedule updates. The result is kept on the device, so the schedule is right even when the feed can't be reached.
//
// Seasons: the season shown is the one ESPN reports as current, and it stays until the league's season is officially over (the end
// of the Super Bowl period). After that the next season's schedule is loaded as soon as the feed has one; until then the finished
// season stays on screen.
//
// The same code runs in Node (tools/update_schedule.ts) to write the bundled file.
import {
    bundledChecked, bundledSeason, clockLabel, darkNoteFor, homeGames, parseClock, setHomeGames, templateById, templateIdFor,
} from "./gameday";
import type { HomeGame } from "./gameday";

const API = "https://site.api.espn.com/apis/site/v2/sports/football/nfl";
const TEAM = "phi";
const TEAM_CODE = "PHI";
const CACHE_KEY = "lff.schedule.v1";
/** A check within this long of the last one is skipped unless the visitor asked for it. */
export const MIN_CHECK_MS = 30 * 60 * 1000;
const TIMEOUT_MS = 10000;

const WEEKDAYS = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
const MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];

// ---- Eastern time ----

const nthSunday = (year: number, month: number, n: number) => {
    const first = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
    return 1 + ((7 - first) % 7) + (n - 1) * 7;
};

/** Eastern date (YYYY-MM-DD) and minutes after midnight for a UTC moment. Daylight time runs from 2:00 AM on the second Sunday of March to 2:00 AM on the first of November. */
export function easternParts(utcMs: number): { date: string; minutes: number } {
    const year = new Date(utcMs).getUTCFullYear();
    const dstStart = Date.UTC(year, 2, nthSunday(year, 3, 2), 7);
    const dstEnd = Date.UTC(year, 10, nthSunday(year, 11, 1), 6);
    const local = new Date(utcMs + (utcMs >= dstStart && utcMs < dstEnd ? -4 : -5) * 3600000);
    return { date: local.toISOString().slice(0, 10), minutes: local.getUTCHours() * 60 + local.getUTCMinutes() };
}

// ---- Seasons ----

export interface SeasonInfo {
    year: number;
    /** ISO moment the season ends (the league's calendar puts it after the Super Bowl). */
    endDate: string;
}

/**
 * The season to show. ESPN's current season stays current until its end date (after the Super Bowl); from then on it is the next
 * one. Without the feed: the league year starts on March 1, a safe stand-in for "the Super Bowl is long over".
 */
export function activeSeason(info: SeasonInfo | null, now: Date, floor = Number(bundledSeason())): number {
    if (info) {
        const end = Date.parse(info.endDate);
        return Math.max(floor, Number.isFinite(end) && now.getTime() >= end ? info.year + 1 : info.year);
    }
    const year = now.getUTCFullYear();
    return Math.max(floor, now.getUTCMonth() < 2 ? year - 1 : year);
}

// ---- The feed ----

interface EspnCompetitor {
    homeAway: string;
    team?: { abbreviation?: string; displayName?: string; shortDisplayName?: string; nickname?: string };
}
interface EspnEvent {
    date: string;
    competitions?: {
        neutralSite?: boolean;
        timeValid?: boolean;
        competitors?: EspnCompetitor[];
        broadcasts?: { media?: { shortName?: string } }[];
        status?: { type?: { name?: string } };
    }[];
}
export interface EspnSchedule {
    events?: EspnEvent[];
}

const NETWORK_NAMES: Record<string, string> = { Fox: "FOX" };

/** The words the scoreboard and Settings show for who is broadcasting: "Monday Night Football, ESPN" or just "CBS". */
export function broadcastText(weekday: string, kickoffMinutes: number, networks: string[]): string {
    if (weekday === "Monday" && networks.includes("ESPN")) return "Monday Night Football, ESPN";
    if (networks.includes("Prime Video")) return weekday === "Thursday" ? "Thursday Night Football, Prime Video" : "Prime Video";
    if (weekday === "Sunday" && kickoffMinutes >= 19 * 60 && networks.includes("NBC")) return "Sunday Night Football, NBC";
    return networks[0] ?? "";
}

/** The home games in an ESPN team schedule (regular season and playoffs), as the app's own. `known` supplies what the feed doesn't. */
export function parseEspnSchedule(feeds: EspnSchedule[], known: HomeGame[] = []): HomeGame[] {
    const out: HomeGame[] = [];
    for (const feed of feeds) {
        for (const event of feed.events ?? []) {
            const comp = event.competitions?.[0];
            const utc = Date.parse(event.date);
            if (!comp || !Number.isFinite(utc) || comp.neutralSite) continue;
            const status = comp.status?.type?.name ?? "";
            if (/CANCEL|POSTPONE/i.test(status)) continue;
            const home = comp.competitors?.find((c) => c.homeAway === "home");
            const away = comp.competitors?.find((c) => c.homeAway === "away");
            if (!home || !away || home.team?.abbreviation !== TEAM_CODE || !away.team) continue;
            const t = away.team;
            const code = t.abbreviation === "WSH" ? "WAS" : t.abbreviation ?? "";
            const when = easternParts(utc);
            const weekday = WEEKDAYS[new Date(`${when.date}T12:00:00Z`).getUTCDay()];
            const [, mo, d] = when.date.split("-").map(Number);
            // The league lists a game whose time isn't set yet at a placeholder hour: keep the time we had, else the usual 1:00 PM.
            const sameDay = known.find((g) => g.Date === when.date) ?? known.find((g) => g.OpponentCode === code);
            const minutes = comp.timeValid === false
                ? parseClock(templateById(sameDay?.Template ?? "")?.Kickoff) ?? 13 * 60
                : when.minutes;
            const networks = (comp.broadcasts ?? []).map((b) => NETWORK_NAMES[b.media?.shortName ?? ""] ?? b.media?.shortName ?? "").filter(Boolean);
            out.push({
                Date: when.date,
                Weekday: weekday,
                DateText: `${MONTHS[mo - 1]} ${d}`,
                Opponent: t.displayName ?? t.shortDisplayName ?? code,
                OpponentShort: t.shortDisplayName ?? t.nickname ?? t.displayName ?? code,
                OpponentCode: code,
                Template: templateIdFor(minutes),
                Broadcast: networks.length ? broadcastText(weekday, minutes, networks) : sameDay?.Broadcast ?? "",
                DarkNote: darkNoteFor(when.date, minutes),
            });
        }
    }
    return out.sort((a, b) => a.Date.localeCompare(b.Date));
}

async function getJson<T>(url: string, fetchImpl: typeof fetch): Promise<T> {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    try {
        const response = await fetchImpl(url, { cache: "no-store", signal: controller.signal });
        if (!response.ok) throw new Error(`${response.status} from ${new URL(url).pathname}`);
        return (await response.json()) as T;
    } finally {
        clearTimeout(timer);
    }
}

/** ESPN's current season and when it ends, or null when the feed can't say. */
export async function fetchSeasonInfo(fetchImpl: typeof fetch = fetch): Promise<SeasonInfo | null> {
    try {
        const board = await getJson<{ leagues?: { season?: { year?: number; endDate?: string } }[] }>(`${API}/scoreboard`, fetchImpl);
        const season = board.leagues?.[0]?.season;
        return season?.year && season.endDate ? { year: season.year, endDate: season.endDate } : null;
    } catch {
        return null;
    }
}

export interface FetchedSchedule {
    season: number;
    games: HomeGame[];
}

/** The home games for the season to show now (see activeSeason). Throws when the regular-season schedule can't be read. */
export async function fetchHomeSchedule(now: Date, known: HomeGame[], fetchImpl: typeof fetch = fetch): Promise<FetchedSchedule> {
    const season = activeSeason(await fetchSeasonInfo(fetchImpl), now);
    const regular = await getJson<EspnSchedule>(`${API}/teams/${TEAM}/schedule?season=${season}&seasontype=2`, fetchImpl);
    // The playoffs are listed once the matchups are set; a missing list is not a failed check.
    const playoffs = await getJson<EspnSchedule>(`${API}/teams/${TEAM}/schedule?season=${season}&seasontype=3`, fetchImpl).catch(() => ({} as EspnSchedule));
    return { season, games: parseEspnSchedule([regular, playoffs], known) };
}

// ---- What changed ----

const sameGame = (a: HomeGame, b: HomeGame) => a.Date === b.Date && a.OpponentCode === b.OpponentCode;
const kickoffOf = (g: HomeGame) => clockLabel(parseClock(templateById(g.Template)?.Kickoff) ?? 0);

/** Plain sentences for what differs between two schedules, for games not played yet. */
export function describeChanges(before: HomeGame[], after: HomeGame[], today: string): string[] {
    const notes: string[] = [];
    for (const g of after) {
        if (g.Date < today) continue;
        const old = before.find((b) => sameGame(b, g)) ?? before.find((b) => b.OpponentCode === g.OpponentCode && b.Date >= today);
        if (!old) notes.push(`${g.OpponentShort} game added on ${g.Weekday}, ${g.DateText} at ${kickoffOf(g)}.`);
        else if (old.Date !== g.Date) notes.push(`${g.OpponentShort} game moved to ${g.Weekday}, ${g.DateText} at ${kickoffOf(g)}.`);
        else if (old.Template !== g.Template) notes.push(`${g.OpponentShort} game is now at ${kickoffOf(g)} (was ${kickoffOf(old)}).`);
    }
    for (const b of before) {
        if (b.Date >= today && !after.some((g) => g.OpponentCode === b.OpponentCode)) notes.push(`${b.OpponentShort} game is no longer on the schedule.`);
    }
    return notes;
}

// ---- The device's copy and the updater ----

interface SavedSchedule {
    season: number;
    checkedAt: number;
    games: HomeGame[];
}

export interface ScheduleStatus {
    /** Epoch ms of the last successful check, or 0 when this device has never reached the feed. */
    checkedAt: number;
    season: number;
    checking: boolean;
    /** One line for Settings: what the last check found. */
    message: string;
}

export interface KeyValueStorage {
    getItem(key: string): string | null;
    setItem(key: string, value: string): void;
}

function deviceStorage(): KeyValueStorage | null {
    try {
        return typeof window !== "undefined" ? window.localStorage : null;
    } catch {
        return null;
    }
}

const isGame = (g: unknown): g is HomeGame => {
    const x = g as HomeGame;
    return !!x && typeof x.Date === "string" && typeof x.Template === "string" && typeof x.OpponentShort === "string"
        && typeof x.Opponent === "string" && !!templateById(x.Template);
};

export class ScheduleUpdater {
    private listeners: ((changes: string[], asked: boolean) => void)[] = [];
    private statusListeners: ((status: ScheduleStatus) => void)[] = [];
    private inFlight: Promise<string[]> | null = null;
    status: ScheduleStatus = { checkedAt: 0, season: Number(bundledSeason()), checking: false, message: "" };

    constructor(
        private readonly now: () => Date = () => new Date(),
        private readonly fetchImpl: typeof fetch = (...args) => fetch(...args),
        private readonly storage: KeyValueStorage | null = deviceStorage(),
    ) {}

    /** Puts the schedule this device saved on its last check in place of the bundled one, if the saved one is newer. Call before the first game day is resolved. */
    loadSaved(): void {
        const saved = this.readSaved();
        if (!saved) return;
        const bundled = Date.parse(bundledChecked());
        if (Number.isFinite(bundled) && saved.checkedAt <= bundled && saved.season <= Number(bundledSeason())) return;
        setHomeGames(saved.games);
        this.status = { ...this.status, checkedAt: saved.checkedAt, season: saved.season, message: "" };
    }

    private readSaved(): SavedSchedule | null {
        try {
            const saved = JSON.parse(this.storage?.getItem(CACHE_KEY) ?? "null") as SavedSchedule | null;
            if (!saved || !Array.isArray(saved.games) || saved.games.length === 0 || !saved.games.every(isGame)) return null;
            return saved;
        } catch {
            return null;
        }
    }

    /** `asked` is true when the visitor pressed Check for schedule updates. */
    onChanged(listener: (changes: string[], asked: boolean) => void): void {
        this.listeners.push(listener);
    }

    onStatus(listener: (status: ScheduleStatus) => void): void {
        this.statusListeners.push(listener);
    }

    private setStatus(patch: Partial<ScheduleStatus>): void {
        this.status = { ...this.status, ...patch };
        for (const listener of this.statusListeners) listener(this.status);
    }

    /** Whether a check is due: the app has not checked in the last half hour. */
    get due(): boolean {
        return this.now().getTime() - this.status.checkedAt >= MIN_CHECK_MS;
    }

    /**
     * Checks the feed and, when the schedule differs, puts the new one in place and tells the listeners. `force` (a visitor's tap)
     * skips the half-hour wait. Returns what changed; never throws (a failed check leaves the schedule as it was).
     */
    refresh(force = false): Promise<string[]> {
        if (this.inFlight) return this.inFlight;
        if (!force && !this.due) return Promise.resolve([]);
        this.inFlight = this.check(force).finally(() => {
            this.inFlight = null;
        });
        return this.inFlight;
    }

    private async check(asked: boolean): Promise<string[]> {
        this.setStatus({ checking: true, message: "Checking the schedule…" });
        const now = this.now();
        const before = homeGames();
        try {
            const { season, games } = await fetchHomeSchedule(now, before, this.fetchImpl);
            if (games.length === 0) {
                // The season is listed but has no home games yet (the league releases the next schedule in the spring): keep what we have.
                this.setStatus({ checking: false, checkedAt: now.getTime(), message: "No newer schedule has been released yet." });
                return [];
            }
            const today = easternParts(now.getTime()).date;
            let changes = describeChanges(before, games, today);
            if (season > this.status.season) changes = [`The ${season} season's home schedule is loaded.`];
            const changed = changes.length > 0 || JSON.stringify(before) !== JSON.stringify(games);
            if (changed) setHomeGames(games);
            try {
                this.storage?.setItem(CACHE_KEY, JSON.stringify({ season, checkedAt: now.getTime(), games } satisfies SavedSchedule));
            } catch {
                /* private mode: the schedule is right for this visit */
            }
            this.setStatus({
                checking: false, checkedAt: now.getTime(), season,
                message: changes.length ? `Schedule updated. ${changes.join(" ")}` : changed ? "Schedule updated." : "The schedule is up to date.",
            });
            if (changed) for (const listener of this.listeners) listener(changes, asked);
            return changes;
        } catch (error) {
            console.warn("Schedule check failed:", error);
            this.setStatus({ checking: false, message: this.status.checkedAt ? "Could not reach the schedule. Showing the last one saved." : "Could not reach the schedule. Showing the one that came with the app." });
            return [];
        }
    }
}
