// Port of ALFFCameraRig (Source/VirtualStadium/Private/Camera/LFFCameraRig.cpp). Applies camera motion only and owns no
// journey state. Poses are in Unreal world coordinates (util/ue.ts); scene/geo.ts puts them on the Cesium camera. The
// player camera manager's fade becomes `fadeAmount`, drawn by the HUD's fade layer in FadeColor.
import { Quat, Rotator, Transform, Vec3, clamp, fInterpTo, interpEaseInOut, lerp, rInterpTo, smoothStep, unwindDegrees, vInterpTo } from "../util/ue";
import { Spline } from "../util/spline";
import { TimerHandle, Timers } from "../util/timers";
import type { TransitionProfile } from "../journey/types";

export interface FollowTarget {
    getLocation(): Vec3;
    getQuat(): Quat;
    isValid(): boolean;
}

export type TransitionFinished = (transitionId: number) => void;

const FOLLOW_MAX_SPEED = 1400;
const PULSE_RAMP_SECONDS = 0.8;

export class CameraRig {
    // The actor's pose: location (cm), rotation and the camera component's FieldOfView (Unreal's horizontal FOV at the
    // 16:9 reference; geo.ts keeps the vertical FOV for any aspect, as AspectRatio_MaintainYFOV does).
    location = new Vec3();
    rotation = new Rotator();
    fieldOfView = 70;

    private startTransform = new Transform();
    private endTransform = new Transform();
    private startFov = 70;
    private endFov = 70;
    private elapsed = 0;
    private duration = 0;
    private activeTransitionId = 0;
    private transitionSerial = 0;
    private easing = false;
    private easePaused = false;

    private orbiting = false;
    private orbitEasing = false;
    private orbitCenter = new Vec3();
    private orbitLookAt = new Vec3();
    private orbitRadius = 0;
    private orbitHeight = 0;
    private orbitPeriod = 36;
    private orbitFov = 70;
    private orbitAngle = 0;
    private orbitEaseFrom = 0;
    private orbitEaseTo = 0;

    private pulseOn = false;
    private pulseElapsed = 0;
    private pulsePeriod = 4;
    private pulsePeakStops = 0;
    /** The Lights preview's exposure lift, in stops (read by the scene's exposure stage). */
    pulseStops = 0;

    private pathSpline: Spline | null = null;
    private pathing = false;
    private pathCutAtEnd = false;
    private pathEndTransform = new Transform();
    private pathEndFov = 70;
    private pathFadeSeconds = 0.35;
    private pathFadeColor = "";

    private followTarget: FollowTarget | null = null;
    private followOffset = new Vec3();
    private followFov = 60;
    private following = false;

    private readonly fadeOutTimer = new TimerHandle();
    private readonly fadeInTimer = new TimerHandle();

    // APlayerCameraManager's fade (StartCameraFade / StopCameraFade).
    fadeAmount = 0;
    fadeColor = "rgb(79, 79, 89)";
    private fadeFrom = 0;
    private fadeTo = 0;
    private fadeTime = 0;
    private fadeElapsed = 0;
    private fading = false;

    constructor(private readonly timers: Timers, private readonly onFinished: TransitionFinished) {}

    getTransform(): Transform {
        return Transform.fromRotator(this.rotation, this.location);
    }

    private setPose(location: Vec3, rotation: Rotator): void {
        this.location = location.clone();
        this.rotation = new Rotator(rotation.pitch, rotation.yaw, 0);
    }

    tick(dt: number): void {
        this.tickFade(dt);
        this.tickLightPulse(dt);
        this.tickOrbit(dt);
        this.tickPath(dt);
        this.tickFollow(dt);
        if (!this.easing || this.easePaused) return;

        this.elapsed += dt;
        const alpha = this.duration <= 1e-4 ? 1 : clamp(this.elapsed / this.duration, 0, 1);
        const eased = interpEaseInOut(0, 1, alpha, 2);
        const rotation = Quat.slerp(this.startTransform.rotation, this.endTransform.rotation, eased).rotator();
        rotation.roll = 0;
        this.setPose(Vec3.lerp(this.startTransform.translation, this.endTransform.translation, eased), rotation);
        this.fieldOfView = lerp(this.startFov, this.endFov, eased);
        if (alpha >= 1) this.finishTransition(this.activeTransitionId);
    }

    beginTransition(target: Transform, targetFov: number, profile: TransitionProfile, easeSeconds: number, fadeSeconds: number,
        fadeColor: string, transitionId: number): void {
        this.stopActiveTransition();
        this.activeTransitionId = transitionId;
        const serial = this.transitionSerial;

        switch (profile) {
            case "Ease": {
                this.clearFade(fadeSeconds, fadeColor);
                // Start from wherever the camera is now, so retargeting mid-move never jumps.
                this.startTransform = this.getTransform();
                this.endTransform = target.clone();
                this.startFov = this.fieldOfView;
                this.endFov = targetFov;
                this.elapsed = 0;
                this.duration = easeSeconds;
                this.easing = true;
                break;
            }
            case "ReducedMotion": {
                // Fade to a neutral colour, cut while hidden, fade back. No camera path is ever visible.
                const fade = Math.max(fadeSeconds, 0.05);
                this.startFade(1, fade, fadeColor, true);
                const cut = target.clone();
                this.timers.setTimer(this.fadeOutTimer, () => {
                    if (transitionId !== this.activeTransitionId || serial !== this.transitionSerial) return;
                    this.snapTo(cut, targetFov);
                    this.startFade(0, fade, fadeColor, false);
                    this.timers.setTimer(this.fadeInTimer, () => {
                        if (serial === this.transitionSerial) this.finishTransition(transitionId);
                    }, fade);
                }, fade);
                break;
            }
            default: {
                this.clearFade(fadeSeconds, fadeColor);
                this.snapTo(target, targetFov);
                // Report on the next tick so callers never re-enter the journey mid-command.
                this.timers.setTimerForNextTick(() => {
                    if (serial === this.transitionSerial) this.finishTransition(transitionId);
                });
            }
        }
    }

    isBusy(): boolean {
        const fadingTimers = this.timers.isTimerActive(this.fadeOutTimer) || this.timers.isTimerActive(this.fadeInTimer);
        return this.easing || this.pathing || this.orbiting || this.orbitEasing || (this.followTarget?.isValid() ?? false) || fadingTimers;
    }

    snapTo(target: Transform, targetFov: number): void {
        const rotation = target.rotator();
        rotation.roll = 0;
        this.setPose(target.translation, rotation);
        this.fieldOfView = targetFov;
    }

    /** Cancels all travel at the current pose, including a held fade. Used when the visitor takes control in Explore. */
    holdPosition(): void {
        this.stopActiveTransition();
        this.activeTransitionId = 0;
        this.fading = false;
        this.fadeAmount = 0;
    }

    beginPathTransition(points: Vec3[], target: Transform, targetFov: number, seconds: number, cutAtEnd: boolean, fadeSeconds: number,
        fadeColor: string, transitionId: number): void {
        this.stopActiveTransition();
        this.activeTransitionId = transitionId;
        this.clearFade(fadeSeconds, fadeColor);
        const route = [this.location.clone(), ...points.map((p) => p.clone())];
        if (!cutAtEnd) route.push(target.translation.clone());
        this.pathSpline = new Spline(route);
        this.startTransform = this.getTransform();
        this.pathEndTransform = target.clone();
        this.pathEndFov = targetFov;
        this.pathCutAtEnd = cutAtEnd;
        this.pathFadeSeconds = fadeSeconds;
        this.pathFadeColor = fadeColor;
        this.startFov = this.fieldOfView;
        this.endFov = cutAtEnd ? this.startFov : targetFov;
        this.elapsed = 0;
        this.duration = Math.max(seconds, 1);
        this.pathing = true;
    }

    private tickPath(dt: number): void {
        if (!this.pathing || this.easePaused || this.pathSpline === null) return;
        this.elapsed += dt;
        const alpha = clamp(this.elapsed / this.duration, 0, 1);
        // A slow start and a slow finish, constant in between: no lurch at either end.
        const eased = interpEaseInOut(0, 1, alpha, 2);
        const length = this.pathSpline.length();
        const distance = eased * length;
        const location = this.pathSpline.locationAtDistance(distance);
        // Look a few metres ahead along the route, level-ish; blend in from the start pose and out to the end pose.
        const ahead = this.pathSpline.locationAtDistance(Math.min(distance + 600, length));
        const along = ahead.sub(location).isNearlyZero() ? this.rotation.clone() : ahead.sub(location).rotation();
        along.pitch = clamp(along.pitch, -15, 10);
        let rotation = along.quaternion();
        rotation = Quat.slerp(this.startTransform.rotation, rotation, smoothStep(0, 0.2, alpha));
        if (!this.pathCutAtEnd) rotation = Quat.slerp(rotation, this.pathEndTransform.rotation, smoothStep(0.7, 1, alpha));
        const final = rotation.rotator();
        final.roll = 0;
        this.setPose(location, final);
        this.fieldOfView = lerp(this.startFov, this.endFov, eased);
        if (alpha < 1) return;

        this.pathing = false;
        if (this.pathCutAtEnd) {
            // The last stretch is behind a wall: the same calm fade and cut as less movement.
            this.beginTransition(this.pathEndTransform, this.pathEndFov, "ReducedMotion", 0, this.pathFadeSeconds, this.pathFadeColor,
                this.activeTransitionId);
            return;
        }
        this.snapTo(this.pathEndTransform, this.pathEndFov);
        this.finishTransition(this.activeTransitionId);
    }

    /** Keeps the camera close beside target (offset in the target's own frame), lagging softly and looking at it. */
    startFollow(target: FollowTarget, offset: Vec3, fov: number): void {
        this.stopActiveTransition();
        this.followTarget = target;
        this.followOffset = offset.clone();
        this.followFov = fov;
        this.following = true;
    }

    stopFollow(): void {
        this.following = false;
        this.followTarget = null;
    }

    private tickFollow(dt: number): void {
        if (!this.following) return;
        const target = this.followTarget;
        if (target === null || !target.isValid()) {
            this.stopFollow();
            return;
        }
        // Soft lag on both position and aim: the camera drifts after the bird, never snaps to it.
        const focus = target.getLocation();
        const desired = focus.add(target.getQuat().rotateVector(this.followOffset));
        // Capped a little above the eagle's own speed, so joining it is a glide rather than a lunge.
        const current = this.location;
        const step = vInterpTo(current, desired, dt, 1.2).sub(current).clampedToMaxSize(FOLLOW_MAX_SPEED * dt);
        const location = current.add(step);
        const aim = rInterpTo(this.rotation, focus.sub(location).rotation(), dt, 2);
        aim.roll = 0;
        this.setPose(location, aim);
        this.fieldOfView = fInterpTo(this.fieldOfView, this.followFov, dt, 1);
    }

    /** An Ease back to a pose that reports nothing (the journey is already at its step). */
    easeQuietly(target: Transform, targetFov: number, seconds: number): void {
        this.stopActiveTransition();
        this.activeTransitionId = 0;
        this.startTransform = this.getTransform();
        this.endTransform = target.clone();
        this.startFov = this.fieldOfView;
        this.endFov = targetFov;
        this.elapsed = 0;
        this.duration = Math.max(seconds, 0.5);
        this.easing = true;
    }

    /** Freezes an Ease move in place. Fades are short and are allowed to finish. */
    setTransitionPaused(paused: boolean): void {
        this.easePaused = paused;
    }

    startOrbit(center: Vec3, radius: number, height: number, lookAt: Vec3, periodSeconds: number, fov: number, startAngleDegrees: number): void {
        this.stopActiveTransition();
        this.orbitCenter = center.clone();
        this.orbitRadius = Math.max(radius, 100);
        this.orbitHeight = height;
        this.orbitLookAt = lookAt.clone();
        this.orbitPeriod = Math.max(periodSeconds, 10);
        this.orbitFov = fov;
        this.orbitAngle = startAngleDegrees;
        this.clearFade(0.35, this.fadeColor);
        this.orbiting = true;
        this.orbitEasing = false;
        this.applyOrbitAngle(this.orbitAngle);
    }

    stopOrbit(): void {
        this.orbiting = false;
        this.orbitEasing = false;
    }

    isOrbiting(): boolean {
        return this.orbiting;
    }

    getOrbitAngle(center: Vec3): number {
        const offset = this.location.sub(center);
        return (Math.atan2(offset.y, offset.x) * 180) / Math.PI;
    }

    /** Glides along the orbit (the short way round) to targetAngle and stops there; reports by id like any move. */
    easeOrbitTo(targetAngleDegrees: number, seconds: number, transitionId: number): void {
        if (!this.orbiting) return;
        this.activeTransitionId = transitionId;
        this.orbitEaseFrom = this.orbitAngle;
        this.orbitEaseTo = this.orbitAngle + unwindDegrees(targetAngleDegrees - this.orbitAngle);
        this.elapsed = 0;
        this.duration = Math.max(seconds, 0.1);
        this.orbitEasing = true;
        this.easePaused = false;
    }

    private tickOrbit(dt: number): void {
        if (!this.orbiting) return;
        if (!this.orbitEasing) {
            this.orbitAngle = unwindDegrees(this.orbitAngle + (360 * dt) / this.orbitPeriod);
            this.applyOrbitAngle(this.orbitAngle);
            return;
        }
        if (this.easePaused) return;
        this.elapsed += dt;
        const alpha = clamp(this.elapsed / this.duration, 0, 1);
        this.orbitAngle = lerp(this.orbitEaseFrom, this.orbitEaseTo, interpEaseInOut(0, 1, alpha, 2));
        this.applyOrbitAngle(this.orbitAngle);
        if (alpha >= 1) {
            this.orbiting = false;
            this.orbitEasing = false;
            this.finishTransition(this.activeTransitionId);
        }
    }

    private applyOrbitAngle(angleDegrees: number): void {
        const radians = (angleDegrees * Math.PI) / 180;
        const location = new Vec3(this.orbitCenter.x + this.orbitRadius * Math.cos(radians), this.orbitCenter.y + this.orbitRadius * Math.sin(radians),
            this.orbitHeight);
        const rotation = this.orbitLookAt.sub(location).rotation();
        rotation.roll = 0;
        this.setPose(location, rotation);
        this.fieldOfView = this.orbitFov;
    }

    /** Lights preview: exposure rises and falls as a smooth sine; stopping ramps back over 0.8 s. Never flickers. */
    setLightPulse(on: boolean, periodSeconds: number, peakStops: number): void {
        if (on && !this.pulseOn) this.pulseElapsed = 0;
        this.pulseOn = on;
        // 3 s is the floor: at most one gentle rise every few seconds, nowhere near the 3 Hz flash threshold.
        this.pulsePeriod = Math.max(periodSeconds, 3);
        this.pulsePeakStops = clamp(peakStops, 0, 1.5);
    }

    private tickLightPulse(dt: number): void {
        if (!this.pulseOn && this.pulseStops <= 0) return;
        if (this.pulseOn) {
            this.pulseElapsed += dt;
            // 0 -> peak -> 0 as a raised cosine, so it starts from the normal picture with no step.
            this.pulseStops = this.pulsePeakStops * 0.5 * (1 - Math.cos((2 * Math.PI * this.pulseElapsed) / this.pulsePeriod));
        } else {
            this.pulseStops = Math.max(0, this.pulseStops - (dt * Math.max(this.pulsePeakStops, 0.1)) / PULSE_RAMP_SECONDS);
        }
    }

    private stopActiveTransition(): void {
        ++this.transitionSerial;
        this.easing = false;
        this.easePaused = false;
        this.orbiting = false;
        this.orbitEasing = false;
        this.pathing = false;
        this.following = false;
        this.followTarget = null;
        this.timers.clearTimer(this.fadeOutTimer);
        this.timers.clearTimer(this.fadeInTimer);
    }

    private finishTransition(transitionId: number): void {
        // A newer transition has replaced this one; its own completion will report.
        if (transitionId !== this.activeTransitionId) return;
        if (this.easing) this.snapTo(this.endTransform, this.endFov);
        this.easing = false;
        // A quiet ease (id 0) belongs to no journey move.
        if (transitionId === 0) return;
        this.onFinished(this.activeTransitionId);
    }

    /** A move that replaces a fade-and-cut halfway must never leave the picture dimmed. */
    private clearFade(seconds: number, color: string): void {
        if (this.fadeAmount > 0) this.startFade(0, Math.max(seconds, 0.05), color, false);
    }

    /** Starts from the current fade amount so an interrupted fade never pops. */
    private startFade(to: number, seconds: number, color: string, _hold: boolean): void {
        this.fadeColor = color;
        this.fadeFrom = this.fadeAmount;
        this.fadeTo = to;
        this.fadeTime = Math.max(seconds, 1e-3);
        this.fadeElapsed = 0;
        this.fading = true;
    }

    private tickFade(dt: number): void {
        if (!this.fading) return;
        this.fadeElapsed += dt;
        const t = clamp(this.fadeElapsed / this.fadeTime, 0, 1);
        this.fadeAmount = lerp(this.fadeFrom, this.fadeTo, t);
        if (t >= 1) this.fading = false;
    }
}
