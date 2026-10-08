#pragma once

#include "CoreMinimal.h"
#include "UI/LFFJourneyPanel.h"
#include "LFFCalmToggle.generated.h"

class UWidgetAnimation;

/** Visual state mirrors the journey; the Widget Blueprint owns the reversible transition. */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFCalmToggle : public ULFFJourneyPanel
{
    GENERATED_BODY()

protected:
    virtual void NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot) override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Calm Mode")
    bool bCalmMode = false;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Calm Mode", meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> CalmTransition;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> CalmModeThumb;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CalmModeActiveScene;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CalmModeQuietScene;

private:
    bool bVisualStateInitialized = false;
};
