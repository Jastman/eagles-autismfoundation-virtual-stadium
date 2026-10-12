#include "UI/LFFTimelineArc.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateTypes.h"
#include "VirtualStadium.h"

#define LOCTEXT_NAMESPACE "LFFHUD"

namespace
{
    const TCHAR* ArcFontPath = TEXT("/Game/LFF/UI/Fonts/F_LFF_Atkinson.F_LFF_Atkinson");
    // The node labels use the project's label floor (UI_DESIGN.md: 18 px at 1080p).
    constexpr int32 NodeFontSize = 18;
    const FLinearColor Ink(0.905f, 0.888f, 0.838f, 1.0f);
    const FLinearColor InkMuted(0.479f, 0.558f, 0.565f, 1.0f);

    FSlateBrush RoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth, const float Radius)
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
}

float FLFFArcShape::Radius() const
{
    // LFF_TimelineArc.hlsl step 1: half-width W and rise H give the circle through the arc.
    const float W = AspectRatio * 0.5f;
    const float H = FMath::Max(CurveIntensity, FlatBelow);
    return (W * W + H * H) / (2.0f * H);
}

float FLFFArcShape::CurveV(const float U) const
{
    // Steps 1-3 of the shader, solved for the line's centre (S = 0) at P.x = U * AspectRatio instead of measured from it:
    // length(P - C) = R, so P.y = C.y + sqrt(R^2 - (P.x - C.x)^2), taking the circle's lower edge (the smile).
    if (CurveIntensity < FlatBelow)
    {
        return ArcCenterV;
    }
    const float W = AspectRatio * 0.5f;
    const float R = Radius();
    const FVector2f C(W, ArcCenterV - R);
    const float Px = U * AspectRatio;
    return C.Y + FMath::Sqrt(FMath::Max(R * R - FMath::Square(Px - C.X), 0.0f));
}

float FLFFArcShape::SignedDistance(const FVector2D& UV) const
{
    // Shader step 3, as written there: S = length(P - C) - R, or the height difference when the line is straight.
    if (CurveIntensity < FlatBelow)
    {
        return static_cast<float>(UV.Y) - ArcCenterV;
    }
    const float W = AspectRatio * 0.5f;
    const float R = Radius();
    const FVector2f P(static_cast<float>(UV.X) * AspectRatio, static_cast<float>(UV.Y));
    const FVector2f C(W, ArcCenterV - R);
    return (P - C).Size() - R;
}

float FLFFArcShape::EdgeFade(const float U) const
{
    const float Fade = FMath::Max(FadeBoundaryWidth, 0.0001f);
    return FMath::SmoothStep(0.0f, Fade, U) * FMath::SmoothStep(0.0f, Fade, 1.0f - U);
}

FVector2D FLFFArcShape::PointAtU(const float U, const FVector2D& WidgetSize) const
{
    return FVector2D(U * WidgetSize.X, CurveV(U) * WidgetSize.Y);
}

void FLFFArcShape::ApplyTo(UMaterialInstanceDynamic* Material) const
{
    if (Material == nullptr)
    {
        return;
    }
    Material->SetScalarParameterValue(TEXT("AspectRatio"), AspectRatio);
    Material->SetScalarParameterValue(TEXT("ArcCenterV"), ArcCenterV);
    Material->SetScalarParameterValue(TEXT("CurveIntensity"), CurveIntensity);
    Material->SetScalarParameterValue(TEXT("ArcThickness"), ArcThickness);
    Material->SetScalarParameterValue(TEXT("FadeBoundaryWidth"), FadeBoundaryWidth);
}

void ULFFTimelineArc::EnsureArcMaterial()
{
    if (ArcImage == nullptr || ArcMaterialInstance != nullptr)
    {
        return;
    }
    UMaterialInterface* Material = ArcMaterial.LoadSynchronous();
    if (Material == nullptr)
    {
        // Never a white box over the stadium: without the material the arc draws nodes only.
        UE_LOG(LogVirtualStadium, Warning, TEXT("Timeline arc: material %s not found; the arc shows its nodes without the line."), *ArcMaterial.ToString());
        ArcImage->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    ArcMaterialInstance = UMaterialInstanceDynamic::Create(Material, this);
    ArcImage->SetBrushFromMaterial(ArcMaterialInstance);
    ArcImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULFFTimelineArc::NativePreConstruct()
{
    Super::NativePreConstruct();

    EnsureArcMaterial();
    if (IsDesignTime())
    {
        // The Designer preview: the shape as authored and the line done up to the middle node, so both colours show.
        FLFFJourneySnapshot Preview;
        Preview.bStarted = true;
        SynchronizeArcParameters(Preview, NodeU.Num() / 2);
    }
}

void ULFFTimelineArc::SetArcShape(const FLFFArcShape& NewShape)
{
    const float Measured = Shape.AspectRatio;
    Shape = NewShape;
    Shape.AspectRatio = Measured;
    Shape.ApplyTo(ArcMaterialInstance);
    PlacedSize = FVector2D::ZeroVector;
}

void ULFFTimelineArc::SetArcThickness(const float Thickness)
{
    FLFFArcShape NewShape = Shape;
    NewShape.ArcThickness = FMath::Clamp(Thickness, 0.001f, 0.2f);
    SetArcShape(NewShape);
}

void ULFFTimelineArc::SetCurveIntensity(const float Intensity)
{
    FLFFArcShape NewShape = Shape;
    NewShape.CurveIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    SetArcShape(NewShape);
}

void ULFFTimelineArc::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    EnsureArcMaterial();
    if (ArcNodes != nullptr)
    {
        // Clicks and drags pass between the nodes to the 3D view.
        ArcNodes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    BuildNodes();
}

void ULFFTimelineArc::SetArcShown(const bool bShown, const bool bInstant)
{
    ShowTarget = bShown ? 1.0f : 0.0f;
    if (bShown && GetVisibility() == ESlateVisibility::Collapsed)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    if (bInstant)
    {
        ShowAlpha = ShowTarget;
    }
    ApplyShow();
}

void ULFFTimelineArc::ApplyShow()
{
    // Smoothstep: eases out on the way in and in on the way out, so it settles rather than stops.
    const float Eased = ShowAlpha * ShowAlpha * (3.0f - 2.0f * ShowAlpha);
    SetRenderOpacity(Eased);
    SetRenderTranslation(FVector2D(0.0, -ShowDrop * (1.0f - Eased)));
    if (ShowAlpha <= 0.0f && ShowTarget <= 0.0f)
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ULFFTimelineArc::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (ShowAlpha != ShowTarget)
    {
        ShowAlpha = FMath::FInterpConstantTo(ShowAlpha, ShowTarget, InDeltaTime, 1.0f / FMath::Max(ShowSeconds, 0.01f));
        ApplyShow();
    }

    if (ArcNodes == nullptr)
    {
        return;
    }
    const FVector2D Size = ArcNodes->GetCachedGeometry().GetLocalSize();
    if (Size.X > 1.0 && Size.Y > 1.0 && !Size.Equals(PlacedSize, 0.5))
    {
        RepositionTimelineStepNodes(Size);
    }
}

void ULFFTimelineArc::BuildNodes()
{
    if (ArcNodes == nullptr || WidgetTree == nullptr)
    {
        return;
    }
    ArcNodes->ClearChildren();
    NodeButtons.Reset();
    NodeDots.Reset();
    NodeNames.Reset();
    NodePlaces.Reset();
    NodeHandlers.Reset();
    PlacedSize = FVector2D::ZeroVector;

    const UObject* Font = LoadObject<UObject>(nullptr, ArcFontPath);
    if (Font == nullptr)
    {
        Font = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    auto MakeLine = [this, Font](UVerticalBox* Stack)
    {
        UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        if (Font != nullptr)
        {
            Line->SetFont(FSlateFontInfo(Font, NodeFontSize, TEXT("Regular")));
        }
        Line->SetJustification(ETextJustify::Center);
        if (UVerticalBoxSlot* LineSlot = Stack->AddChildToVerticalBox(Line))
        {
            LineSlot->SetHorizontalAlignment(HAlign_Fill);
        }
        return Line;
    };

    // No fill at rest, so the band shows through; the same rounded shape on hover, which the focus ring also follows.
    FButtonStyle Style;
    Style.Normal = RoundedBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.0f, 16.0f);
    Style.Hovered = RoundedBrush(NodeHoverFill, FLinearColor::Transparent, 0.0f, 16.0f);
    Style.Pressed = RoundedBrush(FMath::Lerp(NodeHoverFill, FLinearColor::Black, 0.3f), FLinearColor::Transparent, 0.0f, 16.0f);
    Style.Disabled = Style.Normal;
    Style.NormalPadding = FMargin(0.0f);
    Style.PressedPadding = FMargin(0.0f);

    for (int32 Index = 0; Index < Chapters.Num(); ++Index)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Button->IsFocusable = false;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Button->SetStyle(Style);

        // Top: the dot, centred in a NodeDotArea square that is itself centred on the line. Then the two label lines.
        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        USizeBox* DotArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        DotArea->SetHeightOverride(NodeDotArea);
        UOverlay* DotCentre = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (UOverlaySlot* DotSlot = DotCentre->AddChildToOverlay(Dot))
        {
            DotSlot->SetHorizontalAlignment(HAlign_Center);
            DotSlot->SetVerticalAlignment(VAlign_Center);
        }
        DotArea->SetContent(DotCentre);
        Stack->AddChildToVerticalBox(DotArea);
        UTextBlock* Name = MakeLine(Stack);
        Name->SetText(Chapters[Index].Name);
        UTextBlock* Place = MakeLine(Stack);
        Button->SetContent(Stack);

        ULFFTimelineMarkHandler* Handler = NewObject<ULFFTimelineMarkHandler>(this);
        Handler->StepIndex = Chapters[Index].FirstStep;
        Handler->Panel = this;
        RegisterButtonFeedback(Button);
        Button->OnClicked.AddDynamic(Handler, &ULFFTimelineMarkHandler::HandleClicked);

        if (UCanvasPanelSlot* NodeSlot = ArcNodes->AddChildToCanvas(Button))
        {
            NodeSlot->SetAutoSize(false);
            NodeSlot->SetAnchors(FAnchors(0.0f, 0.0f));
            // Top centre of the node: the dot's centre is NodeDotArea / 2 below it, on the line.
            NodeSlot->SetAlignment(FVector2D(0.5, 0.0));
            NodeSlot->SetSize(FVector2D(NodeWidth, NodeDotArea + NodeLabelHeight));
        }
        NodeButtons.Add(Button);
        NodeDots.Add(Dot);
        NodeNames.Add(Name);
        NodePlaces.Add(Place);
        NodeHandlers.Add(Handler);
    }
}

void ULFFTimelineArc::RepositionTimelineStepNodes(const FVector2D& Size)
{
    // The shader measures in widget heights, so it needs the arc's real aspect (never the viewport's: that would stretch
    // the circle into an oval). Push it first, so the line and the dots are drawn from the same numbers this frame.
    Shape.AspectRatio = static_cast<float>(Size.X / Size.Y);
    Shape.ApplyTo(ArcMaterialInstance);

    for (int32 Index = 0; Index < NodeButtons.Num(); ++Index)
    {
        // Spare chapters (a blueprint with more than NodeU lists) spread evenly over the same span.
        const float U = NodeU.IsValidIndex(Index) ? NodeU[Index]
            : FMath::Lerp(0.22f, 0.78f, NodeButtons.Num() > 1 ? static_cast<float>(Index) / (NodeButtons.Num() - 1) : 0.5f);

        // The line's centre at U in pixels: x = U * width, y = CurveV(U) * height (the shader's curve, solved for S = 0).
        const FVector2D OnLine = Shape.PointAtU(U, Size);

        // The node's canvas anchor is its top centre; its dot is centred in the NodeDotArea square at the top, so the
        // anchor sits half that square above the line and the dot's centre lands exactly on it.
        if (UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(NodeButtons[Index]->Slot))
        {
            NodeSlot->SetPosition(OnLine - FVector2D(0.0, NodeDotArea * 0.5f));
        }
    }
    PlacedSize = Size;
}

void ULFFTimelineArc::SynchronizeArcParameters(const FLFFJourneySnapshot& Snapshot, const int32 Current)
{
    if (ArcMaterialInstance == nullptr)
    {
        return;
    }
    // The line is "done" up to the current node; before the tour starts it's all ahead.
    const float ProgressU = NodeU.IsValidIndex(Current) ? NodeU[Current] : -1.0f;
    const bool bHighContrast = Snapshot.Settings.bHighContrast;
    ArcMaterialInstance->SetScalarParameterValue(TEXT("ProgressU"), ProgressU);
    ArcMaterialInstance->SetScalarParameterValue(TEXT("BandAbove"), BandAbove);
    ArcMaterialInstance->SetScalarParameterValue(TEXT("BandBelow"), BandBelow);
    ArcMaterialInstance->SetScalarParameterValue(TEXT("BandSoftness"), BandSoftness);
    ArcMaterialInstance->SetScalarParameterValue(TEXT("BandOpacity"), bHighContrast ? 1.0f : BandOpacity);
    ArcMaterialInstance->SetVectorParameterValue(TEXT("BandColor"), bHighContrast ? FLinearColor::Black : BandColor);
    ArcMaterialInstance->SetVectorParameterValue(TEXT("DoneColor"), DoneColor);
    ArcMaterialInstance->SetVectorParameterValue(TEXT("AheadColor"), AheadColor);
    ArcMaterialInstance->SetScalarParameterValue(TEXT("AheadOpacity"), bHighContrast ? AheadOpacityHighContrast : AheadOpacity);
    Shape.ApplyTo(ArcMaterialInstance);
}

void ULFFTimelineArc::NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot)
{
    if (NodeButtons.Num() != Chapters.Num())
    {
        BuildNodes();
    }

    const bool bExplore = Snapshot.Mode == ELFFSimulationMode::FreeExplore;
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;
    // Free explore picked on the welcome screen has no place in the tour yet, so no node is current.
    const int32 Current = Snapshot.bStarted ? ChapterOf(Snapshot.StepIndex) : INDEX_NONE;
    SynchronizeArcParameters(Snapshot, Current);

    for (int32 Index = 0; Index < NodeButtons.Num(); ++Index)
    {
        const bool bCurrent = Index == Current;
        const bool bDone = Current != INDEX_NONE && Index < Current;

        if (UButton* Button = NodeButtons[Index].Get())
        {
            Button->SetIsEnabled((Snapshot.bStarted || bExplore) && !bModal);
            Button->SetToolTipText(FText::Format(bExplore ? LOCTEXT("ArcNodeTipExplore", "Back to the tour: {0}") : LOCTEXT("ArcNodeTip", "Go to {0}"),
                Chapters[Index].Name));
        }
        if (ULFFTimelineMarkHandler* Handler = NodeHandlers[Index].Get())
        {
            Handler->StepIndex = Chapters[Index].FirstStep;
        }

        // Size and fill tell the states apart, never colour alone: done is a small solid dot, current a large one, ahead a ring.
        if (UImage* Dot = NodeDots[Index].Get())
        {
            const float Size = bCurrent ? DotCurrent : DotSmall;
            Dot->SetBrush(bCurrent || bDone ? RoundedBrush(Ink, Ink, 0.0f, 0.0f) : RoundedBrush(FLinearColor::Transparent, InkMuted, 2.0f, 0.0f));
            Dot->SetDesiredSizeOverride(FVector2D(Size, Size));
        }
        if (UTextBlock* Name = NodeNames[Index].Get())
        {
            FSlateFontInfo Font = Name->GetFont();
            Font.TypefaceFontName = bCurrent ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
            Name->SetFont(Font);
            Name->SetColorAndOpacity(FSlateColor(bCurrent || bDone ? Ink : InkMuted));
        }
        if (UTextBlock* Place = NodePlaces[Index].Get())
        {
            // Only the current node has a second line; the others keep the empty line so nothing reflows.
            FText Text = FText::GetEmpty();
            if (bCurrent)
            {
                int32 Step = 0, Count = 1;
                ChapterPlace(Index, Snapshot, Step, Count);
                Text = bExplore ? LOCTEXT("ArcMyPlace", "my place")
                                : FText::Format(LOCTEXT("ArcPlace", "{0} of {1}"), FText::AsNumber(Step), FText::AsNumber(Count));
            }
            Place->SetText(Text);
            Place->SetColorAndOpacity(FSlateColor(InkMuted));
        }
    }
}

#undef LOCTEXT_NAMESPACE
