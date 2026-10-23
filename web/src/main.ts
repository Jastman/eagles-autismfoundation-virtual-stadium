// The web build's entry point: CesiumJS viewer, the level, the journey, the HUD and input, ticked together each frame
// (Unreal's order: input and flight, journey timers and camera rig, scene cues, then the HUD).
import "cesium/Build/Cesium/Widgets/widgets.css";
import "./ui/hud.css";
import { Cartesian3, Color, Ion, SceneTransforms, SkyAtmosphere, Viewer } from "cesium";
import { AudioEngine } from "./audio/audio";
import { GameMode, readDeploymentConfig } from "./config";
import { Input } from "./input/input";
import { amenities } from "./journey/data";
import { GameDayController, easternToUtcMs } from "./journey/gameday";
import type { GameDay } from "./journey/gameday";
import { Journey } from "./journey/journey";
import type { JourneySnapshot } from "./journey/types";
import { isCrowdShown, isReducedMotionEffective } from "./journey/types";
import { Geo } from "./scene/geo";
import { FireworksCue } from "./scene/cues";
import { FloodlightShading } from "./scene/floodShading";
import { PhoneLightShow } from "./scene/lightShow";
import { StadiumDisplays } from "./scene/screens";
import { Floodlights, ThreeLayer, aimBanks } from "./scene/threeLayer";
import { FireworksLayer, PhoneLightsLayer } from "./scene/threeParticles";
import { TimeOfDay, lightBanks } from "./scene/timeOfDay";
import { sceneData } from "./scene/sceneData";
import { SceneLook } from "./scene/look";
import type { UnrealBounds } from "./scene/models";
import type { QualitySettings } from "./scene/quality";
import { FrameRateGovernor, chooseTier, lowerTier, rememberTier, settingsFor, watchForCrashes } from "./scene/quality";
import { loadTiles } from "./scene/tiles";
import { StadiumWorld } from "./scene/world";
import { CalloutLayer } from "./ui/callouts";
import { h } from "./ui/dom";
import { Hud } from "./ui/hud";
import { formFactor, unitsPerPixelFor } from "./ui/layout";
import { LoadingScreen } from "./ui/loading";
import { Vec2 } from "./util/ue";
import { Timers } from "./util/timers";

/** Logical UMG units per CSS pixel: Unreal's ShortestSide rule (1080 = 1.0) on tablets, a legible scale on phones. */
const unitsPerPixel = () => unitsPerPixelFor(window.innerWidth, window.innerHeight);

function applyUiScale(): void {
    // 1rem = 10 logical units (hud.css); the form factor classes pick the phone arrangements in hud.css and hud.ts.
    const root = document.documentElement;
    root.style.fontSize = `${10 / unitsPerPixel()}px`;
    const ff = formFactor(window.innerWidth, window.innerHeight);
    root.classList.toggle("phone", ff.phone);
    root.classList.toggle("portrait", ff.portrait);
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
    watchForCrashes();
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
        // Render errors are recovered from below rather than shown in Cesium's error box.
        showRenderLoopErrors: false,
    });
    const scene = viewer.scene;
    // Unreal's camera is the only one: no Cesium mouse/touch camera controls.
    const controller = scene.screenSpaceCameraController;
    controller.enableInputs = false;
    scene.backgroundColor = Color.fromCssColorString("#9fb6c8");
    // CesiumSunSky's clock: each step's time on the chosen game's date (scene/timeOfDay.ts), not a running clock.
    viewer.clock.shouldAnimate = false;

    const geo = new Geo(sceneData.georeference);
    const look = new SceneLook(viewer, geo, config.stadiumBaked);
    look.apply(settings);
    console.info(`Quality: ${settings.tier}${quality.fixed ? " (fixed by ?quality=)" : ""}` +
        `${quality.afterCrash ? "; the last visit ended unexpectedly" + (quality.fixed ? "" : ", so one tier lower") : ""}.`);

    // A rendering error stops Cesium's render loop and shows its error box. Recover instead: drop a tier and carry on;
    // if errors keep coming, reload at the lowest tier.
    const renderErrors: number[] = [];
    scene.renderError.addEventListener((_scene, error) => {
        console.error("Rendering error; recovering.", error);
        const now = performance.now();
        renderErrors.push(now);
        while (renderErrors.length > 0 && now - renderErrors[0] > 20000) renderErrors.shift();
        if (renderErrors.length >= 3) {
            rememberTier("low");
            location.reload();
            return;
        }
        const lower = lowerTier(settings.tier);
        if (lower && !quality.fixed) {
            settings = settingsFor(lower);
            applyQuality(settings);
            rememberTier(lower);
        }
        viewer.useDefaultRenderLoop = true;
    });
    // The GPU context lost (memory pressure): nothing can be drawn again on this page, so reload a tier lower.
    viewer.canvas.addEventListener("webglcontextlost", (event) => {
        event.preventDefault();
        console.error("WebGL context lost; reloading at lower quality.");
        rememberTier(lowerTier(settings.tier) ?? "low");
        setTimeout(() => location.reload(), 300);
    });
    // Exported models are keyed by Unreal asset name (the manifest also lists sub-folders, e.g. people/...).
    const byAsset = Object.fromEntries(Object.entries(models).map(([k, v]) => [k.split("/").pop()!, v]));
    const world = new StadiumWorld(scene, geo, sceneData, { ...byAsset, ...models }, modelBounds);
    const timers = new Timers();
    // Which home game the tour is for (Settings > Game day): its kickoff template's steps, times and lighting.
    const gameDay = new GameDayController();
    const journey = new Journey(world, timers, audio, gameDay.current.steps, amenities(), undefined, gameDay.current.tourFirstSteps);
    const flood = new FloodlightShading(scene, world.effects.fixedToSite);
    const timeOfDay = new TimeOfDay(viewer, geo, world.effects, flood);
    const displays = new StadiumDisplays(scene, geo);
    // Light and particles with bloom in a three.js layer over Cesium (scene/threeLayer.ts); Cesium-only versions if it
    // can't start (or with ?noThree).
    const three = params.has("noThree") ? null : ThreeLayer.create(app, scene, geo);
    const floodlights = three ? new Floodlights(geo, aimBanks(lightBanks())) : null;
    let lightShow: { set(shown: boolean, animate: boolean): void };
    let cesiumLightShow: PhoneLightShow | null = null;
    if (three && floodlights) {
        const phones = new PhoneLightsLayer();
        void phones.load(geo);
        three.add(floodlights);
        three.add(phones);
        three.add(new FireworksLayer(() => world.cueActors.filter((c): c is FireworksCue => c instanceof FireworksCue)));
        three.setQuality(settings.tier);
        timeOfDay.setBanksShown(false);
        FireworksCue.drawSparks = false;
        lightShow = phones;
    } else {
        cesiumLightShow = new PhoneLightShow(scene, geo);
        void cesiumLightShow.load();
        lightShow = cesiumLightShow;
    }
    const applyQuality = (next: QualitySettings) => {
        look.apply(next);
        three?.setQuality(next.tier);
    };
    const showGame = (day: GameDay) => displays.setGame({
        opponentShort: day.game.OpponentShort, opponentCode: day.game.OpponentCode, kickoffLabel: day.template.Label,
        weekday: day.game.Weekday, broadcast: day.game.Broadcast.split(",")[0],
    });
    showGame(gameDay.current);
    // The scene follows the tour: each step's time of day, the boards' pre-game or game content, the light show.
    const followTour = (s: JourneySnapshot, fade: boolean) => {
        const day = gameDay.current;
        // The welcome screen and Explore show the stadium at kickoff; tour steps at their own times.
        const minutes = s.bStarted && s.Mode === "GuidedTour" ? day.sceneMinutes[s.StepIndex] ?? day.kickoffMinutes : day.kickoffMinutes;
        const calm = isReducedMotionEffective(s.Settings);
        timeOfDay.setTime(easternToUtcMs(day.game.Date, minutes), fade ? (calm ? "reduced" : "fade") : "instant");
        displays.setGameOn(minutes >= day.kickoffMinutes);
        displays.setStill(calm);
        lightShow.set(s.bStarted && s.Mode === "GuidedTour" && s.StepId === "light-show" && isCrowdShown(s.Settings), !calm);
    };
    journey.onChanged((s) => followTour(s, true));
    gameDay.onChanged((day) => {
        showGame(day);
        journey.setSteps(day.steps, day.tourFirstSteps);
    });

    // Callouts project through the Cesium camera; behind the camera there is no position.
    CalloutLayer.projector = (p) => {
        const fixed = geo.worldToFixed(p);
        const toPoint = Cartesian3.subtract(fixed, scene.camera.positionWC, new Cartesian3());
        if (Cartesian3.dot(toPoint, scene.camera.directionWC) <= 1) return null;
        const win = SceneTransforms.worldToWindowCoordinates(scene, fixed);
        return win ? new Vec2(win.x, win.y) : null;
    };

    const touch = matchMedia("(pointer: coarse)").matches || navigator.maxTouchPoints > 1;
    const hud = new Hud(journey, { touch, ticketFrames: ticket.frames, unitsPerPixel, gameDay });
    const fade = h("div", { class: "camera-fade", style: { background: GameMode.FadeColor } });
    app.append(fade, hud.root);
    const input = new Input(journey, hud, container);

    journey.initialize();
    followTour(journey.snapshot, false);
    const tiles = loadTiles(scene, geo, config, sceneData.footprintLonLat, (set, role) => {
        look.addTileset(set, role);
        // The stadium model lit by its floodlights at night (the photographed city keeps the post-process relight).
        if (role !== "city") flood.apply(set);
    });
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
        // Scenery only: a fault here must never stop the render loop.
        for (const scenery of [timeOfDay, displays, cesiumLightShow]) {
            try {
                scenery?.tick(dt);
            } catch (error) {
                console.error("Scenery update failed; carrying on.", error);
            }
        }
        floodlights?.setLevel(timeOfDay.glow);
        look.setNight(timeOfDay.night, timeOfDay.golden);
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
                applyQuality(settings);
                rememberTier(lower);
                console.info(`Quality: frame rate low, now ${lower}.`);
            }
        }
    });
    // Development hooks for automated checks (Docs/WEB_PARITY.md, verification).
    (window as unknown as { lff: unknown }).lff = { journey, world, viewer, hud, look, settingsFor, gameDay, timeOfDay, displays, lightShow, three, floodlights,
        V2: (x: number, y: number) => new Vec2(x, y) };
}

void main();
