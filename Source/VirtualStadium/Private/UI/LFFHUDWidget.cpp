#include "UI/LFFHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFCalloutLayer.h"
#include "UI/LFFJourneyPanel.h"
#include "UI/LFFMapPanel.h"
#include "UI/LFFTimelineArc.h"
#include "VirtualStadium.h"

void ULFFHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    const TPair<UWidgetSwitcher*, ULFFJourneyPanel*> SwitcherChildren[] = {
        { NavSwitcher.Get(), TourNav.Get() }, { NavSwitcher.Get(), ExploreNav.Get() } };
    for (const TPair<UWidgetSwitcher*, ULFFJourneyPanel*>& Pair : SwitcherChildren)
    {
        if (Pair.Key != nullptr && Pair.Value != nullptr && Pair.Value->GetParent() != Pair.Key)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("HUD: %s must be a direct child of %s, or the mode switch can't show it."),
                *Pair.Value->GetName(), *Pair.Key->GetName());
        }
    }

    if (SidebarBackdrop != nullptr)
    {
        BackdropColor = SidebarBackdrop->GetBrushColor();
    }
    if (Callouts != nullptr)
    {
        // A place's head never shows under an opaque panel (each counts only while it's on screen).
        Callouts->SetBlockers({ InformationSize.Get(), ControlSize.Get() });
    }

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
    ULFFJourneySubsystem* Journey = GetJourney();
    const FVector2D Size = MyGeometry.GetLocalSize();
    if (Journey != nullptr && Size.X > 1.0 && Size.Y > 1.0)
    {
        const bool bLarge = Journey->GetSnapshot().Settings.bLargeText;
        if (!Size.Equals(LastLayoutSize, 1.0) || bLarge != bLastLargeText)
        {
            LastLayoutSize = Size;
            bLastLargeText = bLarge;
            if (Welcome != nullptr)
            {
                Welcome->LayoutWelcome(Size);
            }
            const float CaptionScale = bLarge ? 1.25f : 1.0f;
            const float InfoWidth = FMath::Min(480.0f * CaptionScale, static_cast<float>(Size.X * 0.43));
            const float ControlWidth = FMath::Min(540.0f, static_cast<float>(Size.X * 0.43));
            if (InformationSize != nullptr)
            {
                InformationSize->SetWidthOverride(InfoWidth);
                InformationSize->SetMaxDesiredHeight(FMath::Max(64.0, Size.Y - 112.0));
            }
            if (ControlSize != nullptr)
            {
                ControlSize->SetWidthOverride(ControlWidth);
                ControlSize->SetMaxDesiredHeight(FMath::Max(64.0, Size.Y - 112.0));
            }
            if (TimelineArc != nullptr)
            {
                TimelineArc->LayoutChapters(FMath::Max(0.0f, ControlWidth - 24.0f), bLarge);
            }
            if (USizeBox* Limit = Cast<USizeBox>(GetWidgetFromName(TEXT("NarrativeLimit"))))
            {
                Limit->SetMaxDesiredHeight(FMath::Max(64.0, Size.Y - 228.0));
            }
            if (Narrative != nullptr)
            {
                const float TextWidth = FMath::Max(128.0f, (InfoWidth - 40.0f) / CaptionScale);
                if (USizeBox* Width = Cast<USizeBox>(Narrative->GetWidgetFromName(TEXT("CaptionWidth"))))
                {
                    Width->SetWidthOverride(TextWidth);
                }
                Narrative->WidgetTree->ForEachWidget([TextWidth](UWidget* Widget)
                {
                    if (UTextBlock* Text = Cast<UTextBlock>(Widget); Text != nullptr && Text->GetWrapTextAt() > 0.0f)
                    {
                        Text->SetWrapTextAt(TextWidth);
                    }
                    if (URichTextBlock* Text = Cast<URichTextBlock>(Widget); Text != nullptr && Text->GetWrapTextAt() > 0.0f)
                    {
                        Text->SetWrapTextAt(TextWidth);
                    }
                });
            }
            if (ExploreMap != nullptr)
            {
                ExploreMap->WidgetTree->ForEachWidget([ControlWidth](UWidget* Widget)
                {
                    if (UTextBlock* Text = Cast<UTextBlock>(Widget); Text != nullptr && Text->GetWrapTextAt() > 0.0f)
                    {
                        Text->SetWrapTextAt(FMath::Max(128.0f, ControlWidth - 96.0f));
                    }
                });
            }
        }
        TArray<FBox2D> Panels;
        for (USizeBox* Panel : { InformationSize.Get(), ControlSize.Get() })
        {
            if (Panel != nullptr && Panel->IsVisible() && Panel->GetCachedGeometry().GetLocalSize().SizeSquared() > 1.0)
            {
                const FGeometry& Geometry = Panel->GetCachedGeometry();
                Panels.Emplace(MyGeometry.AbsoluteToLocal(Geometry.GetAbsolutePosition()),
                    MyGeometry.AbsoluteToLocal(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize()));
            }
        }
        const float Inset = Callouts != nullptr ? FMath::Max(48.0f, Callouts->GetHitSize() * 0.5f) : 48.0f;
        const FBox2D Safe = LFFJourneyRules::UnobstructedViewport(Size, Panels, Inset, 112.0f);
        const FBox2D Normalized = Safe.bIsValid ? FBox2D(Safe.Min / Size, Safe.Max / Size) : FBox2D(ForceInit);
        Journey->SetMapViewportFrame(Size, Normalized, Callouts != nullptr ? Callouts->GetStemHeight() : 2500.0f);
    }
    PlaceFocusRing();
    if (Welcome != nullptr)
    {
        Welcome->UpdateWelcomeHint(IsOnScreen(FocusedButton.Get()) ? FocusedButton.Get() : nullptr);
    }
}

UWidget* ULFFHUDWidget::ParentAcrossTrees(const UWidget* Widget)
{
    if (UWidget* Parent = Widget->GetParent())
    {
        return Parent;
    }
    // The root of a user widget's tree has no UMG parent: carry on from the user widget itself, which sits in the
    // tree that holds it (a panel in the HUD, maybe under a switcher). The HUD's own outer isn't a user widget, so it ends there.
    return Widget->GetTypedOuter<UUserWidget>();
}

bool ULFFHUDWidget::IsOnScreen(const UButton* Button)
{
    // A button that can't be clicked (hit-test invisible: the tour's place heads) can't be pressed from the ring either.
    if (Button == nullptr || !Button->GetIsEnabled() || !Button->GetCachedWidget().IsValid()
        || Button->GetVisibility() == ESlateVisibility::HitTestInvisible)
    {
        return false;
    }
    // Every parent up to the HUD, through the panel that holds the button and the switcher that may hold the panel.
    for (const UWidget* Widget = Button; Widget != nullptr; Widget = ParentAcrossTrees(Widget))
    {
        if (!Widget->IsVisible())
        {
            return false;
        }
        // A Widget Switcher arranges only its active child but leaves every child Visible, and an inactive child's
        // buttons keep the geometry they had when it was last shown. Visible isn't enough here: only the active branch is drawn.
        if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget->GetParent()))
        {
            if (Switcher->GetActiveWidget() != Widget)
            {
                return false;
            }
        }
    }
    return Button->GetCachedGeometry().GetLocalSize().SizeSquared() > 1.0;
}

void ULFFHUDWidget::CollectFocusable(TArray<UButton*>& Out) const
{
    const ULFFJourneySubsystem* Journey = GetJourney();
    const FLFFJourneySnapshot Snapshot = Journey != nullptr ? Journey->GetSnapshot() : FLFFJourneySnapshot();
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;

    // Caption first, then the control dock, chapters, navigation, places and spatial markers.
    // A modal card keeps the ring inside it, and so does the welcome screen.
    // Both modes' panels are listed: IsOnScreen drops the hidden ones.
    TArray<ULFFJourneyPanel*, TInlineAllocator<12>> Order;
    if (bModal)
    {
        Order.Add(Settings.Get());
    }
    else if (IsWelcome(Snapshot))
    {
        Order.Add(Welcome.Get());
    }
    else
    {
        Order.Append({ Narrative.Get(), StageChrome.Get(), TimelineArc.Get(), TourNav.Get(), ExploreNav.Get(), ExploreMap.Get(), Callouts.Get() });
    }

    for (ULFFJourneyPanel* Panel : Order)
    {
        if (Panel == nullptr || Panel->WidgetTree == nullptr)
        {
            continue;
        }
        Panel->WidgetTree->ForEachWidgetAndDescendants([&Out](UWidget* Widget)
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
    for (UWidget* Widget = Buttons[Next]; Widget != nullptr; Widget = ParentAcrossTrees(Widget))
    {
        if (UScrollBox* Scroll = Cast<UScrollBox>(Widget))
        {
            Scroll->ScrollWidgetIntoView(Buttons[Next], false, EDescendantScrollDestination::IntoView, 8.0f);
        }
    }

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
    for (ULFFJourneyPanel* Panel : GetPanels())
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

TArray<ULFFJourneyPanel*, TInlineAllocator<12>> ULFFHUDWidget::GetPanels() const
{
    return { SidebarHeader.Get(), Narrative.Get(), ExploreMap.Get(), Callouts.Get(), TimelineArc.Get(), StageChrome.Get(),
        TourNav.Get(), ExploreNav.Get(), Welcome.Get(), Settings.Get() };
}

bool ULFFHUDWidget::IsWelcome(const FLFFJourneySnapshot& Snapshot)
{
    return !Snapshot.bStarted && Snapshot.Mode == ELFFSimulationMode::GuidedTour;
}

void ULFFHUDWidget::HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot)
{
    // Reduced motion and Calm Mode turn every UI fade into an instant cut.
    const bool bInstant = Snapshot.Settings.IsReducedMotionEffective();
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;
    const bool bWelcome = IsWelcome(Snapshot);
    const bool bExplore = Snapshot.Mode == ELFFSimulationMode::FreeExplore;

    for (ULFFJourneyPanel* Panel : GetPanels())
    {
        if (Panel != nullptr)
        {
            Panel->ApplySnapshot(Snapshot);
        }
    }

    // The header card and the top right (Calm Mode, Settings) are the same in both modes; the rest follows the mode.
    ShowPanel(Welcome.Get(), bWelcome, bInstant);
    for (USizeBox* Group : { InformationSize.Get(), ControlSize.Get() })
    {
        if (Group != nullptr)
        {
            Group->SetVisibility(bWelcome ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
        }
    }
    if (SidebarSize != nullptr)
    {
        SidebarSize->SetVisibility(bWelcome ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
    if (SidebarBackdrop != nullptr)
    {
        // Visible, not hit-test invisible: in Free explore a click on the sidebar's empty parts must not reach the scene.
        SidebarBackdrop->SetVisibility(bExplore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        SidebarBackdrop->SetBrushColor(Snapshot.Settings.bHighContrast ? FLinearColor::Black : BackdropColor);
    }
    for (ULFFJourneyPanel* Panel : { SidebarHeader.Get(), StageChrome.Get() })
    {
        ShowPanel(Panel, !bWelcome, bInstant);
    }
    // The arc is the tour's: it comes down with the tour and goes back up when you leave it. The places show in both modes:
    // Explore's map, and the tour step's own (the layer picks which).
    if (TimelineArc != nullptr)
    {
        TimelineArc->SetArcShown(!bWelcome && !bExplore, bInstant);
    }
    ShowPanel(Callouts.Get(), !bWelcome, bInstant);
    const bool bNarrativeAppeared = ShowPanel(Narrative.Get(), !bWelcome && !bExplore, bInstant);
    ShowPanel(ExploreMap.Get(), !bWelcome && bExplore, bInstant);
    ShowSwitcherChild(NavSwitcher.Get(), TourNav.Get(), !bWelcome && !bExplore, bInstant);
    ShowSwitcherChild(NavSwitcher.Get(), ExploreNav.Get(), !bWelcome && bExplore, bInstant);
    ShowPanel(Settings.Get(), bModal, bInstant);

    if (!bExplore && Snapshot.bStarted && !bNarrativeAppeared && Snapshot.StepIndex != LastStepIndex && Narrative != nullptr)
    {
        // New step, same card: fade the new words in so the change is noticeable but not sudden.
        Narrative->FadeIn(bInstant);
    }
    LastStepIndex = Snapshot.bStarted ? Snapshot.StepIndex : INDEX_NONE;
}

bool ULFFHUDWidget::ShowSwitcherChild(UWidgetSwitcher* Switcher, ULFFJourneyPanel* Child, const bool bShown, const bool bInstant)
{
    // Collapsed as well as inactive, so nothing under it can be reached even if a switcher is rebuilt or re-indexed.
    const bool bAppeared = ShowPanel(Child, bShown, bInstant);
    if (bShown && Switcher != nullptr && Child != nullptr)
    {
        Switcher->SetActiveWidget(Child);
    }
    return bAppeared;
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
