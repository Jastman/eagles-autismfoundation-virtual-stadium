#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFHUDWidget.generated.h"

class ULFFJourneyPanel;
class ULFFJourneySubsystem;

/**
 * Base class for WBP_LFF_HUD (/Game/LFF/UI/WBP_LFF_HUD), the one HUD on screen.
 * It holds the five panels, decides which are shown for the current screen, and pushes every
 * journey snapshot into them:
 *  - Splash (before Start): the welcome card. TopChrome shows only the badge and Settings.
 *  - Step (after Start): TopChrome, StepCaption and BottomRail.
 *  - Settings: the modal layer (Settings, Paused, Start again?) over a dimmed world.
 * Required bindings make a HUD without one of the panels fail to compile.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot);

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Splash;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> TopChrome;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> StepCaption;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> BottomRail;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Settings;

private:
    ULFFJourneySubsystem* GetJourney() const;

    /** Returns true if the panel went from hidden to shown. */
    static bool ShowPanel(ULFFJourneyPanel* Panel, bool bShown, bool bInstant);

    /** Logs an error if a panel is missing the badge or disclaimer widgets the disclaimer rule needs. */
    void ValidateContract() const;

    TWeakObjectPtr<ULFFJourneySubsystem> BoundJourney;
    int32 LastStepIndex = INDEX_NONE;
};
