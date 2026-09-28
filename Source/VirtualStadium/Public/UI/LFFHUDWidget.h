#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class ULFFJourneySubsystem;

/**
 * Base class for WBP_LFF_HUD (/Game/LFF/UI/WBP_LFF_HUD). Layout and styling live in the Blueprint;
 * this class pushes the journey snapshot into named widgets and routes clicks to the subsystem.
 * Required bindings make a Blueprint without the disclaimer or prototype badge fail to compile.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Visual-only reactions (Calm Mode theme, large text). Runs before the disclaimer is re-enforced. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Journey")
    void OnSnapshotChanged(const FLFFJourneySnapshot& Snapshot);

    UFUNCTION()
    void HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot);

    // Required.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DisclaimerText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PrototypeBadgeText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> BodyText;

    // Optional text.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ProgressText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TargetMissingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NextLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PauseLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ReducedMotionLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SoundLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CalmModeLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LargeTextLabel;

    // Optional panels, shown and hidden from the snapshot.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> DisclaimerPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> IntroPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PausePanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SettingsPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> RestartConfirmPanel;

    // Optional buttons.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> NextButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PauseButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ResumeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsCloseButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartYesButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartNoButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ReducedMotionButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SoundButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CalmModeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LargeTextButton;

private:
    ULFFJourneySubsystem* GetJourney() const;
    void BindButtons();
    void ReturnFocusToGame() const;

    UFUNCTION()
    void HandleNextClicked();
    UFUNCTION()
    void HandleBackClicked();
    UFUNCTION()
    void HandlePauseClicked();
    UFUNCTION()
    void HandleResumeClicked();
    UFUNCTION()
    void HandleSettingsClicked();
    UFUNCTION()
    void HandleRestartClicked();
    UFUNCTION()
    void HandleRestartYesClicked();
    UFUNCTION()
    void HandleRestartNoClicked();
    UFUNCTION()
    void HandleReducedMotionClicked();
    UFUNCTION()
    void HandleSoundClicked();
    UFUNCTION()
    void HandleCalmModeClicked();
    UFUNCTION()
    void HandleLargeTextClicked();

    TWeakObjectPtr<ULFFJourneySubsystem> BoundJourney;
};
