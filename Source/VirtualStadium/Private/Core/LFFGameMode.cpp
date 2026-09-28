#include "Core/LFFGameMode.h"

#include "Core/LFFPlayerController.h"
#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "LFFGameMode"

ALFFGameMode::ALFFGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ALFFPlayerController::StaticClass();

    JourneyTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/LFF/Data/DT_LFF_GoldenPath.DT_LFF_GoldenPath")));

    // Placeholder copy. Final wording is approved by the project owner with the Eagles Autism Foundation (LEDGER F24).
    IntroTitle = LOCTEXT("IntroTitle", "Welcome to game day");
    IntroBody = LOCTEXT("IntroBody", "This preview may help you get ready for a game at Lincoln Financial Field. We will go one step at a time. You choose when to move on.\n\nThis is an illustration. It is not directions for the real stadium.");
    PrototypeBadgeText = LOCTEXT("PrototypeBadge", "Prototype · Illustrative · Example ticket");
    DisclaimerIllustrative = LOCTEXT("DisclaimerIllustrative", "Illustration only. This is not a navigation instruction for the real stadium.");
    DisclaimerApproximate = LOCTEXT("DisclaimerApproximate", "Approximate view. This is not a navigation instruction for the real stadium.");
    TargetMissingText = LOCTEXT("TargetMissing", "This step is not available yet.");
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

#undef LOCTEXT_NAMESPACE
