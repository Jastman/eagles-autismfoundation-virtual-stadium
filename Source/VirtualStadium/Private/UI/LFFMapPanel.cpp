#include "UI/LFFMapPanel.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Journey/LFFJourneySubsystem.h"

#define LOCTEXT_NAMESPACE "LFFHUD"

namespace
{
    FText LevelName(const ELFFMapLevel Level)
    {
        switch (Level)
        {
        case ELFFMapLevel::Field: return LOCTEXT("LevelField", "Field");
        case ELFFMapLevel::LowerConcourse: return LOCTEXT("LevelLower", "Lower concourse");
        case ELFFMapLevel::Club: return LOCTEXT("LevelClub", "Club level");
        case ELFFMapLevel::Suites: return LOCTEXT("LevelSuites", "Suites");
        case ELFFMapLevel::UpperDeck: return LOCTEXT("LevelUpper", "Upper deck");
        }
        return FText::GetEmpty();
    }

    FText AmenityName(const ELFFAmenity Amenity)
    {
        switch (Amenity)
        {
        case ELFFAmenity::SensoryRoom: return LOCTEXT("AmenitySensory", "Sensory rooms");
        case ELFFAmenity::QuietZone: return LOCTEXT("AmenityQuiet", "Quiet zones");
        case ELFFAmenity::GuestServices: return LOCTEXT("AmenityGuest", "Guest Services");
        case ELFFAmenity::Restroom: return LOCTEXT("AmenityRestroom", "Restrooms");
        case ELFFAmenity::Food: return LOCTEXT("AmenityFood", "Food");
        }
        return FText::GetEmpty();
    }
}

void ULFFMapButtonHandler::HandleClicked()
{
    if (ULFFMapPanel* Owner = Panel.Get())
    {
        Owner->HandleMapButton(Kind, Index);
    }
}

void ULFFMapPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    BindMapButton(FieldTab.Get(), ULFFMapButtonHandler::EKind::Level, static_cast<int32>(ELFFMapLevel::Field));
    BindMapButton(LowerConcourseTab.Get(), ULFFMapButtonHandler::EKind::Level, static_cast<int32>(ELFFMapLevel::LowerConcourse));
    BindMapButton(ClubTab.Get(), ULFFMapButtonHandler::EKind::Level, static_cast<int32>(ELFFMapLevel::Club));
    BindMapButton(SuitesTab.Get(), ULFFMapButtonHandler::EKind::Level, static_cast<int32>(ELFFMapLevel::Suites));
    BindMapButton(UpperDeckTab.Get(), ULFFMapButtonHandler::EKind::Level, static_cast<int32>(ELFFMapLevel::UpperDeck));
    BindMapButton(SensoryToggle.Get(), ULFFMapButtonHandler::EKind::Amenity, static_cast<int32>(ELFFAmenity::SensoryRoom));
    BindMapButton(QuietToggle.Get(), ULFFMapButtonHandler::EKind::Amenity, static_cast<int32>(ELFFAmenity::QuietZone));
    BindMapButton(GuestToggle.Get(), ULFFMapButtonHandler::EKind::Amenity, static_cast<int32>(ELFFAmenity::GuestServices));
    BindMapButton(RestroomToggle.Get(), ULFFMapButtonHandler::EKind::Amenity, static_cast<int32>(ELFFAmenity::Restroom));
    BindMapButton(FoodToggle.Get(), ULFFMapButtonHandler::EKind::Amenity, static_cast<int32>(ELFFAmenity::Food));
    if (HideAllButton != nullptr) { HideAllButton->OnClicked.AddUniqueDynamic(this, &ULFFMapPanel::HandleHideAllClicked); }
    if (ShowMeButton != nullptr) { ShowMeButton->OnClicked.AddUniqueDynamic(this, &ULFFMapPanel::HandleShowMeClicked); }
    if (LevelsToggleButton != nullptr) { LevelsToggleButton->OnClicked.AddUniqueDynamic(this, &ULFFMapPanel::HandleLevelsClicked); }

    const TPair<UTextBlock*, ELFFMapLevel> TabLabels[] = {
        { FieldTabLabel.Get(), ELFFMapLevel::Field }, { LowerConcourseTabLabel.Get(), ELFFMapLevel::LowerConcourse },
        { ClubTabLabel.Get(), ELFFMapLevel::Club }, { SuitesTabLabel.Get(), ELFFMapLevel::Suites }, { UpperDeckTabLabel.Get(), ELFFMapLevel::UpperDeck } };
    for (const TPair<UTextBlock*, ELFFMapLevel>& Tab : TabLabels)
    {
        if (Tab.Key != nullptr)
        {
            Tab.Key->SetText(LevelName(Tab.Value));
        }
    }
}

void ULFFMapPanel::BindMapButton(UButton* Button, const ULFFMapButtonHandler::EKind Kind, const int32 Index)
{
    if (Button == nullptr)
    {
        return;
    }
    ULFFMapButtonHandler* Handler = NewObject<ULFFMapButtonHandler>(this);
    Handler->Kind = Kind;
    Handler->Index = Index;
    Handler->Panel = this;
    Button->OnClicked.AddDynamic(Handler, &ULFFMapButtonHandler::HandleClicked);
    ButtonHandlers.Add(Handler);
}

void ULFFMapPanel::NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot)
{
    const bool bExplore = Snapshot.Mode == ELFFSimulationMode::FreeExplore;
    const bool bModal = Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen || Snapshot.bPaused;
    const bool bCanUse = bExplore && !bModal;
    const ULFFJourneySubsystem* Journey = GetJourney();
    if (LevelsToggleButton != nullptr)
    {
        LevelsToggleButton->SetIsEnabled(bCanUse);
    }

    // Level buttons: the picked one is the light chip, and the header says it in words.
    const TPair<UButton*, UTextBlock*> Tabs[] = {
        { FieldTab.Get(), FieldTabLabel.Get() }, { LowerConcourseTab.Get(), LowerConcourseTabLabel.Get() }, { ClubTab.Get(), ClubTabLabel.Get() },
        { SuitesTab.Get(), SuitesTabLabel.Get() }, { UpperDeckTab.Get(), UpperDeckTabLabel.Get() } };
    for (int32 Index = 0; Index < LFFMapLevelCount; ++Index)
    {
        ApplyModeButton(Tabs[Index].Key, Tabs[Index].Value, static_cast<ELFFMapLevel>(Index) == Snapshot.MapLevel);
        if (Tabs[Index].Key != nullptr)
        {
            Tabs[Index].Key->SetIsEnabled(bCanUse);
        }
    }
    if (ViewingText != nullptr)
    {
        ViewingText->SetText(FText::Format(LOCTEXT("Viewing", "Viewing: {0}"), LevelName(Snapshot.MapLevel)));
    }

    // Place toggles: the name and On / Off in words, the On state filled.
    const TPair<UButton*, UTextBlock*> Toggles[] = {
        { SensoryToggle.Get(), SensoryLabel.Get() }, { QuietToggle.Get(), QuietLabel.Get() }, { GuestToggle.Get(), GuestLabel.Get() },
        { RestroomToggle.Get(), RestroomLabel.Get() }, { FoodToggle.Get(), FoodLabel.Get() } };
    int32 AvailableMask = 0;
    for (int32 Index = 0; Index < LFFAmenityCount; ++Index)
    {
        const ELFFAmenity Kind = static_cast<ELFFAmenity>(Index);
        const bool bMapped = Journey != nullptr && Journey->GetAmenities().ContainsByPredicate([Kind](const FLFFAmenityMarker& Place)
            { return Place.Category == Kind; });
        AvailableMask |= bMapped ? 1 << Index : 0;
        const bool bOn = bMapped && (Snapshot.AmenityMask & (1 << Index)) != 0;
        ApplyToggleText(Toggles[Index].Value, AmenityName(Kind), !bMapped ? LOCTEXT("NotMapped", "Not mapped")
            : bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"));
        ApplyModeButton(Toggles[Index].Key, Toggles[Index].Value, bOn);
        if (Toggles[Index].Key != nullptr)
        {
            Toggles[Index].Key->SetIsEnabled(bCanUse && bMapped);
            Toggles[Index].Key->SetToolTipText(bMapped ? FText::Format(LOCTEXT("ShowKind", "Show or hide {0} across all levels"), AmenityName(Kind))
                : LOCTEXT("NotMappedTip", "No location data for this category is included. This does not mean the stadium has none."));
        }
    }
    if (HideAllButton != nullptr)
    {
        HideAllButton->SetIsEnabled(bCanUse && (Snapshot.AmenityMask & AvailableMask) != 0);
    }

    // The picked place's card (the seat included), or the hint on how to pick one.
    FLFFAmenityMarker Picked;
    const bool bCard = Journey != nullptr && Journey->GetAmenity(Snapshot.SelectedAmenityId, Picked);
    PickedAmenityId = bCard ? Picked.AmenityId : NAME_None;
    if (PoiCard != nullptr)
    {
        PoiCard->SetVisibility(bCard ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (PickHint != nullptr)
    {
        PickHint->SetVisibility(bCard ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (bCard)
    {
        if (PoiTitle != nullptr) { PoiTitle->SetText(Picked.Label); }
        if (PoiNote != nullptr) { PoiNote->SetText(FText::Format(LOCTEXT("PickedWhere", "{0} · {1}"), LevelName(Picked.Level), Picked.Note)); }
    }
    if (ShowMeButton != nullptr)
    {
        ShowMeButton->SetIsEnabled(bCanUse && bCard);
    }
}

void ULFFMapPanel::HandleMapButton(const ULFFMapButtonHandler::EKind Kind, const int32 Index)
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        switch (Kind)
        {
        case ULFFMapButtonHandler::EKind::Level: Journey->SetMapLevel(static_cast<ELFFMapLevel>(Index)); break;
        case ULFFMapButtonHandler::EKind::Amenity: Journey->ToggleAmenity(static_cast<ELFFAmenity>(Index)); break;
        }
    }
    ReturnFocusToGame();
}

void ULFFMapPanel::HandleHideAllClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney()) { Journey->HideAllAmenities(); }
    ReturnFocusToGame();
}

void ULFFMapPanel::HandleShowMeClicked()
{
    if (ULFFJourneySubsystem* Journey = GetJourney(); Journey != nullptr && !PickedAmenityId.IsNone())
    {
        Journey->FocusAmenity(PickedAmenityId);
    }
    ReturnFocusToGame();
}

void ULFFMapPanel::HandleLevelsClicked()
{
    bLevelsOpen = !bLevelsOpen;
    if (LevelsGroup != nullptr)
    {
        LevelsGroup->SetVisibility(bLevelsOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (LevelsToggleLabel != nullptr)
    {
        LevelsToggleLabel->SetText(bLevelsOpen ? LOCTEXT("CloseLevels", "Close levels") : LOCTEXT("ChangeLevel", "Change level"));
    }
    ReturnFocusToGame();
}

#undef LOCTEXT_NAMESPACE
