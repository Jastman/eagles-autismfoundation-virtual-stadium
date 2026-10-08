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
    /** Free flight: adds the flight keys while Free explore can take them, and feeds the journey each frame. */
    virtual void PlayerTick(float DeltaTime) override;

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
    void HandleInputSwitchToTour();
    void HandleInputSwitchToExplore();
    void HandleInputFocusMySeat();
    /** Reads this frame's flight input (value types: Axis2D move, Axis1D rise, bool fast, Axis2D look, Axis1D wheel). */
    void TickFlight(float DeltaTime);

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

    /** T: Guided tour (Start on the welcome screen, Resume tour from Free explore). */
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SwitchToTourAction;

    /** E: Free explore. Two keys, one meaning each, so pressing either twice is harmless. */
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SwitchToExploreAction;

    /** H: Free explore's My seat (the map turns to the seat's level, the camera goes to the seat). */
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FocusMySeatAction;

    /**
     * Free flight's keys, in their own context (IMC_LFF_Fly, priority 1), added only while Free explore can fly and
     * removed for the tour, a modal or a pause. While it's on it takes its keys from the journey context: W A S D and the
     * arrows move, E / Space / Page Up rise, Q / C / Page Down sink, Shift is faster, the wheel steps forward and back,
     * and a drag with either mouse button over the 3D view looks around. Keys held when it's added are ignored until
     * released, so the E that opened Free explore doesn't also lift the camera.
     */
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> FlyContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FlyMoveAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FlyRiseAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FlyFastAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FlyLookAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FlyWheelAction;

    bool bFlyContextOn = false;

    UPROPERTY(Transient)
    TObjectPtr<ULFFHUDWidget> HUDWidget;
};
