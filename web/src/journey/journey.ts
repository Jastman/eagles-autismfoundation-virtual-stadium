// Port of ULFFJourneySubsystem (Source/VirtualStadium/Private/Journey/LFFJourneySubsystem.cpp): the guided journey for
// one scene. State machine, visitor settings, camera rig (splash orbit included), labels, audio (beds, previews, voice),
// scene cues and the optional intensity previews. Input and UI only call commands here and read the snapshot.
//
// Method names and order follow the C++ so a change on either side is easy to carry across (Docs/WEB_PARITY.md).
import { AudioEngine, SoundHandle } from "../audio/audio";
import { GameMode } from "../config";
import { CameraRig } from "../scene/cameraRig";
import {
    Box2, Rotator, Transform, Vec2, Vec3, clamp, deg2rad, getMappedRangeValueClamped, rad2deg, unwindDegrees, vInterpTo,
} from "../util/ue";
import { TimerHandle, Timers } from "../util/timers";
import { soundName } from "./data";
import * as Rules from "./rules";
import { JourneyStateMachine } from "./stateMachine";
import type {
    Amenity, AmenityMarker, IntensityPreview, JourneySnapshot, JourneyStep, MapLevel, SimulationMode, TimelineMark, TransitionProfile,
    VisitorSettings,
} from "./types";
import {
    AMENITIES, DEFAULT_AMENITY_MASK, MAP_LEVELS, NONE, amenityBit, isCrowdShown, isReducedMotionEffective, isSoundEffective,
    isVoiceEffective,
} from "./types";
import type { CameraPath, CueActor, JourneyWorld } from "./world";

type CueReason = "StepChanged" | "Arrived" | "SettingsChanged";

/** The bed dips under the preview sample so the sample stays the only new sound. */
const BED_DUCK_WHILE_PREVIEW = 0.35;
/** Camera "size" for the wall check, so a move can't graze a surface either. */
const CAMERA_PROBE_RADIUS = 40;
/** Free explore's own camera moves use an id the journey never issues, so their finish is ignored. */
const EXPLORE_MOVE_ID = 0;
const LOOK_RESPONSE_SECONDS = 0.045;
const EXTERIOR_CHECK_SECONDS = 0.2;

export const MY_SEAT_AMENITY_ID = "my-seat";

/** Where world lands in the frame seen from view (hfov degrees). Null if it's behind the camera. */
function projectToView(view: Transform, horizontalFov: number, aspect: number, world: Vec3): Vec2 | null {
    const local = view.inverseTransformPositionNoScale(world);
    if (local.x <= 1) return null;
    const tanHalf = Math.tan(deg2rad(horizontalFov * 0.5));
    const ndcX = local.y / (local.x * tanHalf);
    const ndcY = local.z / ((local.x * tanHalf) / Math.max(aspect, 0.1));
    return new Vec2(0.5 + 0.5 * ndcX, 0.5 - 0.5 * ndcY);
}

export class Journey {
    readonly machine = new JourneyStateMachine();
    settings: VisitorSettings = { ...GameMode.DefaultVisitorSettings };
    snapshot!: JourneySnapshot;

    private listeners: ((s: JourneySnapshot) => void)[] = [];
    private voiceWordListeners: ((index: number) => void)[] = [];

    private settingsOpen = false;
    private restartConfirmOpen = false;
    private targetMissing = false;
    private journeyMissing = false;
    private previewOn = false;
    private mapLevel: MapLevel = "LowerConcourse";
    private amenityMask = DEFAULT_AMENITY_MASK;
    private selectedAmenityId = "";
    private mapOpened = false;
    private levelHighlight = false;
    private amenities: AmenityMarker[] = [];
    private mapViewportSize = new Vec2();
    private mapSafeRect = Box2.invalid();
    private mapStemHeight = 2500;
    private mapAutoFramed = false;
    private loggedFramingUnavailable = false;
    private flyVelocity = new Vec3();
    private pendingLookDegrees = new Vec2();
    private lastLookRotation = new Rotator();
    private lookSmoothingActive = false;
    private flyDollyLeft = 0;

    private viewTargetId = "";
    private arrived = false;
    private voicePlaying = false;
    private voiceWordTimes: number[] = [];
    private voiceWordIndex = NONE;
    private previewCuesPlaying = false;
    private anthemSequence = false;
    private anthemEagle: CueActor | null = null;
    private anthemFlightLeft = 0;
    private exteriorHidden = false;
    private firstCueIndex = new Map<string, number>();
    private timelineMarks: TimelineMark[] = [];
    private roomCrowdGain = 1;
    private appliedBedVolume = -1;
    private fadingBedBaseVolume = 1;
    private appliedAnthemVolume = -1;

    private bedAudio: SoundHandle | null = null;
    private fadingBedAudio: SoundHandle | null = null;
    private previewAudio: SoundHandle | null = null;
    private voiceAudio: SoundHandle | null = null;
    private anthemAudio: SoundHandle | null = null;
    private uiClickAudio: SoundHandle | null = null;
    /** A sound requested before it finished decoding; applyAudio runs again once it has. */
    private pendingAudio = new Set<string>();

    private readonly voiceTimer = new TimerHandle();
    private readonly previewCueTimer = new TimerHandle();
    private readonly exteriorTimer = new TimerHandle();
    private readonly anthemTimer = new TimerHandle();
    private readonly anthemAudioStartTimer = new TimerHandle();
    private readonly anthemAudioEndTimer = new TimerHandle();
    private readonly anthemAudioWatchTimer = new TimerHandle();

    readonly rig: CameraRig;

    constructor(
        private readonly world: JourneyWorld,
        readonly timers: Timers,
        private readonly audio: AudioEngine,
        private readonly steps: JourneyStep[],
        amenityRows: AmenityMarker[],
        private readonly openUrl: (url: string) => void = (url) => window.open(url, "_blank", "noopener"),
    ) {
        this.rig = new CameraRig(timers, (id) => this.handleRigTransitionFinished(id));
        this.rig.fadeColor = GameMode.FadeColor;
        this.amenities = amenityRows;
    }

    onChanged(listener: (s: JourneySnapshot) => void): void {
        this.listeners.push(listener);
    }

    onVoiceWord(listener: (index: number) => void): void {
        this.voiceWordListeners.push(listener);
    }

    // ---- InitializeJourney ----

    initialize(): void {
        this.settings = { ...GameMode.DefaultVisitorSettings };
        this.journeyMissing = this.steps.length === 0;
        this.machine.load(this.steps);
        this.machine.setTourSteps(GameMode.TourStepFirstSteps);
        this.resetLookInput();
        this.loadAmenities();

        this.timelineMarks = [];
        this.firstCueIndex.clear();
        this.steps.forEach((step, index) => {
            if (step.TimelineLabel !== "") this.timelineMarks.push({ StepIndex: index, Label: step.TimelineLabel });
            for (const cue of step.SceneCues) {
                if (!this.firstCueIndex.has(cue)) this.firstCueIndex.set(cue, index);
            }
        });
        for (const [cue, index] of this.firstCueIndex) {
            const count = this.world.cueActors.filter((a) => a.cueId === cue).length;
            console.info(`Scene cue ${cue}: step ${index + 1}, ${count} actors.`);
            if (count === 0) console.warn(`Scene cue ${cue} has no cue actor in the level; that step shows no movement.`);
        }

        this.timers.setTimer(this.exteriorTimer, () => {
            this.updateExteriorVisibility();
            this.updateRoomAudio();
        }, EXTERIOR_CHECK_SECONDS, true);

        // The splash starts on the first step's wide shot and slowly orbits from there (still with less movement).
        this.snapCameraToCurrentStep();
        this.applyLabels();
        this.applySensory();
        this.applyCues("StepChanged");
        this.enterSplash(false);
        this.prefetchVoice();
        this.broadcast();
    }

    private findTarget(id: string) {
        return id ? this.world.findTarget(id) : undefined;
    }

    // ---- Commands ----

    /** Intro: start. Last step: back to the intro. Otherwise: next step. Ignored while a question is open. */
    next(): void {
        if (this.isModalOpen() || this.isExploring()) return;
        if (this.machine.getPhase() === "Intro") {
            if (this.machine.start() === "Accepted") this.moveCameraToFirstStep();
            this.broadcast();
            return;
        }
        if (this.machine.isAtEnd() && this.machine.getPhase() === "AtStep" && !this.machine.isPaused()) {
            // "Start again" at the end is an explicit choice, so it needs no extra question.
            this.confirmRestart();
            return;
        }
        if (this.machine.next() === "Accepted") this.moveCameraToCurrentStep(false);
        this.broadcast();
    }

    /** Previous step. On the first step, back to the splash. */
    back(): void {
        if (this.isModalOpen() || this.isExploring()) return;
        const step = this.machine.getCurrentStep();
        if (Rules.shouldBackReturnToIntro(this.machine.hasStarted(), this.machine.getIndex(), this.machine.isPaused(), step?.bBackEnabled ?? false)) {
            // The first step is the orbit's wide shot, so the splash orbit picks up from right here.
            this.machine.restart();
            this.arrived = false;
            this.resetPreview();
            this.applyCues("StepChanged");
            this.enterSplash(false);
            this.broadcast();
            return;
        }
        if (this.machine.back() === "Accepted") this.moveCameraToCurrentStep(false);
        this.broadcast();
    }

    togglePause(): void {
        if (this.isModalOpen()) return;
        if (this.machine.isPaused()) {
            this.resume();
            return;
        }
        if (this.machine.pause() === "Accepted") this.rig.setTransitionPaused(true);
        // "Nothing will move until you resume": a running preview, the anthem flight and the voice stop too.
        this.stopVoice();
        this.stopActivePreview();
        this.broadcast();
    }

    resume(): void {
        const wasPaused = this.machine.isPaused();
        this.machine.resume();
        this.rig.setTransitionPaused(false);
        // The card is read again from the start.
        if (wasPaused) this.scheduleVoice();
        this.applySensory();
        this.broadcast();
    }

    /** Asks "Start again?" rather than restarting straight away. From Settings, the question replaces the panel. */
    requestRestart(): void {
        if (!this.machine.hasStarted()) return;
        this.stopActivePreview();
        this.settingsOpen = false;
        this.restartConfirmOpen = true;
        this.broadcast();
    }

    confirmRestart(): void {
        this.restartConfirmOpen = false;
        this.settingsOpen = false;
        this.stopVoice();
        this.resetMap();
        if (this.machine.restart() === "Accepted") {
            this.rig.setTransitionPaused(false);
            // Back to the splash with a gentle fade; the orbit starts once the picture is back.
            this.arrived = false;
            this.resetPreview();
            this.applyLabels();
            this.applySensory();
            this.applyCues("StepChanged");
            this.enterSplash(true);
        }
        // Visitor settings are kept on restart so nobody loses reduced motion or Calm Mode mid-session.
        this.broadcast();
    }

    /** Answers "yes" to the open question; with none open, same as Next. */
    confirm(): void {
        if (this.restartConfirmOpen) {
            this.confirmRestart();
            return;
        }
        if (this.settingsOpen) return;
        if (this.machine.isPaused()) {
            this.resume();
            return;
        }
        this.next();
    }

    /** Closes the open question or settings; otherwise resumes if paused. */
    cancel(): void {
        this.stopActivePreview();
        if (this.restartConfirmOpen) {
            this.restartConfirmOpen = false;
            this.applySensory();
            this.scheduleVoice();
            this.broadcast();
            return;
        }
        if (this.settingsOpen) {
            this.settingsOpen = false;
            this.applySensory();
            this.scheduleVoice();
            this.broadcast();
            return;
        }
        if (this.machine.isPaused()) this.resume();
    }

    toggleSettings(): void {
        if (this.restartConfirmOpen) return;
        this.settingsOpen = !this.settingsOpen;
        if (this.settingsOpen) {
            this.stopActivePreview();
            this.stopVoice();
        } else {
            this.applySensory();
            this.scheduleVoice();
        }
        this.broadcast();
    }

    /** Ignored while Calm Mode is on (Calm Mode already means less movement). */
    toggleReducedMotion(): void {
        if (this.settings.bCalmMode) return;
        this.settings.bReducedMotion = !this.settings.bReducedMotion;
        this.applyCrowd();
        this.applyCues("SettingsChanged");
        if (!this.machine.hasStarted()) this.enterSplash(false);
        this.broadcast();
    }

    /** Off mutes every sound, UI included. On restores ambience and enabled narration, not a cancelled anthem. */
    toggleSound(): void {
        if (this.settings.bCalmMode) return;
        this.settings.bSoundEnabled = !this.settings.bSoundEnabled;
        // Otherwise the sample would start again by itself when Sound comes back on.
        if (!this.settings.bSoundEnabled && this.getCurrentPreviewKind() === "Sound") this.previewOn = false;
        this.applySensory();
        if (isSoundEffective(this.settings)) this.scheduleVoice();
        this.broadcast();
    }

    /** On: less movement, sound off, crowd hidden, previews locked off, camera cuts only. */
    toggleCalmMode(): void {
        // The underlying Sound and Less movement choices are kept, so turning Calm Mode off restores them.
        this.settings.bCalmMode = !this.settings.bCalmMode;
        if (this.settings.bCalmMode) this.previewOn = false;
        this.applySensory();
        this.applyCues("SettingsChanged");
        if (!this.machine.hasStarted()) this.enterSplash(false);
        else if (!this.settings.bCalmMode) this.scheduleVoice();
        this.broadcast();
    }

    /** Standard | Calm. Standard turns the full day on: sound, voice, movement, crowd. */
    setExperienceMode(calm: boolean): void {
        if (!calm) {
            this.settings.bSoundEnabled = true;
            this.settings.bVoiceEnabled = true;
            this.settings.bReducedMotion = false;
            this.settings.bCrowdEnabled = true;
        }
        if (this.settings.bCalmMode !== calm) {
            this.toggleCalmMode();
            return;
        }
        this.applySensory();
        this.applyCues("SettingsChanged");
        if (this.machine.hasStarted() && !calm) this.scheduleVoice();
        this.broadcast();
    }

    /** A timeline mark or chapter chip: straight to that step. In Free explore it resumes the tour there. */
    jumpToStep(stepIndex: number): void {
        if (this.isModalOpen()) return;
        if (this.isExploring()) {
            // A chapter chip in Free explore goes back to the tour. The chapter you left resumes at your exact step.
            const ownChapter = this.machine.tourStepOf(stepIndex) === this.machine.getBookmark().TourStep;
            this.resumeTourAt(ownChapter ? NONE : stepIndex);
            return;
        }
        if (this.machine.jumpTo(stepIndex) === "Accepted") {
            console.info(`Timeline: jump to step ${stepIndex + 1}.`);
            this.moveCameraToCurrentStep(false);
        }
        this.broadcast();
    }

    /** The Guided tour / Explore switch (keys T and E). */
    switchToMode(newMode: SimulationMode): void {
        if (this.isModalOpen()) return;
        if (newMode === "GuidedTour") {
            if (this.isExploring()) this.resumeTour();
            // Guided tour on the welcome screen is Start.
            else if (this.machine.getPhase() === "Intro") this.next();
            return;
        }
        if (this.machine.switchToMode(newMode) !== "Accepted") return;

        // Take control exactly here; retire the tour's travel and arrival callbacks rather than continuing an old sweep.
        this.stopVoice();
        this.stopAnthemSequence(true);
        this.resetPreview();
        this.rig.holdPosition();
        if (!this.mapOpened) {
            this.mapLevel = GameMode.MySeatLevel;
            this.mapOpened = true;
        }
        this.levelHighlight = false;
        this.flyVelocity = new Vec3();
        this.resetLookInput();
        this.flyDollyLeft = 0;
        this.mapAutoFramed = false;
        this.applyLevelHighlight();
        this.applySensory();
        const step = this.machine.getCurrentStep();
        if (step) this.viewTargetId = step.CameraTargetId;
        const bookmark = this.machine.getBookmark();
        console.info(`Free explore: tour bookmarked at step ${bookmark.StepIndex + 1} (${bookmark.TourStep})${bookmark.bTourStarted ? "" : ", not started yet"}.`);
        this.broadcast();
    }

    /** Free explore -> the bookmarked step, with a fade-cut (a cut in Calm Mode); the card is read again on arrival. */
    resumeTour(): void {
        this.resumeTourAt(NONE);
    }

    private resumeTourAt(stepIndex: number): void {
        if (this.isModalOpen() || this.machine.resumeTour(stepIndex) !== "Accepted") return;
        this.levelHighlight = false;
        this.flyVelocity = new Vec3();
        this.applyLevelHighlight();
        this.rig.stopOrbit();
        // Always a fade-cut: Free explore may have left the camera anywhere. Calm Mode makes it a cut.
        console.info(`Resume tour at step ${this.machine.getIndex() + 1} (${this.machine.getTourStep()}), fade-cut.`);
        this.moveCameraToCurrentStep(true);
        this.broadcast();
    }

    /** Free explore (key H): the map turns to the seat's level and the camera fade-cuts to the seat's view. */
    goToMySeat(): void {
        if (!this.canUseMap()) return;
        this.mapLevel = GameMode.MySeatLevel;
        this.showTarget(GameMode.MySeatTargetId);
        this.broadcast();
    }

    /** Free explore's Show me: the camera fade-cuts to a camera target (a cut in Calm Mode). */
    showTarget(cameraTargetId: string): void {
        if (!this.canUseMap()) return;
        const target = this.findTarget(cameraTargetId);
        if (!target) {
            console.warn(`Show me: camera target '${cameraTargetId}' not found. The camera stays put.`);
            return;
        }
        this.mapAutoFramed = false;
        this.rig.stopOrbit();
        this.flyVelocity = new Vec3();
        this.flyDollyLeft = 0;
        const profile = Rules.resolveMove("ReducedMotion", this.settings, false);
        this.rig.beginTransition(target.transform, target.fov, profile, GameMode.DefaultEaseSeconds, GameMode.ReducedMotionFadeSeconds,
            GameMode.FadeColor, EXPLORE_MOVE_ID);
        this.viewTargetId = cameraTargetId;
        this.broadcast();
    }

    /** Free explore's level buttons: picks the level, highlights its band and fade-cuts to an oblique overview. */
    setMapLevel(level: MapLevel): void {
        if (!this.canUseMap()) return;
        this.mapLevel = level;
        this.levelHighlight = true;
        this.applyLevelHighlight();
        this.flyToLevel(level);
        this.broadcast();
    }

    private flyToLevel(level: MapLevel): void {
        this.mapAutoFramed = false;
        const view = GameMode.LevelViews[MAP_LEVELS.indexOf(level)];
        if (!view) return;
        const world = Rules.levelOverviewPose(view).mul(this.world.siteFrame);
        const look = world.rotator();
        look.roll = 0;
        this.rig.stopOrbit();
        this.flyVelocity = new Vec3();
        this.flyDollyLeft = 0;
        // A brief fade-cut avoids a slow roof sweep; Calm Mode cuts without a fade.
        const profile = Rules.resolveMove("ReducedMotion", this.settings, false);
        console.info(`Level ${level}: ${profile} to its overview.`);
        this.rig.beginTransition(Transform.fromRotator(look, world.translation), 60, profile, 2.5, GameMode.ReducedMotionFadeSeconds,
            GameMode.FadeColor, EXPLORE_MOVE_ID);
        this.viewTargetId = "";
    }

    private applyLevelHighlight(): void {
        const view = GameMode.LevelViews[MAP_LEVELS.indexOf(this.mapLevel)];
        const on = this.isExploring() && this.levelHighlight && view !== undefined;
        this.world.setLevelHighlight(on ? { minZ: view.MinZ, maxZ: view.MaxZ } : null);
    }

    /**
     * Camera input, once a frame. move is (right, forward) -1..1, rise up/down, lookDelta the drag in pixels, dolly wheel
     * notches (or pinch). Waits while a move or fade is running; eases into and out of moving (at once with less
     * movement). Stays in the site's flyable space. Tour steps accept look only, once their camera has arrived.
     */
    flyTick(move: Vec2, rise: number, lookDelta: Vec2, dolly: number, fast: boolean, dt: number): void {
        const rig = this.rig;
        if (this.canLookAround() && dt > 0) {
            const current = rig.rotation.clone();
            if (!this.lookSmoothingActive || !current.equals(this.lastLookRotation, 0.001)) {
                this.resetLookInput();
                this.lookSmoothingActive = true;
            }
            this.pendingLookDegrees = this.pendingLookDegrees.add(lookDelta.scale(GameMode.FlyLookDegreesPerPixel));
            if (!this.pendingLookDegrees.isNearlyZero()) {
                this.pendingLookDegrees.y = clamp(current.pitch + this.pendingLookDegrees.y, -85, 70) - current.pitch;
            }
            // Exponential decay preserves total travel across uneven frames without the raw sample's abrupt jump.
            const alpha = 1 - Math.exp(-dt / LOOK_RESPONSE_SECONDS);
            const applied = this.pendingLookDegrees.scale(alpha);
            this.pendingLookDegrees = this.pendingLookDegrees.sub(applied);
            const look = new Rotator(current.pitch + applied.y, unwindDegrees(current.yaw + applied.x), 0);
            if (!applied.isNearlyZero(0.00001) && !look.equals(current, 0.00001)) rig.rotation = look;
            this.lastLookRotation = rig.rotation.clone();
        } else {
            this.resetLookInput();
        }
        if (!move.isNearlyZero() || !lookDelta.isNearlyZero() || Math.abs(rise) > 1e-4 || Math.abs(dolly) > 1e-4) this.mapAutoFramed = false;

        if (!this.canFly() || rig.isBusy() || dt <= 0) {
            // Nothing carries over: after a move or a modal, flight starts from rest.
            this.flyVelocity = new Vec3();
            this.flyDollyLeft = 0;
            return;
        }
        const look = rig.rotation.clone();
        look.roll = 0;
        // Forward follows where you look; sideways stays level; up and down follow the site.
        const quat = look.quaternion();
        const forward = look.vector();
        const right = quat.right();
        const up = this.world.siteFrame.unitAxis("Z");
        const gentle = isReducedMotionEffective(this.settings);
        const speed = GameMode.FlySpeed * (fast ? GameMode.FlyFastMultiplier : 1) * (gentle ? 0.6 : 1);
        const desired = forward.scale(move.y).add(right.scale(move.x)).add(up.scale(rise)).clampedToMaxSize(1).scale(speed);
        // Less movement and Calm Mode start and stop at once: no drifting after you let go.
        const ease = GameMode.FlyEaseSeconds;
        this.flyVelocity = gentle || ease <= 0 ? desired : vInterpTo(this.flyVelocity, desired, dt, 3 / ease);

        this.flyDollyLeft += dolly * GameMode.FlyWheelStep;
        const dollyStep = gentle ? this.flyDollyLeft : this.flyDollyLeft * Math.min(dt * 8, 1);
        this.flyDollyLeft -= dollyStep;

        const from = rig.location.clone();
        let to = from.add(this.flyVelocity.scale(dt)).add(forward.scale(dollyStep));
        if (!to.equals(from, 0.01)) {
            // Authored tours/overview fits may sit outside the free-flight envelope; never pull them in on an idle tick.
            const site = this.world.siteFrame;
            const siteFrom = site.inverseTransformPosition(from);
            to = site.transformPosition(Rules.clampFlyPosition(site.inverseTransformPosition(to),
                Math.max(GameMode.FlyBoundsRadius, siteFrom.size2D()), Math.min(GameMode.FlyMinZ, siteFrom.z), Math.max(GameMode.FlyMaxZ, siteFrom.z)));
        }
        if (!to.equals(from, 0.01) || !look.equals(rig.rotation, 0.001)) {
            rig.location = to;
            rig.rotation = look;
        }
    }

    canLookAround(): boolean {
        return !this.rig.isBusy() && !this.isModalOpen() && !this.machine.isPaused()
            && (this.isExploring() || (this.arrived && this.machine.getPhase() === "AtStep"));
    }

    resetLookInput(): void {
        this.pendingLookDegrees = new Vec2();
        this.lookSmoothingActive = false;
    }

    toggleAmenity(amenity: Amenity): void {
        if (!this.canUseMap()) return;
        this.amenityMask ^= amenityBit(amenity);
        // Turning a kind on shows its places on every level, so the camera makes sure they're all in view.
        if ((this.amenityMask & amenityBit(amenity)) !== 0) this.framePlacesShown();
        this.broadcast();
    }

    private framePlacesShown(): void {
        const points: Vec3[] = [];
        for (const amenity of this.amenities) {
            if ((this.amenityMask & amenityBit(amenity.Category)) !== 0) {
                const p = Vec3.from(amenity.Location);
                points.push(p, p.add(new Vec3(0, 0, this.mapStemHeight)));
            }
        }
        const seat = this.getAmenity(MY_SEAT_AMENITY_ID);
        if (seat) {
            const p = Vec3.from(seat.Location);
            points.push(p, p.add(new Vec3(0, 0, this.mapStemHeight)));
        }
        this.frameMapPoints(points);
        this.mapAutoFramed = true;
    }

    /** Actual viewport and opaque corner-panel geometry, supplied by the HUD after layout. */
    setMapViewportFrame(size: Vec2, safeRect: Box2, stemHeight: number): void {
        const changed = Math.abs(size.x - this.mapViewportSize.x) > 1 || Math.abs(size.y - this.mapViewportSize.y) > 1
            || !safeRect.equals(this.mapSafeRect) || Math.abs(stemHeight - this.mapStemHeight) > 1e-4;
        this.mapViewportSize = size;
        this.mapSafeRect = safeRect;
        this.mapStemHeight = stemHeight;
        if (changed && this.mapAutoFramed && this.canUseMap()) this.framePlacesShown();
    }

    /** Uses the authored interior view when present, otherwise frames this mapped place. Never fabricates an interior. */
    focusAmenity(amenityId: string): void {
        if (!this.canUseMap()) return;
        const place = this.getAmenity(amenityId);
        if (!place) {
            console.warn(`Focus place: '${amenityId}' is not mapped.`);
            return;
        }
        if (place.CameraTargetId !== "") {
            this.showTarget(place.CameraTargetId);
            return;
        }
        const p = Vec3.from(place.Location);
        this.frameMapPoints([p, p.add(new Vec3(0, 0, this.mapStemHeight))]);
        this.mapAutoFramed = false;
    }

    private frameMapPoints(points: Vec3[]): void {
        if (points.length === 0) return;
        if (!this.mapSafeRect.valid || this.mapViewportSize.x <= 1 || this.mapViewportSize.y <= 1) {
            if (!this.loggedFramingUnavailable) {
                console.warn("Places: no unobstructed HUD frame yet; expand the window or close a drawer.");
                this.loggedFramingUnavailable = true;
            }
            return;
        }
        this.loggedFramingUnavailable = false;
        const viewportAspect = this.mapViewportSize.x / this.mapViewportSize.y;
        // The rig preserves the authored vertical frame (60 deg horizontal at 16:9); fitting needs the viewport's horizontal FOV.
        const horizontalFov = rad2deg(2 * Math.atan(Math.tan(deg2rad(30)) * viewportAspect / (16 / 9)));
        const view = GameMode.LevelViews[MAP_LEVELS.indexOf(this.mapLevel)] ?? GameMode.LevelViews[1];
        const local = Rules.framePointsPose(points, GameMode.MapFramePitchDegrees, view.ViewYawDegrees, horizontalFov, viewportAspect,
            this.mapSafeRect, 400, 6000);
        const world = local.mul(this.world.siteFrame);
        const look = world.rotator();
        look.roll = 0;
        this.rig.stopOrbit();
        this.flyVelocity = new Vec3();
        this.flyDollyLeft = 0;
        const profile = Rules.resolveMove("ReducedMotion", this.settings, false);
        console.info(`Places: framing ${points.length} anchors within ${this.mapViewportSize.x.toFixed(0)}x${this.mapViewportSize.y.toFixed(0)} (${profile}).`);
        this.rig.beginTransition(Transform.fromRotator(look, world.translation), 60, profile, 2.5, GameMode.ReducedMotionFadeSeconds,
            GameMode.FadeColor, EXPLORE_MOVE_ID);
        this.viewTargetId = "";
    }

    hideAllAmenities(): void {
        if (!this.canUseMap() || this.amenityMask === 0) return;
        this.amenityMask = 0;
        this.broadcast();
    }

    /** Free explore: picks a marker (its card shows Show me). Empty clears the pick. */
    selectAmenity(amenityId: string): void {
        if (!this.canUseMap() || amenityId === this.selectedAmenityId) return;
        this.selectedAmenityId = amenityId;
        this.broadcast();
    }

    private resetMap(): void {
        this.levelHighlight = false;
        this.flyVelocity = new Vec3();
        this.flyDollyLeft = 0;
        this.mapAutoFramed = false;
        // The next visitor gets the map as it starts: no picks, the default markers, opening on the seat's level.
        this.mapLevel = GameMode.MySeatLevel;
        this.amenityMask = DEFAULT_AMENITY_MASK;
        this.selectedAmenityId = "";
        this.mapOpened = false;
        this.viewTargetId = "";
        this.applyLevelHighlight();
    }

    private loadAmenities(): void {
        // A place marked by an actor in the level stands where that actor is now.
        let placed = 0;
        for (const amenity of this.amenities) {
            if (amenity.LocationActorTag === "") continue;
            const location = this.world.placeActorLocation(amenity.LocationActorTag);
            if (!location) {
                console.warn(`Places: no actor tagged ${amenity.LocationActorTag}; ${amenity.AmenityId} keeps its data location.`);
                continue;
            }
            const site = this.world.siteFrame.inverseTransformPosition(location);
            amenity.Location = { X: site.x, Y: site.y, Z: site.z };
            ++placed;
        }
        console.info(`Places: ${this.amenities.length} (${placed} at their actors).`);
    }

    getAmenities(): readonly AmenityMarker[] {
        return this.amenities;
    }

    /** A place by id, the seat included. Undefined if there's no such place. */
    getAmenity(amenityId: string): AmenityMarker | undefined {
        if (amenityId === MY_SEAT_AMENITY_ID) {
            const seat = this.findTarget(GameMode.MySeatTargetId);
            if (!seat) return undefined;
            const site = this.world.siteFrame.inverseTransformPosition(seat.transform.translation);
            return {
                AmenityId: MY_SEAT_AMENITY_ID, Label: GameMode.MySeatLabel, Note: GameMode.MySeatNote, Category: "GuestServices",
                Level: GameMode.MySeatLevel, Location: { X: site.x, Y: site.y, Z: site.z }, LocationActorTag: "",
                CameraTargetId: GameMode.MySeatTargetId, VerificationSource: "",
            };
        }
        return amenityId === "" ? undefined : this.amenities.find((a) => a.AmenityId === amenityId);
    }

    getSiteFrame(): Transform {
        return this.world.siteFrame;
    }

    /** Voice reads each card (Title, then Body). Needs Sound on; ignored while Sound is off or Calm Mode is on. */
    toggleVoice(): void {
        if (!isSoundEffective(this.settings)) return;
        this.settings.bVoiceEnabled = !this.settings.bVoiceEnabled;
        console.info(`Voice ${this.settings.bVoiceEnabled ? "on" : "off"}.`);
        if (this.settings.bVoiceEnabled) this.scheduleVoice();
        else this.stopVoice();
        this.broadcast();
    }

    /** The voice button on each card: Voice off, or Voice on (turning Sound on with it if needed). Nothing in Calm Mode. */
    toggleVoiceFromCard(): void {
        if (this.settings.bCalmMode) return;
        if (isVoiceEffective(this.settings)) {
            this.toggleVoice();
            return;
        }
        if (!isSoundEffective(this.settings)) this.toggleSound();
        if (!this.settings.bVoiceEnabled) this.toggleVoice();
    }

    getVoiceWordIndex(): number {
        return this.voiceWordIndex;
    }

    /** Crowd: Low or Off. Ignored while Calm Mode is on (it hides them anyway). */
    toggleCrowd(): void {
        if (this.settings.bCalmMode) return;
        this.settings.bCrowdEnabled = !this.settings.bCrowdEnabled;
        this.applyCrowd();
        this.broadcast();
    }

    /** "Learn More" on the last step: opens the Eagles Autism Foundation page. */
    openLearnMore(): void {
        if (!GameMode.LearnMoreUrl) return;
        this.stopVoice();
        console.info(`Learn More: opening ${GameMode.LearnMoreUrl}`);
        this.openUrl(GameMode.LearnMoreUrl);
        this.broadcast();
    }

    /** "Preview this? Off / Soft" on a loud or bright step. Ignored elsewhere, while paused or in Calm Mode. */
    togglePreview(): void {
        if (this.isModalOpen() || this.machine.isPaused() || !this.machine.hasStarted()
            || !Rules.shouldShowPreviewControls(this.getCurrentPreviewKind(), this.settings)) return;
        this.previewOn = !this.previewOn;
        // A preview never starts under the voice.
        if (this.previewOn) this.stopVoice();
        this.applySensory();
        this.broadcast();
    }

    toggleLargeText(): void {
        this.settings.bLargeText = !this.settings.bLargeText;
        this.broadcast();
    }

    toggleHighContrast(): void {
        this.settings.bHighContrast = !this.settings.bHighContrast;
        this.broadcast();
    }

    /** Button feedback only. Sound off / Calm Mode are silent; repeated presses never stack click sounds. */
    playUIClick(): void {
        if (!isSoundEffective(this.settings)) return;
        const name = GameMode.UIClickSound;
        if (!this.audio.ready(name)) {
            void this.audio.preload(name);
            return;
        }
        this.uiClickAudio?.stop();
        this.uiClickAudio = this.audio.create(name);
        this.uiClickAudio?.play(clamp(GameMode.UIClickVolume, 0, 1));
    }

    // ---- Camera ----

    /** Called by the camera rig. Stale ids are ignored by the state machine. */
    private handleRigTransitionFinished(transitionId: number): void {
        // The rig only ever reports its latest move, so a current id is the camera arriving where the journey is.
        const current = !this.isExploring() && transitionId === this.machine.getTransitionId();
        const finished = this.machine.notifyTransitionFinished(transitionId);
        if (current) {
            if (this.machine.hasStarted() && !this.arrived) this.onArrived();
            // The restart fade is over: the splash orbit can begin.
            else this.startSplashOrbit();
        }
        if (finished || current) this.broadcast();
    }

    /** Moves the camera to the current step. forceFade is used for restart and Resume tour so the jump back is calm. */
    private moveCameraToCurrentStep(forceFade: boolean): void {
        this.resetLookInput();
        const step = this.machine.getCurrentStep();
        const target = step ? this.findTarget(step.CameraTargetId) : undefined;
        this.targetMissing = target === undefined;

        // A new step silences the old one at once: no voice overlaps the next card, no eagle flight carries on.
        this.stopVoice();
        this.stopAnthemSequence(false);
        this.arrived = false;
        this.resetPreview();
        this.applyLabels();
        this.applySensory();
        this.applyCues("StepChanged");
        this.prefetchVoice();

        if (!target || !step) {
            if (step) console.error(`Step ${step.StepId}: camera target '${step.CameraTargetId}' not found. The camera stays put.`);
            // Fail closed: don't move, don't leave the journey stuck mid-transition. The card is still read.
            this.machine.notifyTransitionFinished(this.machine.getTransitionId());
            if (this.machine.hasStarted()) this.onArrived();
            return;
        }

        // An authored route replaces the straight move where one exists. Calm Mode still cuts and less movement still fades.
        const path = forceFade ? undefined : this.findPath(step.CameraTargetId);
        if (path && !this.settings.bCalmMode && !isReducedMotionEffective(this.settings)) {
            console.info(`Step ${step.StepId}: route ${path.name} to ${step.CameraTargetId} (${path.waypoints.length} points, ${path.travelSeconds.toFixed(1)} s).`);
            this.rig.beginPathTransition(path.waypoints, target.transform, target.fov, path.travelSeconds, path.cutAtEnd,
                GameMode.ReducedMotionFadeSeconds, GameMode.FadeColor, this.machine.getTransitionId());
            return;
        }

        const pathBlocked = step.TransitionProfile === "Ease"
            && this.world.isPathBlocked(this.rig.location, target.transform.translation, CAMERA_PROBE_RADIUS);
        if (pathBlocked) console.info(`Step ${step.StepId}: the straight path to ${step.CameraTargetId} crosses geometry; fading and cutting.`);
        const profile: TransitionProfile = Rules.resolveMove(forceFade ? "ReducedMotion" : step.TransitionProfile, this.settings, pathBlocked);
        console.info(`Step ${step.StepId}: ${profile} to ${step.CameraTargetId}.`);
        const easeSeconds = Rules.clampDuration(step.TransitionSeconds, GameMode.DefaultEaseSeconds, GameMode.MinTransitionSeconds,
            GameMode.MaxTransitionSeconds);
        this.rig.beginTransition(target.transform, target.fov, profile, easeSeconds, GameMode.ReducedMotionFadeSeconds, GameMode.FadeColor,
            this.machine.getTransitionId());
    }

    /** Start: glides along the splash orbit into the first step's wide shot (or moves there like any step). */
    private moveCameraToFirstStep(): void {
        this.resetLookInput();
        const step = this.machine.getCurrentStep();
        const orbit = this.getSplashOrbit();
        const onOrbit = this.rig.isOrbiting() && step !== undefined && step.CameraTargetId === GameMode.SplashOrbitTargetId && orbit !== null;
        if (!onOrbit || orbit === null || step === undefined) {
            this.moveCameraToCurrentStep(false);
            return;
        }
        this.stopVoice();
        this.arrived = false;
        this.targetMissing = false;
        this.resetPreview();
        this.applyLabels();
        this.applySensory();
        this.applyCues("StepChanged");
        // The longer the way round, the longer the glide, within the usual move limits.
        const degrees = Math.abs(unwindDegrees(orbit.anchorAngle - this.rig.getOrbitAngle(orbit.center)));
        const seconds = getMappedRangeValueClamped(0, 180, GameMode.MinTransitionSeconds * 3, GameMode.MaxTransitionSeconds, degrees);
        console.info(`Step ${step.StepId}: glide along the splash orbit (${degrees.toFixed(0)} deg, ${seconds.toFixed(1)} s).`);
        this.rig.easeOrbitTo(orbit.anchorAngle, seconds, this.machine.getTransitionId());
    }

    /** The orbit through SplashOrbitTargetId around LFF_SiteRoot. Null if the target is missing. */
    private getSplashOrbit(): { center: Vec3; radius: number; height: number; lookAt: Vec3; fov: number; anchorAngle: number; wide: Transform } | null {
        const target = this.findTarget(GameMode.SplashOrbitTargetId);
        if (!target) return null;
        const wide = target.transform;
        const eye = wide.translation;
        const center = this.world.siteFrame.translation;
        const radius = Vec3.dist2D(eye, center);
        // Look at the point above the centre that the wide shot is aimed at, so the orbit passes exactly through it.
        const forward = wide.rotation.forward();
        const run = Math.max(Math.hypot(forward.x, forward.y), 0.05);
        const lookAt = new Vec3(center.x, center.y, eye.z + (forward.z * radius) / run);
        return { center, radius, height: eye.z, lookAt, fov: target.fov, anchorAngle: rad2deg(Math.atan2(eye.y - center.y, eye.x - center.x)), wide };
    }

    /** The splash camera: a slow orbit, or a still wide shot with less movement. cut fades there first (restart). */
    private enterSplash(cut: boolean): void {
        this.stopVoice();
        this.stopAnthemSequence(false);
        const orbit = this.getSplashOrbit();
        if (!orbit) {
            console.warn(`Splash orbit target ${GameMode.SplashOrbitTargetId} not found; the splash holds the first step's view.`);
            if (cut) this.moveCameraToCurrentStep(true);
            return;
        }
        if (cut) {
            // Restart: fade to the wide shot (a cut in Calm Mode); the orbit starts when the fade reports back.
            const profile: TransitionProfile = this.settings.bCalmMode ? "Instant" : "ReducedMotion";
            this.rig.beginTransition(orbit.wide, orbit.fov, profile, GameMode.DefaultEaseSeconds, GameMode.ReducedMotionFadeSeconds,
                GameMode.FadeColor, this.machine.getTransitionId());
            return;
        }
        this.startSplashOrbit();
    }

    private startSplashOrbit(): void {
        if (this.machine.hasStarted() || this.isExploring()) return;
        if (!Rules.shouldOrbitSplash(this.settings)) {
            if (this.rig.isOrbiting()) console.info("Splash: still wide shot (less movement).");
            this.rig.stopOrbit();
            return;
        }
        const orbit = this.getSplashOrbit();
        if (this.rig.isOrbiting() || !orbit) return;
        const from = this.rig.getOrbitAngle(orbit.center);
        console.info(`Splash: orbit of ${(orbit.radius / 100).toFixed(0)} m at ${(orbit.height / 100).toFixed(0)} m high, one lap in ${GameMode.SplashOrbitSeconds} s.`);
        this.rig.startOrbit(orbit.center, orbit.radius, orbit.height, orbit.lookAt, GameMode.SplashOrbitSeconds, orbit.fov, from);
    }

    /** The camera has arrived at the current step: play its arrival cues and read the card. */
    private onArrived(): void {
        this.arrived = true;
        this.applyCues("Arrived");
        // The anthem plays by itself in Standard; the Soft button only replays it. Calm Mode skips it.
        const step = this.machine.getCurrentStep();
        if (step?.IntensityPreview === "Anthem" && !this.settings.bCalmMode && !this.isExploring() && !this.machine.isPaused() && !this.isModalOpen()) {
            this.startAnthemSequence();
        }
        this.scheduleVoice();
    }

    private applyCues(reason: CueReason): void {
        if (reason === "SettingsChanged" && (this.settings.bCalmMode || isReducedMotionEffective(this.settings))) this.stopAnthemSequence(true);
        const instant = isReducedMotionEffective(this.settings);
        const started = this.machine.hasStarted();
        const index = this.machine.getIndex();
        for (const actor of this.world.cueActors) {
            // People with no cue (the line, the staff) only stand; their visibility follows the crowd settings.
            if (actor.cueId === "") continue;
            if (actor.trigger === "Preview") {
                // Preview cues only ever run inside the preview; a new step puts them away.
                if (reason === "StepChanged") actor.applyState("Before");
                continue;
            }
            const state = Rules.resolveCueState(started, index, this.firstCueIndex.get(actor.cueId) ?? NONE);
            // Calm Mode means no spectacle at all: not even shown already done.
            if (actor.spectacle && this.settings.bCalmMode) {
                actor.applyState("Before");
                continue;
            }
            switch (reason) {
                case "StepChanged":
                    // On its own step it waits for the camera, unless less movement shows it already done.
                    actor.applyState(state === "Play" ? (instant ? "After" : "Before") : state);
                    break;
                case "Arrived":
                    if (state === "Play" && !instant) actor.play(false);
                    break;
                case "SettingsChanged":
                    // Spectacle comes back (already done) when Calm Mode is turned off on its step.
                    if (state === "Play" && (instant || (actor.spectacle && !actor.isPlaying()))) actor.applyState("After");
                    break;
            }
        }
    }

    /** The anthem preview's cues (the eagle, fireworks) play while Soft is picked and end the preview when done. */
    private applyPreviewCues(): void {
        const want = this.previewOn && this.getCurrentPreviewKind() === "Anthem" && !this.machine.isPaused() && !this.isModalOpen()
            && Rules.canPlayPreview("Anthem", this.settings);
        if (want === this.previewCuesPlaying) return;
        this.previewCuesPlaying = want;
        // Soft replays the same sequence the step plays by itself.
        const longest = want ? this.startAnthemSequence() : 0;
        if (!want) this.stopAnthemSequence(true);
        this.timers.clearTimer(this.previewCueTimer);
        if (want) {
            console.info(`Anthem preview playing: ${longest.toFixed(1)} s, one pass.`);
            this.timers.setTimer(this.previewCueTimer, () => this.finishPreviewCues(), Math.max(longest, 0.5) + 0.2);
        }
    }

    private finishPreviewCues(): void {
        // One pass, then the button goes back to Off so it can be seen again.
        if (this.previewOn && this.getCurrentPreviewKind() === "Anthem") this.previewOn = false;
        this.applySensory();
        this.broadcast();
    }

    /** An authored route to toTargetId that starts where the camera is now (its From target), if any. */
    private findPath(toTargetId: string): CameraPath | undefined {
        if (!toTargetId) return undefined;
        for (const path of this.world.cameraPaths) {
            if (path.toTargetId !== toTargetId || path.waypoints.length === 0) continue;
            if (path.fromTargetId === "") return path;
            // Only from where it was authored to start: going back, or jumping in, moves the usual way.
            const from = this.findTarget(path.fromTargetId);
            if (from && Vec3.dist(from.transform.translation, this.rig.location) < 100) return path;
        }
        return undefined;
    }

    /**
     * Anthem, Standard: the eagle flies and the camera joins it close beside, then eases back to the seat and the soft
     * fireworks burst above the rim. Less movement keeps the camera at the seat. Returns the sequence's length, s.
     */
    private startAnthemSequence(): number {
        this.stopAnthemSequence(false);
        const step = this.machine.getCurrentStep();
        if (!step || step.IntensityPreview !== "Anthem" || this.settings.bCalmMode || this.isExploring() || this.machine.isPaused()
            || this.isModalOpen() || !this.arrived) return 0;

        let eagle: CueActor | null = null;
        let fireworks = 0;
        for (const actor of this.world.cueActors) {
            if (actor.trigger !== "Preview" || !step.SceneCues.includes(actor.cueId)) continue;
            if (actor.isFireworks) fireworks = Math.max(fireworks, actor.totalSeconds());
            else if (eagle === null) eagle = actor;
        }
        this.anthemSequence = true;
        this.anthemEagle = eagle;
        const flight = eagle?.totalSeconds() ?? 0;
        const follow = eagle !== null && !isReducedMotionEffective(this.settings);
        eagle?.play(false);

        if (eagle !== null && isSoundEffective(this.settings)) {
            void this.audio.preload(GameMode.AnthemSound);
            // The cue's delay is not flight. Audio begins only when the eagle becomes visible and ends at its own finish.
            if (eagle.playDelaySeconds > 0) this.timers.setTimer(this.anthemAudioStartTimer, () => this.startAnthemAudio(), eagle.playDelaySeconds);
            else this.startAnthemAudio();
            this.timers.setTimer(this.anthemAudioEndTimer, () => this.stopAnthemAudio(), Math.max(flight, 0.01));
        }
        if (follow && eagle !== null) {
            const joinAt = eagle.playDelaySeconds + GameMode.AnthemFollowStart * eagle.playSeconds;
            // Leave the bird a moment before it is gone, so the camera turns home with it still in frame.
            this.anthemFlightLeft = Math.max(flight - joinAt - 0.6, 0.5);
            this.timers.setTimer(this.anthemTimer, () => this.joinAnthemEagle(), Math.max(joinAt, 0.1));
        } else {
            this.timers.setTimer(this.anthemTimer, () => this.playAnthemFireworks(), Math.max(flight, 0.1));
        }
        const total = flight + (follow ? GameMode.AnthemReturnSeconds - 0.6 : 0) + fireworks;
        console.info(`Anthem: eagle ${follow ? "with the camera close beside it" : "seen from the seat"} (${flight.toFixed(1)} s), then fireworks; about ${total.toFixed(1)} s.`);
        return total;
    }

    private startAnthemAudio(): void {
        const eagle = this.anthemEagle;
        if (!this.anthemSequence || eagle === null || !eagle.isPlaying() || eagle.isHidden() || !isSoundEffective(this.settings)
            || this.machine.isPaused() || this.isModalOpen() || this.isExploring()) return;
        const sound = this.audio.create(GameMode.AnthemSound);
        if (sound === null) {
            console.warn(`Anthem flight has no audio ready (${GameMode.AnthemSound}).`);
            return;
        }
        this.anthemAudio = sound;
        this.appliedAnthemVolume = -1;
        sound.play(0);
        this.applyAudio();
        this.timers.setTimer(this.anthemAudioWatchTimer, () => this.updateAnthemAudio(), 0.05, true);
        console.info("Anthem audio started for visible eagle flight only.");
    }

    private updateAnthemAudio(): void {
        const eagle = this.anthemEagle;
        if (!this.anthemSequence || eagle === null || !eagle.isPlaying() || eagle.isHidden() || !isSoundEffective(this.settings)
            || this.machine.isPaused() || this.isModalOpen() || this.isExploring()) this.stopAnthemAudio();
    }

    private stopAnthemAudio(): void {
        this.timers.clearTimer(this.anthemAudioStartTimer);
        this.timers.clearTimer(this.anthemAudioEndTimer);
        this.timers.clearTimer(this.anthemAudioWatchTimer);
        if (this.anthemAudio !== null) {
            this.anthemAudio.stop();
            console.info("Anthem audio stopped.");
        }
        this.anthemAudio = null;
        this.appliedAnthemVolume = -1;
    }

    private joinAnthemEagle(): void {
        const eagle = this.anthemEagle;
        if (!this.anthemSequence || eagle === null) return;
        const offset = Vec3.from(GameMode.AnthemFollowOffset);
        this.rig.startFollow({ getLocation: () => eagle.getLocation(), getQuat: () => eagle.getQuat(), isValid: () => !eagle.isHidden() },
            offset, GameMode.AnthemFollowFieldOfView);
        this.timers.setTimer(this.anthemTimer, () => this.returnFromAnthemEagle(), this.anthemFlightLeft);
    }

    private returnFromAnthemEagle(): void {
        if (!this.anthemSequence) return;
        this.rig.stopFollow();
        const step = this.machine.getCurrentStep();
        const target = step ? this.findTarget(step.CameraTargetId) : undefined;
        if (target) this.rig.easeQuietly(target.transform, target.fov, GameMode.AnthemReturnSeconds);
        this.timers.setTimer(this.anthemTimer, () => this.playAnthemFireworks(), GameMode.AnthemReturnSeconds);
    }

    private playAnthemFireworks(): void {
        this.stopAnthemAudio();
        const step = this.machine.getCurrentStep();
        if (!this.anthemSequence || !step) return;
        let longest = 0;
        for (const actor of this.world.cueActors) {
            if (actor.isFireworks && actor.trigger === "Preview" && step.SceneCues.includes(actor.cueId)) {
                actor.play(false);
                longest = Math.max(longest, actor.totalSeconds());
            }
        }
        this.timers.setTimer(this.anthemTimer, () => {
            this.anthemSequence = false;
            this.anthemEagle = null;
        }, Math.max(longest, 0.1) + 0.2);
    }

    /** Stops the sequence; restoreCamera cuts back to the step's pose (Calm Mode or pause turned on mid-flight). */
    private stopAnthemSequence(restoreCamera: boolean): void {
        this.stopAnthemAudio();
        if (!this.anthemSequence) return;
        this.anthemSequence = false;
        this.anthemEagle = null;
        this.timers.clearTimer(this.anthemTimer);
        // The state machine may already be on the next step; stop the old cues, not the new step's list.
        for (const actor of this.world.cueActors) {
            if (actor.trigger === "Preview") actor.applyState("Before");
        }
        this.rig.stopFollow();
        if (restoreCamera) {
            // A cut, not a move: this only happens when the visitor asked for less (Calm, less movement, pause).
            this.rig.holdPosition();
            this.snapCameraToCurrentStep();
        }
    }

    private stopActivePreview(): void {
        this.previewOn = false;
        this.previewCuesPlaying = false;
        this.timers.clearTimer(this.previewCueTimer);
        this.stopAnthemSequence(true);
        this.stopPreviewAudio();
        this.applySensory();
    }

    // ---- Voice ----

    private voiceName(): string {
        return soundName(this.machine.getCurrentStep()?.VoiceWave ?? "");
    }

    /** Fetches this step's line (and the next one's) ahead of the camera's arrival. */
    private prefetchVoice(): void {
        const index = this.machine.getIndex();
        for (const step of [this.steps[index], this.steps[index + 1]]) {
            const name = soundName(step?.VoiceWave ?? "");
            if (name) void this.audio.preload(name);
        }
    }

    private scheduleVoice(): void {
        this.stopVoice();
        if (!this.arrived || this.machine.isPaused() || this.isModalOpen() || this.isExploring()
            || !Rules.shouldPlayVoice(this.settings, this.machine.hasStarted(), this.previewOn)) return;
        this.timers.setTimer(this.voiceTimer, () => this.startVoice(), Math.max(GameMode.VoiceStartDelaySeconds, 0.01));
    }

    private startVoice(): void {
        const step = this.machine.getCurrentStep();
        if (!step || !this.arrived || this.machine.isPaused() || this.isModalOpen() || this.isExploring()
            || !Rules.shouldPlayVoice(this.settings, this.machine.hasStarted(), this.previewOn)) return;
        const name = this.voiceName();
        if (!name || !this.audio.has(name)) {
            // A missing recording is never an error for the visitor: the card is the caption and stays on screen.
            console.info(`Step ${step.StepId}: no voice line for '${step.VoiceWave}'; skipped, the card stays.`);
            return;
        }
        if (this.audio.ready(name) === null) {
            // Still downloading: start as soon as it's here, if this is still the step and the voice is still wanted.
            const stepIndex = this.machine.getIndex();
            void this.audio.preload(name).then((buffer) => {
                if (buffer !== null && this.machine.getIndex() === stepIndex && !this.timers.isTimerActive(this.voiceTimer) && !this.voicePlaying) {
                    this.startVoice();
                }
            });
            return;
        }
        const voice = this.audio.create(name);
        if (voice === null) return;
        this.voiceAudio = voice;
        voice.onFinished = () => this.handleVoiceFinished(voice);
        this.voiceWordTimes = [];
        void this.loadWordTimes(name, voice);
        this.voicePlaying = true;
        voice.play(clamp(GameMode.VoiceVolume, 0, 1));
        this.applyAudio();
        console.info(`Step ${step.StepId}: voice playing ${name} (${voice.duration.toFixed(1)} s); bed ducked ${GameMode.VoiceDuckDecibels} dB.`);
        this.broadcast();
    }

    private wordTimesCache = new Map<string, number[]>();

    private async loadWordTimes(name: string, voice: SoundHandle): Promise<void> {
        let times = this.wordTimesCache.get(name);
        if (!times) {
            try {
                const data = (await (await fetch(`media/vo/${name}.words.json`)).json()) as { words: { t: number }[] };
                times = data.words.map((w) => w.t);
            } catch {
                times = [];
            }
            this.wordTimesCache.set(name, times);
        }
        if (this.voiceAudio === voice) this.voiceWordTimes = times;
    }

    /** Stops the voice at once. reapplyAudio restores the bed from its duck. */
    private stopVoice(reapplyAudio = true): void {
        this.timers.clearTimer(this.voiceTimer);
        if (this.voiceAudio !== null) {
            this.voiceAudio.onFinished = null;
            // Immediately, not a fade: the next card must never be read over the last one.
            this.voiceAudio.stop();
        }
        this.voiceAudio = null;
        this.setVoiceWord(NONE);
        if (this.voicePlaying) {
            this.voicePlaying = false;
            if (reapplyAudio) this.applyAudio();
        }
    }

    private handleVoiceFinished(voice: SoundHandle): void {
        if (voice !== this.voiceAudio) return;
        this.setVoiceWord(NONE);
        this.voiceAudio = null;
        this.voicePlaying = false;
        this.applyAudio();
        this.broadcast();
    }

    private setVoiceWord(index: number): void {
        if (index !== this.voiceWordIndex) {
            this.voiceWordIndex = index;
            for (const listener of this.voiceWordListeners) listener(index);
        }
    }

    /** The karaoke's clock: the last word that has started. */
    private updateVoiceWord(): void {
        const voice = this.voiceAudio;
        if (voice === null || this.voiceWordTimes.length === 0) return;
        const seconds = voice.position();
        let index = -1;
        for (let i = 0; i < this.voiceWordTimes.length && this.voiceWordTimes[i] <= seconds; ++i) index = i;
        this.setVoiceWord(index >= 0 ? index : NONE);
    }

    // ---- Scene state ----

    private snapCameraToCurrentStep(): void {
        const step = this.machine.getCurrentStep();
        const target = step ? this.findTarget(step.CameraTargetId) : undefined;
        this.targetMissing = step !== undefined && target === undefined;
        if (target) this.rig.snapTo(target.transform, target.fov);
    }

    private applyLabels(): void {
        const step = this.machine.getCurrentStep();
        // Labels are checked against the step's framed view, not wherever the camera is mid-move.
        const target = step ? this.findTarget(step.CameraTargetId) : undefined;
        const aspect = this.world.viewportAspect();
        for (const label of this.world.labels) {
            let show = step !== undefined && step.LabelsToShow.some((tag) => label.tags.includes(tag));
            if (show && target) {
                // Keep the lower third (the caption card and the rail) clear of world labels.
                const fraction = projectToView(target.transform, target.fov, aspect, label.location);
                if (Rules.shouldHideLabelAt(fraction !== null, fraction ?? new Vec2())) show = false;
            }
            label.setHidden(!show);
        }
    }

    /** Everything Calm Mode and the previews touch: audio, the Lights pulse and crowd actors. */
    private applySensory(): void {
        this.applyAudio();
        this.applyCrowd();
        this.applyPreviewCues();
        // Standard brightens the bright-lights step by itself (the same slow sine, never a flash). Calm stays flat.
        const step = this.machine.getCurrentStep();
        const autoBrighten = step?.StepId === "bright-lights" && !this.settings.bCalmMode;
        const pulse = !this.isExploring() && !this.machine.isPaused() && !this.isModalOpen() && (autoBrighten
            || (this.previewOn && this.getCurrentPreviewKind() === "Lights" && Rules.canPlayPreview("Lights", this.settings)));
        this.rig.setLightPulse(pulse, GameMode.LightPulseSeconds, GameMode.LightPulseStops);
    }

    private applyCrowd(): void {
        this.world.setCrowd(isCrowdShown(this.settings), !isReducedMotionEffective(this.settings));
    }

    private updateExteriorVisibility(): void {
        const inside = this.world.isInsideStadium(this.rig.location);
        if (inside === this.exteriorHidden) return;
        this.exteriorHidden = inside;
        this.world.setExteriorHidden(inside);
    }

    /** Room ambience follows the actual camera, never the bookmarked step or a selected marker. */
    private getRoomCrowdGain(): number {
        let gain = 1;
        for (const zone of this.world.quietZones) {
            const inside = zone.kind === "Sensory" ? GameMode.SensoryRoomCrowdGain : GameMode.QuietRoomCrowdGain;
            gain = Math.min(gain, Rules.quietZoneGain(zone.distanceTo(this.rig.location), zone.blend, inside));
        }
        return gain;
    }

    private updateRoomAudio(): void {
        const gain = this.getRoomCrowdGain();
        if (Math.abs(gain - this.roomCrowdGain) > 0.001) {
            this.roomCrowdGain = gain;
            this.applyAudio();
        }
    }

    private resetPreview(): void {
        if (!this.previewOn) return;
        this.previewOn = false;
        this.applySensory();
    }

    private getCurrentPreviewKind(): IntensityPreview {
        const step = this.machine.hasStarted() && !this.isExploring() ? this.machine.getCurrentStep() : undefined;
        return step?.IntensityPreview ?? "None";
    }

    private handlePreviewAudioFinished(sound: SoundHandle): void {
        if (sound !== this.previewAudio) return;
        // The sample is short and plays once; the button goes back to Off so it can be heard again.
        this.previewAudio = null;
        if (this.previewOn && this.getCurrentPreviewKind() === "Sound") {
            this.previewOn = false;
            this.applySensory();
            this.broadcast();
        }
    }

    /** Requests a sound that isn't decoded yet and re-applies the mix once it is. */
    private need(name: string): boolean {
        if (this.audio.ready(name) !== null) return true;
        if (!this.pendingAudio.has(name) && this.audio.has(name)) {
            this.pendingAudio.add(name);
            void this.audio.preload(name).then((buffer) => {
                this.pendingAudio.delete(name);
                // A sound that failed is now missing (has() is false), so re-applying can't ask for it again.
                if (buffer !== null) this.applyAudio();
            });
        }
        return false;
    }

    /** Master mute, ambient bed and the Sound preview. */
    private applyAudio(): void {
        // Sound Off is silence: the whole device is muted, so no UI or engine sound can slip through either.
        const sound = isSoundEffective(this.settings);
        this.audio.setPrimaryVolume(sound ? 1 : 0);
        if (!sound) {
            this.stopVoice(false);
            this.stopAudio();
            return;
        }
        const previewSound = this.previewOn && this.getCurrentPreviewKind() === "Sound" && !this.machine.isPaused() && !this.isModalOpen()
            && Rules.canPlayPreview("Sound", this.settings);

        // Explore must not inherit a quiet bed from the bookmarked tour step. Quieting instead follows the real camera.
        const step = !this.isExploring() ? this.machine.getCurrentStep() : undefined;
        const defaultBed = !this.isExploring() && this.machine.hasStarted() && this.machine.getTourStep() === "Game"
            ? GameMode.StadiumAmbientBed : GameMode.AmbientBed;
        const bed = step && step.AudioBed !== "" ? soundName(step.AudioBed) : defaultBed;
        this.roomCrowdGain = this.getRoomCrowdGain();
        const voiceDuck = this.voicePlaying ? Rules.decibelsToGain(GameMode.VoiceDuckDecibels) : 1;
        const anthemAudible = this.anthemAudio?.isPlaying() ?? false;
        this.voiceAudio?.setVolumeMultiplier(anthemAudible ? 0.7 : 1);
        const bedVolume = clamp(GameMode.BedVolume, 0, 1) * this.roomCrowdGain * (previewSound ? BED_DUCK_WHILE_PREVIEW : 1) * voiceDuck
            * (anthemAudible ? 0.2 : 1);

        // Duck an outgoing cross-fade too, without resetting its fade-to-stop or amplifying it on restore.
        this.fadingBedAudio?.setVolumeMultiplier(clamp(bedVolume / Math.max(this.fadingBedBaseVolume, 0.001), 0, 1));
        if (this.anthemAudio !== null) {
            // The anthem is the foreground of the flight, not another crowd bed buried beneath narration.
            const volume = clamp(GameMode.AnthemVolume, 0, 1) * (this.voicePlaying ? 0.55 : 1);
            if (Math.abs(volume - this.appliedAnthemVolume) > 1e-4) {
                this.anthemAudio.adjustVolume(0.05, volume);
                this.appliedAnthemVolume = volume;
            }
        }

        if (this.bedAudio !== null && this.bedAudio.name === bed) {
            if (Math.abs(bedVolume - this.appliedBedVolume) > 1e-4) {
                this.bedAudio.adjustVolume(this.voicePlaying ? 0 : 0.5, bedVolume);
                this.appliedBedVolume = bedVolume;
            }
        } else if (this.need(bed)) {
            // A different bed (plaza murmur to the stadium crowd) cross-fades; turning Sound on fades in over 2 s.
            const crossfade = this.bedAudio !== null;
            this.fadingBedAudio?.stop();
            this.fadingBedAudio = null;
            if (crossfade && this.bedAudio !== null) {
                this.fadingBedAudio = this.bedAudio;
                this.fadingBedBaseVolume = Math.max(this.appliedBedVolume, 0.001);
                this.fadingBedAudio.fadeOut(GameMode.BedCrossfadeSeconds);
            }
            this.bedAudio = this.audio.create(bed, true);
            if (this.bedAudio !== null) {
                this.appliedBedVolume = bedVolume;
                this.bedAudio.play(bedVolume, crossfade ? GameMode.BedCrossfadeSeconds : 2);
                console.info(`Ambient bed playing: ${bed} at volume ${bedVolume.toFixed(2)}.`);
            }
        }

        if (!previewSound) {
            this.stopPreviewAudio();
            return;
        }
        if (this.previewAudio === null) {
            const sample = step && step.PreviewSound !== "" ? soundName(step.PreviewSound) : GameMode.PreviewSound;
            if (!this.need(sample)) return;
            const preview = this.audio.create(sample);
            if (preview === null) return;
            this.previewAudio = preview;
            preview.onFinished = () => this.handlePreviewAudioFinished(preview);
            preview.play(clamp(GameMode.PreviewVolume, 0, 1), 1);
            console.info(`Sound preview playing: ${sample} (${preview.duration.toFixed(1)} s); bed ducked to ${bedVolume.toFixed(2)}.`);
        }
    }

    private stopAudio(): void {
        this.bedAudio?.stop();
        this.bedAudio = null;
        this.fadingBedAudio?.stop();
        this.fadingBedAudio = null;
        this.appliedBedVolume = -1;
        this.stopPreviewAudio();
        this.stopAnthemAudio();
        this.uiClickAudio?.stop();
        this.uiClickAudio = null;
    }

    private stopPreviewAudio(): void {
        if (this.previewAudio !== null) {
            this.previewAudio.onFinished = null;
            // Keep no orphaned playback that could leak through if Sound is turned back on during a fade.
            this.previewAudio.stop();
        }
        this.previewAudio = null;
    }

    // ---- Per frame ----

    tick(dt: number): void {
        this.timers.tick(dt);
        this.rig.tick(dt);
        this.updateVoiceWord();
    }

    // ---- State ----

    private isExploring(): boolean {
        return this.machine.getMode() === "FreeExplore";
    }

    /** The map's own commands: Free explore only, never in a modal or while paused. */
    canUseMap(): boolean {
        return this.isExploring() && !this.isModalOpen() && !this.machine.isPaused();
    }

    canFly(): boolean {
        return this.canUseMap();
    }

    getMode(): SimulationMode {
        return this.machine.getMode();
    }

    private isModalOpen(): boolean {
        return this.settingsOpen || this.restartConfirmOpen;
    }

    private disclaimerText(level: JourneyStep["DisclaimerLevel"]): string {
        return level === "Verified" ? "" : level === "Approximate" ? GameMode.DisclaimerApproximate : GameMode.DisclaimerIllustrative;
    }

    private disclaimerLine(level: JourneyStep["DisclaimerLevel"]): string {
        return level === "Verified" ? "" : level === "Approximate" ? GameMode.DisclaimerLineApproximate : GameMode.DisclaimerLineIllustrative;
    }

    broadcast(): void {
        const m = this.machine;
        const started = m.hasStarted();
        const paused = m.isPaused();
        const atEnd = m.isAtEnd();
        const step = m.getCurrentStep();
        const backToIntro = Rules.shouldBackReturnToIntro(started, m.getIndex(), paused, step?.bBackEnabled ?? false);
        const previewKind = this.getCurrentPreviewKind();
        const snapshot: JourneySnapshot = {
            Phase: m.getPhase(),
            Mode: m.getMode(),
            TourStep: m.getTourStep(),
            TourStepFirstSteps: [...m.getTourStepFirstSteps()],
            bCanResumeTour: this.canUseMap(),
            MapLevel: this.mapLevel,
            bLevelHighlight: this.isExploring() && this.levelHighlight,
            AmenityMask: this.amenityMask,
            SelectedAmenityId: this.selectedAmenityId,
            TourPlaces: [],
            ViewTargetId: this.viewTargetId,
            MySeatTargetId: GameMode.MySeatTargetId,
            MySeatLevel: GameMode.MySeatLevel,
            StepIndex: m.getIndex(),
            StepCount: m.num(),
            StepId: "",
            Title: "",
            Body: "",
            ProgressText: "",
            ClockText: "",
            TimelineMarks: this.timelineMarks,
            bShowTicketPhone: false,
            bVoicePlaying: this.voicePlaying,
            DisclaimerLevel: "Illustrative",
            DisclaimerText: "",
            bShowDisclaimer: true,
            DisclaimerLine: "",
            PrototypeBadgeText: GameMode.PrototypeBadgeText,
            bStarted: started,
            bPaused: paused,
            bAtEnd: atEnd,
            bCanNext: !this.isModalOpen() && !this.isExploring() && (m.getPhase() === "Intro" || m.canNext() || (atEnd && !paused)),
            bCanBack: !this.isModalOpen() && !this.isExploring() && (m.canBack() || backToIntro),
            bSettingsOpen: this.settingsOpen,
            bRestartConfirmOpen: this.restartConfirmOpen,
            bTargetMissing: this.targetMissing,
            Settings: { ...this.settings },
            IntensityPreview: previewKind,
            bShowPreviewControls: Rules.shouldShowPreviewControls(previewKind, this.settings),
            bPreviewOn: this.previewOn,
            bPreviewAvailable: Rules.canPlayPreview(previewKind, this.settings),
        };
        if (step) {
            snapshot.StepId = step.StepId;
            snapshot.DisclaimerLevel = step.DisclaimerLevel;
            if (started) {
                snapshot.Title = step.Title;
                snapshot.Body = step.Body;
                snapshot.ClockText = step.ClockText;
                snapshot.bShowTicketPhone = step.bShowTicketPhone;
                if (!this.isExploring()) snapshot.TourPlaces = [...step.PlacesToShow];
                snapshot.ProgressText = `Step ${snapshot.StepIndex + 1} of ${snapshot.StepCount}`;
            }
        }
        if (!started) {
            snapshot.Title = GameMode.IntroTitle;
            snapshot.Body = this.journeyMissing ? GameMode.JourneyMissingText : GameMode.IntroBody;
        }
        // The banner shows unless the location is Verified. The intro always shows it.
        snapshot.bShowDisclaimer = !started || Rules.shouldShowDisclaimer(snapshot.DisclaimerLevel);
        snapshot.DisclaimerText = this.disclaimerText(started ? snapshot.DisclaimerLevel : "Illustrative");
        snapshot.DisclaimerLine = this.disclaimerLine(started ? snapshot.DisclaimerLevel : "Illustrative");
        this.snapshot = snapshot;
        for (const listener of this.listeners) listener(snapshot);
    }

    /** The categories that have at least one mapped place (others read "Not mapped"). */
    mappedAmenityMask(): number {
        return AMENITIES.reduce((mask, kind) => (this.amenities.some((a) => a.Category === kind) ? mask | amenityBit(kind) : mask), 0);
    }
}
