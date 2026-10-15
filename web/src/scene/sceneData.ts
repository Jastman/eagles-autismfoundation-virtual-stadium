// web/data/scene.json (tools/build_scene_data.mjs, from the Unreal level) and its transforms in the ported Unreal math.
import sceneJson from "../../data/scene.json";
import { Quat, Rotator, Transform, Vec3 } from "../util/ue";
import type { GeoreferenceData } from "./geo";

export interface XF {
    l: number[];
    q: number[];
    s: number[];
}

export type Shape = "cube" | "cylinder" | "sphere" | "cone" | "plane" | "box" | "car" | "person";

export interface Part extends Partial<XF> {
    shape: Shape;
    colour: number[];
    hidden?: boolean;
    bounds?: { o: number[]; e: number[] };
    mesh?: string;
    instances?: XF[];
}

export interface CueData {
    id: string;
    trigger: "Arrival" | "Preview";
    start: XF;
    playSeconds: number;
    playDelaySeconds: number;
    linear: boolean;
    hiddenBefore: boolean;
    hiddenAfter: boolean;
    spectacle: boolean;
    moving: boolean;
    fireworks?: {
        burstOffsets: number[][];
        burstColours: number[][];
        sparksPerBurst: number;
        burstRadius: number;
        burstSeconds: number;
        burstInterval: number;
        sparkSize: number;
    };
}

export interface ActorData {
    label: string;
    class: string;
    tags: string[];
    root: XF;
    parts: Part[];
    text?: { text: string; worldSize: number; colour: { r: number; g: number; b: number; a: number }; align: string; local: XF };
    boxes?: { name: string; extent: number[]; world: XF }[];
    cue?: CueData;
    attachedTo?: string;
    eagle?: boolean;
}

export interface SceneData {
    georeference: GeoreferenceData;
    contextTilesetIonAssetId: number;
    footprintLonLat: number[][];
    targets: Record<string, { l: number[]; r: number[]; fov: number }>;
    paths: { name: string; from: string; to: string; travelSeconds: number; cutAtEnd: boolean; waypoints: number[][] }[];
    places: Record<string, number[]>;
    actors: ActorData[];
}

export const sceneData = sceneJson as unknown as SceneData;

export const toTransform = (x: XF): Transform =>
    new Transform(new Quat(x.q[0], x.q[1], x.q[2], x.q[3]).normalized(), new Vec3(x.l[0], x.l[1], x.l[2]), new Vec3(x.s[0], x.s[1], x.s[2]));

export const vec = (a: number[]): Vec3 => new Vec3(a[0], a[1], a[2]);

export function targetTransform(t: { l: number[]; r: number[] }): Transform {
    // ALFFCameraTarget::GetCameraTransform: roll forced to zero.
    return Transform.fromRotator(new Rotator(t.r[0], t.r[1], 0), vec(t.l));
}

/** Linear colour (Unreal material parameters) to display sRGB 0..1. */
export function linearToSrgb(c: number): number {
    const v = Math.min(Math.max(c, 0), 1);
    return v <= 0.0031308 ? v * 12.92 : 1.055 * Math.pow(v, 1 / 2.4) - 0.055;
}
