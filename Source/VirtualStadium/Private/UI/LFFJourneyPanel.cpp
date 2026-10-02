#include "UI/LFFJourneyPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/OverlaySlot.h"
#include "Engine/DataTable.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFKaraokeDecorator.h"
#include "Misc/Paths.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "LFFHUD"

namespace
{
    const TCHAR* SurfacePrefix = TEXT("Surface");
    // One pip per step (23 fit in about 420 px); the current one is a longer pill, so position never rests on colour.
    constexpr float PipSize = 12.0f;
    constexpr float CurrentPipWidth = 34.0f;
    constexpr float PipGap = 5.0f;
    constexpr float HighContrastScrimAlpha = 0.85f;
    // Timeline marks keep the project's 64 px minimum hit target.
    constexpr float MarkMinWidth = 128.0f;
    constexpr float MarkMinHeight = 64.0f;
    constexpr float MarkGap = 6.0f;
    constexpr int32 MarkFontSize = 18;

    FSlateBrush Pill(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(Fill);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
        Brush.OutlineSettings.Color = FSlateColor(Outline);
        Brush.OutlineSettings.Width = OutlineWidth;
        return Brush;
    }

    /** A mark: the current one is a filled pill; the others are text with a faint outline that brightens on hover. */
    FButtonStyle MarkStyle(const bool bCurrent, const FLinearColor& CurrentFill, const FLinearColor& Muted)
    {
        const FLinearColor Clear(0.0f, 0.0f, 0.0f, 0.0f);
        const FLinearColor Faint(Muted.R, Muted.G, Muted.B, 0.35f);
        FButtonStyle Style;
        Style.SetNormal(bCurrent ? Pill(CurrentFill, CurrentFill, 1.0f) : Pill(Clear, Faint, 1.0f));
        Style.SetHovered(bCurrent ? Pill(CurrentFill, CurrentFill, 2.0f) : Pill(Clear, Muted, 2.0f));
        Style.SetPressed(bCurrent ? Pill(CurrentFill, CurrentFill, 2.0f) : Pill(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f), Muted, 2.0f));
        Style.SetDisabled(Pill(Clear, Clear, 0.0f));
        Style.SetNormalPadding(FMargin(10.0f, 4.0f));
        Style.SetPressedPadding(FMargin(10.0f, 5.0f, 10.0f, 3.0f));
        return Style;
    }

    /** Groups never take clicks themselves, so a hidden-then-shown panel can't block the buttons under it. */
    void ShowGroup(UWidget* Widget, const bool bShown)
    {
        if (Widget != nullptr)
        {
            Widget->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    }

    void ApplyText(UTextBlock* TextBlock, const FText& Text)
    {
        if (TextBlock != nullptr)
        {
            TextBlock->SetText(Text);
        }
    }

    void Enable(UWidget* Widget, const bool bEnabled)
    {
        if (Widget != nullptr)
        {
            Widget->SetIsEnabled(bEnabled);
        }
    }

    FText OnOffWord(const bool bOn)
    {
        return bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off");
    }

    const TCHAR* UIFontPath = TEXT("/Game/LFF/UI/Fonts/F_LFF_Atkinson.F_LFF_Atkinson");

    FText PreviewNote(const FLFFJourneySnapshot& Snapshot)
    {
        if (Snapshot.IntensityPreview == ELFFIntensityPreview::Sound)
        {
            if (!Snapshot.bPreviewAvailable)
            {
                return LOCTEXT("PreviewNeedsSound", "Turn Sound on first to hear a quiet sample.");
            }
            return Snapshot.bPreviewOn
                ? LOCTEXT("PreviewSoundPlaying", "Playing a short, quiet, muffled sample.")
                : LOCTEXT("PreviewSoundOffer", "Soft plays a short, quiet, muffled sample.");
        }

        if (Snapshot.IntensityPreview == ELFFIntensityPreview::Lights)
        {
            return LOCTEXT("PreviewLights", "Soft is a slow, gentle brightening. No flashing.");
        }

        if (Snapshot.IntensityPreview == ELFFIntensityPreview::Anthem)
        {
            return Snapshot.bPreviewOn
                ? LOCTEXT("PreviewAnthemPlaying", "The eagle flies over once, with soft fireworks.")
                : LOCTEXT("PreviewAnthemOffer", "Soft shows the eagle flying over once, with soft fireworks. No flashing, no sound.");
        }

        return FText::GetEmpty();
    }

    FText StepNumber(const FLFFJourneySnapshot& Snapshot)
    {
        const FText Number = FText::Format(LOCTEXT("StepNumber", "Step {0}"), FText::AsNumber(Snapshot.StepIndex + 1));
        return Snapshot.ClockText.IsEmpty() ? Number : FText::Format(LOCTEXT("StepNumberClock", "{0} · {1}"), Number, Snapshot.ClockText);
    }

    FText ChipText(const ELFFDisclaimerLevel Level)
    {
        switch (Level)
        {
        case ELFFDisclaimerLevel::Verified:
            return LOCTEXT("ChipVerified", "Verified");
        case ELFFDisclaimerLevel::Approximate:
            return LOCTEXT("ChipApproximate", "Approximate");
        case ELFFDisclaimerLevel::Illustrative:
        default:
            return LOCTEXT("ChipIllustration", "Illustration");
        }
    }
}

void ULFFJourneyPanel::NativeConstruct()
{
    Super::NativeConstruct();

    BindButtons();
    CollectSurfaces();
    if (TitleRich != nullptr)
    {
        if (ULFFJourneySubsystem* Journey = GetJourney())
        {
            VoiceWordHandle = Journey->OnVoiceWord.AddUObject(this, &ULFFJourneyPanel::HandleVoiceWord);
        }
    }
}

void ULFFJourneyPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Before the Slate widgets exist: a panel inserted into a built box is drawn last, whatever its index.
    BuildFinaleLogo();
    BuildVoiceButton();
    BuildKaraokeText();
    if (LearnMorePanel != nullptr)
    {
        if (UOverlaySlot* Slot = Cast<UOverlaySlot>(LearnMorePanel->Slot))
        {
            Slot->SetPadding(FMargin(0.0f, 0.0f, LearnMoreRightGap, 0.0f));
        }
    }
}

void ULFFJourneyPanel::BuildVoiceButton()
{
    UHorizontalBox* Header = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("CaptionHeader")));
    if (Header == nullptr || WidgetTree == nullptr || VoiceIconButton != nullptr)
    {
        return;
    }

    // The project's minimum hit target (64 px at 1080p) around a 34 px icon.
    USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VoiceIconSize"));
    Box->SetMinDesiredWidth(64.0f);
    Box->SetMinDesiredHeight(64.0f);
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("VoiceIconButton"));
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Button->IsFocusable = false;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    if (PreviewButton != nullptr)
    {
        // The same rounded secondary style as the card's other button.
        Button->SetStyle(PreviewButton->GetStyle());
        Button->SetBackgroundColor(PreviewButton->GetBackgroundColor());
    }
    Button->SetToolTipText(LOCTEXT("VoiceIconTip", "Voice on or off (key 5)"));
    Box->SetContent(Button);
    // Icon plus the words "Voice: On" or "Voice: Off": a control is never an icon alone.
    UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VoiceIconContent"));
    UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VoiceIconImage"));
    Icon->SetDesiredSizeOverride(FVector2D(28.0, 28.0));
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VoiceIconLabel"));
    if (StepNumberText != nullptr)
    {
        // The card's eyebrow font without its letter spacing, in the card's main text colour (the icon textures are white).
        FSlateFontInfo Font = StepNumberText->GetFont();
        Font.Size = 18;
        Font.LetterSpacing = 0;
        Label->SetFont(Font);
    }
    const FSlateColor IconInk = TitleText != nullptr ? TitleText->GetColorAndOpacity() : FSlateColor(FLinearColor::White);
    Label->SetColorAndOpacity(IconInk);
    Icon->SetColorAndOpacity(IconInk.GetSpecifiedColor());
    Label->SetText(LOCTEXT("VoiceLabelOff", "Voice: Off"));
    if (UHorizontalBoxSlot* IconSlot = Content->AddChildToHorizontalBox(Icon))
    {
        IconSlot->SetVerticalAlignment(VAlign_Center);
        IconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
    }
    if (UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(Label))
    {
        LabelSlot->SetVerticalAlignment(VAlign_Center);
    }
    Button->SetContent(Content);
    VoiceIconLabel = Label;
    if (UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Box))
    {
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
    }
    Button->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleVoiceIconClicked);
    VoiceIconButton = Button;
    VoiceIconImage = Icon;
}

void ULFFJourneyPanel::HandleVoiceIconClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleVoiceFromCard(); }
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

URichTextBlock* ULFFJourneyPanel::MakeKaraokeCopy(UTextBlock* Source, const TCHAR* Name)
{
    UVerticalBox* Stack = Source != nullptr ? Cast<UVerticalBox>(Source->GetParent()) : nullptr;
    if (Stack == nullptr)
    {
        return nullptr;
    }

    // One style, the card's own text. The spoken word is <w>…</> and ULFFKaraokeDecorator draws its highlight and underline.
    UDataTable* Styles = NewObject<UDataTable>(this);
    Styles->RowStruct = FRichTextStyleRow::StaticStruct();
    FRichTextStyleRow Row;
    Row.TextStyle.SetFont(Source->GetFont());
    Row.TextStyle.SetColorAndOpacity(Source->GetColorAndOpacity());
    Styles->AddRow(TEXT("Default"), Row);

    URichTextBlock* Rich = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), Name);
    Rich->SetTextStyleSet(Styles);
    Rich->SetDecorators({ ULFFKaraokeDecorator::StaticClass() });
    Rich->SetAutoWrapText(false);
    Rich->SetWrapTextAt(Source->GetWrapTextAt());
    // Same line spacing as the source, so the card doesn't change height when the copy takes over.
    if (const FFloatProperty* LineHeight = FindFProperty<FFloatProperty>(UTextLayoutWidget::StaticClass(), TEXT("LineHeightPercentage")))
    {
        Rich->SetLineHeightPercentage(LineHeight->GetPropertyValue_InContainer(Source));
    }
    UVerticalBoxSlot* SourceSlot = Cast<UVerticalBoxSlot>(Source->Slot);
    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Stack->InsertChildAt(Stack->GetChildIndex(Source) + 1, Rich)))
    {
        if (SourceSlot != nullptr)
        {
            Slot->SetPadding(SourceSlot->GetPadding());
            Slot->SetHorizontalAlignment(SourceSlot->GetHorizontalAlignment());
        }
    }
    Source->SetVisibility(ESlateVisibility::Collapsed);
    return Rich;
}

void ULFFJourneyPanel::BuildKaraokeText()
{
    // Only the caption card (the panel with CaptionStack) reads aloud; the splash keeps its plain text.
    if (TitleRich != nullptr || WidgetTree == nullptr || GetWidgetFromName(TEXT("CaptionStack")) == nullptr)
    {
        return;
    }
    TitleRich = MakeKaraokeCopy(TitleText.Get(), TEXT("TitleRich"));
    BodyRich = MakeKaraokeCopy(BodyText.Get(), TEXT("BodyRich"));
}

namespace
{
    /** The card text as rich text, word Highlight (in this text) marked <w> for the karaoke decorator. Words are split at spaces, as the
     *  voice timings are (Tools/Audio/make_vo_words.py). */
    FString KaraokeMarkup(const FString& Text, const int32 Highlight, int32& OutWords)
    {
        TArray<FString> Words;
        Text.ParseIntoArrayWS(Words);
        OutWords = Words.Num();
        FString Out;
        for (int32 I = 0; I < Words.Num(); ++I)
        {
            FString Word = Words[I].Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;"))
                .Replace(TEXT("\""), TEXT("&quot;"));
            if (I > 0)
            {
                Out += TEXT(" ");
            }
            Out += I == Highlight ? FString::Printf(TEXT("<w>%s</>"), *Word) : Word;
        }
        return Out;
    }
}

void ULFFJourneyPanel::ApplyKaraoke()
{
    if (TitleRich == nullptr || KaraokeShown == KaraokeWord)
    {
        return;
    }
    KaraokeShown = KaraokeWord;
    int32 TitleWords = 0, BodyWords = 0;
    // Title first (to count its words), then the body carries on from there.
    const FString Title = KaraokeMarkup(KaraokeTitle, KaraokeWord, TitleWords);
    TitleRich->SetText(FText::FromString(Title));
    if (BodyRich != nullptr)
    {
        BodyRich->SetText(FText::FromString(KaraokeMarkup(KaraokeBody, KaraokeWord == INDEX_NONE ? INDEX_NONE : KaraokeWord - TitleWords, BodyWords)));
        // A step with no body (the finale) must not keep an empty line under its title.
        BodyRich->SetVisibility(KaraokeBody.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

void ULFFJourneyPanel::HandleVoiceWord(const int32 WordIndex)
{
    KaraokeWord = WordIndex;
    ApplyKaraoke();
}

void ULFFJourneyPanel::BuildFinaleLogo()
{
    UVerticalBox* Stack = Cast<UVerticalBox>(GetWidgetFromName(TEXT("CaptionStack")));
    UTexture2D* Texture = FinaleLogoTexture.LoadSynchronous();
    if (FinaleLogo != nullptr || Stack == nullptr || BodyText == nullptr || Texture == nullptr || WidgetTree == nullptr)
    {
        return;
    }

    // The wordmark is dark teal, so it sits on a small bone plate rather than on the dark glass.
    UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FinaleLogo"));
    FSlateBrush PlateBrush;
    PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    PlateBrush.TintColor = FSlateColor(FLinearColor::White);
    PlateBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    PlateBrush.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
    Plate->SetBrush(PlateBrush);
    Plate->SetPadding(FMargin(14.0f, 8.0f));

    UImage* Logo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FinaleLogoImage"));
    FSlateBrush LogoBrush;
    LogoBrush.SetResourceObject(Texture);
    LogoBrush.ImageSize = FinaleLogoSize;
    // Only the logo, not the canvas padding either side of it (737 of 1024 px, centred).
    LogoBrush.SetUVRegion(FBox2f(FVector2f(0.14f, 0.0f), FVector2f(0.86f, 1.0f)));
    Logo->SetBrush(LogoBrush);
    Plate->SetContent(Logo);

    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Stack->InsertChildAt(Stack->GetChildIndex(BodyText) + 1, Plate)))
    {
        Slot->SetHorizontalAlignment(HAlign_Center);
        Slot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
    }
    Plate->SetVisibility(ESlateVisibility::Collapsed);
    FinaleLogo = Plate;
}

void ULFFJourneyPanel::NativeDestruct()
{
    if (VoiceWordHandle.IsValid())
    {
        if (ULFFJourneySubsystem* Journey = GetJourney())
        {
            Journey->OnVoiceWord.Remove(VoiceWordHandle);
        }
        VoiceWordHandle.Reset();
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FadeTimer);
        World->GetTimerManager().ClearTimer(TicketTimer);
    }

    Super::NativeDestruct();
}

void ULFFJourneyPanel::ApplySnapshot(const FLFFJourneySnapshot& Snapshot)
{
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen;

    ApplyText(TitleText.Get(), Snapshot.Title);
    ApplyText(BodyText.Get(), Snapshot.Body);
    if (TitleRich != nullptr)
    {
        const FString Title = Snapshot.Title.ToString(), Body = Snapshot.Body.ToString();
        if (Title != KaraokeTitle || Body != KaraokeBody)
        {
            KaraokeTitle = Title;
            KaraokeBody = Body;
            KaraokeShown = INDEX_NONE - 1;
        }
        const ULFFJourneySubsystem* Journey = GetJourney();
        KaraokeWord = Snapshot.bVoicePlaying && Journey != nullptr ? Journey->GetVoiceWordIndex() : INDEX_NONE;
        ApplyKaraoke();
    }
    if (VoiceIconImage != nullptr)
    {
        const bool bVoiceOn = Snapshot.Settings.IsVoiceEffective();
        if (UTexture2D* Icon = (bVoiceOn ? VoiceOnIcon : VoiceOffIcon).LoadSynchronous())
        {
            VoiceIconImage->SetBrushFromTexture(Icon);
            VoiceIconImage->SetDesiredSizeOverride(FVector2D(28.0, 28.0));
        }
        if (VoiceIconLabel != nullptr)
        {
            VoiceIconLabel->SetText(bVoiceOn ? LOCTEXT("VoiceLabelOn", "Voice: On") : LOCTEXT("VoiceLabelOff", "Voice: Off"));
        }
        VoiceIconButton->SetToolTipText(bVoiceOn ? LOCTEXT("VoiceIconOn", "Voice is on. Turn it off (key 5)")
                                                 : LOCTEXT("VoiceIconOff", "Voice is off. Turn it on (key 5)"));
    }
    ApplyText(ProgressText.Get(), Snapshot.ProgressText);
    ApplyText(StepNumberText.Get(), StepNumber(Snapshot));
    ApplyText(DisclaimerText.Get(), Snapshot.DisclaimerText);
    ApplyText(DisclaimerLineText.Get(), Snapshot.DisclaimerLine);
    ApplyText(DisclaimerChipText.Get(), ChipText(Snapshot.DisclaimerLevel));
    ApplyText(PrototypeBadgeText.Get(), Snapshot.PrototypeBadgeText);

    if (TargetMissingText != nullptr)
    {
        ApplyText(TargetMissingText.Get(), LOCTEXT("TargetMissing", "This step is not available yet."));
        ShowGroup(TargetMissingText.Get(), Snapshot.bStarted && Snapshot.bTargetMissing);
    }

    // The dock's Next stays "Next" (and rests on the last step): Start Over and Learn More are on the finale card.
    ApplyText(NextLabel.Get(), LOCTEXT("Next", "Next"));
    ApplyText(PauseLabel.Get(), Snapshot.bPaused ? LOCTEXT("Resume", "Resume") : LOCTEXT("Pause", "Pause"));
    // Labels show what is actually happening: Calm Mode reads as Sound off and Less movement on.
    ApplyToggleText(SoundLabel.Get(), LOCTEXT("Sound", "Sound"), OnOffWord(Snapshot.Settings.IsSoundEffective()));
    ApplyToggleText(ReducedMotionLabel.Get(), LOCTEXT("LessMovement", "Less movement"), OnOffWord(Snapshot.Settings.IsReducedMotionEffective()));
    ApplyToggleText(CalmModeLabel.Get(), LOCTEXT("CalmMode", "Calm Mode"), OnOffWord(Snapshot.Settings.bCalmMode));
    ApplyToggleText(LargeTextLabel.Get(), LOCTEXT("LargeText", "Large text"), OnOffWord(Snapshot.Settings.bLargeText));
    ApplyToggleText(HighContrastLabel.Get(), LOCTEXT("HighContrast", "High contrast"), OnOffWord(Snapshot.Settings.bHighContrast));
    // Voice reads as Off whenever it can't be heard (Sound off, Calm Mode).
    ApplyToggleText(VoiceLabel.Get(), LOCTEXT("Voice", "Voice"), OnOffWord(Snapshot.Settings.IsVoiceEffective()));
    ApplyToggleText(CrowdLabel.Get(), LOCTEXT("Crowd", "Crowd"),
        Snapshot.Settings.IsCrowdShown() ? LOCTEXT("CrowdLow", "Low") : LOCTEXT("CrowdOff", "Off"));
    ApplyText(TimelineNoteText.Get(), LOCTEXT("TimelineNote", "Approximate times\nExample 1:00 PM game"));
    ApplyText(PreviewLabel.Get(), Snapshot.bPreviewOn ? LOCTEXT("PreviewSoft", "Preview this? Soft") : LOCTEXT("PreviewOff", "Preview this? Off"));
    ApplyText(PreviewNoteText.Get(), PreviewNote(Snapshot));
    ShowGroup(PreviewRow.Get(), Snapshot.bStarted && Snapshot.bShowPreviewControls);

    ShowGroup(StepControls.Get(), Snapshot.bStarted);

    // One modal card at a time: the restart question, then Settings, then the pause card.
    ShowGroup(RestartConfirmPanel.Get(), Snapshot.bRestartConfirmOpen);
    ShowGroup(SettingsPanel.Get(), Snapshot.bSettingsOpen && !Snapshot.bRestartConfirmOpen);
    ShowGroup(PausePanel.Get(), Snapshot.bPaused && !bModal);
    if (Scrim != nullptr)
    {
        Scrim->SetVisibility(bModal || Snapshot.bPaused ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // Buttons keep their place and are disabled rather than hidden, so nothing shifts under the pointer.
    Enable(StartButton.Get(), Snapshot.bCanNext);
    Enable(NextButton.Get(), Snapshot.bCanNext && !(Snapshot.bStarted && Snapshot.bAtEnd && FinaleActions == nullptr));
    Enable(BackButton.Get(), Snapshot.bCanBack);
    Enable(PauseButton.Get(), Snapshot.bStarted && !bModal);
    Enable(SettingsButton.Get(), !Snapshot.bRestartConfirmOpen);
    Enable(RestartButton.Get(), Snapshot.bStarted);
    // Calm Mode holds Sound off and Less movement on; the toggles come back when Calm Mode is turned off.
    Enable(SoundButton.Get(), !Snapshot.Settings.bCalmMode);
    Enable(ReducedMotionButton.Get(), !Snapshot.Settings.bCalmMode);
    Enable(VoiceButton.Get(), Snapshot.Settings.IsSoundEffective());
    Enable(VoiceIconButton.Get(), !Snapshot.Settings.bCalmMode && !bModal);
    Enable(CrowdButton.Get(), !Snapshot.Settings.bCalmMode);
    Enable(LearnMoreButton.Get(), Snapshot.bAtEnd && !bModal);
    // Learn More now lives on the finale card (FinaleActions), so the rail keeps only Start Over.
    ShowGroup(LearnMorePanel.Get(), false);
    // Standard | Calm: the chosen side at full strength, the other dimmed (the label says which is which too).
    ApplyModeButton(StandardButton.Get(), StandardLabel.Get(), !Snapshot.Settings.bCalmMode);
    ApplyModeButton(CalmButton.Get(), CalmLabel.Get(), Snapshot.Settings.bCalmMode);
    // The Settings toggles: On is filled, Off is plain, so the state never rests on the word alone.
    ApplyModeButton(CalmModeButton.Get(), CalmModeLabel.Get(), Snapshot.Settings.bCalmMode);
    ApplyModeButton(ReducedMotionButton.Get(), ReducedMotionLabel.Get(), Snapshot.Settings.IsReducedMotionEffective());
    ApplyModeButton(SoundButton.Get(), SoundLabel.Get(), Snapshot.Settings.IsSoundEffective());
    ApplyModeButton(LargeTextButton.Get(), LargeTextLabel.Get(), Snapshot.Settings.bLargeText);
    ApplyModeButton(HighContrastButton.Get(), HighContrastLabel.Get(), Snapshot.Settings.bHighContrast);
    ApplyModeButton(VoiceButton.Get(), VoiceLabel.Get(), Snapshot.Settings.IsVoiceEffective());
    ApplyModeButton(CrowdButton.Get(), CrowdLabel.Get(), Snapshot.Settings.IsCrowdShown());
    Enable(StandardButton.Get(), !bModal);
    Enable(CalmButton.Get(), !bModal);
    // Finale: the caption card moves to the centre of the screen with Start Over and Learn More on it.
    if (FinaleActions != nullptr && TextScale != nullptr)
    {
        const bool bFinale = Snapshot.bStarted && Snapshot.bAtEnd;
        ShowGroup(FinaleActions.Get(), bFinale);
        Enable(FinaleStartOverButton.Get(), bFinale && !bModal);
        Enable(FinaleLearnMoreButton.Get(), bFinale && !bModal);
        if (UOverlaySlot* CardSlot = Cast<UOverlaySlot>(TextScale->Slot))
        {
            // Otherwise lower left, above the timeline (build_wbp_lff_ui.py: CAPTION_BOTTOM).
            CardSlot->SetHorizontalAlignment(bFinale ? HAlign_Center : HAlign_Left);
            CardSlot->SetVerticalAlignment(bFinale ? VAlign_Center : VAlign_Bottom);
            CardSlot->SetPadding(bFinale ? FMargin(0.0f) : FMargin(48.0f, 0.0f, 0.0f, 296.0f));
        }
    }
    ShowGroup(FinaleLogo.Get(), Snapshot.bStarted && Snapshot.bAtEnd);
    Enable(PreviewButton.Get(), Snapshot.bShowPreviewControls && !Snapshot.bPaused && !bModal
        && (Snapshot.bPreviewAvailable || Snapshot.bPreviewOn));

    if (TextScale != nullptr)
    {
        // Only the caption card (the panel with the step title) shrinks on step 1; the chrome keeps its size.
        const bool bFirstStepCard = TitleText != nullptr && Snapshot.bStarted && Snapshot.StepIndex == 0;
        TextScale->SetUserSpecifiedScale((Snapshot.Settings.bLargeText ? LargeTextScale : 1.0f) * (bFirstStepCard ? FirstStepCardScale : 1.0f));
    }

    ApplySurfaces(Snapshot.Settings.bHighContrast);

    if (TimelineMarks != nullptr)
    {
        if (MarkSteps.Num() != Snapshot.TimelineMarks.Num())
        {
            RebuildTimeline(Snapshot.TimelineMarks);
        }
        ApplyTimeline(Snapshot.bStarted ? Snapshot.StepIndex : INDEX_NONE);
        for (UButton* Mark : MarkButtons)
        {
            Enable(Mark, Snapshot.bStarted && !bModal && !Snapshot.bPaused);
        }
    }
    bTimelineAvailable = Snapshot.bStarted && Snapshot.TimelineMarks.Num() > 0;
    bModalOrPaused = bModal || Snapshot.bPaused;
    ApplyTimelineDrawer();

    if (TicketPhone != nullptr)
    {
        ShowTicket(Snapshot.bStarted && Snapshot.bShowTicketPhone && !Snapshot.bTargetMissing, Snapshot.Settings.IsReducedMotionEffective());
    }

    if (ProgressDone != nullptr && ProgressRest != nullptr && Snapshot.StepCount > 0)
    {
        // Two Fill slots in one row: done and the rest. A sliver of the rest stays so the bar never closes on itself.
        const float Fraction = FMath::Clamp(static_cast<float>(Snapshot.StepIndex + 1) / static_cast<float>(Snapshot.StepCount), 0.02f, 1.0f);
        FSlateChildSize DoneSize(ESlateSizeRule::Fill);
        DoneSize.Value = Fraction;
        FSlateChildSize RestSize(ESlateSizeRule::Fill);
        RestSize.Value = FMath::Max(1.0f - Fraction, 0.001f);
        if (UHorizontalBoxSlot* DoneSlot = Cast<UHorizontalBoxSlot>(ProgressDone->Slot))
        {
            DoneSlot->SetSize(DoneSize);
        }
        if (UHorizontalBoxSlot* RestSlot = Cast<UHorizontalBoxSlot>(ProgressRest->Slot))
        {
            RestSlot->SetSize(RestSize);
        }
    }

    if (ProgressPips != nullptr)
    {
        if (Pips.Num() != Snapshot.StepCount)
        {
            RebuildPips(Snapshot.StepCount);
        }
        ApplyPips(Snapshot.StepIndex);
    }

    OnSnapshotChanged(Snapshot);

    // Enforced after the Blueprint event: styling can't hide the badge or the location disclaimer.
    if (PrototypeBadgeText != nullptr)
    {
        ShowGroup(PrototypeBadgeText.Get(), true);
    }
    if (Snapshot.bStarted)
    {
        ShowGroup(DisclaimerChip.Get(), true);
        ShowGroup(DisclaimerChipText.Get(), true);
    }
    if (Snapshot.bShowDisclaimer)
    {
        ShowGroup(DisclaimerText.Get(), true);
    }
    // The badge line is for journey steps; the splash shows the full sentence instead.
    ShowGroup(DisclaimerLineText.Get(), Snapshot.bStarted && Snapshot.bShowDisclaimer);
    ShowGroup(DisclaimerPill.Get(), Snapshot.bStarted && Snapshot.bShowDisclaimer);
}

void ULFFJourneyPanel::FadeIn(const bool bInstant)
{
    UWorld* World = GetWorld();
    if (World != nullptr)
    {
        World->GetTimerManager().ClearTimer(FadeTimer);
    }

    if (bInstant || FadeSeconds <= 0.0f || World == nullptr)
    {
        SetRenderOpacity(1.0f);
        return;
    }

    SetRenderOpacity(0.0f);
    FadeStartSeconds = FPlatformTime::Seconds();
    World->GetTimerManager().SetTimer(FadeTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneyPanel::TickFade), 1.0f / 60.0f, true);
}

void ULFFJourneyPanel::TickFade()
{
    const float Alpha = FMath::Clamp(static_cast<float>((FPlatformTime::Seconds() - FadeStartSeconds) / FadeSeconds), 0.0f, 1.0f);
    // Ease-out cubic: most of the change happens early, then it settles.
    SetRenderOpacity(1.0f - FMath::Pow(1.0f - Alpha, 3.0f));

    if (Alpha >= 1.0f)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(FadeTimer);
        }
    }
}

void ULFFJourneyPanel::CollectSurfaces()
{
    SurfaceBorders.Reset();
    SurfaceColors.Reset();

    if (WidgetTree != nullptr)
    {
        WidgetTree->ForEachWidget([this](UWidget* Widget)
        {
            UBorder* Border = Cast<UBorder>(Widget);
            if (Border != nullptr && Border->GetName().StartsWith(SurfacePrefix))
            {
                SurfaceBorders.Add(Border);
                SurfaceColors.Add(Border->GetBrushColor());
            }
        });
    }

    if (Scrim != nullptr)
    {
        ScrimColor = Scrim->GetBrushColor();
    }
}

void ULFFJourneyPanel::ApplySurfaces(const bool bHighContrast)
{
    for (int32 Index = 0; Index < SurfaceBorders.Num(); ++Index)
    {
        if (UBorder* Border = SurfaceBorders[Index].Get())
        {
            Border->SetBrushColor(bHighContrast ? HighContrastSurfaceColor : SurfaceColors[Index]);
        }
    }

    if (Scrim != nullptr)
    {
        FLinearColor Color = ScrimColor;
        if (bHighContrast)
        {
            Color.A = FMath::Max(Color.A, HighContrastScrimAlpha);
        }
        Scrim->SetBrushColor(Color);
    }
}

void ULFFJourneyPanel::RebuildPips(const int32 Count)
{
    ProgressPips->ClearChildren();
    Pips.Reset();

    if (WidgetTree == nullptr)
    {
        return;
    }

    FSlateBrush PillBrush;
    PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Dot->SetBrush(PillBrush);
        Box->SetContent(Dot);
        Box->SetHeightOverride(PipSize);

        if (UHorizontalBoxSlot* PipSlot = ProgressPips->AddChildToHorizontalBox(Box))
        {
            PipSlot->SetVerticalAlignment(VAlign_Center);
            PipSlot->SetPadding(FMargin(Index == 0 ? 0.0f : PipGap, 0.0f, 0.0f, 0.0f));
        }
        Pips.Add(Box);
    }
}

void ULFFJourneyPanel::ApplyPips(const int32 CurrentIndex)
{
    // The current step is a wider pill, so position never depends on colour alone.
    for (int32 Index = 0; Index < Pips.Num(); ++Index)
    {
        USizeBox* Box = Pips[Index].Get();
        UBorder* Dot = Box != nullptr ? Cast<UBorder>(Box->GetContent()) : nullptr;
        if (Dot == nullptr)
        {
            continue;
        }

        Box->SetWidthOverride(Index == CurrentIndex ? CurrentPipWidth : PipSize);
        Dot->SetBrushColor(Index == CurrentIndex ? PipCurrentColor : (Index < CurrentIndex ? PipDoneColor : PipUpcomingColor));
    }
}

void ULFFTimelineMarkHandler::HandleClicked()
{
    if (ULFFJourneyPanel* Owner = Panel.Get())
    {
        Owner->HandleTimelineMark(StepIndex);
    }
}

void ULFFJourneyPanel::RebuildTimeline(const TArray<FLFFTimelineMark>& Marks)
{
    TimelineMarks->ClearChildren();
    MarkSteps.Reset();
    MarkButtons.Reset();
    MarkLabels.Reset();
    MarkHandlers.Reset();

    if (WidgetTree == nullptr)
    {
        return;
    }

    const UObject* MarkFont = LoadObject<UObject>(nullptr, UIFontPath);
    if (MarkFont == nullptr)
    {
        MarkFont = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    for (int32 Index = 0; Index < Marks.Num(); ++Index)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetMinDesiredWidth(MarkMinWidth);
        Box->SetMinDesiredHeight(MarkMinHeight);

        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        // Not focusable, like every HUD button: C++ hands keyboard focus back to the game. UButton only exposes
        // this before its Slate widget exists, through the deprecated field (the setter is protected).
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Button->IsFocusable = false;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Button->SetStyle(MarkStyle(false, TimelineCurrentFill, TimelineOtherText));
        Box->SetContent(Button);

        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(Marks[Index].Label);
        Label->SetJustification(ETextJustify::Center);
        if (MarkFont != nullptr)
        {
            Label->SetFont(FSlateFontInfo(MarkFont, MarkFontSize, TEXT("Bold")));
        }
        Button->SetContent(Label);

        ULFFTimelineMarkHandler* Handler = NewObject<ULFFTimelineMarkHandler>(this);
        Handler->StepIndex = Marks[Index].StepIndex;
        Handler->Panel = this;
        Button->OnClicked.AddDynamic(Handler, &ULFFTimelineMarkHandler::HandleClicked);

        if (UHorizontalBoxSlot* MarkSlot = TimelineMarks->AddChildToHorizontalBox(Box))
        {
            MarkSlot->SetVerticalAlignment(VAlign_Center);
            MarkSlot->SetPadding(FMargin(Index == 0 ? 0.0f : MarkGap, 0.0f, 0.0f, 0.0f));
        }

        MarkSteps.Add(Marks[Index].StepIndex);
        MarkButtons.Add(Button);
        MarkLabels.Add(Label);
        MarkHandlers.Add(Handler);
    }
}

void ULFFJourneyPanel::ApplyTimeline(const int32 CurrentIndex)
{
    // Only the current step's mark is emphasized; on untimed steps none is.
    for (int32 Index = 0; Index < MarkSteps.Num(); ++Index)
    {
        const bool bCurrent = MarkSteps[Index] == CurrentIndex;
        if (UButton* Button = MarkButtons[Index].Get())
        {
            Button->SetStyle(MarkStyle(bCurrent, TimelineCurrentFill, TimelineOtherText));
        }
        if (UTextBlock* Label = MarkLabels[Index].Get())
        {
            Label->SetColorAndOpacity(FSlateColor(bCurrent ? TimelineCurrentText : TimelineOtherText));
        }
    }
}

void ULFFJourneyPanel::HandleTimelineMark(const int32 StepIndex)
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->JumpToStep(StepIndex); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::ShowTicket(const bool bShow, const bool bStill)
{
    UWorld* World = GetWorld();
    if (!bShow)
    {
        if (bTicketShown)
        {
            ShowGroup(TicketPhone.Get(), false);
            if (World != nullptr)
            {
                World->GetTimerManager().ClearTimer(TicketTimer);
            }
            bTicketShown = false;
        }
        return;
    }

    if (TicketFrames.Num() == 0)
    {
        for (int32 Frame = 0; Frame < TicketFrameCount; ++Frame)
        {
            const FString Name = FString::Printf(TEXT("%s%02d"), *TicketFramePrefix, Frame);
            if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *Name, *FPaths::GetBaseFilename(Name))))
            {
                TicketFrames.Add(Texture);
            }
        }
    }

    if (bTicketShown && bStill == bTicketStill)
    {
        return;
    }

    const bool bWasShown = bTicketShown;
    bTicketShown = true;
    bTicketStill = bStill;
    ShowGroup(TicketPhone.Get(), true);
    if (TicketImage != nullptr && TicketFrames.Num() > 0)
    {
        TicketImage->SetBrushFromTexture(TicketFrames[0]);
    }

    if (World != nullptr)
    {
        World->GetTimerManager().ClearTimer(TicketTimer);
    }

    // Calm Mode and less movement: the phone is already up and still. Otherwise it rises, then the barcode sweeps.
    if (bStill || World == nullptr)
    {
        TicketPhone->SetRenderTranslation(FVector2D::ZeroVector);
        return;
    }

    if (!bWasShown)
    {
        TicketStartSeconds = FPlatformTime::Seconds();
        TicketPhone->SetRenderTranslation(FVector2D(0.0f, TicketRiseDistance));
    }
    World->GetTimerManager().SetTimer(TicketTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneyPanel::TickTicket), 1.0f / 30.0f, true);
}

void ULFFJourneyPanel::TickTicket()
{
    if (TicketPhone == nullptr)
    {
        return;
    }

    const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - TicketStartSeconds);
    const float Rise = FMath::Clamp(Elapsed / TicketRiseSeconds, 0.0f, 1.0f);
    // Ease-out cubic: up quickly, then a slow settle. No bounce.
    TicketPhone->SetRenderTranslation(FVector2D(0.0f, TicketRiseDistance * FMath::Pow(1.0f - Rise, 3.0f)));

    if (TicketImage != nullptr && TicketFrames.Num() > 0 && Rise >= 1.0f)
    {
        const int32 Frame = FMath::FloorToInt((Elapsed - TicketRiseSeconds) / TicketFrameSeconds) % TicketFrames.Num();
        TicketImage->SetBrushFromTexture(TicketFrames[FMath::Max(Frame, 0)]);
    }
}

ULFFJourneySubsystem* ULFFJourneyPanel::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}

void ULFFJourneyPanel::BindButtons()
{
    if (StartButton != nullptr) { StartButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleNextClicked); }
    if (NextButton != nullptr) { NextButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleNextClicked); }
    if (BackButton != nullptr) { BackButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleBackClicked); }
    if (PauseButton != nullptr) { PauseButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandlePauseClicked); }
    if (ResumeButton != nullptr) { ResumeButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleResumeClicked); }
    if (SoundButton != nullptr) { SoundButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleSoundClicked); }
    if (SettingsButton != nullptr) { SettingsButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleSettingsClicked); }
    if (SettingsCloseButton != nullptr) { SettingsCloseButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleSettingsClicked); }
    if (RestartButton != nullptr) { RestartButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleRestartClicked); }
    if (RestartYesButton != nullptr) { RestartYesButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleRestartYesClicked); }
    if (RestartNoButton != nullptr) { RestartNoButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleRestartNoClicked); }
    if (ReducedMotionButton != nullptr) { ReducedMotionButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleReducedMotionClicked); }
    if (CalmModeButton != nullptr) { CalmModeButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleCalmModeClicked); }
    if (LargeTextButton != nullptr) { LargeTextButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleLargeTextClicked); }
    if (HighContrastButton != nullptr) { HighContrastButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleHighContrastClicked); }
    if (PreviewButton != nullptr) { PreviewButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandlePreviewClicked); }
    if (VoiceButton != nullptr) { VoiceButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleVoiceClicked); }
    if (CrowdButton != nullptr) { CrowdButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleCrowdClicked); }
    if (LearnMoreButton != nullptr) { LearnMoreButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleLearnMoreClicked); }
    if (TimelineToggleButton != nullptr) { TimelineToggleButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleTimelineToggleClicked); }
    if (StandardButton != nullptr) { StandardButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleStandardClicked); }
    if (CalmButton != nullptr) { CalmButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleCalmClicked); }
    if (FinaleStartOverButton != nullptr) { FinaleStartOverButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleNextClicked); }
    if (FinaleLearnMoreButton != nullptr) { FinaleLearnMoreButton->OnClicked.AddUniqueDynamic(this, &ULFFJourneyPanel::HandleLearnMoreClicked); }
}

void ULFFJourneyPanel::ReturnFocusToGame() const
{
    // After a mouse click, hand keyboard focus back to the viewport so arrow keys and Space
    // drive the journey instead of moving UMG focus or re-clicking the button.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}

void ULFFJourneyPanel::HandleNextClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Next(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleBackClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Back(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandlePauseClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->TogglePause(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleResumeClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Resume(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleSettingsClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleSettings(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleRestartClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->RequestRestart(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleRestartYesClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ConfirmRestart(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleRestartNoClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->Cancel(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleReducedMotionClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleReducedMotion(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleSoundClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleSound(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleCalmModeClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleCalmMode(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleLargeTextClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleLargeText(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleHighContrastClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleHighContrast(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandlePreviewClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->TogglePreview(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleVoiceClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleVoice(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleCrowdClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->ToggleCrowd(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleStandardClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->SetExperienceMode(false); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleCalmClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->SetExperienceMode(true); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::HandleLearnMoreClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->OpenLearnMore(); }
    ReturnFocusToGame();
}

void ULFFJourneyPanel::ApplyModeButton(UButton* Button, UTextBlock* Label, const bool bSelected)
{
    if (Button == nullptr)
    {
        return;
    }
    const FButtonStyle& BaseStyle = ToggleBaseStyles.FindOrAdd(Button, Button->GetStyle());

    // Chosen or On: a light fill with an accent outline and dark text. Off: the plain secondary look.
    FButtonStyle Style = BaseStyle;
    if (bSelected)
    {
        // Every state keeps the light fill under the dark text: hovering or pressing a chosen button, or a chosen button that
        // Calm Mode has locked, must never fall back to the dark secondary fill.
        const FLinearColor Hover = FMath::Lerp(ModeSelectedFill, FLinearColor::Black, 0.06f);
        const FLinearColor Pressed = FMath::Lerp(ModeSelectedFill, FLinearColor::Black, 0.14f);
        // Opaque: Slate already dims a disabled button's text, so a see-through fill would take the contrast down twice.
        const FLinearColor Locked = FMath::Lerp(ModeSelectedFill, FLinearColor::Black, 0.04f);
        Style.Normal.TintColor = FSlateColor(ModeSelectedFill);
        Style.Normal.OutlineSettings.Color = FSlateColor(ModeSelectedOutline);
        Style.Normal.OutlineSettings.Width = 3.0f;
        Style.Hovered = Style.Normal;
        Style.Hovered.TintColor = FSlateColor(Hover);
        Style.Pressed = Style.Normal;
        Style.Pressed.TintColor = FSlateColor(Pressed);
        Style.Disabled = Style.Normal;
        Style.Disabled.TintColor = FSlateColor(Locked);
        Style.Disabled.OutlineSettings.Color = FSlateColor(FLinearColor(ModeSelectedOutline.R, ModeSelectedOutline.G, ModeSelectedOutline.B, 0.5f));
    }
    Button->SetStyle(Style);
    Button->SetRenderOpacity(1.0f);

    // The name, its state word and any description line inside the button all take the state's text colour.
    const FSlateColor TextColor(bSelected ? ModeSelectedText : ModeMutedText);
    if (WidgetTree != nullptr)
    {
        WidgetTree->ForWidgetAndChildren(Button, [&TextColor](UWidget* Child)
        {
            if (UTextBlock* Text = Cast<UTextBlock>(Child))
            {
                Text->SetColorAndOpacity(TextColor);
            }
        });
    }
    if (Label != nullptr)
    {
        Label->SetColorAndOpacity(TextColor);
    }
    // The state word is bold when On, so the state never rests on colour alone.
    if (UTextBlock* State = Label != nullptr ? FindStateText(Label) : nullptr)
    {
        FSlateFontInfo Font = State->GetFont();
        Font.TypefaceFontName = bSelected ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
        State->SetFont(Font);
    }
}

UTextBlock* ULFFJourneyPanel::FindStateText(const UTextBlock* Label) const
{
    // CalmModeLabel -> CalmModeState: the On / Off word at the end of the row.
    FString Name = Label->GetName();
    if (!Name.RemoveFromEnd(TEXT("Label")))
    {
        return nullptr;
    }
    return Cast<UTextBlock>(GetWidgetFromName(FName(*(Name + TEXT("State")))));
}

void ULFFJourneyPanel::ApplyToggleText(UTextBlock* Label, const FText& Name, const FText& State)
{
    if (Label == nullptr)
    {
        return;
    }
    if (UTextBlock* StateText = FindStateText(Label))
    {
        Label->SetText(Name);
        StateText->SetText(State);
        return;
    }
    Label->SetText(FText::Format(LOCTEXT("ToggleFormat", "{0}: {1}"), Name, State));
}

void ULFFJourneyPanel::ApplyTimelineDrawer()
{
    // The toggle only exists while there is a timeline to show; the drawer only opens from it.
    if (TimelineToggleButton != nullptr)
    {
        TimelineToggleButton->SetVisibility(bTimelineAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    Enable(TimelineToggleButton.Get(), bTimelineAvailable && !bModalOrPaused);
    ApplyText(TimelineToggleLabel.Get(), bTimelineOpen ? LOCTEXT("HideTimeline", "Hide day timeline") : LOCTEXT("ShowTimeline", "Show day timeline"));
    ShowGroup(TimelinePanel.Get(), bTimelineAvailable && bTimelineOpen);
}

void ULFFJourneyPanel::HandleTimelineToggleClicked()
{
    bTimelineOpen = !bTimelineOpen;
    ApplyTimelineDrawer();
    ReturnFocusToGame();
}

#undef LOCTEXT_NAMESPACE
