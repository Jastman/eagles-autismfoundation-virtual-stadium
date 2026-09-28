#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFGameMode.generated.h"

class UDataTable;

/**
 * Journey configuration for the stadium map. Set as GlobalDefaultGameMode in DefaultEngine.ini.
 * No pawn is spawned; the camera rig is the view target.
 * Defaults work without a Blueprint subclass; a data-only BP may override them.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALFFGameMode();

    /** Cooked via DirectoriesToAlwaysCook=/Game/LFF/Data (DefaultGame.ini). */
    UPROPERTY(EditDefaultsOnly, Category = "Journey")
    TSoftObjectPtr<UDataTable> JourneyTable;

    UPROPERTY(EditDefaultsOnly, Category = "Journey")
    FLFFVisitorSettings DefaultVisitorSettings;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText IntroTitle;

    UPROPERTY(EditDefaultsOnly, Category = "Copy", meta = (MultiLine = "true"))
    FText IntroBody;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText PrototypeBadgeText;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerIllustrative;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerApproximate;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText TargetMissingText;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText JourneyMissingText;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float DefaultEaseSeconds = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float MinTransitionSeconds = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float MaxTransitionSeconds = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.05"))
    float ReducedMotionFadeSeconds = 0.35f;

    /** Neutral dark grey rather than black, to soften the fade. */
    UPROPERTY(EditDefaultsOnly, Category = "Timing")
    FLinearColor FadeColor = FLinearColor(0.08f, 0.08f, 0.10f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BedVolume = 0.3f;

    /** Calm Mode multiplies sound volume by this. */
    UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CalmVolumeMultiplier = 0.5f;

    FText GetDisclaimerText(ELFFDisclaimerLevel Level) const;
};
