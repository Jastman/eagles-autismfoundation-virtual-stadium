// What the journey needs from the level: the pieces ULFFJourneySubsystem finds with actor iterators and tags in Unreal.
// scene/world.ts implements it over the CesiumJS scene; tests can supply a stub.
import type { Quat, Transform, Vec3 } from "../util/ue";
import type { CueState } from "./types";

export interface CameraTarget {
    id: string;
    /** World pose with roll forced to zero (ALFFCameraTarget::GetCameraTransform). */
    transform: Transform;
    /** Unreal FieldOfView (horizontal, at the rig's 16:9 reference). */
    fov: number;
}

export interface CameraPath {
    fromTargetId: string;
    toTargetId: string;
    /** World-space points after the camera's own start, in order. */
    waypoints: Vec3[];
    travelSeconds: number;
    cutAtEnd: boolean;
    name: string;
}

export type CueTrigger = "Arrival" | "Preview";

export interface CueActor {
    cueId: string;
    trigger: CueTrigger;
    spectacle: boolean;
    isFireworks: boolean;
    playDelaySeconds: number;
    playSeconds: number;
    applyState(state: CueState): void;
    play(instant: boolean): void;
    isPlaying(): boolean;
    isHidden(): boolean;
    totalSeconds(): number;
    getLocation(): Vec3;
    getQuat(): Quat;
}

export interface WorldLabel {
    tags: string[];
    location: Vec3;
    setHidden(hidden: boolean): void;
}

export interface QuietZone {
    kind: "Sensory" | "Quiet";
    /** Distance from the zone (0 inside), cm. */
    distanceTo(point: Vec3): number;
    /** Blend distance outside the zone, cm. */
    blend: number;
}

export interface JourneyWorld {
    siteFrame: Transform;
    findTarget(id: string): CameraTarget | undefined;
    cameraPaths: CameraPath[];
    cueActors: CueActor[];
    labels: WorldLabel[];
    /** Actors tagged LFF_Place_*: a place marked by one stands where it is. */
    placeActorLocation(tag: string): Vec3 | undefined;
    /** Crowd Low / Off, and whether visible fans may animate (off with less movement). */
    setCrowd(shown: boolean, animate: boolean): void;
    /** The parking lot's cars, hidden while the camera is inside the stadium. */
    isInsideStadium(point: Vec3): boolean;
    setExteriorHidden(hidden: boolean): void;
    quietZones: QuietZone[];
    /** Swept-sphere test along a straight camera move, both ways (Unreal's SweepSingleByChannel on Visibility). */
    isPathBlocked(from: Vec3, to: Vec3, radius: number): boolean;
    /** The picked level's height band highlight; null turns it off. */
    setLevelHighlight(band: { minZ: number; maxZ: number } | null): void;
    /** Viewport aspect (width / height). */
    viewportAspect(): number;
}
