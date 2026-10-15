// UTextRenderComponent signs (LFF_Label actors and the popcorn cart's title) as textured quads fixed in the world, like
// Unreal's: the text faces the actor's +X, reads across its -Y, and is visible from the front only.
import { Appearance, GeometryInstance, Material, MaterialAppearance, Matrix4, PlaneGeometry, Primitive, VertexFormat } from "cesium";
import type { Scene } from "cesium";
import { Vec3 } from "../util/ue";
import type { Transform } from "../util/ue";
import { ueTransformToLocalMatrix } from "./geo";
import type { ActorData } from "./sceneData";
import { toTransform } from "./sceneData";

const PX_PER_UNIT = 4; // canvas pixels per cm of text height

/** Draws the text in the project font; returns the canvas and its size in cm. */
function drawText(text: string, worldSize: number, colour: { r: number; g: number; b: number; a: number }, align: string) {
    const lines = text.split("\n");
    const fontPx = Math.max(24, Math.min(256, Math.round(worldSize * PX_PER_UNIT)));
    const scale = fontPx / worldSize; // px per cm
    const measure = document.createElement("canvas").getContext("2d")!;
    const font = `700 ${fontPx}px "Atkinson Hyperlegible", system-ui, sans-serif`;
    measure.font = font;
    const lineHeight = fontPx * 1.15;
    const width = Math.ceil(Math.max(...lines.map((l) => measure.measureText(l).width)) + fontPx * 0.2);
    const height = Math.ceil(lineHeight * lines.length);
    const canvas = document.createElement("canvas");
    canvas.width = Math.max(4, width);
    canvas.height = Math.max(4, height);
    const ctx = canvas.getContext("2d")!;
    ctx.font = font;
    ctx.fillStyle = `rgba(${colour.r}, ${colour.g}, ${colour.b}, ${colour.a / 255})`;
    ctx.textBaseline = "middle";
    ctx.textAlign = align === "EHTA_Center" ? "center" : align === "EHTA_Right" ? "right" : "left";
    const x = align === "EHTA_Center" ? canvas.width / 2 : align === "EHTA_Right" ? canvas.width : 0;
    lines.forEach((line, i) => ctx.fillText(line, x, lineHeight * (i + 0.5)));
    return { canvas, widthCm: canvas.width / scale, heightCm: canvas.height / scale };
}

export class WorldText {
    readonly primitive: Primitive;
    readonly location: Vec3;

    constructor(scene: Scene, actor: ActorData, rootWorld: Transform) {
        const t = actor.text!;
        const { canvas, widthCm, heightCm } = drawText(t.text, t.worldSize, t.colour, t.align);
        // Unreal aligns the text horizontally on the component origin and centres it vertically. In the mirrored local
        // frame the reading direction (Unreal -Y, the viewer's right) is +Y; `across` is the text centre along it.
        const across = t.align === "EHTA_Center" ? 0 : t.align === "EHTA_Right" ? -widthCm / 2 : widthCm / 2;
        const textWorld = toTransform(t.local).mul(rootWorld);
        this.location = textWorld.transformPosition(new Vec3(0.5, -across, 0));
        const frame = ueTransformToLocalMatrix(textWorld);
        // PlaneGeometry is a unit square in its XY with a +Z normal. Column-major: plane X -> +Y (width), plane Y -> +Z
        // (height), normal -> +X (the actor's front), centred half a centimetre in front of the origin.
        const basis = Matrix4.fromArray([
            0, widthCm, 0, 0,
            0, 0, heightCm, 0,
            1, 0, 0, 0,
            0.5, across, 0, 1,
        ]);
        const modelLocal = Matrix4.multiply(frame, basis, new Matrix4());
        this.primitive = new Primitive({
            geometryInstances: new GeometryInstance({
                geometry: new PlaneGeometry({ vertexFormat: VertexFormat.POSITION_NORMAL_AND_ST }),
                modelMatrix: modelLocal,
            }),
            appearance: new MaterialAppearance({
                material: Material.fromType("Image", { image: canvas, transparent: true }),
                translucent: true,
                faceForward: false,
                closed: false,
                renderState: { cull: { enabled: true } },
            }) as Appearance,
            asynchronous: true,
            allowPicking: false,
        });
        scene.primitives.add(this.primitive);
    }

    setModelMatrix(enuToFixed: Matrix4): void {
        Matrix4.clone(enuToFixed, this.primitive.modelMatrix);
    }

    setHidden(hidden: boolean): void {
        this.primitive.show = !hidden;
    }
}
