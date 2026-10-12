#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFJourneyPanel.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class URichTextBlock;
class UVerticalBox;
class UImage;
class UPanelWidget;
class UTexture2D;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UWidget;
class ULFFJourneySubsystem;

class ULFFJourneyPanel;

/** One chapter of the day in the chapter bar: a short name and the first step (0-based) that belongs to it. */
USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FLFFChapter
{
    GENERATED_BODY()

    FLFFChapter() = default;
    FLFFChapter(const FText& InName, const int32 InFirstStep) : Name(InName), FirstStep(InFirstStep) {}

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter", meta = (ClampMin = "0"))
    int32 FirstStep = 0;
};

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
 * Base class for the HUD pieces (the panel list is in LFFHUDWidget.h). WBP_LFF_HUD (ULFFHUDWidget) owns them, decides
 * which ones are on screen and pushes each snapshot here. A panel fills in whichever named widgets it contains
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

    void LayoutChapters(float AvailableWidth, bool bLargeText);
    void LayoutTimeline(float AvailableWidth);
    /** Idempotently opens/closes the actual narrative drawer; unavailable timelines remain collapsed. */
    void SetTimelineOpen(bool bOpen);

    /** Retains the narrative's animation/bindings when the HUD moves the phone into the viewport overlay. */
    UWidget* GetTicketPhone() const { return TicketPhone.Get(); }

    /** One delegate for both pointer clicks and the HUD's custom keyboard activation. */
    void RegisterButtonFeedback(UButton* Button);

    void LayoutWelcome(const FVector2D& ViewportSize);
    void UpdateWelcomeHint(const UButton* NavigationFocus, bool bParentVisible = true);

    /** Fades the panel in over FadeSeconds with an ease-out. Instant when reduced motion is on. */
    void FadeIn(bool bInstant);

    /** A timeline mark was clicked. */
    void HandleTimelineMark(int32 StepIndex);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Visual-only reactions (Calm Mode theme). Runs before the badge and disclaimer are re-enforced. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Journey")
    void OnSnapshotChanged(const FLFFJourneySnapshot& Snapshot);

    /** For C++ panels built on this one (the timeline arc): runs after the named widgets are filled. */
    virtual void NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot) {}

    /** The chapter a step belongs to (INDEX_NONE before the first). */
    int32 ChapterOf(int32 StepIndex) const;
    /** Where StepIndex is in its chapter: OutPlace of OutCount ("2 of 3"). */
    void ChapterPlace(int32 Chapter, const FLFFJourneySnapshot& Snapshot, int32& OutPlace, int32& OutCount) const;
    /** "Resume tour · Plaza, 2 of 3", or "Start the tour" if Free explore was picked on the welcome screen. */
    FText ResumeText(const FLFFJourneySnapshot& Snapshot) const;

    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "0.0", ClampMax = "0.4"))
    float FadeSeconds = 0.25f;

    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "1.0", ClampMax = "1.5"))
    float LargeTextScale = 1.25f;

    /** The caption card on step 1 only, so it stays clear of the stadium in the wide aerial (on top of Large text). */
    UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = "0.8", ClampMax = "1.0"))
    float FirstStepCardScale = 0.9f;

    /**
     * The day in chapters, in order (SourceData/Journey/LFF_GoldenPath.json, 23 steps). A chapter runs until the next one's
     * FirstStep. The timeline arc shows one node per chapter, and Resume tour names the chapter ("Gates, 2 of 3").
     */
    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    TArray<FLFFChapter> Chapters = {
        { NSLOCTEXT("LFFHUD", "ChapterArrival", "Arrival"), 0 },
        { NSLOCTEXT("LFFHUD", "ChapterGates", "Gates"), 4 },
        { NSLOCTEXT("LFFHUD", "ChapterPlazaShort", "Plaza"), 7 },
        { NSLOCTEXT("LFFHUD", "ChapterToMySeat", "To my seat"), 10 },
        { NSLOCTEXT("LFFHUD", "ChapterGame", "Game"), 14 },
    };

    /** Where the caption sits on journey steps (lower left, above the chapter bar). The finale centres it. */
    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FMargin CaptionRestPadding = FMargin(8.0f, 0.0f, 0.0f, 96.0f);

    /** Chapter chips: the current one is a light raised chip with dark text; the others sit on the bar in light text. */
    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    FLinearColor ChapterCurrentFill = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    FLinearColor ChapterCurrentText = FLinearColor(0.0018f, 0.0242f, 0.0296f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    FLinearColor ChapterText = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    FLinearColor ChapterMutedText = FLinearColor(0.479f, 0.558f, 0.565f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Chapters")
    FLinearColor ChapterAccent = FLinearColor(0.141f, 0.514f, 0.021f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor HighContrastSurfaceColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipCurrentColor = FLinearColor(0.888f, 0.847f, 0.777f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipDoneColor = FLinearColor(0.0f, 0.2f, 0.23f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor PipUpcomingColor = FLinearColor(0.35f, 0.35f, 0.35f, 0.6f);

    /** The current step's timeline mark: the light chip with dark text, as the current mode is. Every other mark is light
     *  text with a faint outline; the ones already passed are in the muted text colour. */
    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineCurrentFill = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineCurrentText = FLinearColor(0.0018f, 0.0242f, 0.0296f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineOtherText = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor TimelineDoneText = FLinearColor(0.479f, 0.558f, 0.565f, 1.0f);

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

    /** "Step 5 · 10:00 AM" (caption card): the step and, when it has one, the example clock. */
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

    /** Filled at runtime with one clickable mark per timed step: a HorizontalBox for a bar, or a VerticalBox for the tour
     *  panel's schedule column, top to bottom. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> TimelineMarks;

    /** Filled at runtime with one chip per chapter (Chapters); a click jumps to the chapter's first step. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> ChapterMarks;

    /** Dock presentation: responsive chapter chips, wrapped labels, no redundant number line. */
    UPROPERTY(EditDefaultsOnly, Category = "Style")
    bool CompactChapterChips = false;

    /** The phone that rises on the ticket step; TicketImage shows the ticket frames. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> TicketPhone;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> TicketImage;

    /** Shown on the last step only. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LearnMorePanel;

    /** Gap from the rail's right edge to Learn More: clears "Start Over", which is wider than "Next". */
    UPROPERTY(EditDefaultsOnly, Category = "Finale")
    float LearnMoreRightGap = 236.0f;

    /**
     * The Eagles Autism Foundation wordmark on the caption card, last step only (with Learn More). If the blueprint
     * has no widget of this name, the caption panel (the one with CaptionStack) builds it under the body text.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> FinaleLogo;

    /** Assets/Logos/Eagles-Autism-Foundation logo.png (Tools/Pipeline/prepare_logos.py: 1024 x 256 canvas, logo 737 wide),
     *  as a UI texture (no mips, never streamed, so it stays sharp). */
    UPROPERTY(EditDefaultsOnly, Category = "Finale")
    TSoftObjectPtr<UTexture2D> FinaleLogoTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LFF/UI/Icons/T_LFF_Logo_EAF_UI.T_LFF_Logo_EAF_UI")));

    /** On-screen size of the wordmark at 1080p (the logo's own 2.88:1). */
    UPROPERTY(EditDefaultsOnly, Category = "Finale")
    FVector2D FinaleLogoSize = FVector2D(207.0, 72.0);

    /** The voice button on the caption card (top right): speaker with waves when the voice is on, crossed out when off. */
    UPROPERTY(EditDefaultsOnly, Category = "Voice")
    TSoftObjectPtr<UTexture2D> VoiceOnIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LFF/UI/Icons/T_LFF_Icon_VoiceOn.T_LFF_Icon_VoiceOn")));

    UPROPERTY(EditDefaultsOnly, Category = "Voice")
    TSoftObjectPtr<UTexture2D> VoiceOffIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LFF/UI/Icons/T_LFF_Icon_VoiceOff.T_LFF_Icon_VoiceOff")));

    // The word the voice is reading is marked by ULFFKaraokeDecorator (highlight and underline, never colour alone).

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

    /** Standard | Calm: the splash choice before Start and the top bar's segmented control. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StandardButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CalmButton;

    /** Finale: Start Over and Learn More on the caption card, which moves to the centre on the last step. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> FinaleActions;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> FinaleStartOverButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> FinaleLearnMoreButton;

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

    /** Opens and closes the day timeline drawer (closed by default). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> TimelineToggleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TimelineToggleLabel;

    /** The Standard | Calm labels: the chosen side is bold and bright, the other muted. */
    /** Guided tour | Explore: the sidebar's mode switch and the welcome screen's two cards. The current mode is the
     *  light chip; on the welcome screen neither is chosen yet. Labels are authored in the blueprint. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> TourModeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TourModeLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ExploreModeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ExploreModeLabel;

    /** Free explore, in Next's slot: "Resume tour · Plaza, 2 of 3". */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ResumeTourButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ResumeTourLabel;

    /** Free explore, in Back's slot. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> MySeatButton;

    /** The "Illustration. Not directions…" pill behind DisclaimerLineText: shown and hidden with it. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> DisclaimerPill;

    /** The progress bar: ProgressDone and ProgressRest are the two Fill slots of one HorizontalBox, resized to the step. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> ProgressDone;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> ProgressRest;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StandardLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CalmLabel;

    /** The chosen side of Standard | Calm and every toggle that is On: a light fill, an accent outline and dark text. */
    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor ModeSelectedFill = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor ModeSelectedOutline = FLinearColor(0.141f, 0.514f, 0.021f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor ModeMutedText = FLinearColor(0.905f, 0.888f, 0.838f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor ModeSelectedText = FLinearColor(0.0018f, 0.0242f, 0.0296f, 1.0f);

    /** Selected or On: brand fill, accent outline and white text. Off: the plain secondary look. Used by every toggle. */
    void ApplyModeButton(UButton* Button, UTextBlock* Label, bool bSelected);
    /** The row's On / Off word (a text block named like the label, ending State instead of Label), if the row has one. */
    UTextBlock* FindStateText(const UTextBlock* Label) const;
    /** "Calm Mode" and "On" in their own blocks when the row has a state word, else "Calm Mode: On" in the label. */
    void ApplyToggleText(UTextBlock* Label, const FText& Name, const FText& State);
    ULFFJourneySubsystem* GetJourney() const;
    /** After a click, keyboard focus goes back to the viewport so the arrow keys and Space keep driving the journey. */
    void ReturnFocusToGame() const;

private:
    UFUNCTION()
    void HandleTimelineToggleClicked();
    void ApplyTimelineDrawer();
    void PrepareTimelineLayout();

    UFUNCTION()
    void HandleUIClickFeedback();

    UPROPERTY(Transient)
    TObjectPtr<UPanelWidget> TimelineLayout;

    float TimelineWidth = 440.0f;

    /** Each toggle's own look, remembered the first time it is touched. */
    TMap<TWeakObjectPtr<UButton>, FButtonStyle> ToggleBaseStyles;
    bool bTimelineOpen = false;
    bool bTimelineAvailable = false;
    bool bModalOrPaused = false;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> VoiceIconLabel;

    void BindButtons();
    void CollectSurfaces();
    void ApplySurfaces(bool bHighContrast);
    void RebuildPips(int32 Count);
    void ApplyPips(int32 CurrentIndex);
    void TickFade();
    void RebuildTimeline(const TArray<FLFFTimelineMark>& Marks);
    void RebuildChapters();
    void ApplyChapters(const FLFFJourneySnapshot& Snapshot);
    void ApplyTimeline(int32 CurrentIndex);
    /** Shows or hides the ticket phone. Still: no rise and a still frame (less movement, Calm Mode). */
    void ShowTicket(bool bShow, bool bStill);
    /** Builds FinaleLogo on the caption card when the blueprint doesn't have one. */
    void BuildFinaleLogo();
    /** Voice in the control dock, or the legacy caption header. */
    void BuildVoiceButton();
    /** Rich-text copies of the card's title and body (the text blocks stay as the source, collapsed), for the highlight. */
    void BuildKaraokeText();
    URichTextBlock* MakeKaraokeCopy(UTextBlock* Source, const TCHAR* Name);
    void ApplyKaraoke();
    void HandleVoiceWord(int32 WordIndex);
    UFUNCTION()
    void HandleVoiceIconClicked();
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
    UFUNCTION()
    void HandleStandardClicked();
    UFUNCTION()
    void HandleCalmClicked();
    UFUNCTION()
    void HandleTourModeClicked();
    UFUNCTION()
    void HandleExploreModeClicked();
    UFUNCTION()
    void HandleResumeTourClicked();
    UFUNCTION()
    void HandleMySeatClicked();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SurfaceBorders;

    TArray<FLinearColor> SurfaceColors;
    FLinearColor ScrimColor = FLinearColor::Transparent;

    /** One SizeBox per step, each holding a rounded Border. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<USizeBox>> Pips;

    FTimerHandle FadeTimer;

    UPROPERTY(Transient)
    TObjectPtr<UButton> VoiceIconButton;

    UPROPERTY(Transient)
    TObjectPtr<UImage> VoiceIconImage;

    UPROPERTY(Transient)
    TObjectPtr<URichTextBlock> TitleRich;

    UPROPERTY(Transient)
    TObjectPtr<URichTextBlock> BodyRich;

    FString KaraokeTitle;
    FString KaraokeBody;
    int32 KaraokeWord = INDEX_NONE;
    int32 KaraokeShown = INDEX_NONE - 1;
    FDelegateHandle VoiceWordHandle;
    double FadeStartSeconds = 0.0;

    /** One per mark, in order: the step it jumps to, its button and label. */
    TArray<int32> MarkSteps;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> MarkButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> MarkLabels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFTimelineMarkHandler>> MarkHandlers;

    /** One per chapter chip: its button, its three lines (number, name, progress) and its click handler. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> ChapterButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ChapterNumbers;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ChapterNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ChapterProgress;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULFFTimelineMarkHandler>> ChapterHandlers;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> TicketFrames;

    FTimerHandle TicketTimer;
    double TicketStartSeconds = 0.0;
    bool bTicketShown = false;
    bool bTicketStill = false;
};
