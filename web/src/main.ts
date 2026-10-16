// The web build's entry point: CesiumJS viewer, the level, the journey, the HUD and input, ticked together each frame
// (Unreal's order: input and flight, journey timers and camera rig, scene cues, then the HUD).
import "cesium/Build/Cesium/Widgets/widgets.css";
import "./ui/hud.css";
import { Cartesian3, Color, Ion, JulianDate, SceneTransforms, SkyAtmosphere, Viewer } from "cesium";
import { AudioEngine } from "./audio/audio";
import { GameMode, readDeploymentConfig } from "./config";
import { Input } from "./input/input";
import { amenities, goldenPath } from "./journey/data";
import { Journey } from "./journey/journey";
import { Geo } from "./scene/geo";
import { sceneData } from "./scene/sceneData";
import { SceneLook } from "./scene/look";
import type { UnrealBounds } from "./scene/models";
import { FrameRateGovernor, chooseTier, lowerTier, rememberTier, settingsFor } from "./scene/quality";
import { loadTiles } from "./scene/tiles";
import { StadiumWorld } from "./scene/world";
import { CalloutLayer } from "./ui/callouts";
import { h } from "./ui/dom";
import { Hud } from "./ui/hud";
import { LoadingScreen } from "./ui/loading";
import { Vec2 } from "./util/ue";
import { Timers } from "./util/timers";

/** Logical UMG units per CSS pixel: Unreal's ShortestSide rule, 1080 = 1.0. */
const unitsPerPixel = () => 1080 / Math.max(1, Math.min(window.innerWidth, window.innerHeight));

function applyUiScale(): void {
    // 1rem = 10 logical units (hud.css).
    document.documentElement.style.fontSize = `${10 / unitsPerPixel()}px`;
}

async function fetchJson<T>(url: string, fallback: T): Promise<T> {
    try {
        const response = await fetch(url);
        return response.ok ? ((await response.json()) as T) : fallback;
    } catch {
        return fallback;
    }
}

async function main(): Promise<void> {
    applyUiScale();
    window.addEventListener("resize", applyUiScale);
    const app = document.getElementById("app")!;
    const config = readDeploymentConfig();
    const loading = new LoadingScreen();
    if (!config.noLoadingScreen) document.body.append(loading.root);

    const audio = new AudioEngine();
    const [ticket, models, modelBounds] = await Promise.all([
        fetchJson<{ frames: string[] }>("media/ticket/manifest.json", { frames: [] }),
        fetchJson<Record<string, string>>("models/manifest.json", {}),
        fetchJson<Record<string, UnrealBounds>>("models/bounds.json", {}),
        audio.loadManifest("media/audio/manifest.json"),
        // Signs are drawn into textures, so the project font must be ready first.
        document.fonts.load('700 64px "Atkinson Hyperlegible"').catch(() => undefined),
        document.fonts.load('400 24px "Atkinson Hyperlegible"').catch(() => undefined),
    ]);
    for (const frame of ticket.frames) new Image().src = frame;

    Ion.defaultAccessToken = config.ionToken || config.stadiumIonToken;
    const params = new URLSearchParams(location.search);
    const quality = chooseTier(params);
    let settings = settingsFor(quality.tier);
    const container = h("div", { id: "cesium" });
    app.append(container);
    const viewer = new Viewer(container, {
        globe: false,
        skyAtmosphere: new SkyAtmosphere(),
        baseLayerPicker: false,
        geocoder: false,
        homeButton: false,
        sceneModePicker: false,
        navigationHelpButton: false,
        animation: false,
        timeline: false,
        fullscreenButton: false,
        infoBox: false,
        selectionIndicator: false,
        shadows: false,
        msaaSamples: settings.msaaSamples,
    });
    const scene = viewer.scene;
    // Unreal's camera is the only one: no Cesium mouse/touch camera controls.
    const controller = scene.screenSpaceCameraController;
    controller.enableInputs = false;
    scene.backgroundColor = Color.fromCssColorString("#9fb6c8");
    // CesiumSunSky: 1:00 PM EDT on Sunday, October 18, 2026 (ADR-0004, amended).
    viewer.clock.currentTime = JulianDate.fromIso8601("2026-10-18T17:00:00Z");
    viewer.clock.shouldAnimate = false;

    const geo = new Geo(sceneData.georeference);
    const look = new SceneLook(viewer, geo, config.stadiumBaked);
    look.apply(settings);
    console.info(`Quality: ${settings.tier}${quality.fixed ? " (fixed by ?quality=)" : ""}.`);
    // Exported models are keyed by Unreal asset name (the manifest also lists sub-folders, e.g. people/...).
    const byAsset = Object.fromEntries(Object.entries(models).map(([k, v]) => [k.split("/").pop()!, v]));
    const world = new StadiumWorld(scene, geo, sceneData, { ...byAsset, ...models }, modelBounds);
    const timers = new Timers();
    const journey = new Journey(world, timers, audio, goldenPath(), amenities());

    // Callouts project through the Cesium camera; behind the camera there is no position.
    CalloutLayer.projector = (p) => {
        const fixed = geo.worldToFixed(p);
        const toPoint = Cartesian3.subtract(fixed, scene.camera.positionWC, new Cartesian3());
        if (Cartesian3.dot(toPoint, scene.camera.directionWC) <= 1) return null;
        const win = SceneTransforms.worldToWindowCoordinates(scene, fixed);
        return win ? new Vec2(win.x, win.y) : null;
    };

    const touch = matchMedia("(pointer: coarse)").matches || navigator.maxTouchPoints > 1;
    const hud = new Hud(journey, { touch, ticketFrames: ticket.frames, unitsPerPixel });
    const fade = h("div", { class: "camera-fade", style: { background: GameMode.FadeColor } });
    app.append(fade, hud.root);
    const input = new Input(journey, hud, container);

    journey.initialize();
    const tiles = loadTiles(scene, geo, config, sceneData.footprintLonLat, (set, kind) => look.addTileset(set, kind));
    if (!params.has("noCrowd")) void world.loadCrowd();
    let progress = () => 0;
    void tiles.then((t) => {
        progress = t.progress;
        // A deployment fault, not part of the experience: say plainly what is missing so it can be fixed.
        if (t.problems.length > 0) {
            app.append(h("div", { class: "tiles-notice", role: "status" },
                h("strong", { text: "3D stadium or city not loaded" }),
                ...t.problems.map((p) => h("div", { text: p }))));
        }
    }).catch((error) => {
        console.error(error);
        progress = () => 100;
    });

    // Steps down a tier if the device can't keep up once the stadium is in (never back up during the visit).
    const governor = new FrameRateGovernor();
    let last = performance.now();
    scene.preRender.addEventListener(() => {
        const now = performance.now();
        const dt = Math.min((now - last) / 1000, 0.1);
        last = now;
        // While the cover is up the world holds still, so the splash orbit starts under the fade.
        const covered = !config.noLoadingScreen && !loading.isDone();
        const worldDt = covered ? 0 : dt;
        input.tick(worldDt);
        journey.tick(worldDt);
        world.tick(worldDt);
        const rig = journey.rig;
        geo.applyCamera(scene.camera, rig.location, rig.rotation, rig.fieldOfView);
        world.effects.setExposureStops(rig.pulseStops);
        fade.style.opacity = String(rig.fadeAmount);
        fade.style.background = rig.fadeColor;
        hud.tick();
        if (covered) loading.tick(dt, progress());
        if (!quality.fixed && governor.tick(dt, !covered && progress() >= 100 && !rig.isBusy())) {
            const lower = lowerTier(settings.tier);
            if (lower) {
                settings = settingsFor(lower);
                look.apply(settings);
                rememberTier(lower);
                console.info(`Quality: frame rate low, now ${lower}.`);
            }
        }
    });
    // Development hooks for automated checks (Docs/WEB_PARITY.md, verification).
    (window as unknown as { lff: unknown }).lff = { journey, world, viewer, hud, look, settingsFor };
}

void main();
