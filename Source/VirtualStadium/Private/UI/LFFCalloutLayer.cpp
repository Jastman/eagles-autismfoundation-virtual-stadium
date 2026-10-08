#include "UI/LFFCalloutLayer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Journey/LFFJourneySubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "LFFHUD"

namespace
{
    const TCHAR* CalloutFontPath = TEXT("/Game/LFF/UI/Fonts/F_LFF_Atkinson.F_LFF_Atkinson");
    const FLinearColor Ink(0.905f, 0.888f, 0.838f, 1.0f);
    const FLinearColor Surface(0.0021f, 0.0232f, 0.0284f, 1.0f);
    const FLinearColor Kelly(0.141f, 0.514f, 0.021f, 1.0f);

    FSlateBrush Disc(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth, const float Radius = 0.0f)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(Fill);
        Brush.OutlineSettings.RoundingType = Radius > 0.0f ? ESlateBrushRoundingType::FixedRadius : ESlateBrushRoundingType::HalfHeightRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
        Brush.OutlineSettings.Width = OutlineWidth;
        Brush.OutlineSettings.Color = FSlateColor(Outline);
        return Brush;
    }

    /** Kinds differ by their glyph, never by colour alone. */
    FText Glyph(const ELFFAmenity Amenity)
    {
        switch (Amenity)
        {
        case ELFFAmenity::SensoryRoom: return LOCTEXT("CalloutSensory", "S");
        case ELFFAmenity::QuietZone: return LOCTEXT("CalloutQuiet", "Q");
        case ELFFAmenity::GuestServices: return LOCTEXT("CalloutGuest", "i");
        case ELFFAmenity::Restroom: return LOCTEXT("CalloutRestroom", "WC");
        case ELFFAmenity::Food: return LOCTEXT("CalloutFood", "F");
        }
        return FText::GetEmpty();
    }
}

void ULFFCalloutHandler::HandleClicked()
{
    if (ULFFCalloutLayer* Owner = Layer.Get())
    {
        Owner->HandleCalloutClicked(AmenityId);
    }
}

void ULFFCalloutLayer::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (CalloutCanvas != nullptr)
    {
        // Clicks between the heads go through to the 3D view (a drag there looks around).
        CalloutCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

void ULFFCalloutLayer::SetBlockers(const TArray<UWidget*>& InBlockers)
{
    Blockers.Reset();
    for (UWidget* Blocker : InBlockers)
    {
        if (Blocker != nullptr)
        {
            Blockers.Add(Blocker);
        }
    }
}

void ULFFCalloutLayer::BuildCallouts()
{
    const ULFFJourneySubsystem* Journey = GetJourney();
    if (CalloutCanvas == nullptr || WidgetTree == nullptr || Journey == nullptr)
    {
        return;
    }
    CalloutCanvas->ClearChildren();
    CalloutWidgets.Reset();
    Handlers.Reset();
    Callouts.Reset();

    const UObject* Font = LoadObject<UObject>(nullptr, CalloutFontPath);
    if (Font == nullptr)
    {
        Font = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    auto MakeText = [this, Font](const FText& Text, const int32 Size, const FLinearColor& Color)
    {
        UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        if (Font != nullptr)
        {
            Block->SetFont(FSlateFontInfo(Font, Size, TEXT("Bold")));
        }
        Block->SetText(Text);
        Block->SetColorAndOpacity(FSlateColor(Color));
        Block->SetJustification(ETextJustify::Center);
        return Block;
    };

    TArray<FLFFAmenityMarker> Places = Journey->GetAmenities();
    FLFFAmenityMarker Seat;
    if (Journey->GetAmenity(ULFFJourneySubsystem::MySeatAmenityId, Seat))
    {
        Places.Add(Seat);
    }

    for (const FLFFAmenityMarker& Place : Places)
    {
        FCallout& Callout = Callouts.AddDefaulted_GetRef();
        Callout.Id = Place.AmenityId;
        Callout.bSeat = Place.AmenityId == ULFFJourneySubsystem::MySeatAmenityId;
        Callout.Category = Place.Category;
        Callout.Level = Place.Level;

        // Head: a 52 px disc with the glyph (the seat: a light dot), centred in a 64 px hit area.
        UButton* Head = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Head->IsFocusable = false;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        FButtonStyle Style;
        Style.Normal = Disc(FLinearColor::Transparent, FLinearColor::Transparent, 0.0f);
        Style.Hovered = Style.Normal;
        Style.Pressed = Style.Normal;
        Style.Disabled = Style.Normal;
        Style.NormalPadding = FMargin(0.0f);
        Style.PressedPadding = FMargin(0.0f);
        Head->SetStyle(Style);
        Head->SetToolTipText(Place.Label);
        UOverlay* Centre = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(HeadSize);
        Box->SetHeightOverride(HeadSize);
        UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Fill->SetHorizontalAlignment(HAlign_Center);
        Fill->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Mark = nullptr;
        if (Callout.bSeat)
        {
            USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            DotBox->SetWidthOverride(16.0f);
            DotBox->SetHeightOverride(16.0f);
            UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            Dot->SetBrush(Disc(Ink, Ink, 0.0f));
            DotBox->SetContent(Dot);
            Fill->SetContent(DotBox);
        }
        else
        {
            Mark = MakeText(Glyph(Place.Category), 22, Surface);
            Fill->SetContent(Mark);
        }
        Box->SetContent(Fill);
        if (UOverlaySlot* BoxSlot = Centre->AddChildToOverlay(Box))
        {
            BoxSlot->SetHorizontalAlignment(HAlign_Center);
            BoxSlot->SetVerticalAlignment(VAlign_Center);
        }
        Head->SetContent(Centre);

        ULFFCalloutHandler* Handler = NewObject<ULFFCalloutHandler>(this);
        Handler->AmenityId = Place.AmenityId;
        Handler->Layer = this;
        Head->OnClicked.AddDynamic(Handler, &ULFFCalloutHandler::HandleClicked);
        Handlers.Add(Handler);

        // Name chip, right of the head: opaque, so the words never sit on the scene.
        UBorder* Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Chip->SetBrush(Disc(Surface, Ink, 1.5f, 10.0f));
        Chip->SetPadding(FMargin(12.0f, 6.0f));
        Chip->SetContent(MakeText(Place.Label, 18, Ink));
        Chip->SetVisibility(ESlateVisibility::Collapsed);

        if (UCanvasPanelSlot* HeadSlot = CalloutCanvas->AddChildToCanvas(Head))
        {
            HeadSlot->SetAnchors(FAnchors(0.0f, 0.0f));
            HeadSlot->SetAlignment(FVector2D(0.5, 0.5));
            HeadSlot->SetAutoSize(false);
            HeadSlot->SetSize(FVector2D(HitSize, HitSize));
        }
        if (UCanvasPanelSlot* ChipSlot = CalloutCanvas->AddChildToCanvas(Chip))
        {
            ChipSlot->SetAnchors(FAnchors(0.0f, 0.0f));
            ChipSlot->SetAlignment(FVector2D(0.0, 0.5));
            ChipSlot->SetAutoSize(true);
        }
        Head->SetVisibility(ESlateVisibility::Collapsed);

        Callout.Head = Head;
        Callout.Disc = Fill;
        Callout.Glyph = Mark;
        Callout.Chip = Chip;
        CalloutWidgets.Add(Head);
        CalloutWidgets.Add(Chip);
    }
    BuiltForCount = Journey->GetAmenities().Num();
}

void ULFFCalloutLayer::StyleCallout(FCallout& Callout, const bool bPicked) const
{
    UBorder* Fill = Callout.Disc.Get();
    if (Fill == nullptr)
    {
        return;
    }
    if (Callout.bSeat)
    {
        Fill->SetBrush(Disc(Kelly, Ink, bPicked ? 5.0f : 4.0f));
        return;
    }
    // A light head with a dark glyph; picked turns it dark with a light ring, so the pick never rests on colour alone.
    Fill->SetBrush(bPicked ? Disc(Surface, Ink, 4.0f) : Disc(Ink, Surface, 2.0f));
    if (UTextBlock* Mark = Callout.Glyph.Get())
    {
        Mark->SetColorAndOpacity(FSlateColor(bPicked ? Ink : Surface));
    }
}

void ULFFCalloutLayer::NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot)
{
    LastSnapshot = Snapshot;
    const ULFFJourneySubsystem* Journey = GetJourney();
    if (Journey == nullptr)
    {
        return;
    }
    if (BuiltForCount != Journey->GetAmenities().Num())
    {
        BuildCallouts();
    }

    // Spots in the world, from the site frame (read again on each visit to Free explore).
    const FTransform& Site = Journey->GetSiteFrame();
    const FVector Up = Site.GetUnitAxis(EAxis::Z);
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;
    for (FCallout& Callout : Callouts)
    {
        FLFFAmenityMarker Place;
        if (Journey->GetAmenity(Callout.Id, Place))
        {
            Callout.Spot = Site.TransformPosition(Place.Location);
            Callout.Top = Callout.Spot + Up * StemHeight;
        }
        const bool bKindOn = (Snapshot.AmenityMask & (1 << static_cast<int32>(Callout.Category))) != 0;
        Callout.bWanted = Snapshot.Mode == ELFFSimulationMode::FreeExplore
            ? Callout.bSeat || bKindOn
            : Snapshot.TourPlaces.Contains(Callout.Id);
        StyleCallout(Callout, Callout.Id == Snapshot.SelectedAmenityId);
        if (UButton* Head = Callout.Head.Get())
        {
            Head->SetIsEnabled(!bModal);
        }
    }
    PlaceCallouts();
}

void ULFFCalloutLayer::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // The camera moves every frame in Free explore (flight, eases), so the heads follow it every frame.
    PlaceCallouts();
}

void ULFFCalloutLayer::PlaceCallouts()
{
    APlayerController* Player = GetOwningPlayer();
    const FGeometry& Geometry = GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    const bool bTour = LastSnapshot.Mode == ELFFSimulationMode::GuidedTour;

    // The panels on screen right now, in this layer's space.
    TArray<FBox2D, TInlineAllocator<4>> Covered;
    for (const TWeakObjectPtr<UWidget>& Weak : Blockers)
    {
        const UWidget* Blocker = Weak.Get();
        if (Blocker != nullptr && Blocker->IsVisible() && Blocker->GetCachedWidget().IsValid())
        {
            const FGeometry& Panel = Blocker->GetCachedGeometry();
            if (Panel.GetLocalSize().SizeSquared() > 1.0)
            {
                Covered.Emplace(Geometry.AbsoluteToLocal(Panel.GetAbsolutePosition()),
                    Geometry.AbsoluteToLocal(Panel.GetAbsolutePosition() + Panel.GetAbsoluteSize()));
            }
        }
    }
    const FVector2D Half(HitSize * 0.5, HitSize * 0.5);
    auto IsCovered = [&Covered](const FBox2D& Box)
    {
        return Covered.ContainsByPredicate([&Box](const FBox2D& Panel) { return Panel.Intersect(Box); });
    };
    const FBox2D Screen(FVector2D(Half.X, TopInset), Size - FVector2D(Half.X, BottomInset));

    TArray<FVector2D, TInlineAllocator<16>> Placed;
    for (FCallout& Callout : Callouts)
    {
        UButton* Head = Callout.Head.Get();
        UWidget* Chip = Callout.Chip.Get();
        Callout.bDrawn = false;
        FVector2D Spot, Top;
        const bool bOnScreen = Callout.bWanted && Player != nullptr
            && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Callout.Spot, Spot, false)
            && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Callout.Top, Top, false);
        if (bOnScreen)
        {
            // Close up a stem would run off the screen: keep the head within MaxStemPixels of its spot.
            const FVector2D Stem = Top - Spot;
            const double Length = Stem.Size();
            Callout.SpotPx = Spot;
            Callout.HeadPx = Length > MaxStemPixels ? Spot + Stem / Length * MaxStemPixels : Top;
            const FVector2D Anchor = Callout.HeadPx;
            for (int32 Attempt = 0; Attempt < 17; ++Attempt)
            {
                const int32 Ring = (Attempt + 3) / 4;
                const FVector2D Directions[] = { FVector2D(0, -1), FVector2D(0, 1), FVector2D(-1, 0), FVector2D(1, 0) };
                const FVector2D Candidate = Attempt == 0 ? Anchor : Anchor + Directions[(Attempt - 1) % 4] * Ring * (HitSize + 8.0);
                if (Screen.IsInsideOrOn(Candidate) && !IsCovered(FBox2D(Candidate - Half, Candidate + Half))
                    && !Placed.ContainsByPredicate([&Candidate, this](const FVector2D& Other)
                    { return FMath::Abs(Other.X - Candidate.X) < HitSize && FMath::Abs(Other.Y - Candidate.Y) < HitSize; }))
                {
                    Callout.HeadPx = Candidate;
                    Callout.bDrawn = true;
                    break;
                }
            }
            if (Callout.bDrawn)
            {
                Placed.Add(Callout.HeadPx);
            }
        }
        if (Head != nullptr)
        {
            if (UCanvasPanelSlot* HeadSlot = Cast<UCanvasPanelSlot>(Head->Slot))
            {
                HeadSlot->SetPosition(Callout.HeadPx);
            }
            // In the tour the heads only point; the card says what they are.
            Head->SetVisibility(!Callout.bDrawn ? ESlateVisibility::Collapsed : bTour ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
        }
        if (Chip != nullptr)
        {
            bool bNamed = Callout.bDrawn
                && (bTour || Callout.bSeat || Callout.Id == LastSnapshot.SelectedAmenityId || (Head != nullptr && Head->IsHovered()));
            if (UCanvasPanelSlot* ChipSlot = Cast<UCanvasPanelSlot>(Chip->Slot))
            {
                const FVector2D ChipSize = Chip->GetDesiredSize();
                const double Gap = HitSize * 0.5 + 4.0;
                const FVector2D Right = Callout.HeadPx + FVector2D(Gap, 0.0);
                const FVector2D Left = Callout.HeadPx - FVector2D(Gap + ChipSize.X, 0.0);
                auto Fits = [&Screen, &IsCovered, &ChipSize](const FVector2D& Position)
                {
                    const FBox2D Box(Position - FVector2D(0.0, ChipSize.Y * 0.5), Position + FVector2D(ChipSize.X, ChipSize.Y * 0.5));
                    return Screen.IsInsideOrOn(Box.Min) && Screen.IsInsideOrOn(Box.Max) && !IsCovered(Box);
                };
                if (Fits(Right))
                {
                    ChipSlot->SetPosition(Right);
                }
                else if (Fits(Left))
                {
                    ChipSlot->SetPosition(Left);
                }
                else
                {
                    bNamed = false;
                }
            }
            Chip->SetVisibility(bNamed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}

int32 ULFFCalloutLayer::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
    int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    // Stems: a light line on a dark one, so they read on sky and on concrete alike. They stop at the head's edge (heads are
    // drawn as children, before this) and start at a small ring on the spot itself.
    const FLinearColor Shade(0.0f, 0.0f, 0.0f, 0.55f);
    for (const FCallout& Callout : Callouts)
    {
        if (!Callout.bDrawn)
        {
            continue;
        }
        const FVector2D Stem = Callout.HeadPx - Callout.SpotPx;
        const double Length = Stem.Size();
        if (Length < HeadSize * 0.5 + 2.0)
        {
            continue;
        }
        const FVector2D End = Callout.HeadPx - Stem / Length * (HeadSize * 0.5);
        const TArray<FVector2D> Line = { Callout.SpotPx, End };
        FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None,
            Shade, true, StemThickness + 2.0f);
        FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None,
            Callout.bSeat ? Kelly : Ink, true, StemThickness);
        TArray<FVector2D> Ring;
        for (int32 Index = 0; Index <= 16; ++Index)
        {
            const double Angle = 2.0 * PI * Index / 16.0;
            Ring.Add(Callout.SpotPx + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 5.0);
        }
        FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None,
            Callout.bSeat ? Kelly : Ink, true, 2.0f);
    }
    return Layer + 2;
}

void ULFFCalloutLayer::HandleCalloutClicked(const FName AmenityId)
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->SelectAmenity(AmenityId);
    }
    ReturnFocusToGame();
}

#undef LOCTEXT_NAMESPACE
