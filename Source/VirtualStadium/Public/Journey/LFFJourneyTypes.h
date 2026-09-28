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

/** Authored camera move. Reduced motion / Calm Mode turn Ease into ReducedMotion (fade, cut, fade). */
UENUM(BlueprintType)
enum class ELFFTransitionProfile : uint8
{
    Instant,
    Ease,
    ReducedMotion
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

    /** Optional ambient bed. Plays only when the visitor has turned sound on. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<USoundBase> AudioBed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TSoftObjectPtr<UTexture2D> PreviewImage;

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

    /** Opt-in bundle: reduced motion, quieter sound, muted theme (LEDGER F25). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bCalmMode = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    bool bLargeText = false;

    bool IsReducedMotionEffective() const { return bReducedMotion || bCalmMode; }
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

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    ELFFDisclaimerLevel DisclaimerLevel = ELFFDisclaimerLevel::Illustrative;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    FText DisclaimerText;

    UPROPERTY(BlueprintReadOnly, Category = "Journey")
    bool bShowDisclaimer = true;

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
    TSoftObjectPtr<UTexture2D> PreviewImage;
};
