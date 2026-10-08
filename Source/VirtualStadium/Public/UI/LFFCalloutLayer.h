#pragma once

#include "CoreMinimal.h"
#include "UI/LFFJourneyPanel.h"
#include "LFFCalloutLayer.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UWidget;
class ULFFCalloutLayer;

/** Routes one callout's click to the layer (dynamic delegates can't carry the place's id). */
UCLASS()
class VIRTUALSTADIUM_API ULFFCalloutHandler : public UObject
{
    GENERATED_BODY()

public:
    FName AmenityId = NAME_None;
    TWeakObjectPtr<ULFFCalloutLayer> Layer;

    UFUNCTION()
    void HandleClicked();
};

/**
 * Base class for WBP_LFF_Callouts: places drawn in the 3D view. It fills the screen under the panels and over the stadium.
 * Each place is a thin stem rising from its spot on the model to a round head with the place's glyph.
 *
 * Free explore: hovering or picking a head shows its name on an opaque chip, and a pick fills the sidebar's card (Go
 * there). The seat's head is kelly and always named. The kinds that are on show on every level (turning one on frames
 * them all), plus the seat. Guided tour: only the current step's places (its PlacesToShow), always named, and not clickable (the card
 * says what they are).
 *
 * A head that would sit under one of the HUD's panels (SetBlockers), under the top edge's controls or down among the
 * bottom row's buttons is hidden, so the focus ring can't land on it either. Heads that would overlap stack upwards.
 * Heads and chips are opaque, so no word sits on the scene; nothing animates, the heads ride with the camera.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFCalloutLayer : public ULFFJourneyPanel
{
    GENERATED_BODY()

public:
    void HandleCalloutClicked(FName AmenityId);

    /** The HUD's opaque panels. A head that would be under one of them (while it's shown) is hidden. */
    void SetBlockers(const TArray<UWidget*>& InBlockers);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    /** Fills the layer. C++ adds one head (a button) and one name chip per place. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> CalloutCanvas;

    /** How far above its spot a place's head sits, cm along the site's up. */
    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float StemHeight = 2500.0f;

    /** The longest a stem gets on screen, px (close up, the head stays near its spot). */
    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float MaxStemPixels = 360.0f;

    /** A 52 px head in a 64 px hit area (the project minimum). */
    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float HeadSize = 34.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float HitSize = 64.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float StemThickness = 3.0f;

    /** Heads are hidden under the top edge's controls and down among the bottom row's buttons. Local px. */
    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float TopInset = 48.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Callouts")
    float BottomInset = 112.0f;

public:
    float GetStemHeight() const { return StemHeight; }
    float GetHitSize() const { return HitSize; }

private:
    struct FCallout
    {
        FName Id = NAME_None;
        bool bSeat = false;
        ELFFAmenity Category = ELFFAmenity::GuestServices;
        ELFFMapLevel Level = ELFFMapLevel::LowerConcourse;
        FVector Spot = FVector::ZeroVector;
        FVector Top = FVector::ZeroVector;
        TWeakObjectPtr<UButton> Head;
        TWeakObjectPtr<UBorder> Disc;
        TWeakObjectPtr<UTextBlock> Glyph;
        TWeakObjectPtr<UWidget> Chip;
        bool bWanted = false;
        bool bDrawn = false;
        FVector2D SpotPx = FVector2D::ZeroVector;
        FVector2D HeadPx = FVector2D::ZeroVector;
    };

    void BuildCallouts();
    void PlaceCallouts();
    void StyleCallout(FCallout& Callout, bool bPicked) const;

    TArray<FCallout> Callouts;

    /** Keeps the runtime widgets and click handlers alive (the structs hold weak pointers). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWidget>> CalloutWidgets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFCalloutHandler>> Handlers;

    TArray<TWeakObjectPtr<UWidget>> Blockers;

    FLFFJourneySnapshot LastSnapshot;
    int32 BuiltForCount = INDEX_NONE;
};
