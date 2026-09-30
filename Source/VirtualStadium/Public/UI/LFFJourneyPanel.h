#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFJourneyPanel.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UTexture2D;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UWidget;
class ULFFJourneySubsystem;

class ULFFJourneyPanel;

/** Routes one timeline mark's click to its step (dynamic delegates can't carry the index themselves). */
UCLASS()
class VIRTUALSTADIUM_API ULFFTimelineMarkHandler : public UObject
{
    GENERATED_BODY()

public:
    int32 StepIndex = INDEX_NONE;
    TWeakObjectPtr<ULFFJourneyPanel> Panel;

    UFUNCTION()
    void HandleClicked();
};

/**
 * Base class for the five HUD pieces: WBP_LFF_Splash, WBP_LFF_TopChrome, WBP_LFF_StepCaption,
 * WBP_LFF_BottomRail and WBP_LFF_Settings. WBP_LFF_HUD (ULFFHUDWidget) owns them, decides which ones
 * are on screen and pushes each snapshot here. A panel fills in whichever named widgets it contains
 * and routes its buttons to the journey subsystem. Every binding is optional; the names are the
 * contract in Docs/JOURNEY_SCHEMA.md.
 *
 * Styling conventions read by C++: Border widgets named Surface* switch to opaque in High contrast,
 * and a ScaleBox named TextScale grows by LargeTextScale when Large text is on.
 */
UCLASS(Abstract)
class VIRTUALSTADIUM_API ULFFJourneyPanel : public UUserWidget
{
    GENERATED_BODY()

public:
    void ApplySnapshot(const FLFFJourneySnapshot& Snapshot);

    /** Fades the panel in over FadeSeconds with an ease-out. Instant when reduced motion is on. */
    void FadeIn(bool bInstant);

    /** A timeline mark was clicked. */
    void HandleTimelineMark(int32 StepIndex);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Visual-only reactions (Calm Mode theme). Runs before the badge and disclaimer are re-enforced. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Journey")
    void OnSnapshotChanged(const FLFFJourneySnapshot& Snapshot);

    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "0.0", ClampMax = "0.4"))
    float FadeSeconds = 0.25f;

    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "1.0", ClampMax = "1.5"))
    float LargeTextScale = 1.25f;

    /** The caption card on step 1 only, so it stays clear of the stadium in the wide aerial (on top of Large text). */
    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "0.8", ClampMax = "1.0"))
    float FirstStepCardScale = 0.9f;

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor HighContrastSurfaceColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipCurrentColor = FLinearColor(0.888f, 0.847f, 0.777f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipDoneColor = FLinearColor(0.0f, 0.2f, 0.23f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipUpcomingColor = FLinearColor(0.35f, 0.35f, 0.35f, 0.6f);

    /** The current step's timeline mark: bone pill, midnight text. Every other mark is muted text only. */
    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineCurrentFill = FLinearColor(0.888f, 0.847f, 0.777f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineCurrentText = FLinearColor(0.0f, 0.0723f, 0.0887f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineOtherText = FLinearColor(0.584f, 0.546f, 0.479f, 1.0f);

    /** Ticket frames: /Game/LFF/UI/Ticket/T_LFF_Ticket_00 ... (Tools/Pipeline/prepare_ticket.py). */
    UPROPERTY(EditDefaultsOnly, Category = "Ticket")
    FString TicketFramePrefix = TEXT("/Game/LFF/UI/Ticket/T_LFF_Ticket_");

    UPROPERTY(EditDefaultsOnly, Category = "Ticket", meta = (ClampMin = "1", ClampMax = "120"))
    int32 TicketFrameCount = 30;

    UPROPERTY(EditDefaultsOnly, Category = "Ticket", meta = (ClampMin = "0.03", ClampMax = "0.5"))
    float TicketFrameSeconds = 0.1f;

    /** The phone rises from below the screen edge over this long, easing out. */
    UPROPERTY(EditDefaultsOnly, Category = "Ticket", meta = (ClampMin = "0.3", ClampMax = "3.0"))
    float TicketRiseSeconds = 1.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Ticket", meta = (ClampMin = "100.0"))
    float TicketRiseDistance = 800.0f;

    // Text.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> BodyText;

    /** "Step 2 of 4" (top bar). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ProgressText;

    /** "Step 2" (caption card). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StepNumberText;

    /** Full disclaimer sentence (splash subtitle). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DisclaimerText;

    /** "Illustration. Not directions for the real stadium." (top left, under the badge, journey steps only). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DisclaimerLineText;

    /** Verified / Approximate / Illustration (caption card). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DisclaimerChipText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PrototypeBadgeText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TargetMissingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NextLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PauseLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SoundLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ReducedMotionLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CalmModeLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LargeTextLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> HighContrastLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> VoiceLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CrowdLabel;

    /** "Approximate times · example 1:00 PM game" at the start of the timeline. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TimelineNoteText;

    /** "Preview this? Off" / "Preview this? Soft". */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PreviewLabel;

    /** One short line under the preview button: what Soft does, or why it can't play yet. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PreviewNoteText;

    // Groups shown and hidden from the snapshot.
    /** Shown only once the journey has started (top bar: progress, Pause, Sound). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> StepControls;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> DisclaimerChip;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SettingsPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PausePanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> RestartConfirmPanel;

    /** Shown on loud or bright steps only, and never in Calm Mode. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> PreviewRow;

    /** Dims the world behind a modal. Blocks clicks. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> Scrim;

    /** The thin timeline above Back/Next (journey steps only). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> TimelinePanel;

    /** Filled at runtime with one clickable mark per timed step. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> TimelineMarks;

    /** The phone that rises on the ticket step; TicketImage shows the ticket frames. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> TicketPhone;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> TicketImage;

    /** Shown on the last step only. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LearnMorePanel;

    /**
     * The Eagles Autism Foundation wordmark on the caption card, last step only (with Learn More). If the blueprint
     * has no widget of this name, the caption panel (the one with CaptionStack) builds it under the body text.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> FinaleLogo;

    /** Assets/Logos/Eagles-Autism-Foundation logo.png (Tools/Pipeline/prepare_logos.py: 1024 x 256 canvas, logo 737 wide). */
    UPROPERTY(EditDefaultsOnly, Category = "Finale")
    TSoftObjectPtr<UTexture2D> FinaleLogoTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LFF/Logos/T_LFF_Logo_EAF.T_LFF_Logo_EAF")));

    /** On-screen size of the wordmark at 1080p (the logo's own 2.88:1). */
    UPROPERTY(EditDefaultsOnly, Category = "Finale")
    FVector2D FinaleLogoSize = FVector2D(207.0, 72.0);

    /** Filled at runtime with one pip per step. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> ProgressPips;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScaleBox> TextScale;

    // Buttons.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StartButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> NextButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PauseButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ResumeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SoundButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsCloseButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartYesButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartNoButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ReducedMotionButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CalmModeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LargeTextButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> HighContrastButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> VoiceButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CrowdButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LearnMoreButton;

private:
    ULFFJourneySubsystem* GetJourney() const;
    void BindButtons();
    void ReturnFocusToGame() const;
    void CollectSurfaces();
    void ApplySurfaces(bool bHighContrast);
    void RebuildPips(int32 Count);
    void ApplyPips(int32 CurrentIndex);
    void TickFade();
    void RebuildTimeline(const TArray<FLFFTimelineMark>& Marks);
    void ApplyTimeline(int32 CurrentIndex);
    /** Shows or hides the ticket phone. Still: no rise and a still frame (less movement, Calm Mode). */
    void ShowTicket(bool bShow, bool bStill);
    /** Builds FinaleLogo on the caption card when the blueprint doesn't have one. */
    void BuildFinaleLogo();
    void TickTicket();

    UFUNCTION()
    void HandleNextClicked();
    UFUNCTION()
    void HandleBackClicked();
    UFUNCTION()
    void HandlePauseClicked();
    UFUNCTION()
    void HandleResumeClicked();
    UFUNCTION()
    void HandleSettingsClicked();
    UFUNCTION()
    void HandleRestartClicked();
    UFUNCTION()
    void HandleRestartYesClicked();
    UFUNCTION()
    void HandleRestartNoClicked();
    UFUNCTION()
    void HandleReducedMotionClicked();
    UFUNCTION()
    void HandleSoundClicked();
    UFUNCTION()
    void HandleCalmModeClicked();
    UFUNCTION()
    void HandleLargeTextClicked();
    UFUNCTION()
    void HandleHighContrastClicked();
    UFUNCTION()
    void HandlePreviewClicked();
    UFUNCTION()
    void HandleVoiceClicked();
    UFUNCTION()
    void HandleCrowdClicked();
    UFUNCTION()
    void HandleLearnMoreClicked();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SurfaceBorders;

    TArray<FLinearColor> SurfaceColors;
    FLinearColor ScrimColor = FLinearColor::Transparent;

    /** One SizeBox per step, each holding a rounded Border. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<USizeBox>> Pips;

    FTimerHandle FadeTimer;
    double FadeStartSeconds = 0.0;

    /** One per mark, in order: the step it jumps to, its button and label. */
    TArray<int32> MarkSteps;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> MarkButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> MarkLabels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFTimelineMarkHandler>> MarkHandlers;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> TicketFrames;

    FTimerHandle TicketTimer;
    double TicketStartSeconds = 0.0;
    bool bTicketShown = false;
    bool bTicketStill = false;
};
