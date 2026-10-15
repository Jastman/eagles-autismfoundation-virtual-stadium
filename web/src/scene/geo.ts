// The Unreal level's coordinate frames on the CesiumJS globe (Docs/CESIUM_PLACEMENT.md, ADR-0001).
//
//   Unreal world  Cesium for Unreal places its georeference origin (midfield, turf) at Unreal (0, 0, 0) with +X East,
//                 +Y South, +Z Up, centimetres. Unreal's world is flat around that origin, so straight lines and
//                 constant "up" there are straight lines and the origin's up here.
//   Site frame    LFF_SiteRoot (yaw -6.5 deg): +X across the field to the east stand, +Y to the south end, +Z up. Camera
//                 targets, places, props and crowd seats are all stored relative to it.
//
// Everything above Cesium in this web build (journey, camera rig, cues) works in Unreal world coordinates with the ported
// Unreal math (util/ue.ts); only scene/ code touches Cesium's Earth-fixed frame, through this file.
import { Cartesian3, Matrix4, Transforms } from "cesium";
import type { Camera, PerspectiveFrustum } from "cesium";
import { Rotator, Transform, Vec3, deg2rad, rad2deg } from "../util/ue";

export interface GeoreferenceData {
    originLatitude: number;
    originLongitude: number;
    /** Metres above the WGS84 ellipsoid. */
    originHeight: number;
    /** LFF_SiteRoot's world transform (location cm, rotator deg). */
    siteRoot: { location: { x: number; y: number; z: number }; rotation: { pitch: number; yaw: number; roll: number } };
}

/** Placement revision 2026-09-28a (Docs/CESIUM_PLACEMENT.md, V1 amendments): used if the level data is missing. */
export const DEFAULT_GEOREFERENCE: GeoreferenceData = {
    originLatitude: 39.9008036,
    originLongitude: -75.167479,
    originHeight: -31.04,
    siteRoot: { location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: -6.5, roll: 0 } },
};

/**
 * An Unreal transform (cm, left-handed) as a right-handed matrix in metres in the same axes mirrored on Y (East, North,
 * Up when the transform is in Unreal world space). With F = diag(1, -1, 1): (F M F) e0 = F x, e1 -> -F y, e2 -> F z, and
 * the translation is F p. Exact for shapes symmetric across their own XZ plane, which every stand-in shape is. Shape
 * units stay centimetres: a 100-unit cube is 1 m.
 */
export function ueTransformToLocalMatrix(t: Transform, result = new Matrix4()): Matrix4 {
    const x = t.rotation.rotateVector(new Vec3(t.scale3D.x, 0, 0));
    const y = t.rotation.rotateVector(new Vec3(0, t.scale3D.y, 0));
    const z = t.rotation.rotateVector(new Vec3(0, 0, t.scale3D.z));
    const p = t.translation;
    // Column-major.
    return Matrix4.fromArray([
        x.x / 100, -x.y / 100, x.z / 100, 0,
        -y.x / 100, y.y / 100, -y.z / 100, 0,
        z.x / 100, -z.y / 100, z.z / 100, 0,
        p.x / 100, -p.y / 100, p.z / 100, 1,
    ], 0, result);
}

export class Geo {
    readonly origin: Cartesian3;
    /** East-North-Up at the origin to Earth-fixed. */
    readonly enuToFixed: Matrix4;
    readonly fixedToEnu: Matrix4;
    /** LFF_SiteRoot's world transform (scale removed), the frame places are written in. */
    readonly siteFrame: Transform;
    readonly upFixed: Cartesian3;

    constructor(readonly data: GeoreferenceData = DEFAULT_GEOREFERENCE) {
        this.origin = Cartesian3.fromDegrees(data.originLongitude, data.originLatitude, data.originHeight);
        this.enuToFixed = Transforms.eastNorthUpToFixedFrame(this.origin);
        this.fixedToEnu = Matrix4.inverseTransformation(this.enuToFixed, new Matrix4());
        this.upFixed = Cartesian3.normalize(Matrix4.multiplyByPointAsVector(this.enuToFixed, new Cartesian3(0, 0, 1), new Cartesian3()), new Cartesian3());
        this.siteFrame = Transform.fromRotator(Rotator.from(data.siteRoot.rotation), Vec3.from(data.siteRoot.location));
    }

    /** Unreal world (cm, X East, Y South, Z Up) to Earth-fixed metres. */
    worldToFixed(p: Vec3, result = new Cartesian3()): Cartesian3 {
        return Matrix4.multiplyByPoint(this.enuToFixed, new Cartesian3(p.x / 100, -p.y / 100, p.z / 100), result);
    }

    /** Earth-fixed metres to Unreal world cm. */
    fixedToWorld(p: Cartesian3): Vec3 {
        const enu = Matrix4.multiplyByPoint(this.fixedToEnu, p, new Cartesian3());
        return new Vec3(enu.x * 100, -enu.y * 100, enu.z * 100);
    }

    worldDirToFixed(d: Vec3, result = new Cartesian3()): Cartesian3 {
        Matrix4.multiplyByPointAsVector(this.enuToFixed, new Cartesian3(d.x, -d.y, d.z), result);
        return Cartesian3.normalize(result, result);
    }

    siteToWorld(p: Vec3): Vec3 {
        return this.siteFrame.transformPosition(p);
    }

    /** A full Unreal world transform (with scale) as a Cesium model matrix. */
    worldTransformToModelMatrix(t: Transform, result = new Matrix4()): Matrix4 {
        return Matrix4.multiply(this.enuToFixed, ueTransformToLocalMatrix(t), result);
    }

    /**
     * Points the Cesium camera like an Unreal camera actor at `location` with `rotation` (no roll: the project never
     * rolls). `fov` is the Unreal FieldOfView: horizontal at the rig's 16:9 reference, with the vertical angle kept for
     * any aspect (AspectRatio_MaintainYFOV). Cesium's frustum takes the wider axis, so it is converted.
     */
    applyCamera(camera: Camera, location: Vec3, rotation: Rotator, fov: number): void {
        const destination = this.worldToFixed(location);
        const direction = this.worldDirToFixed(new Rotator(rotation.pitch, rotation.yaw, 0).vector());
        // No roll: up lies in the plane of the forward vector and the origin's up (Unreal's world Z).
        const along = Cartesian3.dot(this.upFixed, direction);
        const up = Cartesian3.subtract(this.upFixed, Cartesian3.multiplyByScalar(direction, along, new Cartesian3()), new Cartesian3());
        if (Cartesian3.magnitudeSquared(up) < 1e-10) {
            const right = this.worldDirToFixed(new Rotator(0, rotation.yaw + 90, 0).vector());
            Cartesian3.cross(right, direction, up);
        }
        Cartesian3.normalize(up, up);
        camera.setView({ destination, orientation: { direction, up } });
        const frustum = camera.frustum as PerspectiveFrustum;
        const ratio = frustum.aspectRatio ?? 0;
        const aspect = ratio > 0 && Number.isFinite(ratio) ? ratio : 16 / 9;
        const vfov = deg2rad(verticalFovFromAuthored(fov));
        frustum.fov = aspect >= 1 ? 2 * Math.atan(Math.tan(vfov / 2) * aspect) : vfov;
    }
}

/** Unreal FieldOfView (horizontal at the 16:9 reference) to the vertical angle the rig keeps. */
export function verticalFovFromAuthored(horizontalDeg: number): number {
    return rad2deg(2 * Math.atan(Math.tan(deg2rad(horizontalDeg) / 2) / (16 / 9)));
}

/** The horizontal field of view the screen shows for an Unreal FieldOfView at this aspect. */
export function effectiveHorizontalFov(authoredDeg: number, aspect: number): number {
    return rad2deg(2 * Math.atan(Math.tan(deg2rad(verticalFovFromAuthored(authoredDeg)) / 2) * aspect));
}
