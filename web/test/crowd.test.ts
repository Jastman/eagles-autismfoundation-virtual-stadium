// Port of LFF.Crowd.Selection (Source/VirtualStadium/Private/Tests/LFFCrowdTest.cpp).
import { describe, expect, it } from "vitest";
import { RandomStream, isCameraSpace, representationCounts } from "../src/scene/crowd";
import { Rotator, Transform, Vec3 } from "../src/util/ue";

describe("LFF.Crowd.Selection", () => {
    const camera = new Transform(new Rotator(-6, 0, 0).quaternion(), new Vec3(0, 0, 270));

    it("clears only the visitor's immediate space", () => {
        expect(isCameraSpace(new Vec3(20, 0, 0), camera, 50, 0)).toBe(true);
        expect(isCameraSpace(new Vec3(92, 0, 0), camera, 50, 0)).toBe(false);
        expect(isCameraSpace(new Vec3(500, 20, 0), camera, 50, 0)).toBe(false);
        expect(isCameraSpace(new Vec3(0, 62, 0), camera, 50, 0)).toBe(false);
    });

    it("bounds each representation and keeps overflow as billboards", () => {
        const counts = representationCounts(48, 1000, 10000, 0.7, 300);
        expect(counts.detailed).toBe(34);
        expect(counts.mass).toBe(300);
        expect(counts.overflow).toBe(400);
        expect(counts.far).toBe(7400);
        const empty = representationCounts(48, 1000, 10000, 0, 300);
        expect(empty.detailed + empty.mass + empty.far).toBe(0);
    });
});

describe("FRandomStream", () => {
    it("is deterministic and stays in range", () => {
        const a = new RandomStream(1018), b = new RandomStream(1018);
        for (let i = 0; i < 1000; ++i) {
            const f = a.fraction();
            expect(f).toBe(b.fraction());
            expect(f).toBeGreaterThanOrEqual(0);
            expect(f).toBeLessThan(1);
        }
        const r = new RandomStream(7);
        for (let i = 1; i < 500; ++i) expect(r.randHelper(i)).toBeLessThan(i);
    });
});
