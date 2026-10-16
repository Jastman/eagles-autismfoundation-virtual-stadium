// Port of ALFFGameMode's defaults (Source/VirtualStadium/{Public,Private}/Core/LFFGameMode.*). Every value here mirrors the
// Unreal default; change both together and record it in Docs/WEB_PARITY.md.
import type { LevelView, MapLevel, VisitorSettings } from "./journey/types";
import { defaultSettings } from "./journey/types";

const view = (FloorZ: number, MinZ: number, MaxZ: number, ViewDistance: number, ViewPitchDegrees: number): LevelView =>
    ({ FloorZ, MinZ, MaxZ, ViewDistance, ViewPitchDegrees, ViewYawDegrees: 20 });

export const GameMode = {
    // Sound and the voiceover start on (project owner, session 5); Calm Mode still silences both.
    DefaultVisitorSettings: { ...defaultSettings(), bSoundEnabled: true, bVoiceEnabled: true } as VisitorSettings,

    /** First step (0-based) of each chapter: Arrival, Gates, Plaza, To my seat, Game. */
    TourStepFirstSteps: [0, 4, 7, 10, 14],

    MySeatTargetId: "CT_SeatFieldView",
    MySeatLevel: "LowerConcourse" as MapLevel,
    MySeatLabel: "My seat",
    MySeatNote: "Section 111, Row 10, Seat 8.",

    /** Packaged builds drop places with no VerificationSource; the web build follows the packaged rule (D6). */
    bShowUnverifiedAmenities: false,

    /** ELFFMapLevel order: the turf, the 7.7 m deck, the club, the suites, the upper deck (site frame, cm above the turf). */
    LevelViews: [
        view(0, -150, 120, 18000, -40),
        view(770, 120, 1000, 19000, -35),
        view(1720, 1000, 1950, 20000, -32),
        view(2180, 1950, 2900, 21000, -30),
        view(3120, 2900, 5000, 22000, -30),
    ] as LevelView[],
    StadiumFootprintRadii: { x: 13000, y: 14000 },
    /** KELLY at 35%, linear. */
    LevelHighlightColor: [0.141, 0.514, 0.021] as [number, number, number],
    LevelHighlightOpacity: 0.35,
    LevelDimOutside: 0.3,

    FlySpeed: 3200,
    FlyFastMultiplier: 2,
    FlyLookDegreesPerPixel: 0.28,
    FlyEaseSeconds: 0.22,
    FlyWheelStep: 3000,
    MapFramePitchDegrees: -30,
    FlyBoundsRadius: 60000,
    FlyMinZ: 150,
    FlyMaxZ: 30000,

    IntroTitle: "Lincoln Financial Field",
    IntroBody: "A calm preview of game day.\nYou choose when to move on.",
    PrototypeBadgeText: "Prototype · Illustrative · Example ticket",
    DisclaimerIllustrative: "Illustration only. This is not a navigation instruction for the real stadium.",
    DisclaimerApproximate: "Approximate view. This is not a navigation instruction for the real stadium.",
    DisclaimerLineIllustrative: "Illustration. Not directions for the real stadium.",
    DisclaimerLineApproximate: "Approximate. Not directions for the real stadium.",
    TargetMissingText: "This step is not available yet.",
    JourneyMissingText: "The journey could not be loaded.",

    DefaultEaseSeconds: 3,
    MinTransitionSeconds: 0.5,
    MaxTransitionSeconds: 6,
    ReducedMotionFadeSeconds: 0.35,
    /** Neutral dark grey rather than black, linear (0.08, 0.08, 0.10). */
    FadeColor: "rgb(79, 79, 89)",

    AmbientBed: "SW_LFF_AmbientCrowd1",
    StadiumAmbientBed: "SW_LFF_AmbientCrowd2",
    AnthemSound: "SW_LFF_AnthemLondon",
    UIClickSound: "SW_LFF_UIClick",
    PreviewSound: "SW_LFF_PreviewCrowdMuffled",
    BedVolume: 0.5,
    PreviewVolume: 0.5,
    AnthemVolume: 0.85,
    UIClickVolume: 1,
    SensoryRoomCrowdGain: 0.035,
    QuietRoomCrowdGain: 0.12,

    LightPulseSeconds: 4,
    LightPulseStops: 0.9,

    AnthemFollowOffset: { x: 200, y: 700, z: -50 },
    AnthemFollowFieldOfView: 55,
    AnthemFollowStart: 0.3,
    AnthemReturnSeconds: 3.5,

    VoiceVolume: 0.9,
    VoiceDuckDecibels: -12,
    VoiceStartDelaySeconds: 0.5,
    BedCrossfadeSeconds: 1.5,

    SplashOrbitTargetId: "CT_StadiumWide",
    SplashOrbitSeconds: 40,

    LearnMoreUrl: "https://www.philadelphiaeagles.com/eaglesautismfoundation/",
    /** Settings > Send us feedback: the Microsoft Forms survey (web addition, Docs/WEB_PARITY.md W19). VITE_FEEDBACK_URL
     *  overrides it per deployment. */
    FeedbackUrl: ((import.meta.env.VITE_FEEDBACK_URL as string | undefined) || "https://forms.cloud.microsoft/r/bj1p1xMZYK").trim(),
};

export type GameModeConfig = typeof GameMode;

/**
 * Deployment settings, not part of the experience. The ion token is per deployment (CESIUM_PLACEMENT §7: dedicated,
 * scoped to the used assets, revocable). Set VITE_CESIUM_ION_TOKEN in web/.env.local, or pass ?ionToken= once on the
 * device (kept in localStorage for that device only).
 */
export interface DeploymentConfig {
    /** Token for the context tiles (Google Photorealistic 3D Tiles). */
    ionToken: string;
    /** Google Photorealistic 3D Tiles on Cesium ion. */
    googleTilesAssetId: number;
    /** The licensed CGTrader model on Cesium ion (team assets): stadium, seats (parts 1-5) and the NFL field. Empty: none,
     *  and Google's own stadium stays as the stand-in (CESIUM_PLACEMENT.md, V1 amendments). */
    stadiumAssetIds: number[];
    /** Stadium, seats and field tileset.json URLs to load instead of the ion assets (a self-hosted or re-processed copy,
     *  e.g. one from Tools/Pipeline/compress_stadium_textures.mjs). ?stadiumUrls=a,b,c or VITE_STADIUM_TILESET_URLS. */
    stadiumTilesetUrls: string[];
    /** A token scoped to the stadium assets; falls back to ionToken. */
    stadiumIonToken: string;
    /**
     * How the stadium assets are placed. "auto" trusts ion's own georeference when it lands within 2 km of midfield, and
     * otherwise applies the ADR-0004 registration (model metres, Z-up, origin at midfield turf: 182.25 deg, -2.25 m E,
     * +2.5 m N in the site's map frame). "registration" always applies it; "ion" never does.
     */
    stadiumPlacement: "auto" | "registration" | "ion";
    /** Fine-tuning on top of the registration, for checking alignment on the device (?stadiumHeading=, ?stadiumOffset=e,n,u). */
    stadiumHeadingAdjustDeg: number;
    stadiumOffsetAdjustM: [number, number, number];
    /** The stadium assets carry baked lighting (Tools/Pipeline/bake_stadium_lighting.py): they cast shadows but don't receive
     *  them, and screen-space ambient occlusion stays off, so the bake isn't darkened twice. */
    stadiumBaked: boolean;
    /** Dev only: skip the loading cover. */
    noLoadingScreen: boolean;
}

function readStored(key: string): string | null {
    try {
        return window.localStorage.getItem(key);
    } catch {
        return null;
    }
}

function store(key: string, value: string): void {
    try {
        window.localStorage.setItem(key, value);
    } catch {
        /* private mode: the URL parameter still works for this visit */
    }
}

export function readDeploymentConfig(): DeploymentConfig {
    const params = new URLSearchParams(window.location.search);
    const fromUrl = params.get("ionToken");
    if (fromUrl) store("lff.ionToken", fromUrl);
    const stadiumTokenFromUrl = params.get("stadiumToken");
    if (stadiumTokenFromUrl) store("lff.stadiumToken", stadiumTokenFromUrl);
    const env = import.meta.env;
    const ionToken = fromUrl ?? readStored("lff.ionToken") ?? (env.VITE_CESIUM_ION_TOKEN as string | undefined) ?? "";
    const ids = (params.get("stadiumAssets") ?? (env.VITE_STADIUM_ASSET_IDS as string | undefined) ?? "6040628,6040635,6040637")
        .split(",").map((s) => Number(s.trim())).filter((n) => Number.isFinite(n) && n > 0);
    const placement = params.get("stadiumPlacement") ?? (env.VITE_STADIUM_PLACEMENT as string | undefined) ?? "auto";
    const offset = (params.get("stadiumOffset") ?? "0,0,0").split(",").map(Number);
    return {
        ionToken,
        googleTilesAssetId: Number(env.VITE_GOOGLE_TILES_ASSET_ID ?? 2275207),
        stadiumAssetIds: ids,
        stadiumTilesetUrls: (params.get("stadiumUrls") ?? (env.VITE_STADIUM_TILESET_URLS as string | undefined) ?? "")
            .split(",").map((s) => s.trim()).filter(Boolean),
        stadiumIonToken: stadiumTokenFromUrl ?? readStored("lff.stadiumToken") ?? (env.VITE_STADIUM_ION_TOKEN as string | undefined) ?? ionToken,
        stadiumPlacement: placement === "registration" || placement === "ion" ? placement : "auto",
        stadiumHeadingAdjustDeg: Number(params.get("stadiumHeading") ?? 0) || 0,
        stadiumOffsetAdjustM: [offset[0] || 0, offset[1] || 0, offset[2] || 0],
        stadiumBaked: ["1", "true"].includes(params.get("stadiumBaked") ?? (env.VITE_STADIUM_BAKED as string | undefined) ?? ""),
        noLoadingScreen: params.has("noLoadingScreen"),
    };
}
