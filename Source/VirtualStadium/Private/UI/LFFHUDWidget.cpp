#include "UI/LFFHUDWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Journey/LFFJourneyStateMachine.h"
#include "Journey/LFFJourneySubsystem.h"

#define LOCTEXT_NAMESPACE "LFFHUD"

namespace
{
    void ShowWidget(UWidget* Widget, const bool bShown)
    {
        if (Widget != nullptr)
        {
            Widget->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }
    }

    void ApplyText(UTextBlock* TextBlock, const FText& Text)
    {
        if (TextBlock != nullptr)
        {
            TextBlock->SetText(Text);
        }
    }

    FText OnOff(const FText& Label, const bool bOn)
    {
        return FText::Format(
            LOCTEXT("OnOffFormat", "{0}: {1}"),
            Label,
            bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"));
    }
}

void ULFFHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BindButtons();

    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        BoundJourney = Journey;
        Journey->OnJourneyChanged.AddDynamic(this, &ULFFHUDWidget::HandleJourneyChanged);
        HandleJourneyChanged(Journey->GetSnapshot());
    }
}

void ULFFHUDWidget::NativeDestruct()
{
    if (ULFFJourneySubsystem* Journey = BoundJourney.Get())
    {
        Journey->OnJourneyChanged.RemoveDynamic(this, &ULFFHUDWidget::HandleJourneyChanged);
    }

    BoundJourney.Reset();
    Super::NativeDestruct();
}

void ULFFHUDWidget::HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot)
{
    ApplyText(TitleText.Get(), Snapshot.Title);
    ApplyText(BodyText.Get(), Snapshot.Body);
    ApplyText(ProgressText.Get(), Snapshot.ProgressText);
    ApplyText(PrototypeBadgeText.Get(), Snapshot.PrototypeBadgeText);
    ApplyText(DisclaimerText.Get(), Snapshot.DisclaimerText);

    if (TargetMissingText != nullptr)
    {
        ApplyText(TargetMissingText.Get(), LOCTEXT("TargetMissing", "This step is not available yet."));
        ShowWidget(TargetMissingText.Get(), Snapshot.bTargetMissing);
    }

    FText NextText = LOCTEXT("Next", "Next");
    if (!Snapshot.bStarted)
    {
        NextText = LOCTEXT("Start", "Start");
    }
    else if (Snapshot.bAtEnd)
    {
        NextText = LOCTEXT("StartAgain", "Start again");
    }
    ApplyText(NextLabel.Get(), NextText);
    ApplyText(PauseLabel.Get(), Snapshot.bPaused ? LOCTEXT("Resume", "Resume") : LOCTEXT("Pause", "Pause"));

    ApplyText(ReducedMotionLabel.Get(), OnOff(LOCTEXT("LessMovement", "Less movement"), Snapshot.Settings.bReducedMotion));
    ApplyText(SoundLabel.Get(), OnOff(LOCTEXT("Sound", "Sound"), Snapshot.Settings.bSoundEnabled));
    ApplyText(CalmModeLabel.Get(), OnOff(LOCTEXT("CalmMode", "Calm Mode"), Snapshot.Settings.bCalmMode));
    ApplyText(LargeTextLabel.Get(), OnOff(LOCTEXT("LargeText", "Large text"), Snapshot.Settings.bLargeText));

    ShowWidget(IntroPanel.Get(), !Snapshot.bStarted);
    ShowWidget(PausePanel.Get(), Snapshot.bPaused);
    ShowWidget(SettingsPanel.Get(), Snapshot.bSettingsOpen);
    ShowWidget(RestartConfirmPanel.Get(), Snapshot.bRestartConfirmOpen);
    ShowWidget(TogglesPanel.Get(), LFFJourneyRules::ShouldShowToggles(Snapshot.bStarted, Snapshot.bSettingsOpen));

    // Buttons stay in place and are disabled rather than hidden, so the layout never shifts.
    if (NextButton != nullptr)
    {
        NextButton->SetIsEnabled(Snapshot.bCanNext);
    }
    if (BackButton != nullptr)
    {
        BackButton->SetIsEnabled(Snapshot.bCanBack);
    }
    if (RestartButton != nullptr)
    {
        RestartButton->SetIsEnabled(Snapshot.bStarted && !Snapshot.bSettingsOpen);
    }
    if (PauseButton != nullptr)
    {
        PauseButton->SetIsEnabled(Snapshot.bStarted && !Snapshot.bSettingsOpen && !Snapshot.bRestartConfirmOpen);
    }

    OnSnapshotChanged(Snapshot);

    // Enforced after the Blueprint event: the disclaimer and prototype badge can't be hidden by styling.
    if (Snapshot.bShowDisclaimer)
    {
        ShowWidget(DisclaimerPanel.Get(), true);
        ShowWidget(DisclaimerText.Get(), true);
    }
    ShowWidget(PrototypeBadgeText.Get(), true);
}

ULFFJourneySubsystem* ULFFHUDWidget::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}

void ULFFHUDWidget::BindButtons()
{
    if (NextButton != nullptr) { NextButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleNextClicked); }
    if (BackButton != nullptr) { BackButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleBackClicked); }
    if (PauseButton != nullptr) { PauseButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandlePauseClicked); }
    if (ResumeButton != nullptr) { ResumeButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleResumeClicked); }
    if (SettingsButton != nullptr) { SettingsButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleSettingsClicked); }
    if (SettingsCloseButton != nullptr) { SettingsCloseButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleSettingsClicked); }
    if (RestartButton != nullptr) { RestartButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleRestartClicked); }
    if (RestartYesButton != nullptr) { RestartYesButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleRestartYesClicked); }
    if (RestartNoButton != nullptr) { RestartNoButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleRestartNoClicked); }
    if (ReducedMotionButton != nullptr) { ReducedMotionButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleReducedMotionClicked); }
    if (SoundButton != nullptr) { SoundButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleSoundClicked); }
    if (CalmModeButton != nullptr) { CalmModeButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleCalmModeClicked); }
    if (LargeTextButton != nullptr) { LargeTextButton->OnClicked.AddUniqueDynamic(this, &ULFFHUDWidget::HandleLargeTextClicked); }
}

void ULFFHUDWidget::ReturnFocusToGame() const
{
    // After a mouse click, hand keyboard focus back to the viewport so arrow keys and Space
    // drive the journey instead of moving UMG focus or re-clicking the button.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}

void ULFFHUDWidget::HandleNextClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Next(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleBackClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Back(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandlePauseClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->TogglePause(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleResumeClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Resume(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleSettingsClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleSettings(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleRestartClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->RequestRestart(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleRestartYesClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ConfirmRestart(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleRestartNoClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Cancel(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleReducedMotionClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleReducedMotion(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleSoundClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleSound(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleCalmModeClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleCalmMode(); }
    ReturnFocusToGame();
}

void ULFFHUDWidget::HandleLargeTextClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleLargeText(); }
    ReturnFocusToGame();
}

#undef LOCTEXT_NAMESPACE
