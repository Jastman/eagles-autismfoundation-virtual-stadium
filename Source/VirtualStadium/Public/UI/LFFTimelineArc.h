#pragma once

#include "CoreMinimal.h"
#include "UI/LFFJourneyPanel.h"
#include "LFFTimelineArc.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextBlock;

/**
 * The timeline arc's shape in the widget's own UV (U 0-1 across, V 0-1 down). The same numbers drive M_LFF_TimelineArc
 * (SourceData/UI/Materials/LFF_TimelineArc.hlsl) and place the node buttons, so every dot sits on the drawn line.
 * Distances are in widget heights: the shader multiplies U by AspectRatio, so the curve is a true circle, not an oval.
 * The circle's centre sits far above the widget, so its lowest stretch is a gentle smile with both ends rising.
 */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFArcShape
{
    GENERATED_BODY()

    /** Widget width / height. ULFFTimelineArc sets it from the arc's real size before every placement. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "1.0"))
    float AspectRatio = 820.0f / 168.0f;

    /** V of the line at its lowest point, the middle (0 is the top of the widget). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArcCenterV = 0.36f;

    /** How far the two ends rise above the middle, in widget heights. 0 is a straight line. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CurveIntensity = 0.34f;

    /** Line thickness in widget heights: 0.024 is 4 px at the 168 px design height. Scales with the UI like everything else. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "0.001", ClampMax = "0.2"))
    float ArcThickness = 0.024f;

    /** The fade at each end, as a fraction of the width. Nodes and their labels must sit inside it, on the full band. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "0.45"))
    float FadeBoundaryWidth = 0.12f;

    /** Below this the line is drawn straight: the radius would be so large the shader's floats lose the line. */
    static constexpr float FlatBelow = 0.001f;

    /** Circle radius in widget heights: R = (w^2 + h^2) / 2h, w half the width and h the CurveIntensity. */
    float Radius() const;
    /** V of the line's centre at U. */
    float CurveV(float U) const;
    /** Signed distance from UV to the line's centre, in widget heights, negative above it. The shader's S. */
    float SignedDistance(const FVector2D& UV) const;
    /** End fade at U: 0 at both edges, 1 inside FadeBoundaryWidth. The shader's Edge. */
    float EdgeFade(float U) const;
    /** The point on the line at U, in the widget's local units (Slate units; the DPI scale is applied on top). */
    FVector2D PointAtU(float U, const FVector2D& WidgetSize) const;
    /** Pushes the shape into the arc material: scalar parameters named like the fields. */
    void ApplyTo(UMaterialInstanceDynamic* Material) const;
};

/**
 * Base class for WBP_LFF_TimelineArc. The active HUD uses the inherited compact ChapterMarks grid in its right dock.
 * Optional legacy ArcImage/ArcNodes bindings retain the curve APIs: their shader and nodes share FLFFArcShape.
 * A chapter button goes to its chapter; in Free explore it resumes the tour there (the bookmarked chapter restores
 * the exact step). A new step restyles controls in place.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFTimelineArc : public ULFFJourneyPanel
{
    GENERATED_BODY()

public:
    /** Replaces the whole shape (AspectRatio aside: it's always measured), pushes it to the material and re-places the nodes. */
    UFUNCTION(BlueprintCallable, Category = "Arc")
    void SetArcShape(const FLFFArcShape& NewShape);

    UFUNCTION(BlueprintCallable, Category = "Arc")
    void SetArcThickness(float Thickness);

    UFUNCTION(BlueprintCallable, Category = "Arc")
    void SetCurveIntensity(float Intensity);

    /**
     * The arc belongs to the Guided tour: it comes down from the top edge as it fades in, and goes back up as it fades out,
     * over ShowSeconds (eased). Instant (less movement, Calm Mode) just shows or hides it. Hidden, it's collapsed.
     */
    void SetArcShown(bool bShown, bool bInstant);

protected:
    /** Runs in the Designer too: the arc previews with its material and these parameters while you lay it out. */
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot) override;

    /** Fills the arc's SizeBox. Hit-test invisible, so the band's faded tails never take a click or drag. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> ArcImage;

    /** Same size as ArcImage, over it. C++ fills it with the node buttons. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> ArcNodes;

    UPROPERTY(EditAnywhere, Category = "Arc")
    FLFFArcShape Shape;

    /** U of each node, in chapter order. Each node is NodeWidth wide; all of them must sit inside the fade boundary. */
    UPROPERTY(EditAnywhere, Category = "Arc")
    TArray<float> NodeU = { 0.22f, 0.36f, 0.50f, 0.64f, 0.78f };

    /** User Interface domain, Translucent, one Custom node (SourceData/UI/Materials/LFF_TimelineArc.hlsl). */
    UPROPERTY(EditDefaultsOnly, Category = "Arc")
    TSoftObjectPtr<UMaterialInterface> ArcMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/LFF/UI/Materials/M_LFF_TimelineArc.M_LFF_TimelineArc")));

    /** The soft band the labels sit on, above and below the line, in widget heights. Both label lines of every node end
     *  above BandBelow - BandSoftness, on the band's solid part. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Band")
    float BandAbove = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Band")
    float BandBelow = 0.62f;

    /** Feather on the band's lower edge. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Band")
    float BandSoftness = 0.06f;

    /** SURFACE #072A2F. Black in High contrast. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Band")
    FLinearColor BandColor = FLinearColor(0.0021f, 0.0232f, 0.0284f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Band", meta = (ClampMin = "0.9", ClampMax = "1.0"))
    float BandOpacity = 0.94f;

    /** The line up to and including the current node: INK. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Line")
    FLinearColor DoneColor = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    /** The line after the current node: INK_MUTED, part transparent. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Line")
    FLinearColor AheadColor = FLinearColor(0.479f, 0.558f, 0.565f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Line", meta = (ClampMin = "0.3", ClampMax = "1.0"))
    float AheadOpacity = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Line", meta = (ClampMin = "0.3", ClampMax = "1.0"))
    float AheadOpacityHighContrast = 0.8f;

    /** Each node: a 64 px square centred on the line (the dot), then its name and its place ("2 of 3") under it. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    float NodeWidth = 112.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    float NodeDotArea = 64.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    float NodeLabelHeight = 48.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    float DotSmall = 14.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    float DotCurrent = 24.0f;

    /** RAISED #0F3B41: a node under the pointer. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Nodes")
    FLinearColor NodeHoverFill = FLinearColor(0.0048f, 0.0437f, 0.0529f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Arc|Show", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ShowSeconds = 0.35f;

    /** How far above its place the arc starts (and leaves to), px. */
    UPROPERTY(EditDefaultsOnly, Category = "Arc|Show", meta = (ClampMin = "0.0", ClampMax = "120.0"))
    float ShowDrop = 48.0f;

private:
    /** Creates the material instance on ArcImage once (Designer and game), or collapses the image if the material is missing. */
    void EnsureArcMaterial();
    void BuildNodes();
    /** Places every chapter node's dot on the drawn line, for the arc's current size. */
    void RepositionTimelineStepNodes(const FVector2D& Size);
    /** Pushes every material parameter: the shape, the current node (ProgressU), the band and line colours, High contrast. */
    void SynchronizeArcParameters(const FLFFJourneySnapshot& Snapshot, int32 Current);

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ArcMaterialInstance;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> NodeButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UImage>> NodeDots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> NodeNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> NodePlaces;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFTimelineMarkHandler>> NodeHandlers;

    /** The size the nodes were last placed for; the arc re-places them when its geometry changes. */
    FVector2D PlacedSize = FVector2D::ZeroVector;

    void ApplyShow();
    /** 0 hidden (above, transparent) to 1 shown; Target is where it's going. */
    float ShowAlpha = 0.0f;
    float ShowTarget = 0.0f;
};
