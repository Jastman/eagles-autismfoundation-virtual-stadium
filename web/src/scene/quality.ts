// Rendering quality tiers: the web's stand-in for Unreal's scalability settings (Docs/WEB_PARITY.md W16). The experience
// is the same on every tier; only how the picture is made changes. A tier is picked from the device before the viewer
// exists, and stepped down once the stadium has loaded if the frame rate can't keep up. ?quality=low|medium|high fixes it.

export type QualityTier = "low" | "medium" | "high";

export interface QualitySettings {
    tier: QualityTier;
    /** Render resolution as a multiple of CSS pixels (device pixels are capped at this). */
    pixelRatio: number;
    msaaSamples: number;
    shadows: boolean;
    shadowMapSize: number;
    softShadows: boolean;
    ambientOcclusion: boolean;
    clouds: boolean;
    /** Screen-space error for the stadium model and the Google tiles (lower is sharper). */
    stadiumError: number;
    cityError: number;
}

const TIERS: Record<QualityTier, Omit<QualitySettings, "tier">> = {
    low: {
        pixelRatio: 1, msaaSamples: 1, shadows: false, shadowMapSize: 1024, softShadows: false, ambientOcclusion: false,
        clouds: false, stadiumError: 16, cityError: 32,
    },
    medium: {
        pixelRatio: 1.5, msaaSamples: 1, shadows: true, shadowMapSize: 2048, softShadows: false, ambientOcclusion: false,
        clouds: true, stadiumError: 12, cityError: 24,
    },
    high: {
        pixelRatio: 2, msaaSamples: 4, shadows: true, shadowMapSize: 4096, softShadows: true, ambientOcclusion: true,
        clouds: true, stadiumError: 8, cityError: 16,
    },
};

const ORDER: QualityTier[] = ["low", "medium", "high"];

export function settingsFor(tier: QualityTier): QualitySettings {
    return { tier, ...TIERS[tier] };
}

export function lowerTier(tier: QualityTier): QualityTier | null {
    const i = ORDER.indexOf(tier);
    return i > 0 ? ORDER[i - 1] : null;
}

interface DeviceInfo {
    /** iPad or iPhone (iPadOS reports itself as a Mac with touch). */
    apple: boolean;
    touch: boolean;
    /** The GPU's name where the browser shares it ("Apple GPU" on iOS, so not much help there). */
    gpu: string;
    /** navigator.deviceMemory in GB where available (not on Safari). */
    memory: number | null;
    cores: number;
    shortSide: number;
}

/** The starting tier for a device, before anything is measured. */
export function initialTier(device: DeviceInfo): QualityTier {
    const gpu = device.gpu.toLowerCase();
    if (/swiftshader|llvmpipe|software|basic render/.test(gpu)) return "low";
    if (device.apple && device.touch) {
        // iPads: M-series and recent A-series (8+ cores) start high; older ones medium. Phones medium.
        if (device.shortSide < 600) return "medium";
        return device.cores >= 8 ? "high" : "medium";
    }
    if (device.touch) return device.memory !== null && device.memory >= 8 ? "medium" : "low";
    if (/nvidia|geforce|rtx|radeon|apple m\d|apple gpu/.test(gpu)) return "high";
    if (/intel/.test(gpu)) return "medium";
    return device.memory !== null && device.memory < 4 ? "low" : "medium";
}

export function readDevice(): DeviceInfo {
    let gpu = "";
    try {
        const gl = document.createElement("canvas").getContext("webgl2") ?? document.createElement("canvas").getContext("webgl");
        const ext = gl?.getExtension("WEBGL_debug_renderer_info");
        gpu = (ext && gl ? (gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) as string) : gl?.getParameter(gl.RENDERER) as string) ?? "";
    } catch {
        /* no WebGL here: the viewer will report it */
    }
    const ua = navigator.userAgent;
    const touch = navigator.maxTouchPoints > 1 || matchMedia("(pointer: coarse)").matches;
    return {
        apple: /iPad|iPhone|iPod/.test(ua) || (/Macintosh/.test(ua) && touch),
        touch,
        gpu,
        memory: (navigator as unknown as { deviceMemory?: number }).deviceMemory ?? null,
        cores: navigator.hardwareConcurrency || 4,
        shortSide: Math.min(screen.width, screen.height),
    };
}

/** The tier to start with: ?quality= wins, then a tier remembered on this device from an earlier visit, then a guess. */
export function chooseTier(params: URLSearchParams): { tier: QualityTier; fixed: boolean } {
    const asked = params.get("quality");
    if (asked === "low" || asked === "medium" || asked === "high") return { tier: asked, fixed: true };
    try {
        const remembered = window.localStorage.getItem("lff.quality");
        if (remembered === "low" || remembered === "medium" || remembered === "high") return { tier: remembered, fixed: false };
    } catch {
        /* private mode */
    }
    return { tier: initialTier(readDevice()), fixed: false };
}

export function rememberTier(tier: QualityTier): void {
    try {
        window.localStorage.setItem("lff.quality", tier);
    } catch {
        /* private mode */
    }
}

/**
 * Watches the frame rate once the world is settled and asks for a lower tier when it stays under the target. It only
 * ever steps down, so the picture never flips back and forth during the visit.
 */
export class FrameRateGovernor {
    private samples: number[] = [];
    private cooldown = 0;

    constructor(private readonly targetFps = 28, private readonly windowSeconds = 6) {}

    /** Returns true when the tier should drop now. */
    tick(dt: number, settled: boolean): boolean {
        if (!settled || dt <= 0) {
            this.samples = [];
            return false;
        }
        if (this.cooldown > 0) {
            this.cooldown -= dt;
            return false;
        }
        this.samples.push(dt);
        const total = this.samples.reduce((a, b) => a + b, 0);
        if (total < this.windowSeconds) return false;
        const fps = this.samples.length / total;
        this.samples = [];
        if (fps >= this.targetFps) return false;
        this.cooldown = 4;
        return true;
    }
}
