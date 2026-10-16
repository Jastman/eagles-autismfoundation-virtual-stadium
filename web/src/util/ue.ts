// The Unreal math the journey and camera code rely on, with Unreal's conventions, so the ported code reads like the C++
// and gives the same numbers: left-handed, X forward, Y right, Z up, centimetres, rotators in degrees (Pitch, Yaw, Roll).
// In the stadium's Unreal world (Cesium georeference origin at midfield) +X is East, +Y South, +Z Up; scene/geo.ts maps
// that frame onto the globe.

export class Vec3 {
    constructor(public x = 0, public y = 0, public z = 0) {}

    static readonly zero = Object.freeze(new Vec3());

    clone(): Vec3 {
        return new Vec3(this.x, this.y, this.z);
    }
    add(o: Vec3): Vec3 {
        return new Vec3(this.x + o.x, this.y + o.y, this.z + o.z);
    }
    sub(o: Vec3): Vec3 {
        return new Vec3(this.x - o.x, this.y - o.y, this.z - o.z);
    }
    scale(s: number): Vec3 {
        return new Vec3(this.x * s, this.y * s, this.z * s);
    }
    mul(o: Vec3): Vec3 {
        return new Vec3(this.x * o.x, this.y * o.y, this.z * o.z);
    }
    dot(o: Vec3): number {
        return this.x * o.x + this.y * o.y + this.z * o.z;
    }
    cross(o: Vec3): Vec3 {
        return new Vec3(this.y * o.z - this.z * o.y, this.z * o.x - this.x * o.z, this.x * o.y - this.y * o.x);
    }
    size(): number {
        return Math.hypot(this.x, this.y, this.z);
    }
    sizeSquared(): number {
        return this.x * this.x + this.y * this.y + this.z * this.z;
    }
    size2D(): number {
        return Math.hypot(this.x, this.y);
    }
    safeNormal(tolerance = 1e-8): Vec3 {
        const sq = this.sizeSquared();
        return sq <= tolerance ? new Vec3() : this.scale(1 / Math.sqrt(sq));
    }
    /** FVector::GetClampedToMaxSize. */
    clampedToMaxSize(max: number): Vec3 {
        if (max < 1e-4) return new Vec3();
        const sq = this.sizeSquared();
        return sq > max * max ? this.scale(max / Math.sqrt(sq)) : this.clone();
    }
    isNearlyZero(tolerance = 1e-4): boolean {
        return Math.abs(this.x) <= tolerance && Math.abs(this.y) <= tolerance && Math.abs(this.z) <= tolerance;
    }
    equals(o: Vec3, tolerance = 1e-4): boolean {
        return Math.abs(this.x - o.x) <= tolerance && Math.abs(this.y - o.y) <= tolerance && Math.abs(this.z - o.z) <= tolerance;
    }
    static dist(a: Vec3, b: Vec3): number {
        return a.sub(b).size();
    }
    static dist2D(a: Vec3, b: Vec3): number {
        return Math.hypot(a.x - b.x, a.y - b.y);
    }
    static lerp(a: Vec3, b: Vec3, t: number): Vec3 {
        return new Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
    }
    /** FVector::Rotation: the rotator pointing along this vector (no roll). */
    rotation(): Rotator {
        return new Rotator(rad2deg(Math.atan2(this.z, Math.hypot(this.x, this.y))), rad2deg(Math.atan2(this.y, this.x)), 0);
    }
    static from(o: { X: number; Y: number; Z: number } | { x: number; y: number; z: number }): Vec3 {
        return "X" in o ? new Vec3(o.X, o.Y, o.Z) : new Vec3(o.x, o.y, o.z);
    }
}

export class Vec2 {
    constructor(public x = 0, public y = 0) {}
    add(o: Vec2): Vec2 {
        return new Vec2(this.x + o.x, this.y + o.y);
    }
    sub(o: Vec2): Vec2 {
        return new Vec2(this.x - o.x, this.y - o.y);
    }
    scale(s: number): Vec2 {
        return new Vec2(this.x * s, this.y * s);
    }
    size(): number {
        return Math.hypot(this.x, this.y);
    }
    isNearlyZero(tolerance = 1e-4): boolean {
        return Math.abs(this.x) <= tolerance && Math.abs(this.y) <= tolerance;
    }
}

/** FBox2D. */
export class Box2 {
    constructor(public min: Vec2, public max: Vec2, public valid = true) {}
    static invalid(): Box2 {
        return new Box2(new Vec2(), new Vec2(), false);
    }
    size(): Vec2 {
        return this.max.sub(this.min);
    }
    area(): number {
        const s = this.size();
        return s.x * s.y;
    }
    isInsideOrOn(p: Vec2): boolean {
        return p.x >= this.min.x && p.x <= this.max.x && p.y >= this.min.y && p.y <= this.max.y;
    }
    intersects(o: Box2): boolean {
        return !(this.min.x > o.max.x || o.min.x > this.max.x || this.min.y > o.max.y || o.min.y > this.max.y);
    }
    equals(o: Box2): boolean {
        if (this.valid !== o.valid) return false;
        if (!this.valid) return true;
        return this.min.x === o.min.x && this.min.y === o.min.y && this.max.x === o.max.x && this.max.y === o.max.y;
    }
}

export const deg2rad = (d: number): number => (d * Math.PI) / 180;
export const rad2deg = (r: number): number => (r * 180) / Math.PI;
export const clamp = (v: number, lo: number, hi: number): number => Math.min(Math.max(v, lo), hi);
export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

/** FMath::UnwindDegrees: into (-180, 180]. */
export function unwindDegrees(a: number): number {
    while (a > 180) a -= 360;
    while (a < -180) a += 360;
    return a;
}

/** FMath::InterpEaseInOut. */
export function interpEaseInOut(a: number, b: number, alpha: number, exp: number): number {
    const t = alpha < 0.5 ? 0.5 * Math.pow(2 * alpha, exp) : 1 - 0.5 * Math.pow(2 * (1 - alpha), exp);
    return lerp(a, b, t);
}

/** FMath::SmoothStep. */
export function smoothStep(a: number, b: number, x: number): number {
    if (x < a) return 0;
    if (x >= b) return 1;
    const t = (x - a) / (b - a);
    return t * t * (3 - 2 * t);
}

/** FMath::FInterpTo. */
export function fInterpTo(current: number, target: number, dt: number, speed: number): number {
    if (speed <= 0) return target;
    const dist = target - current;
    if (dist * dist < 1e-8) return target;
    return current + dist * clamp(dt * speed, 0, 1);
}

/** FMath::FInterpConstantTo. */
export function fInterpConstantTo(current: number, target: number, dt: number, speed: number): number {
    const dist = target - current;
    if (dist * dist < 1e-8) return target;
    const step = speed * dt;
    return current + clamp(dist, -step, step);
}

/** FMath::VInterpTo. */
export function vInterpTo(current: Vec3, target: Vec3, dt: number, speed: number): Vec3 {
    if (speed <= 0) return target.clone();
    const dist = target.sub(current);
    if (dist.sizeSquared() < 1e-8) return target.clone();
    return current.add(dist.scale(clamp(dt * speed, 0, 1)));
}

export function getMappedRangeValueClamped(inA: number, inB: number, outA: number, outB: number, value: number): number {
    const t = clamp((value - inA) / (inB - inA), 0, 1);
    return lerp(outA, outB, t);
}

export class Rotator {
    constructor(public pitch = 0, public yaw = 0, public roll = 0) {}

    clone(): Rotator {
        return new Rotator(this.pitch, this.yaw, this.roll);
    }

    /** FRotator::Vector: the unit forward vector. */
    vector(): Vec3 {
        const p = deg2rad(this.pitch);
        const y = deg2rad(this.yaw);
        return new Vec3(Math.cos(p) * Math.cos(y), Math.cos(p) * Math.sin(y), Math.sin(p));
    }

    quaternion(): Quat {
        const p = deg2rad(this.pitch) * 0.5;
        const y = deg2rad(this.yaw) * 0.5;
        const r = deg2rad(this.roll) * 0.5;
        const sp = Math.sin(p), cp = Math.cos(p), sy = Math.sin(y), cy = Math.cos(y), sr = Math.sin(r), cr = Math.cos(r);
        return new Quat(
            cr * sp * sy - sr * cp * cy,
            -cr * sp * cy - sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy,
        );
    }

    equals(o: Rotator, tolerance = 1e-4): boolean {
        return Math.abs(unwindDegrees(this.pitch - o.pitch)) <= tolerance && Math.abs(unwindDegrees(this.yaw - o.yaw)) <= tolerance
            && Math.abs(unwindDegrees(this.roll - o.roll)) <= tolerance;
    }

    /** FRotator::GetNormalized. */
    normalized(): Rotator {
        return new Rotator(unwindDegrees(this.pitch), unwindDegrees(this.yaw), unwindDegrees(this.roll));
    }

    static from(o: { pitch: number; yaw: number; roll: number } | { Pitch: number; Yaw: number; Roll: number }): Rotator {
        return "Pitch" in o ? new Rotator(o.Pitch, o.Yaw, o.Roll) : new Rotator(o.pitch, o.yaw, o.roll);
    }
}

/** FMath::RInterpTo. */
export function rInterpTo(current: Rotator, target: Rotator, dt: number, speed: number): Rotator {
    if (dt === 0 || current.equals(target)) return current.clone();
    if (speed <= 0) return target.clone();
    const alpha = clamp(dt * speed, 0, 1);
    const delta = new Rotator(target.pitch - current.pitch, target.yaw - current.yaw, target.roll - current.roll).normalized();
    if (delta.equals(new Rotator(), 1e-3)) return target.clone();
    return new Rotator(current.pitch + delta.pitch * alpha, current.yaw + delta.yaw * alpha, current.roll + delta.roll * alpha).normalized();
}

export class Quat {
    constructor(public x = 0, public y = 0, public z = 0, public w = 1) {}

    static readonly identity = Object.freeze(new Quat());

    clone(): Quat {
        return new Quat(this.x, this.y, this.z, this.w);
    }

    /** Hamilton product: (this * o) applies o first, then this (as FQuat). */
    mul(o: Quat): Quat {
        return new Quat(
            this.w * o.x + this.x * o.w + this.y * o.z - this.z * o.y,
            this.w * o.y - this.x * o.z + this.y * o.w + this.z * o.x,
            this.w * o.z + this.x * o.y - this.y * o.x + this.z * o.w,
            this.w * o.w - this.x * o.x - this.y * o.y - this.z * o.z,
        );
    }

    inverse(): Quat {
        return new Quat(-this.x, -this.y, -this.z, this.w);
    }

    normalized(): Quat {
        const n = Math.hypot(this.x, this.y, this.z, this.w);
        return n > 1e-12 ? new Quat(this.x / n, this.y / n, this.z / n, this.w / n) : new Quat();
    }

    rotateVector(v: Vec3): Vec3 {
        // v' = v + 2w(q x v) + 2 q x (q x v)
        const q = new Vec3(this.x, this.y, this.z);
        const t = q.cross(v).scale(2);
        return v.add(t.scale(this.w)).add(q.cross(t));
    }

    unrotateVector(v: Vec3): Vec3 {
        return this.inverse().rotateVector(v);
    }

    forward(): Vec3 {
        return this.rotateVector(new Vec3(1, 0, 0));
    }
    right(): Vec3 {
        return this.rotateVector(new Vec3(0, 1, 0));
    }
    up(): Vec3 {
        return this.rotateVector(new Vec3(0, 0, 1));
    }

    /** FQuat::Rotator. */
    rotator(): Rotator {
        const singularityTest = this.z * this.x - this.w * this.y;
        const yawY = 2 * (this.w * this.z + this.x * this.y);
        const yawX = 1 - 2 * (this.y * this.y + this.z * this.z);
        const threshold = 0.4999995;
        const yaw = rad2deg(Math.atan2(yawY, yawX));
        if (singularityTest < -threshold) {
            return new Rotator(-90, yaw, unwindDegrees(-yaw - 2 * rad2deg(Math.atan2(this.x, this.w))));
        }
        if (singularityTest > threshold) {
            return new Rotator(90, yaw, unwindDegrees(yaw - 2 * rad2deg(Math.atan2(this.x, this.w))));
        }
        return new Rotator(
            rad2deg(Math.asin(2 * singularityTest)),
            yaw,
            rad2deg(Math.atan2(-2 * (this.w * this.x + this.y * this.z), 1 - 2 * (this.x * this.x + this.y * this.y))),
        );
    }

    /** FQuat::Slerp (shortest path, normalized). */
    static slerp(a: Quat, b: Quat, t: number): Quat {
        let cos = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        const sign = cos < 0 ? -1 : 1;
        cos *= sign;
        let s0: number, s1: number;
        if (cos < 0.9999) {
            const omega = Math.acos(cos);
            const inv = 1 / Math.sin(omega);
            s0 = Math.sin((1 - t) * omega) * inv;
            s1 = sign * Math.sin(t * omega) * inv;
        } else {
            s0 = 1 - t;
            s1 = sign * t;
        }
        return new Quat(s0 * a.x + s1 * b.x, s0 * a.y + s1 * b.y, s0 * a.z + s1 * b.z, s0 * a.w + s1 * b.w).normalized();
    }
}

/** FTransform without non-uniform scale shear: rotation, translation, scale. Composition matches Unreal: (A * B) applies A, then B. */
export class Transform {
    constructor(public rotation = new Quat(), public translation = new Vec3(), public scale3D = new Vec3(1, 1, 1)) {}

    static fromRotator(r: Rotator, t: Vec3, s = new Vec3(1, 1, 1)): Transform {
        return new Transform(r.quaternion(), t, s);
    }

    clone(): Transform {
        return new Transform(this.rotation.clone(), this.translation.clone(), this.scale3D.clone());
    }

    getLocation(): Vec3 {
        return this.translation;
    }

    rotator(): Rotator {
        return this.rotation.rotator();
    }

    transformPosition(p: Vec3): Vec3 {
        return this.rotation.rotateVector(p.mul(this.scale3D)).add(this.translation);
    }

    transformPositionNoScale(p: Vec3): Vec3 {
        return this.rotation.rotateVector(p).add(this.translation);
    }

    inverseTransformPosition(p: Vec3): Vec3 {
        const local = this.rotation.unrotateVector(p.sub(this.translation));
        return new Vec3(local.x / (this.scale3D.x || 1), local.y / (this.scale3D.y || 1), local.z / (this.scale3D.z || 1));
    }

    inverseTransformPositionNoScale(p: Vec3): Vec3 {
        return this.rotation.unrotateVector(p.sub(this.translation));
    }

    transformVector(v: Vec3): Vec3 {
        return this.rotation.rotateVector(v.mul(this.scale3D));
    }

    unitAxis(axis: "X" | "Y" | "Z"): Vec3 {
        return this.rotation.rotateVector(axis === "X" ? new Vec3(1, 0, 0) : axis === "Y" ? new Vec3(0, 1, 0) : new Vec3(0, 0, 1));
    }

    /** this * parent: this transform expressed in the parent's space, then the parent applied (Unreal's A * B). */
    mul(parent: Transform): Transform {
        return new Transform(
            parent.rotation.mul(this.rotation).normalized(),
            parent.transformPosition(this.translation),
            this.scale3D.mul(parent.scale3D),
        );
    }
}
