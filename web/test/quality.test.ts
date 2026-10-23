// Quality tiers: the starting guess per device and the one-way frame-rate step-down.
import { describe, expect, it } from "vitest";
import { FrameRateGovernor, initialTier, lowerTier, settingsFor } from "../src/scene/quality";

const device = (o: Partial<Parameters<typeof initialTier>[0]>) =>
    ({ apple: false, touch: false, gpu: "", memory: null, cores: 8, shortSide: 1080, ...o });

describe("quality tiers", () => {
    it("starts iPads by class and software rendering low", () => {
        // Safari's per-tab memory limit, not the GPU, decides: iPads never start high.
        expect(initialTier(device({ apple: true, touch: true, cores: 8, shortSide: 820 }))).toBe("medium");
        expect(initialTier(device({ apple: true, touch: true, cores: 6, shortSide: 768 }))).toBe("low");
        expect(initialTier(device({ apple: true, touch: true, shortSide: 390 }))).toBe("low");
        expect(initialTier(device({ gpu: "ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device))" }))).toBe("low");
        expect(initialTier(device({ gpu: "ANGLE (NVIDIA GeForce RTX 4070)" }))).toBe("high");
    });

    it("only ever steps down", () => {
        expect(lowerTier("high")).toBe("medium");
        expect(lowerTier("medium")).toBe("low");
        expect(lowerTier("low")).toBeNull();
        expect(settingsFor("low").shadows).toBe(false);
        expect(settingsFor("high").shadowMapSize).toBeGreaterThan(settingsFor("medium").shadowMapSize);
    });

    it("keeps the iPad tiers' tile caches well under Safari's tab limit", () => {
        for (const tier of ["low", "medium"] as const) {
            const s = settingsFor(tier);
            // The three stadium tilesets and the city, each allowed its overflow at worst. The old uncapped build reached
            // 1.5 GB of tiles before Safari closed the tab.
            const worstMB = s.structureCacheMB + s.seatsCacheMB + s.fieldCacheMB + s.cityCacheMB + 4 * s.cacheOverflowMB;
            expect(worstMB).toBeLessThanOrEqual(900);
        }
    });

    it("gives the stadium structure and seats room for a view from inside the bowl", () => {
        // Measured inside the bowl at the low tier: the structure needs 175-330 MB (its coarsest tiles alone 160+); below
        // that CesiumJS coarsens it without limit and floors and stands disappear. The seats coarsen gracefully.
        for (const tier of ["low", "medium", "high"] as const) {
            expect(settingsFor(tier).structureCacheMB).toBeGreaterThanOrEqual(288);
            expect(settingsFor(tier).seatsCacheMB).toBeGreaterThanOrEqual(112);
        }
    });

    it("asks for a lower tier after a slow window, not during loading", () => {
        const governor = new FrameRateGovernor(28, 6);
        let asked = false;
        for (let t = 0; t < 10; t += 0.05) asked ||= governor.tick(0.05, false); // 20 fps while loading: ignored
        expect(asked).toBe(false);
        for (let t = 0; t < 7; t += 0.05) asked ||= governor.tick(0.05, true); // 20 fps settled
        expect(asked).toBe(true);
        const fast = new FrameRateGovernor(28, 6);
        let fastAsked = false;
        for (let t = 0; t < 20; t += 1 / 60) fastAsked ||= fast.tick(1 / 60, true);
        expect(fastAsked).toBe(false);
    });
});
