// Port of Source/VirtualStadium/Public/Journey/LFFJourneyTypes.h. Enum values are the UE enum names, so the shared
// SourceData/Journey/*.json files load without translation.

export type DisclaimerLevel = "Verified" | "Approximate" | "Illustrative";
export type TransitionProfile = "Instant" | "Ease" | "ReducedMotion";
export type IntensityPreview = "None" | "Sound" | "Lights" | "Anthem";
export type CueState = "Before" | "Play" | "After";
export type JourneyPhase = "Empty" | "Intro" | "Transitioning" | "AtStep";
export type SimulationMode = "GuidedTour" | "FreeExplore";

/** ELFFTourStep, in order: one chip each. */
export const TOUR_STEPS = ["Arrival", "Gates", "Plaza", "ToMySeat", "Game"] as const;
export type TourStep = (typeof TOUR_STEPS)[number];
export const TOUR_STEP_COUNT = TOUR_STEPS.length;

/** ELFFMapLevel, from the ground up. */
export const MAP_LEVELS = ["Field", "LowerConcourse", "Club", "Suites", "UpperDeck"] as const;
export type MapLevel = (typeof MAP_LEVELS)[number];

/** ELFFAmenity; the bit index in AmenityMask is the position here. */
export const AMENITIES = ["SensoryRoom", "QuietZone", "GuestServices", "Restroom", "Food"] as const;
export type Amenity = (typeof AMENITIES)[number];

export const amenityBit = (amenity: Amenity): number => 1 << AMENITIES.indexOf(amenity);
/** Sensory rooms and quiet zones are on by default (LFFDefaultAmenityMask). */
export const DEFAULT_AMENITY_MASK = amenityBit("SensoryRoom") | amenityBit("QuietZone");

export type CommandResult =
    | "Accepted"
    | "RejectedEmpty"
    | "RejectedNotStarted"
    | "RejectedAlreadyStarted"
    | "RejectedAtStart"
    | "RejectedAtEnd"
    | "RejectedDisabledByStep"
    | "RejectedPaused"
    | "RejectedInExplore"
    | "RejectedSameMode";

/** INDEX_NONE. */
export const NONE = -1;

/** One row of SourceData/Journey/LFF_GoldenPath.json (FLFFJourneyStep). Steps never store coordinates. */
export interface JourneyStep {
    StepId: string;
    Title: string;
    Body: string;
    DisclaimerLevel: DisclaimerLevel;
    ClockText: string;
    TimelineLabel: string;
    VerificationSource: string;
    CameraTargetId: string;
    TransitionProfile: TransitionProfile;
    TransitionSeconds: number;
    LabelsToShow: string[];
    PlacesToShow: string[];
    /** Unreal soft path ("/Game/LFF/Audio/..."); empty uses the game mode's bed. */
    AudioBed: string;
    IntensityPreview: IntensityPreview;
    PreviewSound: string;
    PreviewImage: string;
    /** Unreal soft path of the voice line ("/Game/LFF/Audio/VO/VO_02_Intro.VO_02_Intro"). */
    VoiceWave: string;
    SceneCues: string[];
    bShowTicketPhone: boolean;
    bNextEnabled: boolean;
    bBackEnabled: boolean;
}

/** One row of SourceData/Journey/LFF_Amenities.json (FLFFAmenityMarker). Location is in the site frame, cm. */
export interface AmenityMarker {
    AmenityId: string;
    Label: string;
    Note: string;
    Category: Amenity;
    Level: MapLevel;
    Location: Vec3Like;
    LocationActorTag: string;
    CameraTargetId: string;
    VerificationSource: string;
}

export interface Vec3Like {
    X: number;
    Y: number;
    Z: number;
}

/** FLFFLevelView: a level's height band (cm above the turf) and its overview. */
export interface LevelView {
    FloorZ: number;
    MinZ: number;
    MaxZ: number;
    ViewDistance: number;
    ViewPitchDegrees: number;
    ViewYawDegrees: number;
}

/** FLFFVisitorSettings. Held in memory only; nothing is saved (LEDGER D11). */
export interface VisitorSettings {
    bReducedMotion: boolean;
    bSoundEnabled: boolean;
    bCalmMode: boolean;
    bLargeText: boolean;
    bHighContrast: boolean;
    bVoiceEnabled: boolean;
    bCrowdEnabled: boolean;
}

export const defaultSettings = (): VisitorSettings => ({
    bReducedMotion: false,
    bSoundEnabled: false,
    bCalmMode: false,
    bLargeText: false,
    bHighContrast: false,
    bVoiceEnabled: false,
    bCrowdEnabled: true,
});

export const isReducedMotionEffective = (s: VisitorSettings): boolean => s.bReducedMotion || s.bCalmMode;
/** Calm Mode always means silence, whatever the Sound toggle says. */
export const isSoundEffective = (s: VisitorSettings): boolean => s.bSoundEnabled && !s.bCalmMode;
/** Sound Off mutes the voice too, and Calm Mode forces it off. */
export const isVoiceEffective = (s: VisitorSettings): boolean => s.bVoiceEnabled && isSoundEffective(s);
export const isCrowdShown = (s: VisitorSettings): boolean => s.bCrowdEnabled && !s.bCalmMode;

export interface TimelineMark {
    StepIndex: number;
    Label: string;
}

/** FLFFJourneySnapshot: everything the UI needs, pushed on every change. Widgets hold no journey state. */
export interface JourneySnapshot {
    Phase: JourneyPhase;
    Mode: SimulationMode;
    TourStep: TourStep;
    TourStepFirstSteps: number[];
    bCanResumeTour: boolean;
    MapLevel: MapLevel;
    bLevelHighlight: boolean;
    AmenityMask: number;
    SelectedAmenityId: string;
    TourPlaces: string[];
    ViewTargetId: string;
    MySeatTargetId: string;
    MySeatLevel: MapLevel;
    StepIndex: number;
    StepCount: number;
    StepId: string;
    Title: string;
    Body: string;
    ProgressText: string;
    ClockText: string;
    TimelineMarks: TimelineMark[];
    bShowTicketPhone: boolean;
    bVoicePlaying: boolean;
    DisclaimerLevel: DisclaimerLevel;
    DisclaimerText: string;
    bShowDisclaimer: boolean;
    DisclaimerLine: string;
    PrototypeBadgeText: string;
    bStarted: boolean;
    bPaused: boolean;
    bAtEnd: boolean;
    bCanNext: boolean;
    bCanBack: boolean;
    bSettingsOpen: boolean;
    bRestartConfirmOpen: boolean;
    bTargetMissing: boolean;
    Settings: VisitorSettings;
    IntensityPreview: IntensityPreview;
    bShowPreviewControls: boolean;
    bPreviewOn: boolean;
    bPreviewAvailable: boolean;
}
