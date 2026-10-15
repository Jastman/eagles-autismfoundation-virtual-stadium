// The bowl crowd (ALFFStadiumCrowdSpawner): Content/LFF/Crowd/seats.json and seat_treads.json at 70% occupancy, the
// visitor's own seat kept clear. Unreal draws nearby fans as animated MetaHumans, the middle distance with Mass and the far
// bowl as billboards; MetaHumans and Mass are Unreal-only, so the web draws every fan as a card, and the few closest to
// the seat as small figures (Docs/WEB_PARITY.md). Crowd Off and Calm Mode hide it; Less movement freezes the figures.
import { BillboardCollection, Cartesian2, Cartesian3, HorizontalOrigin, NearFarScalar, VerticalOrigin } from "cesium";
import type { Scene } from "cesium";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";
import { hash01 } from "./props";

interface SeatsFile {
    seats: number[][];
}
interface TreadsFile {
    tread_z_cm?: number[][];
    seated_offset_cm?: number;
}

const OCCUPANCY = 0.7;
const CLEAR_RADIUS_CM = 50;
const SHIRTS = ["#004C54", "#F4F2EC", "#151617", "#69BE28", "#A5ACAF", "#004C54", "#0B3A3F"];
const SKINS = ["#F0C8A8", "#C8916B", "#8D6145", "#5C3D29", "#DDAE88"];

/** One fan card: head and shoulders, seated, in a shirt colour. Drawn once per variant. */
function fanCard(shirt: string, skin: string): HTMLCanvasElement {
    const c = document.createElement("canvas");
    c.width = 32;
    c.height = 48;
    const ctx = c.getContext("2d")!;
    ctx.fillStyle = shirt;
    ctx.beginPath();
    ctx.roundRect(4, 20, 24, 28, 9);
    ctx.fill();
    ctx.fillStyle = skin;
    ctx.beginPath();
    ctx.arc(16, 12, 8, 0, Math.PI * 2);
    ctx.fill();
    return c;
}

export class StadiumCrowd {
    private readonly collection: BillboardCollection;
    private shown = true;

    private constructor(scene: Scene) {
        this.collection = new BillboardCollection({ scene });
        scene.primitives.add(this.collection);
    }

    static async load(scene: Scene, geo: Geo, seatCameraWorld: Vec3 | null): Promise<StadiumCrowd> {
        const crowd = new StadiumCrowd(scene);
        try {
            const [seats, treads] = await Promise.all([
                fetch("media/crowd/seats.json").then((r) => r.json() as Promise<SeatsFile>),
                fetch("media/crowd/seat_treads.json").then((r) => r.json() as Promise<TreadsFile>).catch(() => ({} as TreadsFile)),
            ]);
            const cards = SHIRTS.flatMap((shirt) => SKINS.map((skin) => fanCard(shirt, skin)));
            const seatSite = seatCameraWorld ? geo.siteFrame.inverseTransformPosition(seatCameraWorld) : null;
            let placed = 0, skipped = 0;
            seats.seats.forEach((seat, index) => {
                const key = `seat${index}`;
                if (hash01(key, 7) >= OCCUPANCY) return;
                const tread = treads.tread_z_cm?.[index]?.[0];
                // Measured treads only: an unmeasured seat is never given a guessed height.
                if (treads.tread_z_cm && tread === undefined) {
                    ++skipped;
                    return;
                }
                const floor = tread ?? seat[2];
                if (seatSite && Math.hypot(seat[0] - seatSite.x, seat[1] - seatSite.y) < CLEAR_RADIUS_CM + 40 && Math.abs(floor - (seatSite.z - 120)) < 150) return;
                const site = new Vec3(seat[0], seat[1], floor + (treads.seated_offset_cm ?? 45));
                const position = geo.worldToFixed(geo.siteToWorld(site), new Cartesian3());
                crowd.collection.add({
                    position,
                    image: cards[Math.floor(hash01(key, 8) * cards.length)],
                    sizeInMeters: true,
                    width: 0.55,
                    height: 0.82,
                    verticalOrigin: VerticalOrigin.BOTTOM,
                    horizontalOrigin: HorizontalOrigin.CENTER,
                    pixelOffset: Cartesian2.ZERO,
                    // Far cards fade out a little so the bowl reads as a crowd, not confetti.
                    translucencyByDistance: new NearFarScalar(150, 1, 3000, 0.75),
                });
                ++placed;
            });
            console.info(`Crowd: ${placed} fans on ${seats.seats.length} seats (${Math.round(OCCUPANCY * 100)}% occupancy, ${skipped} unmeasured seats excluded).`);
        } catch (error) {
            console.warn("Crowd seats could not be loaded; the bowl stays empty.", error);
        }
        return crowd;
    }

    setShown(shown: boolean): void {
        if (shown === this.shown) return;
        this.shown = shown;
        this.collection.show = shown;
    }
}
