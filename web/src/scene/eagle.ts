// The anthem's bald eagle (SI_EagleMesh, a 2 s flap-loop geometry cache riding the SI_EagleFlight cue). The web plays the
// source glTF's own flap animation: web/models/eagle.glb, from Assets/bald-eagle by tools/prepare_models.mjs. Without it
// this stand-in flies: The stand-in: a dark body, white head and tail, 2.8 m wingspan, slow wing beat.
import { Axis, BoxGeometry, Cartesian3, Color, ColorGeometryInstanceAttribute, EllipsoidGeometry, GeometryInstance, Matrix4, Model,
    PerInstanceColorAppearance, Primitive } from "cesium";
import type { Scene } from "cesium";
import { Quat, Rotator, Transform, Vec3 } from "../util/ue";
import type { CueVisual } from "./cues";
import { ueTransformToLocalMatrix } from "./geo";
import type { Geo } from "./geo";

const FLAP_SECONDS = 1.958; // the geometry cache's loop
const WINGSPAN_CM = 280;
/** worldTransformToModelMatrix mirrors content across Y (Unreal is left-handed); a glTF model needs that undone. */
const MIRROR_Y = Matrix4.fromScale(new Cartesian3(1, -1, 1));

function part(geometry: BoxGeometry | EllipsoidGeometry, t: Transform, colour: Color): GeometryInstance {
    return new GeometryInstance({ geometry, modelMatrix: ueTransformToLocalMatrix(t), attributes: { color: ColorGeometryInstanceAttribute.fromColor(colour) } });
}

export class EagleVisual implements CueVisual {
    private readonly body: Primitive;
    private readonly wings: Primitive[];
    private model: Model | null = null;
    private world = new Transform();
    private readonly matrix = new Matrix4();
    private shown = false;
    private readonly orient = Matrix4.clone(Matrix4.IDENTITY);

    constructor(private readonly geo: Geo, scene: Scene, private readonly relative: Transform, modelUrl: string | null) {
        const sphere = new EllipsoidGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, radii: new Cartesian3(50, 50, 50) });
        const box = BoxGeometry.fromDimensions({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, dimensions: new Cartesian3(100, 100, 100) });
        const brown = Color.fromBytes(58, 40, 26);
        const white = Color.fromBytes(240, 238, 230);
        const gold = Color.fromBytes(232, 178, 40);
        const appearance = () => new PerInstanceColorAppearance({ flat: false, translucent: false, closed: true });
        this.body = new Primitive({
            geometryInstances: [
                part(sphere, new Transform(new Quat(), new Vec3(0, 0, 0), new Vec3(0.9, 0.34, 0.32)), brown),
                part(sphere, new Transform(new Quat(), new Vec3(48, 0, 6), new Vec3(0.26, 0.2, 0.22)), white),
                part(sphere, new Transform(new Quat(), new Vec3(64, 0, 2), new Vec3(0.1, 0.06, 0.06)), gold),
                part(box, new Transform(new Quat(), new Vec3(-55, 0, 0), new Vec3(0.35, 0.3, 0.04)), white),
            ],
            appearance: appearance(),
            asynchronous: true,
            allowPicking: false,
            show: false,
        });
        scene.primitives.add(this.body);
        this.wings = [1, -1].map((side) => {
            const wing = new Primitive({
                geometryInstances: [part(box, new Transform(new Quat(), new Vec3(0, (side * WINGSPAN_CM) / 4, 0), new Vec3(0.55, WINGSPAN_CM / 200, 0.04)), brown)],
                appearance: appearance(),
                asynchronous: true,
                allowPicking: false,
                show: false,
            });
            scene.primitives.add(wing);
            return wing;
        });
        if (modelUrl) void this.loadModel(scene, modelUrl);
    }

    /**
     * eagle.glb keeps the source's Y-up axes (CesiumJS corrects them); eagle.orient.json (tools/prepare_models.mjs) then
     * turns it to fly along +X with Z up, 2.8 m across. It is applied here rather than baked into the file because a root
     * node above the rig breaks CesiumJS's skinning.
     */
    private async loadModel(scene: Scene, modelUrl: string): Promise<void> {
        try {
            const orient = await fetch(modelUrl.replace(/\.glb$/, ".orient.json")).then((r) => (r.ok ? r.json() : null)) as { matrix?: number[] } | null;
            if (orient?.matrix?.length === 16) Matrix4.fromArray(orient.matrix, 0, this.orient);
            const model = await Model.fromGltfAsync({ url: modelUrl, show: false, upAxis: Axis.Y, forwardAxis: Axis.X });
            this.model = model;
            scene.primitives.add(model);
            model.readyEvent.addEventListener(() => model.activeAnimations.addAll({ loop: 2 /* REPEAT */ }));
            this.placeModel();
            this.setShown(this.shown);
        } catch (error) {
            console.warn("Eagle model failed to load; the stand-in flies instead.", error);
        }
    }

    setWorld(actorWorld: Transform): void {
        this.world = this.relative.mul(actorWorld);
        this.geo.worldTransformToModelMatrix(this.world, this.matrix);
        Matrix4.clone(this.matrix, this.body.modelMatrix);
        this.placeModel();
    }

    /** The cue's transform, then Unreal's mirror undone (the model is right-handed), then the model's own orientation. */
    private placeModel(): void {
        if (!this.model) return;
        Matrix4.multiply(Matrix4.multiply(this.matrix, MIRROR_Y, this.model.modelMatrix), this.orient, this.model.modelMatrix);
    }

    setShown(shown: boolean): void {
        this.shown = shown;
        const useModel = this.model !== null;
        this.body.show = shown && !useModel;
        for (const wing of this.wings) wing.show = shown && !useModel;
        if (this.model) this.model.show = shown;
    }

    animate(movingSeconds: number): void {
        if (!this.shown || this.model) return;
        // A slow, even beat (one per loop of the original cache), never a flutter.
        const angle = 22 * Math.sin((2 * Math.PI * movingSeconds) / FLAP_SECONDS);
        [1, -1].forEach((side, i) => {
            const hinge = new Transform(new Rotator(0, 0, side * angle).quaternion(), new Vec3(0, side * 8, 4));
            this.geo.worldTransformToModelMatrix(hinge.mul(this.world), this.wings[i].modelMatrix);
        });
    }
}
