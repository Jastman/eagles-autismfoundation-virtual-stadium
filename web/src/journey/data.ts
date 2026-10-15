// The journey's content, read straight from the Unreal project's SourceData/ (the same files DT_LFF_GoldenPath and
// DT_LFF_Amenities are imported from), so editing a step updates both builds. Only the row parsing lives here.
import amenitiesJson from "../../../SourceData/Journey/LFF_Amenities.json";
import goldenPathJson from "../../../SourceData/Journey/LFF_GoldenPath.json";
import { GameMode } from "../config";
import type { AmenityMarker, JourneyStep } from "./types";

type Row = Record<string, unknown>;

const str = (row: Row, key: string, fallback = ""): string => (typeof row[key] === "string" ? (row[key] as string) : fallback);
const list = (row: Row, key: string): string[] => (Array.isArray(row[key]) ? (row[key] as string[]).filter((v) => typeof v === "string") : []);
const bool = (row: Row, key: string, fallback: boolean): boolean => (typeof row[key] === "boolean" ? (row[key] as boolean) : fallback);

/** As ULFFJourneySubsystem::LoadSteps: the row Name becomes StepId; Verified without a source is downgraded. */
export function parseSteps(rows: Row[]): JourneyStep[] {
    return rows.map((row) => {
        const step: JourneyStep = {
            StepId: str(row, "Name"),
            Title: str(row, "Title"),
            Body: str(row, "Body"),
            DisclaimerLevel: (str(row, "DisclaimerLevel", "Illustrative") as JourneyStep["DisclaimerLevel"]),
            ClockText: str(row, "ClockText"),
            TimelineLabel: str(row, "TimelineLabel"),
            VerificationSource: str(row, "VerificationSource"),
            CameraTargetId: str(row, "CameraTargetId"),
            TransitionProfile: (str(row, "TransitionProfile", "Ease") as JourneyStep["TransitionProfile"]),
            TransitionSeconds: typeof row.TransitionSeconds === "number" ? row.TransitionSeconds : 0,
            LabelsToShow: list(row, "LabelsToShow"),
            PlacesToShow: list(row, "PlacesToShow"),
            AudioBed: str(row, "AudioBed"),
            IntensityPreview: (str(row, "IntensityPreview", "None") as JourneyStep["IntensityPreview"]),
            PreviewSound: str(row, "PreviewSound"),
            PreviewImage: str(row, "PreviewImage"),
            VoiceWave: str(row, "VoiceWave"),
            SceneCues: list(row, "SceneCues"),
            bShowTicketPhone: bool(row, "bShowTicketPhone", false),
            bNextEnabled: bool(row, "bNextEnabled", true),
            bBackEnabled: bool(row, "bBackEnabled", true),
        };
        if (step.DisclaimerLevel === "Verified" && step.VerificationSource === "") {
            console.warn(`Step ${step.StepId} is marked Verified with no VerificationSource; treating it as Illustrative.`);
            step.DisclaimerLevel = "Illustrative";
        }
        return step;
    });
}

/** As ULFFJourneySubsystem::LoadAmenities: unverified places are dropped (the packaged-build rule). */
export function parseAmenities(rows: Row[], keepUnverified = GameMode.bShowUnverifiedAmenities): AmenityMarker[] {
    const out: AmenityMarker[] = [];
    let dropped = 0;
    for (const row of rows) {
        const verification = str(row, "VerificationSource");
        if (verification === "" && !keepUnverified) {
            ++dropped;
            continue;
        }
        const loc = (row.Location ?? {}) as { X?: number; Y?: number; Z?: number };
        out.push({
            AmenityId: str(row, "Name"),
            Label: str(row, "Label"),
            Note: str(row, "Note"),
            Category: str(row, "Category", "GuestServices") as AmenityMarker["Category"],
            Level: str(row, "Level", "LowerConcourse") as AmenityMarker["Level"],
            Location: { X: loc.X ?? 0, Y: loc.Y ?? 0, Z: loc.Z ?? 0 },
            LocationActorTag: str(row, "LocationActorTag"),
            CameraTargetId: str(row, "CameraTargetId"),
            VerificationSource: verification,
        });
    }
    if (dropped > 0) console.info(`Places: ${out.length} kept, ${dropped} unverified dropped.`);
    return out;
}

export const goldenPath = (): JourneyStep[] => parseSteps(goldenPathJson as Row[]);
export const amenities = (): AmenityMarker[] => parseAmenities(amenitiesJson as Row[]);

/** "/Game/LFF/Audio/VO/VO_02_Intro.VO_02_Intro" -> "VO_02_Intro" (the SourceData/Audio file name). */
export function soundName(softPath: string): string {
    if (!softPath) return "";
    const last = softPath.split("/").pop() ?? "";
    return last.split(".")[0];
}
