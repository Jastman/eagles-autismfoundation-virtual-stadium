// Port of SLFFLoadingScreen + ULFFTileStreamingSubsystem's cover: midnight green, the EAF and Five Below marks, a bar
// that only ever moves forward, and a status line that follows the bar but keeps moving if one stretch is slow.
import { h } from "./dom";

const LINES = [
    "Finding Lincoln Financial Field...",
    "Mowing the grass...",
    "Painting the end zones midnight green...",
    "Warming up the stadium lights...",
    "Saving your seat in Section 111...",
    "Finding the quiet spaces...",
    "Almost game time...",
];
export const MINIMUM_LOADING_SECONDS = 2.5;
export const LOADING_TIMEOUT_SECONDS = 45;
const SETTLED_SECONDS = 1;
const LOADED_PERCENT = 99.5;

export function statusText(percent: number, elapsed: number): string {
    if (percent >= 100) return "Fly, Eagles, Fly!";
    const byProgress = Math.floor((Math.min(Math.max(percent, 0), 99.9) / 100) * LINES.length);
    const byTime = Math.floor(Math.max(elapsed, 0) / 3.5);
    return LINES[Math.min(Math.max(Math.max(byProgress, byTime), 0), LINES.length - 1)];
}

export class LoadingScreen {
    readonly root: HTMLElement;
    private readonly fill = h("div", { class: "fill" });
    private readonly percent = h("div", { class: "percent", text: "0%" });
    private readonly status = h("div", { class: "status", text: LINES[0], "aria-live": "polite" });
    private elapsed = 0;
    private displayed = 0;
    private settled = 0;
    private sawIncomplete = false;
    private done = false;

    constructor() {
        this.root = h("div", { class: "loading", role: "progressbar", "aria-label": "Loading the stadium" },
            h("div", { class: "logos" },
                h("div", { class: "logo", style: { backgroundImage: "url(media/ui/T_LFF_Logo_EAF_White.png)" }, role: "img", "aria-label": "Eagles Autism Foundation" }),
                h("div", { class: "divider" }),
                h("div", { class: "logo", style: { backgroundImage: "url(media/ui/T_LFF_Logo_FiveBelow_UI.png)" }, role: "img", "aria-label": "Five Below Foundation" })),
            h("div", { class: "bar" }, this.fill), this.percent, this.status);
    }

    /** Per frame with the tiles' raw progress (0..100). Returns true once the cover has finished. */
    tick(dt: number, raw: number): boolean {
        if (this.done) return true;
        dt = Math.min(dt, 0.1);
        this.elapsed += dt;
        this.sawIncomplete ||= raw < LOADED_PERCENT;
        // A tileset reports done for a moment before it knows which tiles it needs; trust it once it has been below.
        const loaded = raw >= LOADED_PERCENT && (this.sawIncomplete || this.elapsed > 6);
        this.settled = loaded ? this.settled + dt : 0;
        const finished = (this.settled >= SETTLED_SECONDS && this.elapsed >= MINIMUM_LOADING_SECONDS) || this.elapsed >= LOADING_TIMEOUT_SECONDS;
        const target = finished ? 100 : Math.min(raw, 99);
        if (target > this.displayed) {
            const eased = this.displayed + (target - this.displayed) * Math.min(dt * 3, 1) + 8 * dt;
            this.displayed = Math.min(target, eased);
        }
        this.fill.style.width = `${this.displayed}%`;
        this.percent.textContent = `${Math.floor(this.displayed)}%`;
        this.status.textContent = statusText(this.displayed, this.elapsed);
        if (finished && this.displayed >= 100) {
            if (this.elapsed >= LOADING_TIMEOUT_SECONDS) console.warn(`Loading screen: tiles still loading after ${LOADING_TIMEOUT_SECONDS} s; showing the stadium anyway.`);
            this.done = true;
            this.root.classList.add("done");
            window.setTimeout(() => this.root.remove(), 1200);
        }
        return this.done;
    }

    isDone(): boolean {
        return this.done;
    }
}
