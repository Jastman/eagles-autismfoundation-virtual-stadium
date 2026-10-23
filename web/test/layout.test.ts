// Port of LFF.UI.DisplayLayout.SafeBounds (Source/VirtualStadium/Private/Tests/LFFUILayoutTest.cpp), plus the iPad
// viewports this build targets, in logical units (shortest side = 1080).
import { describe, expect, it } from "vitest";
import { cornerPanels, formFactor, ticketBounds, timelineColumns, unitsPerPixelFor } from "../src/ui/layout";
import { Box2, Vec2 } from "../src/util/ue";

const logical = (w: number, h: number) => new Vec2((w * 1080) / Math.min(w, h), (h * 1080) / Math.min(w, h));

const VIEWPORTS: [string, Vec2][] = [
    ["3584x1280", new Vec2(3584, 1280)],
    ["3584x1280 scaled", new Vec2(3584 / (1280 / 1080), 1280 / (1280 / 1080))],
    ["1920x1080", new Vec2(1920, 1080)],
    ["1280x720 scaled", new Vec2(1280 / (720 / 1080), 720 / (720 / 1080))],
    ["iPad Air 11 landscape (1180x820)", logical(1180, 820)],
    ["iPad Pro 13 landscape (1376x1032)", logical(1376, 1032)],
    ["iPad 10.9 landscape (1180x820)", logical(1180, 820)],
];

describe("LFF.UI.DisplayLayout.SafeBounds", () => {
    for (const [name, viewport] of VIEWPORTS) {
        for (const large of [false, true]) {
            it(`${name} large=${large}`, () => {
                const layout = cornerPanels(viewport, large);
                expect(layout.InformationWidth).toBe(large ? 600 : 480);
                expect(layout.ControlWidth).toBe(540);
                expect(layout.CaptionWidth * (large ? 1.25 : 1)).toBeLessThanOrEqual(layout.InformationWidth - 40 + 0.01);
                const columns = timelineColumns(layout.CaptionWidth, 128, 6);
                expect(columns).toBeGreaterThanOrEqual(1);
                expect(columns * 128 + (columns - 1) * 6).toBeLessThanOrEqual(layout.CaptionWidth + 0.01);

                const information = new Box2(new Vec2(24, 24), new Vec2(24 + layout.InformationWidth, 24 + layout.MaximumHeight));
                const controls = new Box2(new Vec2(viewport.x - 24 - layout.ControlWidth, 24), viewport.sub(new Vec2(24, 88)));
                const phone = ticketBounds(viewport, information, controls, new Vec2(240, 480));
                expect(phone.valid).toBe(true);
                expect(phone.min.x).toBeGreaterThanOrEqual(information.max.x + 24);
                expect(phone.min.x).toBeGreaterThanOrEqual(viewport.x * 0.5);
                expect(phone.max.x).toBeLessThanOrEqual(controls.min.x - 24);
                expect(phone.min.y).toBeGreaterThanOrEqual(24);
                expect(phone.max.y).toBeLessThanOrEqual(viewport.y - 24);
                expect((phone.max.x - phone.min.x) / (phone.max.y - phone.min.y)).toBeCloseTo(0.5, 4);
            });
        }
    }

    it("handles narrow lanes and invalid viewports", () => {
        expect(timelineColumns(100, 128, 6)).toBe(1);
        const narrow = ticketBounds(new Vec2(1000, 800), new Box2(new Vec2(24, 24), new Vec2(454, 400)),
            new Box2(new Vec2(546, 24), new Vec2(976, 350)), new Vec2(240, 480));
        expect(narrow.valid && narrow.min.y >= 374).toBe(true);
        expect(ticketBounds(new Vec2(), Box2.invalid(), Box2.invalid(), new Vec2(240, 480)).valid).toBe(false);
    });
});

describe("Phones and tablets", () => {
    it("keeps Unreal's ShortestSide scale on tablets", () => {
        expect(unitsPerPixelFor(1180, 820)).toBeCloseTo(1080 / 820);
        expect(unitsPerPixelFor(1366, 1024)).toBeCloseTo(1080 / 1024);
        expect(formFactor(1180, 820)).toEqual({ phone: false, portrait: false });
        expect(formFactor(820, 1180)).toEqual({ phone: false, portrait: true });
    });

    it("keeps phone labels as large as on an iPad", () => {
        const ipadLabelPx = 18 / unitsPerPixelFor(1180, 820);
        for (const [w, h] of [[402, 874], [874, 402], [375, 667], [440, 956]]) {
            expect(formFactor(w, h).phone).toBe(true);
            const labelPx = 18 / unitsPerPixelFor(w, h);
            expect(labelPx).toBeGreaterThanOrEqual(11.5);
            expect(labelPx).toBeGreaterThan(ipadLabelPx * 0.85);
        }
    });

    it("fits the corner panels side by side on a phone held sideways", () => {
        const upp = unitsPerPixelFor(874, 402);
        const size = new Vec2(874 * upp, 402 * upp);
        const panels = cornerPanels(size, false);
        expect(panels.InformationWidth + panels.ControlWidth + panels.EdgeInset * 3).toBeLessThanOrEqual(size.x);
    });
});
