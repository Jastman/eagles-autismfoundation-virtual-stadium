// The bowl crowd (ALFFStadiumCrowdSpawner + ULFFSeatSpawnDataGenerator): Content/LFF/Crowd/seats.json and the measured
// treads, the same seat selection (CollectStandPoints, RepresentationCounts and the FRandomStream shuffle, seed 1018),
// and the same billboard fans (tools/draw_crowd_fans.py runs Unreal's drawing code): 60 x 240 cm standing cards, 70 cm
// in front of each seat, facing the seat's way.
//
// Unreal draws the nearest 48 neighbours as animated MetaHumans and the next rows with Mass; the web draws those people
// as cards too (Docs/WEB_PARITY.md W4): standing ones as the standing card, seated ones as the card's upper body at hip
// height. Crowd Off and Calm Mode hide it.
import {
    BoundingSphere, ComponentDatatype, Geometry, GeometryAttribute, GeometryAttributes, GeometryInstance, Material,
    MaterialAppearance, Primitive, PrimitiveType, ShadowMode,
} from "cesium";
import type { Scene } from "cesium";
import { Rotator, Transform, Vec3 } from "../util/ue";
import type { Geo } from "./geo";

interface SeatsFile {
    /** LFF_SiteRoot local, cm: [x, y, floor z, yaw deg]. */
    seats: number[][];
}
interface TreadsFile {
    /** Per seat: [standing floor z, seated floor z] (cm), null where unmeasured. */
    tread_z_cm: (number | null)[][];
    standing_offset_cm: number;
    seated_offset_cm: number;
    seated_lateral_offset_cm: number;
}

/** The level's SI_StadiumCrowd and the class defaults it keeps. */
const SPAWNER = {
    Occupancy: 0.7,
    MaxPeople: 6000,
    BillboardSize: { x: 60, y: 240 },
    Looks: 8,
    ClearRadius: 50,
    ClearAhead: 0,
    DetailedRadius: 700,
    MaxDetailedFans: 48,
    MetaHumanRadius: 2000,
    RandomSelectionSeed: 1018,
};
/** The seated card: the fan art from the hips (80 cm) to above a raised hand (215 cm), its bottom at hip height. */
const SEATED = { fromCm: 80, toCm: 215, hipCm: 45 };

/** Unreal's FRandomStream, bit for bit: the same seed gives the same shuffle as the packaged build. */
export class RandomStream {
    private seed: number;
    private readonly bits = new DataView(new ArrayBuffer(4));

    constructor(seed: number) {
        this.seed = seed | 0;
    }

    fraction(): number {
        this.seed = (Math.imul(this.seed, 196314165) + 907633515) | 0;
        this.bits.setUint32(0, 0x3f800000 | (this.seed >>> 9));
        return Math.fround(this.bits.getFloat32(0) - 1);
    }

    randHelper(a: number): number {
        return a > 0 ? Math.min(Math.trunc(Math.fround(this.fraction() * Math.fround(a))), a - 1) : 0;
    }
}

/** LFFCrowdRules::IsCameraSpace. */
export function isCameraSpace(point: Vec3, view: Transform, radius: number, ahead: number): boolean {
    const dx = point.x - view.translation.x, dy = point.y - view.translation.y;
    if (dx * dx + dy * dy < radius * radius) return true;
    const relative = view.rotation.unrotateVector(point.sub(view.translation));
    return ahead > 0 && relative.x > 0 && relative.x < ahead && Math.abs(relative.y) < relative.x * 0.8 + radius;
}

/** LFFCrowdRules::RepresentationCounts. */
export function representationCounts(detailed: number, near: number, far: number, occupancy: number, massLimit: number) {
    const fraction = Math.min(Math.max(occupancy, 0), 1);
    const nearOccupied = Math.round(Math.max(0, near) * fraction);
    const mass = Math.min(Math.max(0, massLimit), nearOccupied);
    const overflow = nearOccupied - mass;
    return { detailed: Math.round(Math.max(0, detailed) * fraction), mass, overflow, far: Math.round(Math.max(0, far) * fraction) + overflow };
}

interface Card {
    at: Transform;
    look: number;
    seated: boolean;
}

/** Which seats are occupied and how, exactly as ALFFStadiumCrowdSpawner::BeginPlay chooses them (world, cm). */
export function chooseCards(seats: SeatsFile, treads: TreadsFile, site: Transform, seatView: Transform | null): Card[] {
    const near: Transform[] = [], far: Transform[] = [];
    const detailed: { standing: Transform; seated: Transform; d2: number }[] = [];
    seats.seats.forEach((seat, index) => {
        const floor = treads.tread_z_cm[index];
        if (!floor || floor[0] === null || floor[1] === null) return;
        const yaw = new Rotator(0, seat[3], 0);
        const local = new Vec3(seat[0], seat[1], floor[0]).add(yaw.vector().scale(treads.standing_offset_cm));
        const sitting = new Vec3(seat[0], seat[1], floor[1]).add(yaw.quaternion().rotateVector(new Vec3(treads.seated_offset_cm, treads.seated_lateral_offset_cm, 0)));
        const world = site.transformPosition(local);
        if (seatView && isCameraSpace(world, seatView, SPAWNER.ClearRadius, SPAWNER.ClearAhead)) return;
        const toSeat = seatView ? world.sub(seatView.translation) : null;
        const d2 = toSeat ? toSeat.x * toSeat.x + toSeat.y * toSeat.y + toSeat.z * toSeat.z : Number.MAX_VALUE;
        const rotation = site.rotation.mul(yaw.quaternion());
        if (d2 < SPAWNER.DetailedRadius ** 2) {
            detailed.push({ standing: new Transform(rotation, world), seated: new Transform(rotation, site.transformPosition(sitting)), d2 });
        } else {
            (d2 < SPAWNER.MetaHumanRadius ** 2 ? near : far).push(new Transform(rotation, world));
        }
    });
    detailed.sort((a, b) => a.d2 - b.d2);
    for (const extra of detailed.splice(SPAWNER.MaxDetailedFans)) {
        (extra.d2 < SPAWNER.MetaHumanRadius ** 2 ? near : far).push(extra.standing);
    }
    const random = new RandomStream(SPAWNER.RandomSelectionSeed);
    for (const points of [near, far]) {
        for (let i = points.length - 1; i > 0; --i) {
            const j = random.randHelper(i + 1);
            [points[i], points[j]] = [points[j], points[i]];
        }
    }
    const counts = representationCounts(detailed.length, near.length, far.length, SPAWNER.Occupancy, SPAWNER.MaxPeople);
    const billboards = far.slice(0, counts.far - counts.overflow).concat(near.slice(counts.mass, counts.mass + counts.overflow));
    const cards: Card[] = billboards.map((at, i) => ({ at, look: i % SPAWNER.Looks, seated: false }));
    near.slice(0, counts.mass).forEach((at, i) => cards.push({ at, look: (i * 5 + 3) % SPAWNER.Looks, seated: false }));
    detailed.slice(0, counts.detailed).forEach((fan, i) => {
        const standing = i % 7 === 0;
        cards.push({ at: standing ? fan.standing : fan.seated, look: (i * 3 + 1) % SPAWNER.Looks, seated: !standing });
    });
    return cards;
}

const CARD_SHADER = `
czm_material czm_getMaterial(czm_materialInput materialInput) {
    czm_material material = czm_getDefaultMaterial(materialInput);
    vec4 colour = texture(image, materialInput.st);
    // The Unreal material is masked at 0.33.
    if (colour.a < 0.33) discard;
    material.diffuse = colour.rgb;
    material.alpha = 1.0;
    return material;
}`;

export class StadiumCrowd {
    private primitive: Primitive | null = null;
    private shown = true;

    static async load(scene: Scene, geo: Geo, seatView: Transform | null): Promise<StadiumCrowd> {
        const crowd = new StadiumCrowd();
        try {
            const [seats, treads] = await Promise.all([
                fetch("media/crowd/seats.json").then((r) => r.json() as Promise<SeatsFile>),
                fetch("media/crowd/seat_treads.json").then((r) => r.json() as Promise<TreadsFile>),
            ]);
            const art = await fetch("media/crowd/fans.png", { method: "HEAD" }).then((r) => r.ok).catch(() => false);
            const cards = chooseCards(seats, treads, geo.siteFrame, seatView);
            crowd.primitive = new Primitive({
                geometryInstances: new GeometryInstance({ geometry: cardGeometry(cards) }),
                appearance: new MaterialAppearance({
                    material: new Material({
                        fabric: { uniforms: { image: art ? "media/crowd/fans.png" : fallbackAtlas() }, source: CARD_SHADER },
                    }),
                    flat: true,
                    closed: false,
                    translucent: false,
                    faceForward: true,
                    materialSupport: MaterialAppearance.MaterialSupport.TEXTURED,
                }),
                modelMatrix: geo.enuToFixed,
                asynchronous: false,
                shadows: ShadowMode.ENABLED,
                allowPicking: false,
            });
            crowd.primitive.show = crowd.shown;
            scene.primitives.add(crowd.primitive);
            const seated = cards.filter((c) => c.seated).length;
            console.info(`Crowd: ${cards.length} fans on ${seats.seats.length} seats (${Math.round(SPAWNER.Occupancy * 100)}% occupancy; ` +
                `${seated} seated near My Seat)${art ? "" : "; Unreal's fan art missing (needs Python + Pillow), simple cards instead"}.`);
        } catch (error) {
            console.warn("Crowd seats could not be loaded; the bowl stays empty.", error);
        }
        return crowd;
    }

    setShown(shown: boolean): void {
        this.shown = shown;
        if (this.primitive) this.primitive.show = shown;
    }
}

/** All cards as one mesh in ENU metres around the origin (the primitive's model matrix puts it on the globe). */
function cardGeometry(cards: Card[]): Geometry {
    const positions = new Float64Array(cards.length * 12);
    const normals = new Float32Array(cards.length * 12);
    const st = new Float32Array(cards.length * 8);
    const indices = new Uint32Array(cards.length * 6);
    const w = SPAWNER.BillboardSize.x, h = SPAWNER.BillboardSize.y;
    cards.forEach((card, i) => {
        const across = card.at.rotation.rotateVector(new Vec3(0, w / 2, 0));
        const bottomZ = card.seated ? SEATED.hipCm : 0;
        const height = card.seated ? SEATED.toCm - SEATED.fromCm : h;
        const corners = [
            card.at.translation.add(across.scale(-1)).add(new Vec3(0, 0, bottomZ)),
            card.at.translation.add(across).add(new Vec3(0, 0, bottomZ)),
            card.at.translation.add(across).add(new Vec3(0, 0, bottomZ + height)),
            card.at.translation.add(across.scale(-1)).add(new Vec3(0, 0, bottomZ + height)),
        ];
        corners.forEach((c, k) => {
            // Unreal cm (X East, Y South, Z Up) -> ENU metres.
            positions[i * 12 + k * 3] = c.x / 100;
            positions[i * 12 + k * 3 + 1] = -c.y / 100;
            positions[i * 12 + k * 3 + 2] = c.z / 100;
        });
        // Lit from the field side like Unreal's two-sided card; faceForward turns it for the back.
        const facing = card.at.rotation.rotateVector(new Vec3(1, 0, 0));
        for (let k = 0; k < 4; ++k) normals.set([facing.x, -facing.y, facing.z], i * 12 + k * 3);
        const u0 = card.look / SPAWNER.Looks, u1 = (card.look + 1) / SPAWNER.Looks;
        const v0 = card.seated ? SEATED.fromCm / h : 0, v1 = card.seated ? SEATED.toCm / h : 1;
        st.set([u0, v0, u1, v0, u1, v1, u0, v1], i * 8);
        indices.set([0, 1, 2, 0, 2, 3].map((n) => i * 4 + n), i * 6);
    });
    return new Geometry({
        attributes: {
            position: new GeometryAttribute({ componentDatatype: ComponentDatatype.DOUBLE, componentsPerAttribute: 3, values: positions }),
            normal: new GeometryAttribute({ componentDatatype: ComponentDatatype.FLOAT, componentsPerAttribute: 3, values: normals }),
            st: new GeometryAttribute({ componentDatatype: ComponentDatatype.FLOAT, componentsPerAttribute: 2, values: st }),
        } as unknown as GeometryAttributes,
        indices,
        primitiveType: PrimitiveType.TRIANGLES,
        boundingSphere: BoundingSphere.fromVertices(Array.from(positions)),
    });
}

/** Simple cards when the Unreal fan art wasn't drawn: a figure per look in the project's shirt colours. */
function fallbackAtlas(): HTMLCanvasElement {
    const shirts = ["#004C54", "#ECECE8", "#004C54", "#A5ACAF", "#004C54", "#18181A", "#004C54", "#003A40"];
    const skins = ["#F1CDAA", "#E0AC80", "#C6865C", "#8D5536", "#603C26"];
    const c = document.createElement("canvas");
    c.width = 128 * SPAWNER.Looks;
    c.height = 512;
    const ctx = c.getContext("2d")!;
    for (let i = 0; i < SPAWNER.Looks; ++i) {
        const x = i * 128 + 64, px = 128 / 60;
        ctx.fillStyle = "#26262A";
        ctx.fillRect(x - 16 * px, 512 - 85 * px, 32 * px, 85 * px);
        ctx.fillStyle = shirts[i];
        ctx.beginPath();
        ctx.roundRect(x - 22 * px, 512 - 150 * px, 44 * px, 70 * px, 10 * px);
        ctx.fill();
        ctx.fillStyle = skins[i % skins.length];
        ctx.beginPath();
        ctx.arc(x, 512 - 167 * px, 11 * px, 0, Math.PI * 2);
        ctx.fill();
    }
    return c;
}

