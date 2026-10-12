#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UOverlay;
class ULFFJourneyPanel;
class ULFFCalloutLayer;
class ULFFJourneySubsystem;
class ULFFMapPanel;
class ULFFTimelineArc;
class USizeBox;
class UScaleBox;
class UWidget;
class UWidgetSwitcher;

/**
 * Base class for WBP_LFF_HUD (/Game/LFF/UI/WBP_LFF_HUD), the one HUD on screen (Docs/UI_MODES_SPEC.md). It holds the
 * panels below, decides which are shown for the current screen and mode, and pushes every journey snapshot into them.
 * Required bindings make a HUD without one of them fail to compile, and the compile error names what's missing.
 *
 *   RootOverlay (Overlay)
 *   ├─ Callouts (full screen, under everything: places on the model, Free explore's and the tour step's; none under a panel)
 *   ├─ InformationSize (top left: identity card and the Guided tour caption with expandable day timeline)
 *   ├─ ControlSize (top right, scrollable: mode/settings/toggles, compact chapter chips and mode navigation,
 *   │                                      then ExploreMap with categories, expandable levels and picked-place detail)
 *   ├─ Welcome (full screen, before a mode is chosen)
 *   ├─ Settings (the modal layer: Settings, Paused, Start again? over a dimmed world)
 *   └─ FocusRing (runtime)
 *
 * The stadium fills the viewport; there is no permanent sidebar or central arc. Both corner groups adapt to the actual
 * logical viewport. Their measured bounds govern marker avoidance and map framing. NavSwitcher shows TourNav or
 * ExploreNav; the other child is collapsed too, and the focus ring skips anything under an inactive switcher child.
 *
 * Keyboard focus: HUD buttons never take Slate focus (the viewport keeps the arrow keys and Space), so the HUD runs its
 * own. Tab and Shift+Tab (D-pad Down and Up) move a focus ring through the buttons on screen, in reading order, and
 * Enter (A) presses the one in the ring. A modal card keeps the ring inside itself.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Moves the focus ring to the next (+1) or previous (-1) button on screen. The first press lands on the first one. */
    void MoveFocus(int32 Direction);

    /** Presses the button in the focus ring. False if there is none (Enter then keeps its usual meaning). */
    bool ActivateFocus();

    void ClearFocus();
    /** Used by the non-Shipping LFF.UI.Timeline capture command. */
    bool SetDayTimelineOpen(bool bOpen);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot);

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> InformationSize;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> ControlSize;

    /** Optional legacy sidebar bindings allow rebuilding an older saved widget in place. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SidebarSize;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> SidebarBackdrop;

    /** WBP_LFF_Callouts (parent ULFFCalloutLayer): the places on the stadium, in both modes. First child of RootOverlay. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFCalloutLayer> Callouts;

    /** WBP_LFF_Welcome: the two mode cards (TourModeButton, ExploreModeButton) and Calm Mode. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Welcome;

    /** WBP_LFF_SidebarHeader: identity-only card; the legacy asset name is retained. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> SidebarHeader;

    /** WBP_LFF_Narrative: caption, expandable day timeline, previews and finale actions.
     *  Its ticket phone is moved into the viewport overlay at initialization. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Narrative;

    /** WBP_LFF_ExploreMap (parent ULFFMapPanel): the level buttons, the place toggles and the picked place's card. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFMapPanel> ExploreMap;

    /** WBP_LFF_TimelineArc (parent ULFFTimelineArc). */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFTimelineArc> TimelineArc;

    /** WBP_LFF_StageChrome: modes, Settings, Voice, Calm Mode and Crowd; the same control dock in both modes. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> StageChrome;

    /** Child 0 TourNav, child 1 ExploreNav. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> NavSwitcher;

    /** WBP_LFF_TourNav: BackButton and NextButton. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> TourNav;

    /** WBP_LFF_ExploreNav: MySeatButton and ResumeTourButton. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> ExploreNav;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Settings;

    /** The ring around the focused button: a light ring inside a thin dark halo, so it reads on the dark cards and on a
     *  bright sky alike (WCAG 2.4.11, 3:1 against both). Light, not kelly green: kelly already means "On" and "spoken". */
    UPROPERTY(EditDefaultsOnly, Category = "Focus")
    FLinearColor FocusRingColor = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Focus")
    FLinearColor FocusHaloColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

    UPROPERTY(EditDefaultsOnly, Category = "Focus", meta = (ClampMin = "2.0", ClampMax = "6.0"))
    float FocusRingWidth = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Focus", meta = (ClampMin = "1.0", ClampMax = "4.0"))
    float FocusHaloWidth = 2.0f;

    /** Space between the button's edge and the ring, so the ring never covers the button's own outline. */
    UPROPERTY(EditDefaultsOnly, Category = "Focus", meta = (ClampMin = "0.0", ClampMax = "8.0"))
    float FocusRingGap = 4.0f;

private:
    ULFFJourneySubsystem* GetJourney() const;

    /** The buttons the ring may visit right now, in reading order. */
    void CollectFocusable(TArray<UButton*>& Out) const;
    /**
     * Enabled, laid out, and visible all the way up to the HUD, crossing from each panel's tree into the HUD's. Under a
     * Widget Switcher only the active child counts: the others stay Visible and keep their last geometry, so without
     * this check Tab would land on buttons that aren't drawn.
     */
    static bool IsOnScreen(const UButton* Button);
    /** The next widget up: the slot's parent, or, at the root of a panel's tree, the panel itself in the HUD's tree. */
    static UWidget* ParentAcrossTrees(const UWidget* Widget);
    void PlaceFocusRing();

    /** Every panel, in focus reading order (welcome and modal layer last). */
    TArray<ULFFJourneyPanel*, TInlineAllocator<12>> GetPanels() const;
    /** The welcome screen is up until a mode is chosen: before Start in the tour, never in Free explore. */
    static bool IsWelcome(const FLFFJourneySnapshot& Snapshot);
    /** Shows Child in its switcher, or collapses it. Returns true if it went from hidden to shown (it fades in). */
    static bool ShowSwitcherChild(UWidgetSwitcher* Switcher, ULFFJourneyPanel* Child, bool bShown, bool bInstant);

    TWeakObjectPtr<UButton> FocusedButton;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> FocusRing;

    /** Returns true if the panel went from hidden to shown. */
    static bool ShowPanel(ULFFJourneyPanel* Panel, bool bShown, bool bInstant);
    void BuildTicketPresentation(UOverlay* Root);
    void LayoutTicketPresentation(const FGeometry& HUDGeometry);

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> TicketPresentation;

    UPROPERTY(Transient)
    TObjectPtr<UScaleBox> TicketScale;

    bool bShowTicketPresentation = false;

    TWeakObjectPtr<ULFFJourneySubsystem> BoundJourney;
    int32 LastStepIndex = INDEX_NONE;
    FLinearColor BackdropColor = FLinearColor::Black;
    FVector2D LastLayoutSize = FVector2D::ZeroVector;
    bool bLastLargeText = false;
};
