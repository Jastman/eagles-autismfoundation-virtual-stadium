#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LFFPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ULFFHUDWidget;
class ULFFJourneySubsystem;

/**
 * Keyboard/mouse first, controller as backup (LEDGER D16).
 * Input actions and key mappings are built in code so every binding is reviewable in git and no
 * input assets need to be authored. The HUD widget Blueprint is loaded from a fixed path.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ALFFPlayerController();

    /** Cooked via DirectoriesToAlwaysCook=/Game/LFF/UI (DefaultGame.ini). */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSoftClassPtr<ULFFHUDWidget> HUDWidgetClass;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void BuildInputObjects();
    UInputAction* MakeAction(const TCHAR* Name);
    ULFFJourneySubsystem* GetJourney() const;

    void HandleNext();
    void HandleBack();
    void HandlePause();
    void HandleSettings();
    void HandleRestart();
    void HandleConfirm();
    void HandleCancel();
    void HandleToggleReducedMotion();
    void HandleToggleSound();
    void HandleToggleCalmMode();
    void HandleToggleLargeText();
    void HandleTogglePreview();
    void HandleToggleVoice();
    void HandleToggleCrowd();
    /** Tab (Shift+Tab goes back) and D-pad Down / Up: the HUD's focus ring (ULFFHUDWidget::MoveFocus). */
    void HandleFocusNext();
    void HandleFocusPrevious();

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> JourneyContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> NextAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> BackAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> PauseAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SettingsAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> RestartAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ConfirmAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> CancelAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleReducedMotionAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleSoundAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleCalmModeAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleLargeTextAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> TogglePreviewAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleVoiceAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ToggleCrowdAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FocusNextAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FocusPreviousAction;

    UPROPERTY(Transient)
    TObjectPtr<ULFFHUDWidget> HUDWidget;
};
