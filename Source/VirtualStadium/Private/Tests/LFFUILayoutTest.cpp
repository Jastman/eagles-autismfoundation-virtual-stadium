#include "UI/LFFUILayout.h"
#include "UI/LFFHUDWidget.h"
#include "UI/LFFJourneyPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    void ArrangeTimelineWidgets(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry,
        TMap<const SWidget*, FGeometry>& Geometries, const double Time, const int32 Depth = 0)
    {
        if (Depth > 80)
        {
            return;
        }
        Widget->Tick(Geometry, Time, 1.0f / 60.0f);
        Geometries.Add(&Widget.Get(), Geometry);
        FArrangedChildren Children(EVisibility::Visible);
        Widget->ArrangeChildren(Geometry, Children, true);
        for (int32 Index = 0; Index < Children.Num(); ++Index)
        {
            ArrangeTimelineWidgets(Children[Index].Widget, Children[Index].Geometry, Geometries, Time, Depth + 1);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFDisplayLayoutTest, "LFF.UI.DisplayLayout.SafeBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFDisplayLayoutTest::RunTest(const FString& Parameters)
{
    const FVector2D Viewports[] = {
        FVector2D(3584.0, 1280.0),
        FVector2D(3584.0, 1280.0) / (1280.0 / 1080.0),
        FVector2D(1920.0, 1080.0),
        FVector2D(1280.0, 720.0) / (720.0 / 1080.0)
    };
    for (const FVector2D& Viewport : Viewports)
    {
        for (const bool bLargeText : { false, true })
        {
            const LFFUILayout::FCornerPanels Layout = LFFUILayout::CornerPanels(Viewport, bLargeText);
            const FString Context = FString::Printf(TEXT("%.0fx%.0f large=%d"), Viewport.X, Viewport.Y, bLargeText);
            TestEqual(Context + TEXT(": identity panel stays compact"), Layout.InformationWidth, bLargeText ? 600.0f : 480.0f);
            TestEqual(Context + TEXT(": dock never stretches across the panorama"), Layout.ControlWidth, 540.0f);
            TestTrue(Context + TEXT(": caption fits its panel"),
                Layout.CaptionWidth * (bLargeText ? 1.25f : 1.0f) <= Layout.InformationWidth - 40.0f + 0.01f);

            const int32 Columns = LFFUILayout::TimelineColumns(Layout.CaptionWidth, 128.0f, 6.0f);
            TestTrue(Context + TEXT(": day timeline has no off-screen first column"),
                Columns >= 1 && Columns * 128.0f + (Columns - 1) * 6.0f <= Layout.CaptionWidth + 0.01f);

            const FBox2D Information(FVector2D(24.0, 24.0), FVector2D(24.0 + Layout.InformationWidth, 24.0 + Layout.MaximumHeight));
            const FBox2D Controls(FVector2D(Viewport.X - 24.0 - Layout.ControlWidth, 24.0), Viewport - FVector2D(24.0, 88.0));
            const FBox2D Phone = LFFUILayout::TicketBounds(Viewport, Information, Controls, FVector2D(240.0, 480.0));
            TestTrue(Context + TEXT(": ticket has a viewport slot"), Phone.bIsValid);
            if (Phone.bIsValid)
            {
                TestTrue(Context + TEXT(": ticket is right of the narrative and viewport centre"),
                    Phone.Min.X >= Information.Max.X + 24.0 && Phone.Min.X >= Viewport.X * 0.5);
                TestTrue(Context + TEXT(": ticket clears the right dock"), Phone.Max.X <= Controls.Min.X - 24.0);
                TestTrue(Context + TEXT(": ticket stays within safe insets"),
                    Phone.Min.Y >= 24.0 && Phone.Max.Y <= Viewport.Y - 24.0 && Phone.Max.X <= Viewport.X - 24.0);
                TestTrue(Context + TEXT(": ticket aspect ratio is preserved"),
                    FMath::IsNearlyEqual((Phone.Max.X - Phone.Min.X) / (Phone.Max.Y - Phone.Min.Y), 0.5, 0.0001));
            }
        }
    }

    TestEqual(TEXT("A narrow timeline drops to one column"), LFFUILayout::TimelineColumns(100.0f, 128.0f, 6.0f), 1);
    const FBox2D NarrowPhone = LFFUILayout::TicketBounds(FVector2D(1000.0, 800.0),
        FBox2D(FVector2D(24.0, 24.0), FVector2D(454.0, 400.0)),
        FBox2D(FVector2D(546.0, 24.0), FVector2D(976.0, 350.0)), FVector2D(240.0, 480.0));
    TestTrue(TEXT("A tight lane uses safe space below the dock"), NarrowPhone.bIsValid && NarrowPhone.Min.Y >= 374.0);
    TestFalse(TEXT("Invalid viewport yields no overflowing slot"),
        LFFUILayout::TicketBounds(FVector2D::ZeroVector, FBox2D(ForceInit), FBox2D(ForceInit), FVector2D(240.0, 480.0)).bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFResponsiveResizeTest, "LFF.UI.DisplayLayout.ResponsiveResize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFResponsiveResizeTest::RunTest(const FString& Parameters)
{
    struct FSample
    {
        FVector2D Viewport;
        LFFUILayout::FCornerPanels Layout;
        FBox2D Information;
        FBox2D Controls;
        FBox2D Phone;
    };
    const UUserInterfaceSettings* Settings = GetDefault<UUserInterfaceSettings>();
    auto Measure = [Settings](const FIntPoint Pixels, const bool bLargeText)
    {
        FSample Sample;
        const float DPI = Settings->GetDPIScaleBasedOnSize(Pixels);
        Sample.Viewport = FVector2D(Pixels.X, Pixels.Y) / FMath::Max(DPI, 0.01f);
        Sample.Layout = LFFUILayout::CornerPanels(Sample.Viewport, bLargeText);
        const double Inset = Sample.Layout.EdgeInset;
        Sample.Information = FBox2D(FVector2D(Inset, Inset),
            FVector2D(Inset + Sample.Layout.InformationWidth, Sample.Viewport.Y - Inset));
        Sample.Controls = FBox2D(FVector2D(Sample.Viewport.X - Inset - Sample.Layout.ControlWidth, Inset),
            FVector2D(Sample.Viewport.X - Inset, FMath::Min(480.0, Sample.Viewport.Y - Inset)));
        Sample.Phone = LFFUILayout::TicketBounds(Sample.Viewport, Sample.Information, Sample.Controls, FVector2D(240.0, 480.0));
        return Sample;
    };
    auto IsSafe = [](const FSample& Sample)
    {
        const double Inset = Sample.Layout.EdgeInset;
        const int32 Columns = LFFUILayout::TimelineColumns(Sample.Layout.CaptionWidth, 128.0f, 6.0f);
        const float MarkWidth = LFFUILayout::TimelineMarkWidth(Sample.Layout.CaptionWidth, 128.0f, 6.0f);
        const float RowWidth = Columns * MarkWidth + (Columns - 1) * 6.0f;
        return Sample.Phone.bIsValid
            && Sample.Phone.Min.X >= Sample.Information.Max.X + Inset - 0.01
            && Sample.Phone.Min.X >= Sample.Viewport.X * 0.5 - 0.01
            && Sample.Phone.Min.Y >= Inset - 0.01
            && Sample.Phone.Max.X <= Sample.Viewport.X - Inset + 0.01
            && Sample.Phone.Max.Y <= Sample.Viewport.Y - Inset + 0.01
            && (Sample.Phone.Max.X <= Sample.Controls.Min.X - Inset + 0.01
                || Sample.Phone.Min.Y >= Sample.Controls.Max.Y + Inset - 0.01)
            && FMath::IsNearlyEqual((Sample.Phone.Max.X - Sample.Phone.Min.X) / (Sample.Phone.Max.Y - Sample.Phone.Min.Y), 0.5, 0.0001)
            && RowWidth <= Sample.Layout.CaptionWidth + 0.01f
            && RowWidth >= Sample.Layout.CaptionWidth - 0.02f;
    };
    const FIntPoint Displays[] = {
        FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3584, 1280),
        FIntPoint(2240, 1080), FIntPoint(2800, 1200), FIntPoint(3200, 1280)
    };
    for (const bool bLargeText : { false, true })
    {
        for (const FIntPoint Display : Displays)
        {
            TestTrue(FString::Printf(TEXT("%dx%d large=%d: configured shortest-side DPI leaves all UI within safe bounds"),
                Display.X, Display.Y, bLargeText), IsSafe(Measure(Display, bLargeText)));
        }
        for (const bool bHeightResize : { false, true })
        {
            for (const int32 Direction : { 1, -1 })
            {
                bool bSafe = true;
                bool bContinuous = true;
                FSample Previous;
                const int32 Count = bHeightResize ? 90 : 288;
                for (int32 Index = 0; Index <= Count; ++Index)
                {
                    const int32 Offset = (Direction > 0 ? Index : Count - Index) * 8;
                    const FIntPoint Pixels = bHeightResize ? FIntPoint(1920, 720 + Offset) : FIntPoint(1280 + Offset, 1280);
                    const FSample Current = Measure(Pixels, bLargeText);
                    bSafe &= IsSafe(Current);
                    if (Index > 0)
                    {
                        const double Delta = (Current.Viewport - Previous.Viewport).Size();
                        const double Limit = Delta * 4.0 + 0.05;
                        bContinuous &= FMath::Abs(Current.Layout.InformationWidth - Previous.Layout.InformationWidth) <= Delta * 0.43 + 0.05
                            && FMath::Abs(Current.Layout.ControlWidth - Previous.Layout.ControlWidth) <= Delta * 0.43 + 0.05
                            && FMath::Abs(Current.Layout.EdgeInset - Previous.Layout.EdgeInset) <= Delta / 45.0 + 0.05
                            && FMath::Abs(LFFUILayout::WelcomeBottomInset(static_cast<float>(Current.Viewport.X))
                                - LFFUILayout::WelcomeBottomInset(static_cast<float>(Previous.Viewport.X))) <= Delta * 0.48 + 0.05
                            && (Current.Phone.Min - Previous.Phone.Min).GetAbsMax() <= Limit
                            && (Current.Phone.Max - Previous.Phone.Max).GetAbsMax() <= Limit;
                        const int32 Columns = LFFUILayout::TimelineColumns(Current.Layout.CaptionWidth, 128.0f, 6.0f);
                        if (Columns == LFFUILayout::TimelineColumns(Previous.Layout.CaptionWidth, 128.0f, 6.0f))
                        {
                            const float WidthChange = FMath::Abs(LFFUILayout::TimelineMarkWidth(Current.Layout.CaptionWidth, 128.0f, 6.0f)
                                - LFFUILayout::TimelineMarkWidth(Previous.Layout.CaptionWidth, 128.0f, 6.0f));
                            bContinuous &= WidthChange <= FMath::Abs(Current.Layout.CaptionWidth - Previous.Layout.CaptionWidth) / Columns + 0.01f;
                        }
                    }
                    Previous = Current;
                }
                const FString Context = FString::Printf(TEXT("large=%d resize-height=%d direction=%d"), bLargeText, bHeightResize, Direction);
                TestTrue(Context + TEXT(": every live-resize sample stays safe"), bSafe);
                TestTrue(Context + TEXT(": widths, spacing and phone position/scale vary continuously"), bContinuous);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFButtonFeedbackBindingTest, "LFF.UI.ButtonFeedback.SingleBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFButtonFeedbackBindingTest::RunTest(const FString& Parameters)
{
    UButton* Button = NewObject<UButton>();
    FButtonStyle Style = Button->GetStyle();
    USoundWave* SlateSound = NewObject<USoundWave>();
    Style.PressedSlateSound.SetResourceObject(SlateSound);
    Style.HoveredSlateSound.SetResourceObject(SlateSound);
    Button->SetStyle(Style);
    ULFFJourneyPanel* Panel = GetMutableDefault<ULFFJourneyPanel>();
    Panel->RegisterButtonFeedback(Button);
    Panel->RegisterButtonFeedback(Button);
    TestEqual(TEXT("Construct/reconstruct cannot duplicate the mouse/keyboard feedback delegate"),
        Button->OnClicked.GetAllObjects().Num(), 1);
    TestNull(TEXT("Default Slate click sound cannot bypass Sound off"), Button->GetStyle().PressedSlateSound.GetResourceObject());
    TestNull(TEXT("Hover is silent"), Button->GetStyle().HoveredSlateSound.GetResourceObject());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFTimelineWidgetGeometryTest, "LFF.UI.DisplayLayout.TimelineWidgetGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFTimelineWidgetGeometryTest::RunTest(const FString& Parameters)
{
    UClass* PanelClass = LoadClass<ULFFJourneyPanel>(nullptr, TEXT("/Game/LFF/UI/WBP_LFF_Narrative.WBP_LFF_Narrative_C"));
    UDataTable* Steps = LoadObject<UDataTable>(nullptr, TEXT("/Game/LFF/Data/DT_LFF_GoldenPath.DT_LFF_GoldenPath"));
    if (!TestNotNull(TEXT("The actual shipped narrative is available"), PanelClass)
        || !TestNotNull(TEXT("The actual shipped timeline data is available"), Steps))
    {
        return false;
    }
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    ULFFJourneyPanel* Panel = CreateWidget<ULFFJourneyPanel>(Instance, PanelClass);
    bool bPassed = TestNotNull(TEXT("The shipped narrative instantiates"), Panel);
    if (Panel != nullptr)
    {
        TSharedPtr<SWidget> SlateContent = Panel->TakeWidget();
        USizeBox* CaptionWidth = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("CaptionWidth")));
        UScrollBox* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("CaptionScroll")));
        UTextBlock* Note = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("TimelineNoteText")));
        bPassed &= TestNotNull(TEXT("The real caption width is bound"), CaptionWidth);
        bPassed &= TestNotNull(TEXT("The real clipping scroll viewport is bound"), Scroll);
        bPassed &= TestNotNull(TEXT("The real timeline annotations are bound"), Note);
        if (CaptionWidth != nullptr && Scroll != nullptr && Note != nullptr)
        {
            Scroll->SetAlwaysShowScrollbar(true);
            FLFFJourneySnapshot Snapshot;
            Snapshot.bStarted = true;
            Snapshot.StepIndex = 2;
            Snapshot.StepCount = 23;
            Snapshot.Title = FText::FromString(TEXT("I will walk to the entrance and wait in line."));
            Snapshot.Body = FText::FromString(TEXT("There may be a line. Waiting is normal."));
            TArray<FLFFJourneyStep*> Rows;
            Steps->GetAllRows<FLFFJourneyStep>(TEXT("Timeline geometry regression"), Rows);
            for (int32 Index = 0; Index < Rows.Num(); ++Index)
            {
                if (!Rows[Index]->TimelineLabel.IsEmpty())
                {
                    FLFFTimelineMark Mark;
                    Mark.StepIndex = Index;
                    Mark.Label = Rows[Index]->TimelineLabel;
                    Snapshot.TimelineMarks.Add(Mark);
                }
            }
            const FIntPoint Displays[] = { FIntPoint(1280, 720), FIntPoint(1920, 1080),
                FIntPoint(2560, 1440), FIntPoint(3584, 1280) };
            for (const bool bLargeText : { false, true })
            {
                Snapshot.Settings.bLargeText = bLargeText;
                Panel->ApplySnapshot(Snapshot);
                Panel->SetTimelineOpen(true);
                for (const FIntPoint Display : Displays)
                {
                    const float DPI = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Display);
                    const FVector2D Viewport = FVector2D(Display.X, Display.Y) / DPI;
                    const LFFUILayout::FCornerPanels Layout = LFFUILayout::CornerPanels(Viewport, bLargeText);
                    CaptionWidth->SetWidthOverride(Layout.CaptionWidth);
                    Panel->LayoutTimeline(Layout.CaptionWidth);
                    const UScaleBoxSlot* CaptionSlot = Cast<UScaleBoxSlot>(CaptionWidth->Slot);
                    const UWidget* Scale = Panel->GetWidgetFromName(TEXT("TextScale"));
                    const UScrollBoxSlot* ScaleSlot = Scale != nullptr ? Cast<UScrollBoxSlot>(Scale->Slot) : nullptr;
                    bPassed &= TestTrue(TEXT("The actual saved centred caption slot is repaired"),
                        CaptionSlot != nullptr && CaptionSlot->GetHorizontalAlignment() == HAlign_Fill);
                    bPassed &= TestTrue(TEXT("The actual saved scroll child fits the scrollbar-narrowed content"),
                        ScaleSlot != nullptr && ScaleSlot->GetHorizontalAlignment() == HAlign_Fill);
                    const FGeometry Root = FGeometry::MakeRoot(FVector2D(Layout.InformationWidth, Layout.NarrativeHeight),
                        FSlateLayoutTransform(DPI));
                    TMap<const SWidget*, FGeometry> Geometries;
                    for (int32 Pass = 0; Pass < 4; ++Pass)
                    {
                        SlateContent->SlatePrepass(DPI);
                        Geometries.Reset();
                        ArrangeTimelineWidgets(SlateContent.ToSharedRef(), Root, Geometries, FPlatformTime::Seconds() + Pass / 60.0);
                    }
                    const FGeometry* ScrollGeometry = Geometries.Find(Scroll->GetCachedWidget().Get());
                    const FString Context = FString::Printf(TEXT("%dx%d large=%d"), Display.X, Display.Y, bLargeText);
                    bPassed &= TestNotNull(Context + TEXT(": actual scroll viewport was arranged"), ScrollGeometry);
                    if (ScrollGeometry != nullptr)
                    {
                        bool bCheckedNote = false;
                        Panel->WidgetTree->ForEachWidget([this, &Geometries, ScrollGeometry, Note, &bCheckedNote, &bPassed, &Context](UWidget* Widget)
                        {
                            const UTextBlock* Text = Cast<UTextBlock>(Widget);
                            if (Text == nullptr)
                            {
                                return;
                            }
                            bool bTimeline = false;
                            for (const UWidget* Parent = Text; Parent != nullptr; Parent = Parent->GetParent())
                            {
                                bTimeline |= Parent->GetName() == TEXT("TimelinePanel");
                            }
                            if (!bTimeline)
                            {
                                return;
                            }
                            const FGeometry* Geometry = Geometries.Find(Text->GetCachedWidget().Get());
                            bPassed &= TestNotNull(Context + TEXT(": ") + Text->GetName() + TEXT(" arranged"), Geometry);
                            if (Geometry != nullptr)
                            {
                                const FVector2D Min = ScrollGeometry->AbsoluteToLocal(Geometry->GetAbsolutePosition());
                                const FVector2D Max = ScrollGeometry->AbsoluteToLocal(Geometry->GetAbsolutePosition() + Geometry->GetAbsoluteSize());
                                bPassed &= TestTrue(Context + TEXT(": ") + Text->GetName() + TEXT(" stays inside the actual scroll width"),
                                    Min.X >= -0.01 && Max.X <= ScrollGeometry->GetLocalSize().X + 0.01);
                                bCheckedNote |= Text == Note;
                            }
                        });
                        bPassed &= TestTrue(Context + TEXT(": both annotation lines were checked"), bCheckedNote);
                    }
                }
            }
        }
        SlateContent.Reset();
        Panel->ReleaseSlateResources(true);
    }
    Instance->Shutdown();
    if (World != nullptr)
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFHighContrastHUDTest, "LFF.UI.DisplayLayout.HighContrastTour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFHighContrastHUDTest::RunTest(const FString& Parameters)
{
    UClass* HUDClass = LoadClass<ULFFHUDWidget>(nullptr, TEXT("/Game/LFF/UI/WBP_LFF_HUD.WBP_LFF_HUD_C"));
    if (!TestNotNull(TEXT("The shipped native UMG HUD is available"), HUDClass))
    {
        return false;
    }
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    ULFFHUDWidget* HUD = CreateWidget<ULFFHUDWidget>(Instance, HUDClass);
    bool bPassed = TestNotNull(TEXT("The shipped HUD instantiates"), HUD);
    if (HUD != nullptr)
    {
        TSharedPtr<SWidget> SlateWidget = HUD->TakeWidget();
        UFunction* ApplySnapshot = HUD->FindFunction(TEXT("HandleJourneyChanged"));
        bPassed &= TestNotNull(TEXT("The HUD's real snapshot entry point is available"), ApplySnapshot);
        if (ApplySnapshot != nullptr)
        {
            FLFFJourneySnapshot Snapshot;
            Snapshot.bStarted = true;
            Snapshot.StepIndex = 6;
            Snapshot.StepCount = 23;
            Snapshot.Mode = ELFFSimulationMode::GuidedTour;
            Snapshot.Title = FText::FromString(TEXT("Ticket step"));
            FLFFTimelineMark TimelineMark;
            TimelineMark.StepIndex = 6;
            TimelineMark.Label = FText::FromString(TEXT("11:30 AM\nInner gates"));
            Snapshot.TimelineMarks.Add(TimelineMark);
            for (const bool bHighContrast : { false, true, false })
            {
                Snapshot.Settings.bHighContrast = bHighContrast;
                HUD->ProcessEvent(ApplySnapshot, &Snapshot);
                const UWidget* Settings = HUD->GetWidgetFromName(TEXT("Settings"));
                bPassed &= TestTrue(TEXT("High contrast with Settings closed keeps the modal/scrim collapsed"),
                    Settings != nullptr && !Settings->IsVisible());
                HUD->WidgetTree->ForEachWidgetAndDescendants([this, HUD, &bPassed](UWidget* Widget)
                {
                    const UBorder* Surface = Cast<UBorder>(Widget);
                    if (Surface == nullptr || !Surface->GetName().StartsWith(TEXT("Surface")) || Surface->GetBrushColor().A < 0.99f)
                    {
                        return;
                    }
                    bool bVisible = true;
                    bool bCornerPanel = false;
                    for (const UWidget* Ancestor = Surface; Ancestor != nullptr && Ancestor != HUD;)
                    {
                        bVisible &= Ancestor->IsVisible();
                        bCornerPanel |= Ancestor->GetName() == TEXT("InformationSize") || Ancestor->GetName() == TEXT("ControlSize");
                        const UWidget* Parent = Ancestor->GetParent();
                        Ancestor = Parent != nullptr ? Parent : Ancestor->GetTypedOuter<UUserWidget>();
                    }
                    bPassed &= TestTrue(FString::Printf(TEXT("%s: no opaque full-viewport tour surface"), *Surface->GetName()),
                        !bVisible || bCornerPanel);
                });
            }
            ULFFJourneyPanel* Narrative = Cast<ULFFJourneyPanel>(HUD->GetWidgetFromName(TEXT("Narrative")));
            const UWidget* Timeline = Narrative != nullptr ? Narrative->GetWidgetFromName(TEXT("TimelinePanel")) : nullptr;
            bPassed &= TestNotNull(TEXT("The shipped narrative has its real day timeline"), Timeline);
            if (Timeline != nullptr)
            {
                HUD->SetDayTimelineOpen(true);
                HUD->SetDayTimelineOpen(true);
                bPassed &= TestTrue(TEXT("Capture hook opens idempotently rather than toggling closed"), Timeline->IsVisible());
                HUD->SetDayTimelineOpen(false);
                bPassed &= TestFalse(TEXT("Capture hook closes the actual timeline"), Timeline->IsVisible());
            }
        }
        SlateWidget.Reset();
        HUD->ReleaseSlateResources(true);
    }
    Instance->Shutdown();
    if (World != nullptr)
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    }
    return bPassed;
}

#endif
