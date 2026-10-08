#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LFFJourneyTypes.generated.h"

class USoundBase;
class UTexture2D;

/** How much a step's location can be trusted. V1 steps are all Illustrative (LEDGER D9). */
UENUM(BlueprintType)
enum class ELFFDisclaimerLevel : uint8
{
    Verified,
    Approximate,
    Illustrative
};

/** Authored camera move. Reduced motion turns Ease into ReducedMotion (fade, cut, fade); Calm Mode cuts. */
UENUM(BlueprintType)
enum class ELFFTransitionProfile : uint8
{
    Instant,
    Ease,
    ReducedMotion
};

/**
 * Optional sensory preview on a loud or bright step. Never autoplays: the visitor picks "Soft" first.
 * Sound plays a short muffled sample; Lights is a slow brightness pulse that never flickers; Anthem is one slow
 * pass of the bald eagle with soft fireworks (the step's Preview-triggered scene cues). Calm Mode hides all three.
 */
UENUM(BlueprintType)
enum class ELFFIntensityPreview : uint8
{
    None,
    Sound,
    Lights,
    Anthem
};

/** Where a scene cue is in its one-way story (a gate is closed, then opening, then open). */
UENUM(BlueprintType)
enum class ELFFCueState : uint8
{
    /** Before the step that plays it: the gate is closed, the arriving car isn't here yet. */
    Before,
    /** On the step that plays it, once the camera arrives. */
    Play,
    /** Any later step: already open, already parked. */
    After
};

UENUM(BlueprintType)
enum class ELFFJourneyPhase : uint8
{
    Empty,
    Intro,
    Transitioning,
    AtStep
};

/**
 * Guided tour (the day, step by step) or Free explore (the map, at your own pace). Switching never loses the tour's
 * place: Free explore can't move the step, and Resume tour returns to it (Docs/UI_MODES_SPEC.md §2).
 */
UENUM(BlueprintType)
enum class ELFFSimulationMode : uint8
{
    GuidedTour,
    FreeExplore
};

/**
 * The five chapters of the day, one node each on the timeline arc. A step belongs to the last chapter whose first step
 * is at or before it (ALFFGameMode::TourStepFirstSteps); Breaks and help (steps 19-23) are part of Game.
 */
UENUM(BlueprintType)
enum class ELFFTourStep : uint8
{
    Arrival,
    Gates,
    Plaza,
    ToMySeat,
    Game
};

constexpr int32 LFFTourStepCount = static_cast<int32>(ELFFTourStep::Game) + 1;

/** The stadium map's levels, from the ground up. The map's level tabs list them top to bottom, as they stack. */
UENUM(BlueprintType)
enum class ELFFMapLevel : uint8
{
    Field,
    LowerConcourse,
    Club,
    Suites,
    UpperDeck
};

constexpr int32 LFFMapLevelCount = static_cast<int32>(ELFFMapLevel::UpperDeck) + 1;

/** What a map marker is. Each kind has its own icon or glyph, so markers never differ by colour alone. */
UENUM(BlueprintType)
enum class ELFFAmenity : uint8
{
    SensoryRoom,
    QuietZone,
    GuestServices,
    Restroom,
    Food
};

constexpr int32 LFFAmenityCount = static_cast<int32>(ELFFAmenity::Food) + 1;

/** One bit per ELFFAmenity: the categories shown on the map. Sensory rooms and quiet zones are on by default. */
constexpr int32 LFFDefaultAmenityMask = (1 << static_cast<int32>(ELFFAmenity::SensoryRoom)) | (1 << static_cast<int32>(ELFFAmenity::QuietZone));

UENUM(BlueprintType)
enum class ELFFCommandResult : uint8
{
    Accepted,
    RejectedEmpty,
    RejectedNotStarted,
    RejectedAlreadyStarted,
    RejectedAtStart,
    RejectedAtEnd,
    RejectedDisabledByStep,
    RejectedPaused,
    /** Next, Back, Start and jumps belong to the tour; Free explore can't move the step. */
    RejectedInExplore,
    /** Already in that mode. */
    RejectedSameMode
};

/**
 * One row of DT_LFF_GoldenPath. The row name is the step id.
 * Source of truth: SourceData/Journey/LFF_GoldenPath.json (reimport, never hand-edit the table).
 * Steps never store coordinates; they name an ALFFCameraTarget placed in the level (LEDGER D8).
 */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFJourneyStep : public FTableRowBase
{
    GENERATED_BODY()

    /** Filled from the Data Table row name at load time. Not authored. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Journey")
    FName StepId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey", meta = (MultiLine = "true"))
    FText Body;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    ELFFDisclaimerLevel DisclaimerLevel = ELFFDisclaimerLevel::Illustrative;

    /** Example-game clock for this step ("~9:30 AM"), shown next to the step number. Empty on untimed steps. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    FText ClockText;

    /** Non-empty puts this step on the timeline above Back/Next ("10:00 AM\nPlaza gates"). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey", meta = (MultiLine = "true"))
    FText TimelineLabel;

    /** Who verified this location, and when. Required before a step may be marked Verified. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    FString VerificationSource;

    /** Must match exactly one ALFFCameraTarget::TargetId in the level. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    FName CameraTargetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    ELFFTransitionProfile TransitionProfile = ELFFTransitionProfile::Ease;

    /** 0 uses the game mode default. Always clamped to the game mode min/max. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey", meta = (ClampMin = "0.0"))
    float TransitionSeconds = 0.0f;

    /** Actors tagged LFF_Label plus one of these names are shown during this step. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TArray<FName> LabelsToShow;

    /** Places (DT_LFF_Amenities row names) whose callouts stand on the model during this step of the Guided tour. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TArray<FName> PlacesToShow;

    /** Optional ambient bed for this step; empty uses the game mode's AmbientBed. Plays only when sound is on. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<USoundBase> AudioBed;

    /** Offers "Preview this? Off / Soft" on this step. Off on every arrival. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    ELFFIntensityPreview IntensityPreview = ELFFIntensityPreview::None;

    /** The Sound preview sample; empty uses the game mode's PreviewSound. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<USoundBase> PreviewSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<UTexture2D> PreviewImage;

    /**
     * The step's voiceover: the card's Title and Body read aloud, nothing more (SourceData/Audio/VO/VO_MANIFEST.md).
     * Plays once when the camera arrives, only with Voice and Sound on. A missing wave is skipped; the card stays.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<USoundBase> VoiceWave;

    /**
     * ALFFCueActor ids this step plays: arrival cues (a gate opening, the car rolling in) play once the camera
     * arrives and stay done on later steps; preview cues (the eagle, fireworks) play only when Soft is picked.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TArray<FName> SceneCues;

    /** The phone with the mobile ticket rises from the bottom of the screen on this step. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    bool bShowTicketPhone = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    bool bNextEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    bool bBackEnabled = true;
};

/**
 * One row of DT_LFF_Amenities: a place on the Free explore map. The row name is the amenity id.
 * Source of truth: SourceData/Journey/LFF_Amenities.json (reimport, never hand-edit the table).
 * A row with no VerificationSource is shown in the editor and dropped in packaged builds (Docs/UI_MODES_SPEC.md D6):
 * a wrong sensory-room pin could send someone the wrong way.
 */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFAmenityMarker : public FTableRowBase
{
    GENERATED_BODY()

    /** Filled from the Data Table row name at load time. Not authored. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
    FName AmenityId = NAME_None;

    /** 2-4 words, plain: "Sensory room", "Guest Services". */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FText Label;

    /** One short line on where it is, in the words the signs use: "West Lower Suite Level". */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FText Note;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    ELFFAmenity Category = ELFFAmenity::GuestServices;

    /** The plate it's drawn on. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    ELFFMapLevel Level = ELFFMapLevel::LowerConcourse;

    /** Where it is in LFF_SiteRoot's frame, cm: +X across the field toward the east stand, +Y toward the south end, +Z up. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FVector Location = FVector::ZeroVector;

    /** An actor in the level tagged this marks the place: its position replaces Location when the journey loads, so the
     *  marker follows the actor when it's moved. None, or no such actor: Location. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FName LocationActorTag = NAME_None;

    /** Show me goes here. Must match exactly one ALFFCameraTarget::TargetId; none means the marker has no Show me. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FName CameraTargetId = NAME_None;

    /** Who confirmed the location, and when. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    FString VerificationSource;
};

/**
 * One level of the stadium in Free explore: its height band on the model (highlighted when the level is picked) and the
 * overview the camera eases to. Heights are in LFF_SiteRoot's frame, cm above the turf, measured from the model's floor
 * slabs (ALFFGameMode::LevelViews).
 */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFLevelView
{
    GENERATED_BODY()

    /** The level's main floor: the overview looks at it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float FloorZ = 0.0f;

    /** The band the highlight tints, bottom and top. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float MinZ = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float MaxZ = 0.0f;

    /** The overview: how far from the stadium centre, how steeply it looks down, and from which heading (0 looks along +X,
     *  from the west stand, the seat's side). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float ViewDistance = 26000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float ViewPitchDegrees = -35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
    float ViewYawDegrees = 20.0f;
};

/** Visitor choices for this session. Held in memory only; nothing is written to disk (LEDGER D11). */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFVisitorSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bReducedMotion = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bSoundEnabled = false;

    /** Opt-in bundle (LEDGER F25): less movement, sound off, crowd hidden, previews locked off, camera cuts only. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bCalmMode = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bLargeText = false;

    /** Presentation only: opaque panels and a darker scrim instead of frosted glass. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bHighContrast = false;

    /** Voiceover reads each card. Off by default; needs Sound on; Calm Mode forces it off. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bVoiceEnabled = false;

    /** A few stand-in people (Crowd: Low) or none (Off). Calm Mode hides them whatever this says. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bCrowdEnabled = true;

    bool IsReducedMotionEffective() const { return bReducedMotion || bCalmMode; }

    /** Calm Mode always means silence, whatever the Sound toggle says. */
    bool IsSoundEffective() const { return bSoundEnabled && !bCalmMode; }

    /** Sound Off mutes the voice too, and Calm Mode forces it off. */
    bool IsVoiceEffective() const { return bVoiceEnabled && IsSoundEffective(); }

    bool IsCrowdShown() const { return bCrowdEnabled && !bCalmMode; }
};

/** One mark on the timeline above Back/Next: a timed step of the example game. */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFTimelineMark
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    int32 StepIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText Label;
};

/** Everything the UI needs, pushed on every change. Widgets hold no journey state of their own. */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFJourneySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFJourneyPhase Phase = ELFFJourneyPhase::Empty;

    /** In Free explore, StepIndex and TourStep are where Resume tour goes back to. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFSimulationMode Mode = ELFFSimulationMode::GuidedTour;

    /** The chapter of StepIndex: the current node on the timeline arc ("my place" in Free explore). */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFTourStep TourStep = ELFFTourStep::Arrival;

    /** First step (0-based) of each ELFFTourStep, in order (ALFFGameMode::TourStepFirstSteps). */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    TArray<int32> TourStepFirstSteps;

    /** Free explore, no modal, not paused: Resume tour can be pressed. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bCanResumeTour = false;

    /** The level picked in Free explore (its callouts show). Kept across mode switches; reset by Start again. */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    ELFFMapLevel MapLevel = ELFFMapLevel::LowerConcourse;

    /** The picked level is highlighted on the model (after a level button, until the tour resumes). */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    bool bLevelHighlight = false;

    /** One bit per ELFFAmenity shown on the map. */
    UPROPERTY(BlueprintReadOnly, Category = "Map", meta = (Bitmask, BitmaskEnum = "/Script/VirtualStadium.ELFFAmenity"))
    int32 AmenityMask = LFFDefaultAmenityMask;

    /** The marker picked on the map (its card shows Show me). None when nothing is picked. */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    FName SelectedAmenityId = NAME_None;

    /** Guided tour: the current step's PlacesToShow, named on the model. Empty in Free explore and before the tour starts. */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    TArray<FName> TourPlaces;

    /** Where the camera is: the last camera target it went to ("You are here"). */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    FName ViewTargetId = NAME_None;

    /** The visitor's seat: its camera target and the level it's on (ALFFGameMode). */
    UPROPERTY(BlueprintReadOnly, Category = "Map")
    FName MySeatTargetId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Map")
    ELFFMapLevel MySeatLevel = ELFFMapLevel::LowerConcourse;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    int32 StepIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    int32 StepCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FName StepId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText Title;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText Body;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText ProgressText;

    /** The step's example-game clock ("~9:30 AM"); empty on untimed steps and the splash. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText ClockText;

    /** Every timed step, in order. The current step's mark is the only emphasized one. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    TArray<FLFFTimelineMark> TimelineMarks;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bShowTicketPhone = false;

    /** The voiceover is being read right now. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bVoicePlaying = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFDisclaimerLevel DisclaimerLevel = ELFFDisclaimerLevel::Illustrative;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText DisclaimerText;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bShowDisclaimer = true;

    /** One short line under the badge on journey steps ("Illustration. Not directions for the real stadium."). */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText DisclaimerLine;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText PrototypeBadgeText;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bStarted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bPaused = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bAtEnd = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bCanNext = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bCanBack = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bSettingsOpen = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bRestartConfirmOpen = false;

    /** The step's camera target is missing from the level. The UI says so; the camera does not move. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bTargetMissing = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FLFFVisitorSettings Settings;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFIntensityPreview IntensityPreview = ELFFIntensityPreview::None;

    /** The "Preview this?" button is on screen (an intensity step, Calm Mode off). */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bShowPreviewControls = false;

    /** The visitor picked Soft. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bPreviewOn = false;

    /** Soft can actually play: always for Lights, only with Sound on for Sound. */
    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bPreviewAvailable = false;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<UTexture2D> PreviewImage;
};
