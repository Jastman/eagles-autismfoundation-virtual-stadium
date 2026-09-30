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

#undef LOCTEXT_NAMESPACE
