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
    RejectedPaused
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
