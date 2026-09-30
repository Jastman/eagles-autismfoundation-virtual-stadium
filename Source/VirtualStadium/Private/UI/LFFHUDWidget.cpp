#include "UI/LFFHUDWidget.h"

#include "Engine/World.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFJourneyPanel.h"
#include "VirtualStadium.h"

void ULFFHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ValidateContract();

    // Start hidden so the first snapshot fades in only what the current screen needs.
    for (ULFFJourneyPanel* Panel : { Splash.Get(), TopChrome.Get(), StepCaption.Get(), BottomRail.Get(), Settings.Get() })
    {
        if (Panel != nullptr)
        {
            Panel->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

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
    // Reduced motion and Calm Mode turn every UI fade into an instant cut.
    const bool bInstant = Snapshot.Settings.IsReducedMotionEffective();
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;

    for (ULFFJourneyPanel* Panel : { Splash.Get(), TopChrome.Get(), StepCaption.Get(), BottomRail.Get(), Settings.Get() })
    {
        if (Panel != nullptr)
        {
            Panel->ApplySnapshot(Snapshot);
        }
    }

    ShowPanel(TopChrome.Get(), true, bInstant);
    ShowPanel(Splash.Get(), !Snapshot.bStarted, bInstant);
    ShowPanel(BottomRail.Get(), Snapshot.bStarted, bInstant);
    ShowPanel(Settings.Get(), bModal, bInstant);

    const bool bCaptionAppeared = ShowPanel(StepCaption.Get(), Snapshot.bStarted, bInstant);
    if (Snapshot.bStarted && !bCaptionAppeared && Snapshot.StepIndex != LastStepIndex && StepCaption != nullptr)
    {
        // New step, same card: fade the new words in so the change is noticeable but not sudden.
        StepCaption->FadeIn(bInstant);
    }
    LastStepIndex = Snapshot.bStarted ? Snapshot.StepIndex : INDEX_NONE;
}

bool ULFFHUDWidget::ShowPanel(ULFFJourneyPanel* Panel, const bool bShown, const bool bInstant)
{
    if (Panel == nullptr)
    {
        return false;
    }

    const bool bWasShown = Panel->GetVisibility() != ESlateVisibility::Collapsed;
    if (!bShown)
    {
        Panel->SetVisibility(ESlateVisibility::Collapsed);
        return false;
    }

    // Panels fill the screen, so they must never take clicks themselves; only their buttons do.
    Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (!bWasShown)
    {
        Panel->FadeIn(bInstant);
        return true;
    }

    return false;
}

void ULFFHUDWidget::ValidateContract() const
{
    const TPair<const ULFFJourneyPanel*, const TCHAR*> Required[] = {
        { TopChrome.Get(), TEXT("PrototypeBadgeText") },
        { TopChrome.Get(), TEXT("DisclaimerLineText") },
        { StepCaption.Get(), TEXT("DisclaimerChipText") },
        { Splash.Get(), TEXT("DisclaimerText") },
    };

    for (const TPair<const ULFFJourneyPanel*, const TCHAR*>& Entry : Required)
    {
        if (Entry.Key != nullptr && Entry.Key->GetWidgetFromName(Entry.Value) == nullptr)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("%s has no %s. The prototype badge or disclaimer cannot be shown; do not ship this build."),
                *Entry.Key->GetClass()->GetName(), Entry.Value);
        }
    }
}

ULFFJourneySubsystem* ULFFHUDWidget::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}
