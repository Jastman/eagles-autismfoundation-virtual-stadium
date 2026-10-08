#pragma once

#include "CoreMinimal.h"
#include "UI/LFFJourneyPanel.h"
#include "LFFMapPanel.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class ULFFMapPanel;

/** Routes one level button's or place toggle's click to the panel: dynamic delegates can't carry which one it was. */
UCLASS()
class VIRTUALSTADIUM_API ULFFMapButtonHandler : public UObject
{
    GENERATED_BODY()

public:
    enum class EKind : uint8
    {
        Level,
        Amenity
    };

    EKind Kind = EKind::Level;
    int32 Index = 0;
    TWeakObjectPtr<ULFFMapPanel> Panel;

    UFUNCTION()
    void HandleClicked();
};

/**
 * Base class for WBP_LFF_ExploreMap, the right-hand places panel. The map is the stadium in the 3D view (the
 * callouts, ULFFCalloutLayer, and the picked level's highlight). Categories span levels; level buttons are disclosed
 * on demand, and the picked-place card offers Show me. The state (level, toggles, pick) lives in the journey
 * subsystem and arrives in the snapshot.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFMapPanel : public ULFFJourneyPanel
{
    GENERATED_BODY()

public:
    void HandleMapButton(ULFFMapButtonHandler::EKind Kind, int32 Index);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot) override;

    /** "Viewing: Lower concourse". */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ViewingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LevelsGroup;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LevelsToggleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LevelsToggleLabel;

    /** The level buttons. The picked one is the light chip; a click highlights that level and frames it. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> UpperDeckTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> UpperDeckTabLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SuitesTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SuitesTabLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ClubTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ClubTabLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LowerConcourseTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LowerConcourseTabLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> FieldTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> FieldTabLabel;

    /** The place toggles (a Uniform Grid Panel in the blueprint). Each label has a ...State block for On / Off. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SensoryToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SensoryLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> QuietToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> QuietLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> GuestToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GuestLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestroomToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RestroomLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> FoodToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> FoodLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> HideAllButton;

    /** The picked place's card: what it is, where, and Go there. Hidden until a callout is picked. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PoiCard;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PoiTitle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PoiNote;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ShowMeButton;

    /** Shown while nothing is picked: how to pick a place. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PickHint;

private:
    UFUNCTION()
    void HandleHideAllClicked();
    UFUNCTION()
    void HandleShowMeClicked();
    UFUNCTION()
    void HandleLevelsClicked();

    void BindMapButton(UButton* Button, ULFFMapButtonHandler::EKind Kind, int32 Index);

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFMapButtonHandler>> ButtonHandlers;

    FName PickedAmenityId = NAME_None;
    bool bLevelsOpen = false;
};
