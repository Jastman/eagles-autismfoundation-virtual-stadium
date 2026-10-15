// Port of namespace LFFJourneyRules (Source/VirtualStadium/Private/Journey/LFFJourneyStateMachine.cpp).
import { Box2, Rotator, Transform, Vec2, Vec3, clamp, deg2rad, lerp, smoothStep } from "../util/ue";
import type { CueState, DisclaimerLevel, IntensityPreview, LevelView, TourStep, TransitionProfile, VisitorSettings } from "./types";
import { NONE, TOUR_STEPS, TOUR_STEP_COUNT, isReducedMotionEffective, isSoundEffective, isVoiceEffective } from "./types";

/** The chapter a step belongs to: the last one whose first step is at or before it. Arrival before the first. */
export function resolveTourStep(stepIndex: number, firstSteps: number[]): TourStep {
    let found = 0;
    for (let chapter = 0; chapter < Math.min(firstSteps.length, TOUR_STEP_COUNT); ++chapter) {
        if (stepIndex >= firstSteps[chapter]) found = chapter;
    }
    return TOUR_STEPS[found];
}

/** Full quieting inside a bounded room, smoothly returning to normal across its outside blend distance. */
export function quietZoneGain(outsideDistance: number, blendDistance: number, insideGain: number): number {
    const inside = clamp(insideGain, 0, 1);
    if (outsideDistance <= 0) return inside;
    if (blendDistance <= 1e-4) return 1;
    return lerp(inside, 1, smoothStep(0, blendDistance, outsideDistance));
}

/** Free flight's space, in the site frame: within radius of the centre across, and between minZ and maxZ. */
export function clampFlyPosition(site: Vec3, radius: number, minZ: number, maxZ: number): Vec3 {
    let x = site.x, y = site.y;
    if (x * x + y * y > radius * radius) {
        const n = Math.hypot(x, y);
        x = (x / n) * radius;
        y = (y / n) * radius;
    }
    return new Vec3(x, y, clamp(site.z, minZ, Math.max(minZ, maxZ)));
}

/** Where a level's overview camera sits and looks, in the site frame. No roll. */
export function levelOverviewPose(view: LevelView): Transform {
    const look = new Rotator(view.ViewPitchDegrees, view.ViewYawDegrees, 0);
    const target = new Vec3(0, 0, view.FloorZ);
    return Transform.fromRotator(look, target.sub(look.vector().scale(view.ViewDistance)));
}

/** Largest unobstructed rectangle in actual HUD units, with edge/credit clearance. Invalid when no space remains. */
export function unobstructedViewport(size: Vec2, panels: Box2[], inset: number, bottomInset: number): Box2 {
    const bounds = new Box2(new Vec2(inset, inset), new Vec2(size.x - inset, size.y - bottomInset));
    if (bounds.size().x <= 1 || bounds.size().y <= 1) return Box2.invalid();
    const covered: Box2[] = [];
    const xs = [bounds.min.x, bounds.max.x];
    const ys = [bounds.min.y, bounds.max.y];
    const addUnique = (list: number[], v: number) => {
        if (!list.includes(v)) list.push(v);
    };
    for (const panel of panels) {
        if (!panel.valid) continue;
        const grown = new Box2(panel.min.sub(new Vec2(inset, inset)), panel.max.add(new Vec2(inset, inset)));
        covered.push(grown);
        addUnique(xs, clamp(grown.min.x, bounds.min.x, bounds.max.x));
        addUnique(xs, clamp(grown.max.x, bounds.min.x, bounds.max.x));
        addUnique(ys, clamp(grown.min.y, bounds.min.y, bounds.max.y));
        addUnique(ys, clamp(grown.max.y, bounds.min.y, bounds.max.y));
    }
    xs.sort((a, b) => a - b);
    ys.sort((a, b) => a - b);
    let best = Box2.invalid();
    let bestArea = 0;
    for (let l = 0; l < xs.length; ++l)
        for (let r = l + 1; r < xs.length; ++r)
            for (let t = 0; t < ys.length; ++t)
                for (let b = t + 1; b < ys.length; ++b) {
                    const candidate = new Box2(new Vec2(xs[l], ys[t]), new Vec2(xs[r], ys[b]));
                    const area = candidate.area();
                    const blocked = covered.some((p) => candidate.min.x < p.max.x - 0.01 && candidate.max.x > p.min.x + 0.01
                        && candidate.min.y < p.max.y - 0.01 && candidate.max.y > p.min.y + 0.01);
                    if (area > bestArea && !blocked) {
                        best = candidate;
                        bestArea = area;
                    }
                }
    return best;
}

/** Exact perspective fit inside a normalized safe rectangle. Points include marker stems; margin is world-space clearance. */
export function framePointsPose(points: Vec3[], pitchDegrees: number, yawDegrees: number, horizontalFov: number, aspect: number,
    safeRect: Box2, margin: number, minDistance: number): Transform {
    if (!(points.length > 0 && aspect > 0 && horizontalFov > 0 && horizontalFov < 180 && safeRect.valid
        && safeRect.size().x > 0 && safeRect.size().y > 0)) {
        console.error("Invalid map framing inputs");
        return new Transform();
    }
    const lo = new Vec3(Infinity, Infinity, Infinity), hi = new Vec3(-Infinity, -Infinity, -Infinity);
    for (const p of points) {
        lo.x = Math.min(lo.x, p.x); lo.y = Math.min(lo.y, p.y); lo.z = Math.min(lo.z, p.z);
        hi.x = Math.max(hi.x, p.x); hi.y = Math.max(hi.y, p.y); hi.z = Math.max(hi.z, p.z);
    }
    const centre = Vec3.lerp(lo, hi, 0.5);
    const rotation = new Rotator(pitchDegrees, yawDegrees, 0).quaternion();
    const tanH = Math.tan(deg2rad(horizontalFov * 0.5));
    const tanV = tanH / aspect;
    const left = (2 * safeRect.min.x - 1) * tanH, right = (2 * safeRect.max.x - 1) * tanH;
    const bottom = (1 - 2 * safeRect.max.y) * tanV, top = (1 - 2 * safeRect.min.y) * tanV;
    const cx = (left + right) * 0.5, cy = (bottom + top) * 0.5;
    let distance = minDistance;
    for (const point of points) {
        const o = rotation.unrotateVector(point.sub(centre));
        distance = Math.max(distance, -o.x + margin + 1);
        // Each inequality is a frustum plane, including a spherical world-space margin.
        distance = Math.max(distance, (left * o.x - o.y + margin * Math.sqrt(1 + left * left)) / (cx - left));
        distance = Math.max(distance, (o.y - right * o.x + margin * Math.sqrt(1 + right * right)) / (right - cx));
        distance = Math.max(distance, (bottom * o.x - o.z + margin * Math.sqrt(1 + bottom * bottom)) / (cy - bottom));
        distance = Math.max(distance, (o.z - top * o.x + margin * Math.sqrt(1 + top * top)) / (top - cy));
    }
    return new Transform(rotation, centre.sub(rotation.rotateVector(new Vec3(distance, cx * distance, cy * distance))));
}

export function resolveTransitionProfile(authored: TransitionProfile, reducedMotion: boolean): TransitionProfile {
    return reducedMotion && authored === "Ease" ? "ReducedMotion" : authored;
}

/**
 * The move actually used: Calm Mode cuts; less movement, or a path that would pass through a wall or prop, fades and
 * cuts; otherwise the authored profile. The camera never flies through geometry.
 */
export function resolveMove(authored: TransitionProfile, settings: VisitorSettings, pathBlocked: boolean): TransitionProfile {
    if (settings.bCalmMode) return "Instant";
    return resolveTransitionProfile(authored, isReducedMotionEffective(settings) || pathBlocked);
}

/** "Preview this?" shows on intensity steps unless Calm Mode is on. */
export function shouldShowPreviewControls(kind: IntensityPreview, settings: VisitorSettings): boolean {
    return kind !== "None" && !settings.bCalmMode;
}

/** Soft can play: never in Calm Mode, and a Sound preview only when Sound is on. Anthem visuals can stay silent. */
export function canPlayPreview(kind: IntensityPreview, settings: VisitorSettings): boolean {
    if (!shouldShowPreviewControls(kind, settings)) return false;
    return kind !== "Sound" || isSoundEffective(settings);
}

/** World labels stay out of the caption card's way: hidden if behind the camera or in the lower third of the frame. */
export function shouldHideLabelAt(inFront: boolean, screenFraction: Vec2): boolean {
    return !inFront || screenFraction.y > 2 / 3;
}

export function clampDuration(authoredSeconds: number, defaultSeconds: number, minSeconds: number, maxSeconds: number): number {
    const seconds = authoredSeconds > 0 ? authoredSeconds : defaultSeconds;
    return clamp(seconds, minSeconds, Math.max(minSeconds, maxSeconds));
}

/** Back on the first step returns to the splash instead of doing nothing (LEDGER D26). */
export function shouldBackReturnToIntro(started: boolean, index: number, paused: boolean, stepAllowsBack: boolean): boolean {
    return started && index === 0 && !paused && stepAllowsBack;
}

export function shouldShowDisclaimer(level: DisclaimerLevel): boolean {
    return level !== "Verified";
}

/** An arrival cue against the journey: Before until its step, Play on its step, After on every later one. */
export function resolveCueState(started: boolean, currentIndex: number, firstCueIndex: number): CueState {
    if (!started || firstCueIndex === NONE || currentIndex < firstCueIndex) return "Before";
    return currentIndex === firstCueIndex ? "Play" : "After";
}

/** The splash slowly orbits the stadium, unless less movement or Calm Mode asks for a still wide shot. */
export function shouldOrbitSplash(settings: VisitorSettings): boolean {
    return !isReducedMotionEffective(settings);
}

/** The voice reads a step only with Voice and Sound on, never over an intensity preview. */
export function shouldPlayVoice(settings: VisitorSettings, started: boolean, previewOn: boolean): boolean {
    return started && isVoiceEffective(settings) && !previewOn;
}

/** Linear gain for a level change in dB. */
export function decibelsToGain(decibels: number): number {
    return Math.pow(10, decibels / 20);
}
