// Port of ULFFCalloutLayer: places drawn in the 3D view. Each is a thin stem rising from its spot on the model to a
// round head with the place's glyph. Free explore: the kinds that are on, plus the seat; tapping a head picks it (its
// name chip shows, and the Places card offers Focus place). Guided tour: only the step's places, always named, not
// tappable. A head that would sit under a HUD panel, the top edge or down among the bottom row's controls is hidden;
// heads that would overlap stack. Heads and chips are opaque, so no word sits on the scene.
import type { Journey } from "../journey/journey";
import { MY_SEAT_AMENITY_ID } from "../journey/journey";
import type { Amenity, AmenityMarker, JourneySnapshot } from "../journey/types";
import { amenityBit } from "../journey/types";
import { Vec2, Vec3 } from "../util/ue";
import { h, tapOnly } from "./dom";

const STEM_HEIGHT = 2500;
const MAX_STEM = 360;
const HEAD_SIZE = 34;
const HIT_SIZE = 64;
const TOP_INSET = 48;
const BOTTOM_INSET = 112;
const GLYPHS: Record<Amenity, string> = { SensoryRoom: "S", QuietZone: "Q", GuestServices: "i", Restroom: "WC", Food: "F" };

export type Projector = (world: Vec3) => Vec2 | null;

interface Callout {
    id: string;
    seat: boolean;
    place: AmenityMarker;
    head: HTMLButtonElement;
    chip: HTMLElement;
    wanted: boolean;
    drawn: boolean;
    spot: Vec2;
    headPx: Vec2;
    hovered: boolean;
}

export class CalloutLayer {
    readonly root = h("div", { class: "callouts" });
    private readonly canvas = h("canvas");
    private callouts: Callout[] = [];
    private snapshot: JourneySnapshot | null = null;
    static projector: Projector = () => null;

    constructor(private readonly journey: Journey, private readonly blockers: () => HTMLElement[], private readonly unitsPerPixel: () => number) {
        this.root.append(this.canvas);
    }

    private build(): void {
        for (const c of this.callouts) {
            c.head.remove();
            c.chip.remove();
        }
        const places = [...this.journey.getAmenities()];
        const seat = this.journey.getAmenity(MY_SEAT_AMENITY_ID);
        if (seat) places.push(seat);
        this.callouts = places.map((place) => {
            const isSeat = place.AmenityId === MY_SEAT_AMENITY_ID;
            const disc = h("div", { class: "disc" }, isSeat ? h("div", { class: "dot" }) : GLYPHS[place.Category]);
            const head = h("button", { class: `callout-head collapsed${isSeat ? " seat" : ""}`, type: "button", tabindex: "-1", "aria-label": place.Label,
                title: place.Label }, disc);
            const callout: Callout = { id: place.AmenityId, seat: isSeat, place, head, chip: h("div", { class: "callout-chip collapsed", text: place.Label }),
                wanted: false, drawn: false, spot: new Vec2(), headPx: new Vec2(), hovered: false };
            head.addEventListener("click", () => {
                this.journey.playUIClick();
                this.journey.selectAmenity(place.AmenityId);
            });
            head.addEventListener("pointerenter", (e) => {
                if (e.pointerType === "mouse") callout.hovered = true;
            });
            head.addEventListener("pointerleave", () => (callout.hovered = false));
            tapOnly(head);
            this.root.append(head, callout.chip);
            return callout;
        });
    }

    apply(s: JourneySnapshot): void {
        this.snapshot = s;
        if (this.callouts.length !== this.journey.getAmenities().length + 1) this.build();
        const modal = s.bSettingsOpen || s.bRestartConfirmOpen || s.bPaused;
        for (const c of this.callouts) {
            const kindOn = (s.AmenityMask & amenityBit(c.place.Category)) !== 0;
            c.wanted = s.bStarted || s.Mode === "FreeExplore"
                ? (s.Mode === "FreeExplore" ? c.seat || kindOn : s.TourPlaces.includes(c.id))
                : false;
            c.head.classList.toggle("picked", c.id === s.SelectedAmenityId);
            c.head.disabled = modal;
        }
        this.place();
    }

    tick(): void {
        // The camera moves every frame in Free explore (flight, eases), so the heads follow it every frame.
        this.place();
    }

    private place(): void {
        const s = this.snapshot;
        const ctx = this.canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const width = window.innerWidth, height = window.innerHeight;
        if (this.canvas.width !== Math.round(width * dpr) || this.canvas.height !== Math.round(height * dpr)) {
            this.canvas.width = Math.round(width * dpr);
            this.canvas.height = Math.round(height * dpr);
        }
        ctx?.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx?.clearRect(0, 0, width, height);
        if (!s) return;
        const upp = this.unitsPerPixel(); // logical units per CSS px
        const px = (units: number) => units / upp;
        const tour = s.Mode === "GuidedTour";
        const site = this.journey.getSiteFrame();
        const up = site.unitAxis("Z");
        const covered = this.blockers().map((el) => el.getBoundingClientRect()).filter((r) => r.width > 1 && r.height > 1);
        const isCovered = (x0: number, y0: number, x1: number, y1: number) => covered.some((r) => !(x1 < r.left || x0 > r.right || y1 < r.top || y0 > r.bottom));
        const hit = px(HIT_SIZE), half = hit / 2;
        const screen = { x0: half, y0: px(TOP_INSET), x1: width - half, y1: height - px(BOTTOM_INSET) };
        const inScreen = (p: Vec2) => p.x >= screen.x0 && p.x <= screen.x1 && p.y >= screen.y0 && p.y <= screen.y1;
        const placed: Vec2[] = [];

        for (const c of this.callouts) {
            c.drawn = false;
            const spotWorld = site.transformPosition(Vec3.from(c.place.Location));
            const spot = c.wanted ? CalloutLayer.projector(spotWorld) : null;
            const top = spot ? CalloutLayer.projector(spotWorld.add(up.scale(STEM_HEIGHT))) : null;
            if (spot && top) {
                // Close up a stem would run off the screen: keep the head within MaxStemPixels of its spot.
                const stem = top.sub(spot);
                const length = stem.size();
                const maxStem = px(MAX_STEM);
                c.spot = spot;
                c.headPx = length > maxStem ? spot.add(stem.scale(maxStem / length)) : top;
                const anchor = c.headPx;
                const dirs = [new Vec2(0, -1), new Vec2(0, 1), new Vec2(-1, 0), new Vec2(1, 0)];
                for (let attempt = 0; attempt < 17; ++attempt) {
                    const ring = Math.floor((attempt + 3) / 4);
                    const candidate = attempt === 0 ? anchor : anchor.add(dirs[(attempt - 1) % 4].scale(ring * (hit + px(8))));
                    if (inScreen(candidate) && !isCovered(candidate.x - half, candidate.y - half, candidate.x + half, candidate.y + half)
                        && !placed.some((o) => Math.abs(o.x - candidate.x) < hit && Math.abs(o.y - candidate.y) < hit)) {
                        c.headPx = candidate;
                        c.drawn = true;
                        break;
                    }
                }
                if (c.drawn) placed.push(c.headPx);
            }
            c.head.classList.toggle("collapsed", !c.drawn);
            // In the tour the heads only point; the card says what they are.
            c.head.classList.toggle("passive", tour);
            if (c.drawn) {
                c.head.style.left = `${c.headPx.x}px`;
                c.head.style.top = `${c.headPx.y}px`;
            }
            let named = c.drawn && (tour || c.seat || c.id === s.SelectedAmenityId || c.hovered);
            if (named) {
                c.chip.classList.remove("collapsed");
                const w = c.chip.offsetWidth, hgt = c.chip.offsetHeight;
                const gap = half + px(4);
                const right = new Vec2(c.headPx.x + gap, c.headPx.y);
                const left = new Vec2(c.headPx.x - gap - w, c.headPx.y);
                const fits = (p: Vec2) => inScreen(new Vec2(p.x, p.y - hgt / 2)) && inScreen(new Vec2(p.x + w, p.y + hgt / 2))
                    && !isCovered(p.x, p.y - hgt / 2, p.x + w, p.y + hgt / 2);
                const at = fits(right) ? right : fits(left) ? left : null;
                if (at) {
                    c.chip.style.left = `${at.x}px`;
                    c.chip.style.top = `${at.y}px`;
                } else {
                    named = false;
                }
            }
            c.chip.classList.toggle("collapsed", !named);

            // Stems: a light line on a dark one, so they read on sky and on concrete alike, from a small ring on the spot.
            if (c.drawn && ctx) {
                const stem = c.headPx.sub(c.spot);
                const length = stem.size();
                const headRadius = px(HEAD_SIZE) / 2;
                if (length >= headRadius + 2) {
                    const end = c.headPx.sub(stem.scale(headRadius / length));
                    const colour = c.seat ? "#69BE28" : "#F4F2EC";
                    ctx.lineCap = "round";
                    ctx.strokeStyle = "rgba(0, 0, 0, 0.55)";
                    ctx.lineWidth = px(5);
                    ctx.beginPath();
                    ctx.moveTo(c.spot.x, c.spot.y);
                    ctx.lineTo(end.x, end.y);
                    ctx.stroke();
                    ctx.strokeStyle = colour;
                    ctx.lineWidth = px(3);
                    ctx.stroke();
                    ctx.lineWidth = px(2);
                    ctx.beginPath();
                    ctx.arc(c.spot.x, c.spot.y, px(5), 0, Math.PI * 2);
                    ctx.stroke();
                }
            }
        }
    }
}
