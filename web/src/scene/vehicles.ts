// The level's City Sample cars as shaped stand-ins until licensed models are exported (Docs/WEB_PARITY.md W5): one body
// style per City Sample vehicle number, each with a shaped body, glass cabin, roof, tyres and rims, and lights, in the
// paint colour the level gave it. Sizes are typical for each style; the cars stand where the level put them (the
// actor's origin is on the ground, midway between the axles, facing +X).
import {
    BoundingSphere, Color, ColorGeometryInstanceAttribute, ComponentDatatype, CylinderGeometry, Geometry,
    GeometryAttribute, GeometryAttributes, GeometryInstance, Matrix4, PerInstanceColorAppearance, PrimitiveType,
} from "cesium";
import { Rotator, Transform, Vec3 } from "../util/ue";
import { ueTransformToLocalMatrix } from "./geo";

/** A body style (cm). Cabin x positions are from the car's centre, + forward. */
interface CarSpec {
    length: number;
    width: number;
    height: number;
    wheelbase: number;
    wheelRadius: number;
    /** Top of the doors, where the glass begins. */
    belt: number;
    /** Bottom of the body sills. */
    sill: number;
    /** Base of the windscreen, top of the windscreen, top of the rear window, base of the rear window. */
    screenBase: number;
    roofFront: number;
    roofRear: number;
    rearBase: number;
}

const STYLES: Record<string, CarSpec> = {
    // vehicle02: four-door sedan
    "02": { length: 478, width: 184, height: 144, wheelbase: 282, wheelRadius: 33, belt: 96, sill: 30, screenBase: 128, roofFront: 52, roofRear: -92, rearBase: -158 },
    // vehicle03: compact hatchback
    "03": { length: 422, width: 178, height: 148, wheelbase: 262, wheelRadius: 31, belt: 95, sill: 29, screenBase: 118, roofFront: 44, roofRear: -148, rearBase: -192 },
    // vehicle05: mid-size SUV
    "05": { length: 482, width: 194, height: 176, wheelbase: 286, wheelRadius: 37, belt: 112, sill: 40, screenBase: 136, roofFront: 66, roofRear: -206, rearBase: -228 },
    // vehicle06: two-door coupe
    "06": { length: 456, width: 186, height: 132, wheelbase: 270, wheelRadius: 33, belt: 90, sill: 28, screenBase: 104, roofFront: 18, roofRear: -82, rearBase: -168 },
    // vehicle07: full-size sedan
    "07": { length: 502, width: 190, height: 148, wheelbase: 296, wheelRadius: 34, belt: 98, sill: 31, screenBase: 136, roofFront: 56, roofRear: -102, rearBase: -170 },
};

/** Flat-shaded triangles as a Cesium geometry (indexed, so it can batch with the library's indexed shapes). */
function triangles(values: Float64Array, normals: Float32Array): Geometry {
    const count = values.length / 3;
    const indices = count < 65536 ? new Uint16Array(count) : new Uint32Array(count);
    for (let i = 0; i < count; ++i) indices[i] = i;
    return new Geometry({
        attributes: {
            position: new GeometryAttribute({ componentDatatype: ComponentDatatype.DOUBLE, componentsPerAttribute: 3, values }),
            normal: new GeometryAttribute({ componentDatatype: ComponentDatatype.FLOAT, componentsPerAttribute: 3, values: normals }),
        } as unknown as GeometryAttributes,
        indices,
        primitiveType: PrimitiveType.TRIANGLES,
        boundingSphere: BoundingSphere.fromVertices(Array.from(values)),
    });
}

/** Builds a flat-shaded prism from a convex side profile (x, z in cm) extruded to ±halfWidth, tapering by `inset` at the top. */
function prism(profile: [number, number][], halfWidth: number, topInset = 0, topZ = Infinity): Geometry {
    const positions: number[] = [];
    const normals: number[] = [];
    const zMin = Math.min(...profile.map((p) => p[1]));
    const zMax = Math.max(...profile.map((p) => p[1]));
    // Tumblehome: the cabin narrows toward the roof.
    const half = (z: number) => halfWidth - (z >= Math.min(topZ, zMax) ? topInset : topInset * Math.max(0, (z - zMin) / Math.max(1, zMax - zMin)));
    const v = (x: number, y: number, z: number) => new Vec3(x, y, z);
    const tri = (a: Vec3, b: Vec3, c: Vec3) => {
        const cr = b.sub(a).cross(c.sub(a));
        const len = Math.hypot(cr.x, cr.y, cr.z) || 1;
        const n = cr.scale(1 / len);
        for (const p of [a, b, c]) {
            positions.push(p.x, p.y, p.z);
            normals.push(n.x, n.y, n.z);
        }
    };
    const n = profile.length;
    // The two sides (fans; the profile is convex), wound so normals face out.
    for (let i = 1; i < n - 1; ++i) {
        const [a, b, c] = [profile[0], profile[i], profile[i + 1]];
        tri(v(a[0], half(a[1]), a[1]), v(b[0], half(b[1]), b[1]), v(c[0], half(c[1]), c[1]));
        tri(v(a[0], -half(a[1]), a[1]), v(c[0], -half(c[1]), c[1]), v(b[0], -half(b[1]), b[1]));
    }
    // The band around the profile.
    for (let i = 0; i < n; ++i) {
        const a = profile[i], b = profile[(i + 1) % n];
        const a1 = v(a[0], half(a[1]), a[1]), b1 = v(b[0], half(b[1]), b[1]);
        const a2 = v(a[0], -half(a[1]), a[1]), b2 = v(b[0], -half(b[1]), b[1]);
        tri(a1, a2, b2);
        tri(a1, b2, b1);
    }
    // Make every face's normal point away from the prism's centre (the profile's winding is not assumed).
    const cx = profile.reduce((s, p) => s + p[0], 0) / n, cz = profile.reduce((s, p) => s + p[1], 0) / n;
    for (let t = 0; t < positions.length; t += 9) {
        const mx = (positions[t] + positions[t + 3] + positions[t + 6]) / 3 - cx;
        const my = (positions[t + 1] + positions[t + 4] + positions[t + 7]) / 3;
        const mz = (positions[t + 2] + positions[t + 5] + positions[t + 8]) / 3 - cz;
        if (mx * normals[t] + my * normals[t + 1] + mz * normals[t + 2] < 0) {
            for (let k = 0; k < 9; ++k) normals[t + k] = -normals[t + k];
            // Swap two vertices to keep the winding consistent with the flipped normal.
            for (let k = 0; k < 3; ++k) [positions[t + 3 + k], positions[t + 6 + k]] = [positions[t + 6 + k], positions[t + 3 + k]];
        }
    }
    return triangles(new Float64Array(positions), new Float32Array(normals));
}

interface StyleGeometry {
    body: Geometry;
    bumpers: Geometry;
    glass: Geometry;
    roof: Geometry;
    lightsFront: Geometry;
    lightsRear: Geometry;
    spec: CarSpec;
}

const cache = new Map<string, StyleGeometry>();

function styleGeometry(vehicle: string): StyleGeometry {
    const key = STYLES[vehicle] ? vehicle : "02";
    const hit = cache.get(key);
    if (hit) return hit;
    const s = STYLES[key];
    const f = s.length / 2, r = -s.length / 2, hw = s.width / 2;
    const noseTop = s.belt - 14, tailTop = s.belt - 6;
    // Lower body: bumpers rounded by chamfers, bonnet sloping down to the nose, boot to the tail.
    const body = prism([
        [f - 6, s.sill + 6], [f, s.sill + 22], [f, noseTop - 10], [f - 14, noseTop], [s.screenBase, s.belt],
        [s.rearBase, s.belt], [r + 12, tailTop], [r, tailTop - 12], [r, s.sill + 22], [r + 6, s.sill + 6],
    ], hw - 2);
    const bumpers = prism([[f + 3, s.sill + 4], [f + 3, s.sill + 24], [f - 20, s.sill + 24], [f - 20, s.sill + 4]], hw - 6);
    const rearBumper = [[r - 3, s.sill + 4], [r + 20, s.sill + 4], [r + 20, s.sill + 24], [r - 3, s.sill + 24]] as [number, number][];
    // Glass cabin: windscreen, roofline and rear window, narrowing toward the roof.
    const glass = prism([[s.screenBase, s.belt], [s.roofFront, s.height - 3], [s.roofRear, s.height - 3], [s.rearBase, s.belt]], hw - 6, 16, s.height - 3);
    const roof = prism([[s.roofFront - 6, s.height - 4], [s.roofFront - 10, s.height], [s.roofRear + 8, s.height], [s.roofRear + 4, s.height - 4]], hw - 21);
    const lamp = (x0: number, x1: number, z: number, y: number) => prism([[x0, z - 7], [x1, z - 7], [x1, z + 7], [x0, z + 7]], y);
    const geometry: StyleGeometry = {
        body: mergePrisms([body, prism(rearBumper, hw - 6)]),
        bumpers,
        glass,
        roof,
        lightsFront: mergePrisms([lampAt(lamp(f - 3, f + 1, noseTop - 14, 16), hw - 30), lampAt(lamp(f - 3, f + 1, noseTop - 14, 16), -(hw - 30))]),
        lightsRear: mergePrisms([lampAt(lamp(r - 1, r + 3, tailTop - 16, 18), hw - 28), lampAt(lamp(r - 1, r + 3, tailTop - 16, 18), -(hw - 28))]),
        spec: s,
    };
    cache.set(key, geometry);
    return geometry;
}

/** Moves a geometry sideways by y (cm). */
function lampAt(g: Geometry, y: number): Geometry {
    const p = g.attributes.position!.values as Float64Array;
    const out = new Float64Array(p);
    for (let i = 1; i < out.length; i += 3) out[i] += y;
    return triangles(out, g.attributes.normal!.values as Float32Array);
}

function mergePrisms(parts: Geometry[]): Geometry {
    const pos = parts.flatMap((g) => Array.from(g.attributes.position!.values as Float64Array));
    const nor = parts.flatMap((g) => Array.from(g.attributes.normal!.values as Float32Array));
    return triangles(new Float64Array(pos), new Float32Array(nor));
}

const TYRE = new CylinderGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, length: 100, topRadius: 50, bottomRadius: 50, slices: 14 });
const RIM = new CylinderGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, length: 100, topRadius: 50, bottomRadius: 50, slices: 10 });

const GLASS = Color.fromBytes(28, 36, 44);
const TRIM = Color.fromBytes(34, 35, 38);
const TYRE_COLOUR = Color.fromBytes(22, 22, 24);
const RIM_COLOUR = Color.fromBytes(150, 152, 156);
const HEADLAMP = Color.fromBytes(222, 226, 228);
const TAIL_LAMP = Color.fromBytes(150, 24, 28);

const instance = (geometry: Geometry | CylinderGeometry, matrix: Matrix4, colour: Color) =>
    new GeometryInstance({ geometry, modelMatrix: matrix, attributes: { color: ColorGeometryInstanceAttribute.fromColor(colour) } });

/** A car's geometry instances in its actor's root frame (metres, via ueTransformToLocalMatrix like every other part). */
export function carInstances(vehicle: string, at: Transform, paint: Color): GeometryInstance[] {
    const g = styleGeometry(vehicle);
    const s = g.spec;
    const frame = ueTransformToLocalMatrix(at);
    const out = [
        instance(g.body, frame, paint),
        instance(g.roof, frame, paint),
        instance(g.bumpers, frame, TRIM),
        instance(g.glass, frame, GLASS),
        instance(g.lightsFront, frame, HEADLAMP),
        instance(g.lightsRear, frame, TAIL_LAMP),
    ];
    // Wheels: axles at ±wheelbase/2, tyres just inside the body's sides; the cylinder's axis turned from Z to Y.
    const axle = new Rotator(0, 0, 90).quaternion();
    for (const x of [s.wheelbase / 2, -s.wheelbase / 2]) {
        for (const side of [1, -1]) {
            const y = side * (s.width / 2 - 14);
            const tyre = new Transform(axle, new Vec3(x, y, s.wheelRadius), new Vec3(s.wheelRadius / 50, s.wheelRadius / 50, 0.22));
            const rim = new Transform(axle, new Vec3(x, y + side * 1.5, s.wheelRadius), new Vec3(s.wheelRadius * 0.62 / 50, s.wheelRadius * 0.62 / 50, 0.215));
            out.push(instance(TYRE, ueTransformToLocalMatrix(tyre.mul(at)), TYRE_COLOUR));
            out.push(instance(RIM, ueTransformToLocalMatrix(rim.mul(at)), RIM_COLOUR));
        }
    }
    return out;
}

