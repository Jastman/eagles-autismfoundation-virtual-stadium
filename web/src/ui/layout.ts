// Port of LFFUILayout (Source/VirtualStadium/Private/UI/LFFUILayout.cpp). Values are logical UMG units. The web build
// keeps Unreal's DPI rule (Config/DefaultEngine.ini: UIScaleRule=ShortestSide, 1080 = 1.0), so one logical unit is
// (shortest side / 1080) CSS pixels and the layout is the same at any iPad size.
import { Box2, Vec2, clamp, lerp } from "../util/ue";

export interface CornerPanels {
    InformationWidth: number;
    ControlWidth: number;
    MaximumHeight: number;
    NarrativeHeight: number;
    CaptionWidth: number;
    EdgeInset: number;
}

export function viewportInset(viewport: Vec2): number {
    return clamp(Math.min(viewport.x, viewport.y) / 45, 12, 24);
}

export function welcomeBottomInset(viewportWidth: number): number {
    // Keep the full narrow-window Settings clearance until the cards fit, then release the extra space gradually.
    return lerp(208, 112, clamp((viewportWidth - 1460) / 200, 0, 1));
}

export function cornerPanels(viewport: Vec2, largeText: boolean): CornerPanels {
    const edgeInset = viewportInset(viewport);
    const captionScale = largeText ? 1.25 : 1;
    const informationWidth = Math.min(480 * captionScale, viewport.x * 0.43);
    return {
        EdgeInset: edgeInset,
        InformationWidth: informationWidth,
        ControlWidth: Math.min(540, viewport.x * 0.43),
        MaximumHeight: Math.max(64, viewport.y - edgeInset * 2 - 64),
        NarrativeHeight: Math.max(64, viewport.y - edgeInset * 2 - 180),
        CaptionWidth: Math.max(1, (informationWidth - 40) / captionScale),
    };
}

export function timelineColumns(availableWidth: number, markWidth: number, gap: number): number {
    return Math.max(1, Math.floor((Math.max(0, availableWidth) + Math.max(0, gap)) / Math.max(1, markWidth + Math.max(0, gap))));
}

export function timelineMarkWidth(availableWidth: number, minimumWidth: number, gap: number): number {
    const columns = timelineColumns(availableWidth, minimumWidth, gap);
    return Math.max(1, (availableWidth - Math.max(0, gap) * (columns - 1) - 0.01) / columns);
}

export function ticketBounds(viewport: Vec2, information: Box2, controls: Box2, naturalSize: Vec2): Box2 {
    const inset = viewportInset(viewport);
    if (viewport.x <= inset * 2 || viewport.y <= inset * 2 || naturalSize.x <= 0 || naturalSize.y <= 0) return Box2.invalid();
    const left = Math.max(viewport.x * 0.5, information.valid ? information.max.x + inset : inset);
    const beside = new Box2(new Vec2(left, inset), new Vec2(controls.valid ? controls.min.x - inset : viewport.x - inset, viewport.y - inset));
    const below = new Box2(new Vec2(left, controls.valid ? controls.max.y + inset : inset), new Vec2(viewport.x - inset, viewport.y - inset));
    const fitScale = (area: Box2) => {
        const size = area.size();
        return Math.max(0, Math.min(1, size.x / naturalSize.x, size.y / naturalSize.y));
    };
    const besideScale = fitScale(beside);
    const belowScale = fitScale(below);
    let scale = besideScale;
    let belowTravel = 0;
    let rightTravel = 0;
    if (belowScale > 0 && besideScale < belowScale) {
        // As the lane narrows, move down while still beside the dock, then grow into the space below it.
        belowTravel = clamp(2 * (1 - besideScale / belowScale), 0, 1);
        if (besideScale < belowScale * 0.5) {
            scale = belowScale - besideScale;
            rightTravel = 1 - (2 * besideScale) / belowScale;
        }
    }
    if (scale <= 0) return Box2.invalid();
    const size = naturalSize.scale(scale);
    const besideY = beside.min.y + (beside.max.y - beside.min.y - size.y) * 0.5;
    const belowY = below.min.y + (below.max.y - below.min.y - size.y) * 0.5;
    const x = rightTravel > 0 ? left + rightTravel * (below.max.x - left - size.x) : beside.max.x - size.x;
    const position = new Vec2(x, lerp(besideY, belowY, belowTravel));
    return new Box2(position, position.add(size));
}
