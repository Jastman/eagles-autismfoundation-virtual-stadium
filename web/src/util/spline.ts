// A stand-in for USplineComponent with curve points (SetSplinePoints): a smooth curve through every point, sampled by
// distance along it, which is all the camera rig asks of it (GetSplineLength, GetLocationAtDistanceAlongSpline).
import { Vec3 } from "./ue";

const SAMPLES_PER_SEGMENT = 64;

export class Spline {
    private samples: Vec3[] = [];
    private distances: number[] = [];

    constructor(points: Vec3[]) {
        this.setPoints(points);
    }

    setPoints(points: Vec3[]): void {
        this.samples = [];
        this.distances = [];
        if (points.length === 0) return;
        if (points.length === 1) {
            this.samples = [points[0].clone()];
            this.distances = [0];
            return;
        }
        // Unreal's auto tangents on curve points are Catmull-Rom like (half the neighbour difference), clamped at the ends.
        const tangent = (i: number): Vec3 => {
            const prev = points[Math.max(i - 1, 0)];
            const next = points[Math.min(i + 1, points.length - 1)];
            return next.sub(prev).scale(i === 0 || i === points.length - 1 ? 1 : 0.5);
        };
        for (let i = 0; i < points.length - 1; ++i) {
            const p0 = points[i], p1 = points[i + 1], m0 = tangent(i), m1 = tangent(i + 1);
            for (let s = i === 0 ? 0 : 1; s <= SAMPLES_PER_SEGMENT; ++s) {
                const t = s / SAMPLES_PER_SEGMENT, t2 = t * t, t3 = t2 * t;
                const h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
                this.samples.push(p0.scale(h00).add(m0.scale(h10)).add(p1.scale(h01)).add(m1.scale(h11)));
            }
        }
        let total = 0;
        this.distances.push(0);
        for (let i = 1; i < this.samples.length; ++i) {
            total += Vec3.dist(this.samples[i], this.samples[i - 1]);
            this.distances.push(total);
        }
    }

    length(): number {
        return this.distances.length > 0 ? this.distances[this.distances.length - 1] : 0;
    }

    locationAtDistance(distance: number): Vec3 {
        if (this.samples.length === 0) return new Vec3();
        if (distance <= 0) return this.samples[0].clone();
        if (distance >= this.length()) return this.samples[this.samples.length - 1].clone();
        let lo = 0, hi = this.distances.length - 1;
        while (hi - lo > 1) {
            const mid = (lo + hi) >> 1;
            if (this.distances[mid] <= distance) lo = mid;
            else hi = mid;
        }
        const span = this.distances[hi] - this.distances[lo];
        const t = span > 0 ? (distance - this.distances[lo]) / span : 0;
        return Vec3.lerp(this.samples[lo], this.samples[hi], t);
    }
}
