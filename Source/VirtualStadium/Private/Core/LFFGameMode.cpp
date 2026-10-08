#include "Core/LFFGameMode.h"

#include "Core/LFFPlayerController.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "LFFGameMode"

ALFFGameMode::ALFFGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ALFFPlayerController::StaticClass();

    JourneyTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/LFF/Data/DT_LFF_GoldenPath.DT_LFF_GoldenPath")));

    // Sound and the voiceover start on (project owner, session 5); each card's voice button and Settings turn them off,
    // and Calm Mode still silences both.
    DefaultVisitorSettings.bSoundEnabled = true;
    DefaultVisitorSettings.bVoiceEnabled = true;

    // Placeholder copy. Final wording is approved by the project owner with the Eagles Autism Foundation (LEDGER F24).
    // The splash shows "Welcome to" above the title; the disclaimer sits under the Start button.
    IntroTitle = LOCTEXT("IntroTitle", "Lincoln Financial Field");
    IntroBody = LOCTEXT("IntroBody", "A calm preview of game day.\nYou choose when to move on.");
    PrototypeBadgeText = LOCTEXT("PrototypeBadge", "Prototype · Illustrative · Example ticket");
    DisclaimerIllustrative = LOCTEXT("DisclaimerIllustrative", "Illustration only. This is not a navigation instruction for the real stadium.");
    DisclaimerApproximate = LOCTEXT("DisclaimerApproximate", "Approximate view. This is not a navigation instruction for the real stadium.");
    DisclaimerLineIllustrative = LOCTEXT("DisclaimerLineIllustrative", "Illustration. Not directions for the real stadium.");
    DisclaimerLineApproximate = LOCTEXT("DisclaimerLineApproximate", "Approximate. Not directions for the real stadium.");
    TargetMissingText = LOCTEXT("TargetMissing", "This step is not available yet.");

    // Synthesized placeholders (Tools/Audio/make_lff_audio.py), so there is nothing to license. No horns, no fireworks.
    AmbientBed = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/LFF/Audio/SW_LFF_AmbientPlaza.SW_LFF_AmbientPlaza")));
    PreviewSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/LFF/Audio/SW_LFF_PreviewCrowdMuffled.SW_LFF_PreviewCrowdMuffled")));
    JourneyMissingText = LOCTEXT("JourneyMissing", "The journey could not be loaded.");

    MySeatLabel = LOCTEXT("MySeatLabel", "My seat");
    MySeatNote = LOCTEXT("MySeatNote", "Section 111, Row 10, Seat 8.");

    // ELFFMapLevel order. Bands overlap nothing and cover the whole model from the turf to the top of the upper deck; each
    // overview looks at its floor from the west, a little steeper for the lower levels so the bowl doesn't hide them.
    auto View = [](const float FloorZ, const float MinZ, const float MaxZ, const float Distance, const float Pitch)
    {
        FLFFLevelView Level;
        Level.FloorZ = FloorZ;
        Level.MinZ = MinZ;
        Level.MaxZ = MaxZ;
        Level.ViewDistance = Distance;
        Level.ViewPitchDegrees = Pitch;
        return Level;
    };
    LevelViews = {
        View(0.0f, -150.0f, 120.0f, 18000.0f, -55.0f),       // Field: the turf and sidelines
        View(770.0f, 120.0f, 1000.0f, 19000.0f, -45.0f),     // Lower concourse: the grade walkway, the 7.7 m deck, the lower bowl
        View(1720.0f, 1000.0f, 1950.0f, 20000.0f, -38.0f),   // Club level
        View(2180.0f, 1950.0f, 2900.0f, 21000.0f, -34.0f),   // Suites: both suite floors (21.8 and 26.7 m)
        View(3120.0f, 2900.0f, 5000.0f, 22000.0f, -30.0f),   // Upper deck: the upper concourse and seats
    };
}

FText ALFFGameMode::GetDisclaimerText(const ELFFDisclaimerLevel Level) const
{
    switch (Level)
    {
    case ELFFDisclaimerLevel::Verified:
        return FText::GetEmpty();
    case ELFFDisclaimerLevel::Approximate:
        return DisclaimerApproximate;
    case ELFFDisclaimerLevel::Illustrative:
    default:
        return DisclaimerIllustrative;
    }
}

FText ALFFGameMode::GetDisclaimerLine(const ELFFDisclaimerLevel Level) const
{
    switch (Level)
    {
    case ELFFDisclaimerLevel::Verified:
        return FText::GetEmpty();
    case ELFFDisclaimerLevel::Approximate:
        return DisclaimerLineApproximate;
    case ELFFDisclaimerLevel::Illustrative:
    default:
        return DisclaimerLineIllustrative;
    }
}

void ALFFGameMode::StartPlay()
{
    // No engine debug text over the picture (texture pool, shadow cache warnings): visitors see only the HUD.
    GAreScreenMessagesEnabled = false;
    Super::StartPlay();
}

void ALFFGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // The editor shares this global with PIE.
    GAreScreenMessagesEnabled = true;
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
