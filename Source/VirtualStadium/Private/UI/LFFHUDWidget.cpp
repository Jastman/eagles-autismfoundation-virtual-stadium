#include "UI/LFFHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFJourneyPanel.h"
#include "VirtualStadium.h"

void ULFFHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UOverlay* Root = Cast<UOverlay>(GetRootWidget());
    if (Root == nullptr || WidgetTree == nullptr || FocusRing != nullptr)
    {
        return;
    }

    // Last child of the root overlay, so it draws over every panel. It never takes clicks.
    // SizeBox > dark halo > light ring. The halo sits outside the ring, on the stadium side.
    FocusRing = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FocusRing"));
    auto MakeRing = [this](const TCHAR* Name, const FLinearColor& Color, const float Width)
    {
        UBorder* Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(FLinearColor::Transparent);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
        Brush.OutlineSettings.Width = Width;
        Brush.OutlineSettings.Color = FSlateColor(Color);
        Ring->SetBrush(Brush);
        Ring->SetPadding(FMargin(Width));
        return Ring;
    };
    UBorder* Halo = MakeRing(TEXT("FocusRingHalo"), FocusHaloColor, FocusHaloWidth);
    UBorder* Ring = MakeRing(TEXT("FocusRingOutline"), FocusRingColor, FocusRingWidth);
    Halo->SetContent(Ring);
    FocusRing->SetContent(Halo);
    if (UOverlaySlot* Slot = Root->AddChildToOverlay(FocusRing))
    {
        Slot->SetHorizontalAlignment(HAlign_Left);
        Slot->SetVerticalAlignment(VAlign_Top);
    }
    FocusRing->SetVisibility(ESlateVisibility::Collapsed);
}

void ULFFHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    PlaceFocusRing();
}

bool ULFFHUDWidget::IsOnScreen(const UButton* Button)
{
    if (Button == nullptr || !Button->GetIsEnabled() || !Button->GetCachedWidget().IsValid())
    {
        return false;
    }
    // Every parent up to the panel, then the panel itself (its tree root has no UMG parent).
    for (const UWidget* Widget = Button; Widget != nullptr; Widget = Widget->GetParent())
    {
        if (!Widget->IsVisible())
        {
            return false;
        }
    }
    const UUserWidget* Panel = Button->GetTypedOuter<UUserWidget>();
    if (Panel != nullptr && !Panel->IsVisible())
    {
        return false;
    }
    return Button->GetCachedGeometry().GetLocalSize().SizeSquared() > 1.0;
}

void ULFFHUDWidget::CollectFocusable(TArray<UButton*>& Out) const
{
    const ULFFJourneySubsystem* Journey = GetJourney();
    const FLFFJourneySnapshot Snapshot = Journey != nullptr ? Journey->GetSnapshot() : FLFFJourneySnapshot();
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;

    // Reading order: the card in front first, then the dock, then the header. A modal card keeps the ring inside it.
    TArray<ULFFJourneyPanel*, TInlineAllocator<5>> Order;
    if (bModal)
    {
        Order.Add(Settings.Get());
    }
    else
    {
        Order.Append({ Splash.Get(), StepCaption.Get(), BottomRail.Get(), TopChrome.Get() });
    }

    for (ULFFJourneyPanel* Panel : Order)
    {
        if (Panel == nullptr || Panel->WidgetTree == nullptr)
        {
            continue;
        }
        Panel->WidgetTree->ForEachWidget([&Out](UWidget* Widget)
        {
            UButton* Button = Cast<UButton>(Widget);
            if (IsOnScreen(Button))
            {
                Out.Add(Button);
            }
        });
    }
}

void ULFFHUDWidget::MoveFocus(const int32 Direction)
{
    TArray<UButton*> Buttons;
    CollectFocusable(Buttons);
    if (Buttons.Num() == 0)
    {
        ClearFocus();
        return;
    }

    const int32 Current = Buttons.IndexOfByKey(FocusedButton.Get());
    int32 Next = 0;
    if (Current == INDEX_NONE)
    {
        Next = Direction >= 0 ? 0 : Buttons.Num() - 1;
    }
    else
    {
        // Wraps around: the ring never gets stuck at an end.
        Next = (Current + (Direction >= 0 ? 1 : -1) + Buttons.Num()) % Buttons.Num();
    }
    FocusedButton = Buttons[Next];

    if (FocusRing != nullptr)
    {
        // The ring follows the button's own shape: a pill for pills, the same corners plus the gap for cards.
        const FSlateBrush& Shape = Buttons[Next]->GetStyle().Normal;
        const FVector4 R = Shape.OutlineSettings.CornerRadii;
        float Grow = FocusRingGap + FocusRingWidth + FocusHaloWidth;
        for (UWidget* Widget = FocusRing->GetContent(); UBorder* Ring = Cast<UBorder>(Widget); Widget = Ring->GetContent())
        {
            FSlateBrush Brush = Ring->Background;
            Brush.OutlineSettings.RoundingType = Shape.OutlineSettings.RoundingType;
            Brush.OutlineSettings.CornerRadii = FVector4(R.X + Grow, R.Y + Grow, R.Z + Grow, R.W + Grow);
            Ring->SetBrush(Brush);
            Grow -= Ring == FocusRing->GetContent() ? FocusHaloWidth : FocusRingWidth;
        }
    }
    PlaceFocusRing();
}

bool ULFFHUDWidget::ActivateFocus()
{
    UButton* Button = FocusedButton.Get();
    if (!IsOnScreen(Button))
    {
        ClearFocus();
        return false;
    }
    Button->OnClicked.Broadcast();
    return true;
}

void ULFFHUDWidget::ClearFocus()
{
    FocusedButton.Reset();
    if (FocusRing != nullptr)
    {
        FocusRing->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ULFFHUDWidget::PlaceFocusRing()
{
    if (FocusRing == nullptr || !FocusedButton.IsValid())
    {
        return;
    }
    UButton* Button = FocusedButton.Get();
    const UWidget* Root = GetRootWidget();
    if (!IsOnScreen(Button) || Root == nullptr)
    {
        // The button went away (new step, card closed): drop the ring rather than leave it floating.
        ClearFocus();
        return;
    }

    const FGeometry& ButtonGeometry = Button->GetCachedGeometry();
    const FGeometry& RootGeometry = Root->GetCachedGeometry();
    const FVector2D TopLeft = RootGeometry.AbsoluteToLocal(ButtonGeometry.GetAbsolutePosition());
    const FVector2D BottomRight = RootGeometry.AbsoluteToLocal(ButtonGeometry.GetAbsolutePosition() + ButtonGeometry.GetAbsoluteSize());
    const float Grow = FocusRingGap + FocusRingWidth + FocusHaloWidth;

    FocusRing->SetWidthOverride(BottomRight.X - TopLeft.X + Grow * 2.0f);
    FocusRing->SetHeightOverride(BottomRight.Y - TopLeft.Y + Grow * 2.0f);
    FocusRing->SetRenderTranslation(TopLeft - FVector2D(Grow, Grow));
    FocusRing->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULFFHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

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

ULFFJourneySubsystem* ULFFHUDWidget::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}
