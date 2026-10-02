#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFHUDWidget.generated.h"

class UButton;
class ULFFJourneyPanel;
class ULFFJourneySubsystem;
class USizeBox;

/**
 * Base class for WBP_LFF_HUD (/Game/LFF/UI/WBP_LFF_HUD), the one HUD on screen.
 * It holds the five panels, decides which are shown for the current screen, and pushes every
 * journey snapshot into them:
 *  - Splash (before Start): the welcome card. TopChrome shows only the badge and Settings.
 *  - Step (after Start): TopChrome, StepCaption and BottomRail.
 *  - Settings: the modal layer (Settings, Paused, Start again?) over a dimmed world.
 * Required bindings make a HUD without one of the panels fail to compile.
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

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleJourneyChanged(const FLFFJourneySnapshot& Snapshot);

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> Splash;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> TopChrome;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> StepCaption;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULFFJourneyPanel> BottomRail;

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
    static bool IsOnScreen(const UButton* Button);
    void PlaceFocusRing();

    TWeakObjectPtr<UButton> FocusedButton;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> FocusRing;

    /** Returns true if the panel went from hidden to shown. */
    static bool ShowPanel(ULFFJourneyPanel* Panel, bool bShown, bool bInstant);

    TWeakObjectPtr<ULFFJourneySubsystem> BoundJourney;
    int32 LastStepIndex = INDEX_NONE;
};
