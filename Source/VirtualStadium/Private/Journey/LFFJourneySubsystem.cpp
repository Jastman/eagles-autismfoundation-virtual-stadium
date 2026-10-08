#include "Journey/LFFJourneySubsystem.h"

#include "Camera/LFFCameraPath.h"
#include "Camera/LFFCameraRig.h"
#include "Camera/LFFCameraTarget.h"
#include "Algo/BinarySearch.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Core/LFFGameMode.h"
#include "CollisionQueryParams.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "VirtualStadium.h"
#include "World/LFFCueActor.h"
#include "World/LFFCrowdFanActor.h"
#include "World/LFFFireworksActor.h"
#include "MassSpawner.h"

#define LOCTEXT_NAMESPACE "LFFJourney"

const FName ULFFJourneySubsystem::MySeatAmenityId(TEXT("my-seat"));

namespace
{
    const FName LabelTag(TEXT("LFF_Label"));
    /** Stand-in people: shown with Crowd Low, hidden with Crowd Off or Calm Mode. */
    const FName CrowdTag(TEXT("LFF_Crowd"));
    const FName SiteRootTag(TEXT("LFF_SiteRoot"));
    /** Seen only from outside the stadium (the parking lot's cars): hidden while the camera is inside. */
    const FName ExteriorTag(TEXT("LFF_Exterior"));
    /** Actors whose box components together make up the stadium's inside. */
    const FName InteriorTag(TEXT("LFF_StadiumInterior"));
    constexpr float ExteriorCheckSeconds = 0.2f;
    /** The ambient bed dips under the preview sample so the sample stays the only new sound. */
    constexpr float BedDuckWhilePreview = 0.35f;
    /** Camera "size" for the wall check, so a move can't graze a surface either. */
    constexpr float CameraProbeRadius = 40.0f;
    /** Free explore's own camera moves (My seat) use an id the journey never issues, so their finish is ignored and the
     *  tour's place and phase stay as they were. The state machine's ids start at 1 (Load). */
    constexpr uint32 ExploreMoveId = 0;

    /** Where World lands in the frame seen from View (hfov in degrees). False if it's behind the camera. */
    bool ProjectToView(const FTransform& View, const float HorizontalFov, const float Aspect, const FVector& World, FVector2D& OutFraction)
    {
        const FVector Local = View.InverseTransformPositionNoScale(World);
        if (Local.X <= 1.0f)
        {
            return false;
        }

        const float TanHalf = FMath::Tan(FMath::DegreesToRadians(HorizontalFov * 0.5f));
        const float NdcX = Local.Y / (Local.X * TanHalf);
        const float NdcY = Local.Z / (Local.X * TanHalf / FMath::Max(Aspect, 0.1f));
        OutFraction = FVector2D(0.5f + 0.5f * NdcX, 0.5f - 0.5f * NdcY);
        return true;
    }

    void SetPrimaryVolume(const UWorld* World, const float Volume)
    {
        if (World != nullptr)
        {
            if (FAudioDeviceHandle Device = World->GetAudioDevice())
            {
                Device->SetTransientPrimaryVolume(Volume);
            }
        }
    }
}

void ULFFJourneySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    if (!InWorld.IsGameWorld())
    {
        return;
    }

    GameMode = InWorld.GetAuthGameMode<ALFFGameMode>();
    if (GameMode == nullptr)
    {
        return;
    }

    // Wait one tick so every actor (targets, labels, player controller, HUD) has begun play.
    InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::InitializeJourney));
}

void ULFFJourneySubsystem::Deinitialize()
{
    StopVoice(false);
    StopAudio();
    // The editor shares the audio device with PIE; never leave it muted.
    if (GameMode != nullptr)
    {
        SetPrimaryVolume(GetWorld(), 1.0f);
    }
    Super::Deinitialize();
}

void ULFFJourneySubsystem::InitializeJourney()
{
    if (GameMode == nullptr)
    {
        return;
    }

    Settings = GameMode->DefaultVisitorSettings;

    TArray<FLFFJourneyStep> Steps;
    bJourneyMissing = !LoadSteps(Steps);
    Machine.Load(Steps);
    Machine.SetTourSteps(GameMode->TourStepFirstSteps);
    RefreshSiteFrame();
    LoadAmenities();

    TimelineMarks.Reset();
    FirstCueIndex.Reset();
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        if (!Steps[Index].TimelineLabel.IsEmpty())
        {
            FLFFTimelineMark Mark;
            Mark.StepIndex = Index;
            Mark.Label = Steps[Index].TimelineLabel;
            TimelineMarks.Add(Mark);
        }
        for (const FName& Cue : Steps[Index].SceneCues)
        {
            if (!FirstCueIndex.Contains(Cue))
            {
                FirstCueIndex.Add(Cue, Index);
            }
        }
    }

    CueActors.Reset();
    for (TActorIterator<ALFFCueActor> It(GetWorld()); It; ++It)
    {
        CueActors.Add(*It);
    }
    for (const TPair<FName, int32>& Cue : FirstCueIndex)
    {
        const int32 Count = CueActors.FilterByPredicate([&Cue](const ALFFCueActor* Actor) { return Actor != nullptr && Actor->CueId == Cue.Key; }).Num();
        UE_LOG(LogVirtualStadium, Log, TEXT("Scene cue %s: step %d, %d actors."), *Cue.Key.ToString(), Cue.Value + 1, Count);
        if (Count == 0)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Scene cue %s has no ALFFCueActor in the level; that step shows no movement."), *Cue.Key.ToString());
        }
    }

    Rig = FindOrSpawnRig();
    if (Rig != nullptr)
    {
        if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
        {
            PlayerController->SetViewTarget(Rig.Get());
        }
    }

    ExteriorActors.Reset();
    InteriorBoxes.Reset();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(ExteriorTag))
        {
            ExteriorActors.Add(*It);
        }
        if (It->ActorHasTag(InteriorTag))
        {
            TInlineComponentArray<UBoxComponent*> Boxes(*It);
            InteriorBoxes.Append(Boxes);
        }
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Exterior: %d actors hidden while the camera is inside %d stadium boxes."), ExteriorActors.Num(), InteriorBoxes.Num());
    if (ExteriorActors.Num() > 0 && InteriorBoxes.Num() > 0)
    {
        GetWorld()->GetTimerManager().SetTimer(ExteriorTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::UpdateExteriorVisibility),
            ExteriorCheckSeconds, true);
    }

    // The splash starts on the first step's wide shot and slowly orbits from there (still with less movement).
    SnapCameraToCurrentStep();
    ApplyLabels();
    ApplySensory();
    ApplyCues(ELFFCueReason::StepChanged);
    EnterSplash(false);
    Broadcast();
}

bool ULFFJourneySubsystem::LoadSteps(TArray<FLFFJourneyStep>& OutSteps) const
{
    UDataTable* Table = GameMode->JourneyTable.LoadSynchronous();
    if (Table == nullptr)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Journey table not found: %s"), *GameMode->JourneyTable.ToString());
        return false;
    }

    if (Table->GetRowStruct() != FLFFJourneyStep::StaticStruct())
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Journey table %s does not use FLFFJourneyStep rows."), *Table->GetPathName());
        return false;
    }

    static const FString Context(TEXT("LFFJourneySubsystem"));
    for (const FName& RowName : Table->GetRowNames())
    {
        if (const FLFFJourneyStep* Row = Table->FindRow<FLFFJourneyStep>(RowName, Context))
        {
            FLFFJourneyStep Step = *Row;
            Step.StepId = RowName;

            if (Step.DisclaimerLevel == ELFFDisclaimerLevel::Verified && Step.VerificationSource.IsEmpty())
            {
                UE_LOG(LogVirtualStadium, Warning, TEXT("Step %s is marked Verified with no VerificationSource; treating it as Illustrative."), *RowName.ToString());
                Step.DisclaimerLevel = ELFFDisclaimerLevel::Illustrative;
            }

            OutSteps.Add(Step);
        }
    }

    return OutSteps.Num() > 0;
}

ALFFCameraRig* ULFFJourneySubsystem::FindOrSpawnRig()
{
    UWorld* World = GetWorld();
    TActorIterator<ALFFCameraRig> Existing(World);
    if (Existing)
    {
        return *Existing;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<ALFFCameraRig>(ALFFCameraRig::StaticClass(), FTransform::Identity, SpawnParameters);
}

ALFFCameraTarget* ULFFJourneySubsystem::FindTarget(const FName TargetId) const
{
    ALFFCameraTarget* Found = nullptr;
    int32 Matches = 0;

    for (TActorIterator<ALFFCameraTarget> It(GetWorld()); It; ++It)
    {
        if (It->TargetId == TargetId)
        {
            if (Found == nullptr)
            {
                Found = *It;
            }
            ++Matches;
        }
    }

    if (Matches > 1)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("%d camera targets share TargetId %s; using %s."), Matches, *TargetId.ToString(), *GetNameSafe(Found));
    }

    return Found;
}

void ULFFJourneySubsystem::Next()
{
    if (IsModalOpen() || IsExploring())
    {
        return;
    }

    if (Machine.GetPhase() == ELFFJourneyPhase::Intro)
    {
        if (Machine.Start() == ELFFCommandResult::Accepted)
        {
            MoveCameraToFirstStep();
        }
        Broadcast();
        return;
    }

    if (Machine.IsAtEnd() && Machine.GetPhase() == ELFFJourneyPhase::AtStep && !Machine.IsPaused())
    {
        // "Start again" at the end is an explicit choice, so it needs no extra question.
        ConfirmRestart();
        return;
    }

    if (Machine.Next() == ELFFCommandResult::Accepted)
    {
        MoveCameraToCurrentStep(false);
    }

    Broadcast();
}

void ULFFJourneySubsystem::Back()
{
    if (IsModalOpen() || IsExploring())
    {
        return;
    }

    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (LFFJourneyRules::ShouldBackReturnToIntro(Machine.HasStarted(), Machine.GetIndex(), Machine.IsPaused(), Step != nullptr && Step->bBackEnabled))
    {
        // The first step is the orbit's wide shot, so the splash orbit picks up from right here.
        Machine.Restart();
        bArrived = false;
        ResetPreview();
        ApplyCues(ELFFCueReason::StepChanged);
        EnterSplash(false);
        Broadcast();
        return;
    }

    if (Machine.Back() == ELFFCommandResult::Accepted)
    {
        MoveCameraToCurrentStep(false);
    }

    Broadcast();
}

void ULFFJourneySubsystem::TogglePause()
{
    if (IsModalOpen())
    {
        return;
    }

    if (Machine.IsPaused())
    {
        Resume();
        return;
    }

    if (Machine.Pause() == ELFFCommandResult::Accepted && Rig != nullptr)
    {
        Rig->SetTransitionPaused(true);
    }

    // "Nothing will move until you resume": a running preview, the anthem flight and the voice stop too.
    StopVoice();
    StopAnthemSequence(true);
    ResetPreview();
    Broadcast();
}

void ULFFJourneySubsystem::Resume()
{
    const bool bWasPaused = Machine.IsPaused();
    Machine.Resume();
    if (Rig != nullptr)
    {
        Rig->SetTransitionPaused(false);
    }

    if (bWasPaused)
    {
        // The card is read again from the start.
        ScheduleVoice();
    }
    Broadcast();
}

void ULFFJourneySubsystem::RequestRestart()
{
    if (!Machine.HasStarted())
    {
        return;
    }

    // Start again lives in Settings, so the question takes the panel's place.
    bSettingsOpen = false;
    bRestartConfirmOpen = true;
    Broadcast();
}

void ULFFJourneySubsystem::ConfirmRestart()
{
    bRestartConfirmOpen = false;
    bSettingsOpen = false;

    StopVoice();
    ResetMap();
    if (Machine.Restart() == ELFFCommandResult::Accepted)
    {
        if (Rig != nullptr)
        {
            Rig->SetTransitionPaused(false);
        }

        // Back to the splash with a gentle fade; the orbit starts once the picture is back.
        bArrived = false;
        ResetPreview();
        ApplyLabels();
        ApplySensory();
        ApplyCues(ELFFCueReason::StepChanged);
        EnterSplash(true);
    }

    // Visitor settings are kept on restart so nobody loses reduced motion or Calm Mode mid-session.
    Broadcast();
}

void ULFFJourneySubsystem::Confirm()
{
    if (bRestartConfirmOpen)
    {
        ConfirmRestart();
        return;
    }

    if (bSettingsOpen)
    {
        return;
    }

    if (Machine.IsPaused())
    {
        Resume();
        return;
    }

    Next();
}

void ULFFJourneySubsystem::Cancel()
{
    if (bRestartConfirmOpen)
    {
        bRestartConfirmOpen = false;
        Broadcast();
        return;
    }

    if (bSettingsOpen)
    {
        bSettingsOpen = false;
        Broadcast();
        return;
    }

    if (Machine.IsPaused())
    {
        Resume();
    }
}

void ULFFJourneySubsystem::ToggleSettings()
{
    if (bRestartConfirmOpen)
    {
        return;
    }

    bSettingsOpen = !bSettingsOpen;
    Broadcast();
}

void ULFFJourneySubsystem::ToggleReducedMotion()
{
    if (Settings.bCalmMode)
    {
        return;
    }

    Settings.bReducedMotion = !Settings.bReducedMotion;
    ApplyCrowd();
    ApplyCues(ELFFCueReason::SettingsChanged);
    if (!Machine.HasStarted())
    {
        EnterSplash(false);
    }
    Broadcast();
}

void ULFFJourneySubsystem::ToggleSound()
{
    if (Settings.bCalmMode)
    {
        return;
    }

    Settings.bSoundEnabled = !Settings.bSoundEnabled;
    if (!Settings.bSoundEnabled && GetCurrentPreviewKind() == ELFFIntensityPreview::Sound)
    {
        // Otherwise the sample would start again by itself when Sound comes back on.
        bPreviewOn = false;
    }
    ApplySensory();
    if (Settings.IsSoundEffective())
    {
        ScheduleVoice();
    }
    Broadcast();
}

void ULFFJourneySubsystem::ToggleCalmMode()
{
    // The underlying Sound and Less movement choices are kept, so turning Calm Mode off restores them.
    Settings.bCalmMode = !Settings.bCalmMode;
    if (Settings.bCalmMode)
    {
        bPreviewOn = false;
    }
    ApplySensory();
    ApplyCues(ELFFCueReason::SettingsChanged);
    if (!Machine.HasStarted())
    {
        EnterSplash(false);
    }
    else if (!Settings.bCalmMode)
    {
        ScheduleVoice();
    }
    Broadcast();
}

void ULFFJourneySubsystem::SetExperienceMode(const bool bCalm)
{
    if (!bCalm)
    {
        // Standard is the whole day, whatever was toggled off in Settings before.
        Settings.bSoundEnabled = true;
        Settings.bVoiceEnabled = true;
        Settings.bReducedMotion = false;
        Settings.bCrowdEnabled = true;
    }
    if (Settings.bCalmMode != bCalm)
    {
        ToggleCalmMode();
        return;
    }
    ApplySensory();
    ApplyCues(ELFFCueReason::SettingsChanged);
    if (Machine.HasStarted() && !bCalm)
    {
        ScheduleVoice();
    }
    Broadcast();
}

void ULFFJourneySubsystem::JumpToStep(const int32 StepIndex)
{
    if (IsModalOpen())
    {
        return;
    }

    if (IsExploring())
    {
        // An arc node in Free explore goes back to the tour. The chapter you left resumes at your exact step, not its first.
        const bool bOwnChapter = Machine.TourStepOf(StepIndex) == Machine.GetBookmark().TourStep;
        ResumeTourAt(bOwnChapter ? INDEX_NONE : StepIndex);
        return;
    }

    if (Machine.JumpTo(StepIndex) == ELFFCommandResult::Accepted)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Timeline: jump to step %d."), StepIndex + 1);
        MoveCameraToCurrentStep(false);
    }

    Broadcast();
}

void ULFFJourneySubsystem::SwitchToMode(const ELFFSimulationMode NewMode)
{
    if (IsModalOpen())
    {
        return;
    }

    if (NewMode == ELFFSimulationMode::GuidedTour)
    {
        if (IsExploring())
        {
            ResumeTour();
        }
        else if (Machine.GetPhase() == ELFFJourneyPhase::Intro)
        {
            // Guided tour on the welcome screen is Start.
            Next();
        }
        return;
    }

    const ELFFCommandResult Result = Machine.SwitchToMode(NewMode);
    if (Result != ELFFCommandResult::Accepted)
    {
        return;
    }

    // Free explore starts from where you're standing: the camera doesn't move. Only the tour's own sound and motion
    // stop (the voice, a preview, the anthem flight, which cuts back to the seat). A move already in flight still lands.
    StopVoice();
    StopAnthemSequence(true);
    ResetPreview();
    if (Rig != nullptr)
    {
        Rig->StopOrbit();
    }
    if (!bMapOpened && GameMode != nullptr)
    {
        MapLevel = GameMode->MySeatLevel;
        bMapOpened = true;
    }
    RefreshSiteFrame();
    bLevelHighlight = false;
    FlyVelocity = FVector::ZeroVector;
    ApplyLevelHighlight();
    if (const FLFFJourneyStep* Step = Machine.GetCurrentStep())
    {
        ViewTargetId = Step->CameraTargetId;
    }
    const FLFFTourBookmark& Bookmark = Machine.GetBookmark();
    UE_LOG(LogVirtualStadium, Log, TEXT("Free explore: tour bookmarked at step %d (%s)%s."), Bookmark.StepIndex + 1,
        *UEnum::GetDisplayValueAsText(Bookmark.TourStep).ToString(), Bookmark.bTourStarted ? TEXT("") : TEXT(", not started yet"));
    Broadcast();
}

void ULFFJourneySubsystem::ResumeTour()
{
    ResumeTourAt(INDEX_NONE);
}

void ULFFJourneySubsystem::ResumeTourAt(const int32 StepIndex)
{
    if (IsModalOpen() || Machine.ResumeTour(StepIndex) != ELFFCommandResult::Accepted)
    {
        return;
    }
    bLevelHighlight = false;
    FlyVelocity = FVector::ZeroVector;
    ApplyLevelHighlight();

    if (Rig != nullptr)
    {
        Rig->StopOrbit();
    }
    // Always a fade-cut: Free explore may have left the camera anywhere, and a long sweep back across the stadium would
    // be a surprise. The fade says "back to the story". Calm Mode makes it a cut; the card is read again on arrival.
    UE_LOG(LogVirtualStadium, Log, TEXT("Resume tour at step %d (%s), fade-cut."), Machine.GetIndex() + 1,
        *UEnum::GetDisplayValueAsText(Machine.GetTourStep()).ToString());
    MoveCameraToCurrentStep(true);
    Broadcast();
}

void ULFFJourneySubsystem::GoToMySeat()
{
    if (!CanUseMap() || GameMode == nullptr)
    {
        return;
    }
    MapLevel = GameMode->MySeatLevel;
    ShowTarget(GameMode->MySeatTargetId);
    Broadcast();
}

void ULFFJourneySubsystem::ShowTarget(const FName CameraTargetId)
{
    bMapAutoFramed = false;
    if (!CanUseMap() || Rig == nullptr || GameMode == nullptr)
    {
        return;
    }

    const ALFFCameraTarget* Target = FindTarget(CameraTargetId);
    if (Target == nullptr)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Show me: camera target '%s' not found. The camera stays put."), *CameraTargetId.ToString());
        return;
    }

    // Always a fade-cut (a cut in Calm Mode): the spot can be anywhere in the stadium, and a long glide across it is a surprise.
    Rig->StopOrbit();
    FlyVelocity = FVector::ZeroVector;
    FlyDollyLeft = 0.0f;
    const ELFFTransitionProfile Profile = LFFJourneyRules::ResolveMove(ELFFTransitionProfile::ReducedMotion, Settings, false);
    UE_LOG(LogVirtualStadium, Log, TEXT("Show me: %s to %s."), *UEnum::GetDisplayValueAsText(Profile).ToString(), *CameraTargetId.ToString());
    Rig->BeginTransition(Target->GetCameraTransform(), Target->FieldOfView, Profile, GameMode->DefaultEaseSeconds,
        GameMode->ReducedMotionFadeSeconds, GameMode->FadeColor, ExploreMoveId);
    ViewTargetId = CameraTargetId;
    Broadcast();
}

void ULFFJourneySubsystem::SetMapLevel(const ELFFMapLevel Level)
{
    if (!CanUseMap())
    {
        return;
    }
    MapLevel = Level;
    bLevelHighlight = true;
    // Places show on every level (their kinds decide), so a pick stays picked.
    ApplyLevelHighlight();
    FlyToLevel(Level);
    Broadcast();
}

void ULFFJourneySubsystem::FlyToLevel(const ELFFMapLevel Level)
{
    bMapAutoFramed = false;
    if (Rig == nullptr || GameMode == nullptr || !GameMode->LevelViews.IsValidIndex(static_cast<int32>(Level)))
    {
        return;
    }
    const FTransform Local = LFFJourneyRules::LevelOverviewPose(GameMode->LevelViews[static_cast<int32>(Level)]);
    const FTransform World = Local * SiteFrame;
    FRotator Look = World.Rotator();
    Look.Roll = 0.0f;
    Rig->StopOrbit();
    FlyVelocity = FVector::ZeroVector;
    FlyDollyLeft = 0.0f;
    // The overview is a sweep you asked for, so it eases; less movement fades and cuts, Calm Mode cuts.
    const ELFFTransitionProfile Profile = LFFJourneyRules::ResolveMove(ELFFTransitionProfile::Ease, Settings, false);
    UE_LOG(LogVirtualStadium, Log, TEXT("Level %s: %s to its overview."), *UEnum::GetDisplayValueAsText(Level).ToString(),
        *UEnum::GetDisplayValueAsText(Profile).ToString());
    Rig->BeginTransition(FTransform(Look, World.GetLocation()), 60.0f, Profile, 2.5f, GameMode->ReducedMotionFadeSeconds,
        GameMode->FadeColor, ExploreMoveId);
    ViewTargetId = NAME_None;
}

void ULFFJourneySubsystem::ApplyLevelHighlight()
{
    if (Rig == nullptr || GameMode == nullptr)
    {
        return;
    }
    if (LevelHighlight == nullptr)
    {
        UMaterialInterface* Base = GameMode->LevelHighlightMaterial.LoadSynchronous();
        if (Base == nullptr)
        {
            if (!bLoggedHighlightMissing)
            {
                UE_LOG(LogVirtualStadium, Warning, TEXT("Level highlight material %s not found; levels are picked without a highlight."),
                    *GameMode->LevelHighlightMaterial.ToString());
                bLoggedHighlightMissing = true;
            }
            return;
        }
        LevelHighlight = UMaterialInstanceDynamic::Create(Base, this);
    }

    const int32 Index = static_cast<int32>(MapLevel);
    const bool bOn = IsExploring() && bLevelHighlight && GameMode->LevelViews.IsValidIndex(Index);
    if (bOn)
    {
        // The material measures each pixel in the site frame (it has no other idea where the stadium is).
        const FLFFLevelView& View = GameMode->LevelViews[Index];
        auto AsColor = [](const FVector& V) { return FLinearColor(static_cast<float>(V.X), static_cast<float>(V.Y), static_cast<float>(V.Z), 0.0f); };
        LevelHighlight->SetVectorParameterValue(TEXT("SiteOrigin"), AsColor(SiteFrame.GetLocation()));
        LevelHighlight->SetVectorParameterValue(TEXT("SiteAxisX"), AsColor(SiteFrame.GetUnitAxis(EAxis::X)));
        LevelHighlight->SetVectorParameterValue(TEXT("SiteAxisY"), AsColor(SiteFrame.GetUnitAxis(EAxis::Y)));
        LevelHighlight->SetVectorParameterValue(TEXT("SiteAxisZ"), AsColor(SiteFrame.GetUnitAxis(EAxis::Z)));
        LevelHighlight->SetScalarParameterValue(TEXT("BandMin"), View.MinZ);
        LevelHighlight->SetScalarParameterValue(TEXT("BandMax"), View.MaxZ);
        LevelHighlight->SetScalarParameterValue(TEXT("FootprintX"), static_cast<float>(GameMode->StadiumFootprintRadii.X));
        LevelHighlight->SetScalarParameterValue(TEXT("FootprintY"), static_cast<float>(GameMode->StadiumFootprintRadii.Y));
        LevelHighlight->SetVectorParameterValue(TEXT("HighlightColor"), GameMode->LevelHighlightColor);
        LevelHighlight->SetScalarParameterValue(TEXT("HighlightOpacity"), GameMode->LevelHighlightOpacity);
        LevelHighlight->SetScalarParameterValue(TEXT("DimOutside"), GameMode->LevelDimOutside);
    }
    Rig->SetPostProcessBlendable(LevelHighlight, bOn ? 1.0f : 0.0f);
    UE_LOG(LogVirtualStadium, Log, TEXT("Level highlight: %s."), bOn ? *UEnum::GetDisplayValueAsText(MapLevel).ToString() : TEXT("off"));
}

void ULFFJourneySubsystem::FlyTick(const FVector2D& Move, const float Rise, const FVector2D& LookDelta, const float Dolly, const bool bFast,
    const float DeltaSeconds)
{
    if (!Move.IsNearlyZero() || !LookDelta.IsNearlyZero() || !FMath::IsNearlyZero(Rise) || !FMath::IsNearlyZero(Dolly))
    {
        bMapAutoFramed = false;
    }
    FVector2D Input = Move;
#if !UE_BUILD_SHIPPING
    if (ShotFlySecondsLeft > 0.0f)
    {
        Input.Y += 1.0;
        ShotFlySecondsLeft -= DeltaSeconds;
    }
#endif
    if (!CanFly() || Rig == nullptr || GameMode == nullptr || Rig->IsBusy() || DeltaSeconds <= 0.0f)
    {
        // Nothing carries over: after a move or a modal, flight starts from rest.
        FlyVelocity = FVector::ZeroVector;
        FlyDollyLeft = 0.0f;
        return;
    }

    FRotator Look = Rig->GetActorRotation();
    if (!LookDelta.IsNearlyZero())
    {
        // Drag right to turn right, up to look up. Never past straight down or far overhead; never any roll.
        Look.Yaw += static_cast<float>(LookDelta.X) * GameMode->FlyLookDegreesPerPixel;
        Look.Pitch = FMath::Clamp(Look.Pitch + static_cast<float>(LookDelta.Y) * GameMode->FlyLookDegreesPerPixel, -85.0f, 70.0f);
    }
    Look.Roll = 0.0f;

    // Forward follows where you look; sideways stays level; up and down follow the site.
    const FVector Forward = Look.Vector();
    const FVector Right = FRotationMatrix(Look).GetScaledAxis(EAxis::Y);
    const FVector Up = SiteFrame.GetUnitAxis(EAxis::Z);
    const bool bGentle = Settings.IsReducedMotionEffective();
    const float Speed = GameMode->FlySpeed * (bFast ? GameMode->FlyFastMultiplier : 1.0f) * (bGentle ? 0.6f : 1.0f);
    const FVector Desired = (Forward * Input.Y + Right * Input.X + Up * Rise).GetClampedToMaxSize(1.0) * Speed;
    // Less movement and Calm Mode start and stop at once: no drifting after you let go.
    const float Ease = GameMode->FlyEaseSeconds;
    FlyVelocity = bGentle || Ease <= 0.0f ? Desired : FMath::VInterpTo(FlyVelocity, Desired, DeltaSeconds, 3.0f / Ease);

    FlyDollyLeft += Dolly * GameMode->FlyWheelStep;
    const float DollyStep = bGentle ? FlyDollyLeft : FlyDollyLeft * FMath::Min(DeltaSeconds * 8.0f, 1.0f);
    FlyDollyLeft -= DollyStep;

    const FVector From = Rig->GetActorLocation();
    FVector To = From + FlyVelocity * DeltaSeconds + Forward * DollyStep;
    To = SiteFrame.TransformPosition(LFFJourneyRules::ClampFlyPosition(SiteFrame.InverseTransformPosition(To),
        GameMode->FlyBoundsRadius, GameMode->FlyMinZ, GameMode->FlyMaxZ));
    if (!To.Equals(From, 0.01) || !Look.Equals(Rig->GetActorRotation(), 0.001f))
    {
        Rig->SetActorLocationAndRotation(To, Look);
    }
}

void ULFFJourneySubsystem::ToggleAmenity(const ELFFAmenity Amenity)
{
    if (!CanUseMap())
    {
        return;
    }
    AmenityMask ^= 1 << static_cast<int32>(Amenity);
    // Turning a kind on shows its places on every level, so the camera makes sure they're all in view.
    if ((AmenityMask & (1 << static_cast<int32>(Amenity))) != 0)
    {
        FramePlacesShown();
    }
    Broadcast();
}

void ULFFJourneySubsystem::FramePlacesShown()
{
    if (Rig == nullptr || GameMode == nullptr)
    {
        return;
    }
    TArray<FVector> Points;
    for (const FLFFAmenityMarker& Amenity : Amenities)
    {
        if ((AmenityMask & (1 << static_cast<int32>(Amenity.Category))) != 0)
        {
            Points.Add(Amenity.Location);
            Points.Add(Amenity.Location + FVector(0.0, 0.0, MapStemHeight));
        }
    }
    FLFFAmenityMarker Seat;
    if (GetAmenity(MySeatAmenityId, Seat))
    {
        Points.Add(Seat.Location);
        Points.Add(Seat.Location + FVector(0.0, 0.0, MapStemHeight));
    }
    FrameMapPoints(Points);
    bMapAutoFramed = true;
}

void ULFFJourneySubsystem::SetMapViewportFrame(const FVector2D& Size, const FBox2D& SafeRect, const float StemHeight)
{
    const bool bChanged = !Size.Equals(MapViewportSize, 1.0) || SafeRect != MapSafeRect || !FMath::IsNearlyEqual(StemHeight, MapStemHeight);
    MapViewportSize = Size;
    MapSafeRect = SafeRect;
    MapStemHeight = StemHeight;
    if (bChanged && bMapAutoFramed && CanUseMap())
    {
        FramePlacesShown();
    }
}

void ULFFJourneySubsystem::FocusAmenity(const FName AmenityId)
{
    if (!CanUseMap())
    {
        return;
    }
    FLFFAmenityMarker Place;
    if (!GetAmenity(AmenityId, Place))
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Focus place: '%s' is not mapped."), *AmenityId.ToString());
        return;
    }
    if (!Place.CameraTargetId.IsNone())
    {
        ShowTarget(Place.CameraTargetId);
        return;
    }
    FrameMapPoints({ Place.Location, Place.Location + FVector(0.0, 0.0, MapStemHeight) });
    bMapAutoFramed = false;
}

void ULFFJourneySubsystem::FrameMapPoints(const TArray<FVector>& Points)
{
    if (Rig == nullptr || GameMode == nullptr || Points.Num() == 0)
    {
        return;
    }
    if (!MapSafeRect.bIsValid || MapViewportSize.X <= 1.0 || MapViewportSize.Y <= 1.0)
    {
        if (!bLoggedFramingUnavailable)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Places: no unobstructed HUD frame yet; expand the window or close a drawer."));
            bLoggedFramingUnavailable = true;
        }
        return;
    }
    bLoggedFramingUnavailable = false;
    const FLFFLevelView& View = GameMode->LevelViews.IsValidIndex(static_cast<int32>(MapLevel))
        ? GameMode->LevelViews[static_cast<int32>(MapLevel)] : FLFFLevelView();
    const FTransform Local = LFFJourneyRules::FramePointsPose(Points, -50.0f, View.ViewYawDegrees, 60.0f,
        MapViewportSize.X / MapViewportSize.Y, MapSafeRect, 400.0f, 6000.0f);
    const FTransform World = Local * SiteFrame;
    FRotator Look = World.Rotator();
    Look.Roll = 0.0f;
    Rig->StopOrbit();
    FlyVelocity = FVector::ZeroVector;
    FlyDollyLeft = 0.0f;
    const ELFFTransitionProfile Profile = LFFJourneyRules::ResolveMove(ELFFTransitionProfile::Ease, Settings, false);
    UE_LOG(LogVirtualStadium, Log, TEXT("Places: framing %d anchors within actual %.0fx%.0f HUD safe area (%s)."),
        Points.Num(), MapViewportSize.X, MapViewportSize.Y, *UEnum::GetDisplayValueAsText(Profile).ToString());
    Rig->BeginTransition(FTransform(Look, World.GetLocation()), 60.0f, Profile, 2.5f, GameMode->ReducedMotionFadeSeconds,
        GameMode->FadeColor, ExploreMoveId);
    ViewTargetId = NAME_None;
}

void ULFFJourneySubsystem::HideAllAmenities()
{
    if (!CanUseMap() || AmenityMask == 0)
    {
        return;
    }
    AmenityMask = 0;
    Broadcast();
}

void ULFFJourneySubsystem::SelectAmenity(const FName AmenityId)
{
    if (!CanUseMap() || AmenityId == SelectedAmenityId)
    {
        return;
    }
    SelectedAmenityId = AmenityId;
    Broadcast();
}

void ULFFJourneySubsystem::ResetMap()
{
    bLevelHighlight = false;
    FlyVelocity = FVector::ZeroVector;
    // The next visitor gets the map as it starts: no picks, the default markers, opening on the seat's level.
    MapLevel = GameMode != nullptr ? GameMode->MySeatLevel : ELFFMapLevel::LowerConcourse;
    AmenityMask = LFFDefaultAmenityMask;
    SelectedAmenityId = NAME_None;
    bMapOpened = false;
    ViewTargetId = NAME_None;
    ApplyLevelHighlight();
}

void ULFFJourneySubsystem::RefreshSiteFrame()
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(SiteRootTag))
        {
            SiteFrame = It->GetActorTransform();
            SiteFrame.SetScale3D(FVector::OneVector);
            return;
        }
    }
    SiteFrame = FTransform::Identity;
}

void ULFFJourneySubsystem::LoadAmenities()
{
    Amenities.Reset();
    UDataTable* Table = GameMode != nullptr ? GameMode->AmenityTable.LoadSynchronous() : nullptr;
    if (Table == nullptr || Table->GetRowStruct() != FLFFAmenityMarker::StaticStruct())
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Places: no amenity table; Free explore shows the seat only."));
        return;
    }
#if WITH_EDITOR
    const bool bKeepUnverified = true;
#else
    const bool bKeepUnverified = GameMode->bShowUnverifiedAmenities;
#endif
    int32 Dropped = 0;
    Table->ForeachRow<FLFFAmenityMarker>(TEXT("LFFJourneySubsystem"), [this, bKeepUnverified, &Dropped](const FName& RowName, const FLFFAmenityMarker& Row)
    {
        // A wrong sensory-room pin could send someone the wrong way, so unverified places stay out of packaged builds.
        if (Row.VerificationSource.IsEmpty() && !bKeepUnverified)
        {
            ++Dropped;
            return;
        }
        Amenities.Add_GetRef(Row).AmenityId = RowName;
    });

    // A place marked by an actor in the level stands where that actor is now.
    int32 Placed = 0;
    for (FLFFAmenityMarker& Amenity : Amenities)
    {
        if (Amenity.LocationActorTag.IsNone())
        {
            continue;
        }
        const AActor* Marker = nullptr;
        for (TActorIterator<AActor> It(GetWorld()); It && Marker == nullptr; ++It)
        {
            Marker = It->ActorHasTag(Amenity.LocationActorTag) ? *It : nullptr;
        }
        if (Marker == nullptr)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Places: no actor tagged %s; %s keeps its data location."),
                *Amenity.LocationActorTag.ToString(), *Amenity.AmenityId.ToString());
            continue;
        }
        Amenity.Location = SiteFrame.InverseTransformPosition(Marker->GetActorLocation());
        ++Placed;
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Places: %d from %s (%d at their actors)%s."), Amenities.Num(), *Table->GetName(), Placed,
        Dropped > 0 ? *FString::Printf(TEXT(", %d unverified dropped"), Dropped) : TEXT(""));
}

bool ULFFJourneySubsystem::GetAmenity(const FName AmenityId, FLFFAmenityMarker& OutAmenity) const
{
    if (AmenityId == MySeatAmenityId)
    {
        const ALFFCameraTarget* Seat = GameMode != nullptr ? FindTarget(GameMode->MySeatTargetId) : nullptr;
        if (Seat == nullptr)
        {
            return false;
        }
        OutAmenity = FLFFAmenityMarker();
        OutAmenity.AmenityId = MySeatAmenityId;
        OutAmenity.Label = GameMode->MySeatLabel;
        OutAmenity.Note = GameMode->MySeatNote;
        OutAmenity.Level = GameMode->MySeatLevel;
        OutAmenity.Location = SiteFrame.InverseTransformPosition(Seat->GetActorLocation());
        OutAmenity.CameraTargetId = GameMode->MySeatTargetId;
        return true;
    }
    const FLFFAmenityMarker* Found = AmenityId.IsNone() ? nullptr
        : Amenities.FindByPredicate([AmenityId](const FLFFAmenityMarker& Amenity) { return Amenity.AmenityId == AmenityId; });
    if (Found == nullptr)
    {
        return false;
    }
    OutAmenity = *Found;
    return true;
}

void ULFFJourneySubsystem::ToggleVoice()
{
    // Voice needs Sound; Calm Mode forces it off. The button is disabled then, so a key press does nothing either.
    if (!Settings.IsSoundEffective())
    {
        return;
    }

    Settings.bVoiceEnabled = !Settings.bVoiceEnabled;
    UE_LOG(LogVirtualStadium, Log, TEXT("Voice %s."), Settings.bVoiceEnabled ? TEXT("on") : TEXT("off"));
    if (Settings.bVoiceEnabled)
    {
        ScheduleVoice();
    }
    else
    {
        StopVoice();
    }
    Broadcast();
}

void ULFFJourneySubsystem::ToggleVoiceFromCard()
{
    if (Settings.bCalmMode)
    {
        return;
    }
    if (Settings.IsVoiceEffective())
    {
        ToggleVoice();
        return;
    }
    // Voice needs Sound: the card's button turns both on.
    if (!Settings.IsSoundEffective())
    {
        ToggleSound();
    }
    if (!Settings.bVoiceEnabled)
    {
        ToggleVoice();
    }
}

void ULFFJourneySubsystem::ToggleCrowd()
{
    if (Settings.bCalmMode)
    {
        return;
    }

    Settings.bCrowdEnabled = !Settings.bCrowdEnabled;
    ApplyCrowd();
    Broadcast();
}

void ULFFJourneySubsystem::OpenLearnMore()
{
    if (GameMode == nullptr || GameMode->LearnMoreUrl.IsEmpty())
    {
        return;
    }

    StopVoice();
    UE_LOG(LogVirtualStadium, Log, TEXT("Learn More: opening %s"), *GameMode->LearnMoreUrl);
    FString Error;
    FPlatformProcess::LaunchURL(*GameMode->LearnMoreUrl, nullptr, &Error);
    if (!Error.IsEmpty())
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Learn More: the browser could not be opened (%s)."), *Error);
    }
    Broadcast();
}

void ULFFJourneySubsystem::TogglePreview()
{
    if (IsModalOpen() || Machine.IsPaused() || !Machine.HasStarted()
        || !LFFJourneyRules::ShouldShowPreviewControls(GetCurrentPreviewKind(), Settings))
    {
        return;
    }

    bPreviewOn = !bPreviewOn;
    if (bPreviewOn)
    {
        // A preview never starts under the voice.
        StopVoice();
    }
    ApplySensory();
    Broadcast();
}

void ULFFJourneySubsystem::ToggleLargeText()
{
    Settings.bLargeText = !Settings.bLargeText;
    Broadcast();
}

void ULFFJourneySubsystem::ToggleHighContrast()
{
    Settings.bHighContrast = !Settings.bHighContrast;
    Broadcast();
}

void ULFFJourneySubsystem::HandleRigTransitionFinished(const uint32 TransitionId)
{
    // The rig only ever reports its latest move, so a current id is the camera arriving where the journey is.
    const bool bCurrent = TransitionId == Machine.GetTransitionId();
    const bool bFinished = Machine.NotifyTransitionFinished(TransitionId);
    if (bCurrent)
    {
        if (Machine.HasStarted())
        {
            OnArrived();
        }
        else
        {
            // The restart fade is over: the splash orbit can begin.
            StartSplashOrbit();
        }
    }

    if (bFinished || bCurrent)
    {
        Broadcast();
    }
}

void ULFFJourneySubsystem::MoveCameraToCurrentStep(const bool bForceFade)
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    ALFFCameraTarget* Target = Step != nullptr ? FindTarget(Step->CameraTargetId) : nullptr;
    bTargetMissing = Target == nullptr;

    // A new step silences the old one at once: no voice overlaps the next card, no eagle flight carries on.
    StopVoice();
    StopAnthemSequence(false);
    bArrived = false;
    ResetPreview();
    ApplyLabels();
    ApplySensory();
    ApplyCues(ELFFCueReason::StepChanged);

    if (Target == nullptr || Rig == nullptr || GameMode == nullptr)
    {
        if (Step != nullptr)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Step %s: camera target '%s' not found. The camera stays put."), *Step->StepId.ToString(), *Step->CameraTargetId.ToString());
        }

        // Fail closed: don't move, don't leave the journey stuck mid-transition. The card is still read.
        Machine.NotifyTransitionFinished(Machine.GetTransitionId());
        if (Machine.HasStarted())
        {
            OnArrived();
        }
        return;
    }

    const FTransform TargetTransform = Target->GetCameraTransform();

    // An authored route replaces the straight move where one exists (outside the bowl to the concourse, down the
    // concourse into the sensory room). Calm Mode still cuts and less movement still fades.
    const ALFFCameraPath* Path = bForceFade ? nullptr : FindPath(Step->CameraTargetId);
    if (Path != nullptr && !Settings.bCalmMode && !Settings.IsReducedMotionEffective())
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: route %s to %s (%d points, %.1f s%s)."), *Step->StepId.ToString(), *Path->GetName(),
            *Step->CameraTargetId.ToString(), Path->Waypoints.Num(), Path->TravelSeconds, Path->bCutAtEnd ? TEXT(", fade and cut at the end") : TEXT(""));
        Rig->BeginPathTransition(Path->Waypoints, TargetTransform, Target->FieldOfView, Path->TravelSeconds, Path->bCutAtEnd,
            GameMode->ReducedMotionFadeSeconds, GameMode->FadeColor, Machine.GetTransitionId());
        return;
    }

    const bool bPathBlocked = Step->TransitionProfile == ELFFTransitionProfile::Ease
        && IsPathBlocked(Rig->GetActorLocation(), TargetTransform.GetLocation());
    if (bPathBlocked)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: the straight path to %s crosses geometry; fading and cutting instead of easing."),
            *Step->StepId.ToString(), *Step->CameraTargetId.ToString());
    }

    const ELFFTransitionProfile Profile = LFFJourneyRules::ResolveMove(
        bForceFade ? ELFFTransitionProfile::ReducedMotion : Step->TransitionProfile, Settings, bPathBlocked);
    UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: %s to %s."), *Step->StepId.ToString(),
        *UEnum::GetDisplayValueAsText(Profile).ToString(), *Step->CameraTargetId.ToString());

    const float EaseSeconds = LFFJourneyRules::ClampDuration(
        Step->TransitionSeconds, GameMode->DefaultEaseSeconds, GameMode->MinTransitionSeconds, GameMode->MaxTransitionSeconds);

    Rig->BeginTransition(
        TargetTransform,
        Target->FieldOfView,
        Profile,
        EaseSeconds,
        GameMode->ReducedMotionFadeSeconds,
        GameMode->FadeColor,
        Machine.GetTransitionId());
}

void ULFFJourneySubsystem::MoveCameraToFirstStep()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    FVector Center, LookAt;
    float Radius = 0.0f, Height = 0.0f, Fov = 70.0f, Anchor = 0.0f;
    FTransform Wide;
    const bool bOnOrbit = Rig != nullptr && GameMode != nullptr && Step != nullptr && Rig->IsOrbiting()
        && Step->CameraTargetId == GameMode->SplashOrbitTargetId && GetSplashOrbit(Center, Radius, Height, LookAt, Fov, Anchor, Wide);
    if (!bOnOrbit)
    {
        MoveCameraToCurrentStep(false);
        return;
    }

    StopVoice();
    bArrived = false;
    bTargetMissing = false;
    ResetPreview();
    ApplyLabels();
    ApplySensory();
    ApplyCues(ELFFCueReason::StepChanged);

    // The longer the way round, the longer the glide, within the usual move limits.
    const float Degrees = FMath::Abs(FMath::UnwindDegrees(Anchor - Rig->GetOrbitAngle(Center)));
    const float Seconds = FMath::GetMappedRangeValueClamped(FVector2f(0.0f, 180.0f),
        FVector2f(GameMode->MinTransitionSeconds * 3.0f, GameMode->MaxTransitionSeconds), Degrees);
    UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: glide along the splash orbit (%.0f deg, %.1f s) to %s."),
        *Step->StepId.ToString(), Degrees, Seconds, *Step->CameraTargetId.ToString());
    Rig->EaseOrbitTo(Anchor, Seconds, Machine.GetTransitionId());
}

bool ULFFJourneySubsystem::GetSplashOrbit(FVector& OutCenter, float& OutRadius, float& OutHeight, FVector& OutLookAt,
    float& OutFov, float& OutAnchorAngle, FTransform& OutWide) const
{
    if (GameMode == nullptr)
    {
        return false;
    }

    const ALFFCameraTarget* Target = FindTarget(GameMode->SplashOrbitTargetId);
    const AActor* Root = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(SiteRootTag))
        {
            Root = *It;
            break;
        }
    }
    if (Target == nullptr || Root == nullptr)
    {
        return false;
    }

    OutWide = Target->GetCameraTransform();
    const FVector Eye = OutWide.GetLocation();
    OutCenter = Root->GetActorLocation();
    OutRadius = FVector::Dist2D(Eye, OutCenter);
    OutHeight = Eye.Z;
    // Look at the point above the centre that the wide shot is aimed at, so the orbit passes exactly through it.
    const FVector Forward = OutWide.GetRotation().GetForwardVector();
    const float Run = FMath::Max(FVector2D(Forward.X, Forward.Y).Size(), 0.05f);
    OutLookAt = FVector(OutCenter.X, OutCenter.Y, Eye.Z + Forward.Z * OutRadius / Run);
    OutFov = Target->FieldOfView;
    OutAnchorAngle = FMath::RadiansToDegrees(FMath::Atan2(Eye.Y - OutCenter.Y, Eye.X - OutCenter.X));
    return true;
}

void ULFFJourneySubsystem::EnterSplash(const bool bCut)
{
    StopVoice();
    StopAnthemSequence(false);
    if (Rig == nullptr || GameMode == nullptr)
    {
        return;
    }

    FVector Center, LookAt;
    float Radius = 0.0f, Height = 0.0f, Fov = 70.0f, Anchor = 0.0f;
    FTransform Wide;
    if (!GetSplashOrbit(Center, Radius, Height, LookAt, Fov, Anchor, Wide))
    {
        // No orbit target: the splash sits on the first step's view, as before.
        UE_LOG(LogVirtualStadium, Warning, TEXT("Splash orbit target %s or LFF_SiteRoot not found; the splash holds the first step's view."),
            *GameMode->SplashOrbitTargetId.ToString());
        if (bCut)
        {
            MoveCameraToCurrentStep(true);
        }
        return;
    }

    if (bCut)
    {
        // Restart: fade to the wide shot (a cut in Calm Mode); the orbit starts when the fade reports back.
        const ELFFTransitionProfile Profile = Settings.bCalmMode ? ELFFTransitionProfile::Instant : ELFFTransitionProfile::ReducedMotion;
        UE_LOG(LogVirtualStadium, Log, TEXT("Splash: %s to %s."), *UEnum::GetDisplayValueAsText(Profile).ToString(), *GameMode->SplashOrbitTargetId.ToString());
        Rig->BeginTransition(Wide, Fov, Profile, GameMode->DefaultEaseSeconds, GameMode->ReducedMotionFadeSeconds, GameMode->FadeColor,
            Machine.GetTransitionId());
        return;
    }

    StartSplashOrbit();
}

void ULFFJourneySubsystem::StartSplashOrbit()
{
    if (Rig == nullptr || GameMode == nullptr || Machine.HasStarted() || IsExploring())
    {
        return;
    }

    if (!LFFJourneyRules::ShouldOrbitSplash(Settings))
    {
        if (Rig->IsOrbiting())
        {
            UE_LOG(LogVirtualStadium, Log, TEXT("Splash: still wide shot (less movement)."));
        }
        Rig->StopOrbit();
        return;
    }

    FVector Center, LookAt;
    float Radius = 0.0f, Height = 0.0f, Fov = 70.0f, Anchor = 0.0f;
    FTransform Wide;
    if (Rig->IsOrbiting() || !GetSplashOrbit(Center, Radius, Height, LookAt, Fov, Anchor, Wide))
    {
        return;
    }

    const float From = Rig->GetOrbitAngle(Center);
    UE_LOG(LogVirtualStadium, Log, TEXT("Splash: orbit of %.0f m at %.0f m high, one lap in %.0f s, from %.0f deg."),
        Radius / 100.0f, Height / 100.0f, GameMode->SplashOrbitSeconds, From);
    Rig->StartOrbit(Center, Radius, Height, LookAt, GameMode->SplashOrbitSeconds, Fov, From);
}

void ULFFJourneySubsystem::OnArrived()
{
    bArrived = true;
    ApplyCues(ELFFCueReason::Arrived);

    // The anthem plays by itself in Standard; the Soft button only replays it. Calm Mode skips it.
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (Step != nullptr && Step->IntensityPreview == ELFFIntensityPreview::Anthem && !Settings.bCalmMode && !IsExploring())
    {
        StartAnthemSequence();
    }
    ScheduleVoice();
}

void ULFFJourneySubsystem::ApplyCues(const ELFFCueReason Reason)
{
    if (Reason == ELFFCueReason::SettingsChanged && (Settings.bCalmMode || Settings.IsReducedMotionEffective()))
    {
        StopAnthemSequence(true);
    }

    const bool bInstant = Settings.IsReducedMotionEffective();
    const bool bStarted = Machine.HasStarted();
    const int32 Index = Machine.GetIndex();

    TSet<FName> Played;
    TSet<FName> Shown;
    for (ALFFCueActor* Actor : CueActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        // People with no cue (the line, the staff) only stand; leave their visibility to the crowd settings.
        if (Actor->CueId.IsNone())
        {
            continue;
        }

        if (Actor->Trigger == ELFFCueTrigger::Preview)
        {
            // Preview cues only ever run inside the preview (ApplyPreviewCues); a new step puts them away.
            if (Reason == ELFFCueReason::StepChanged)
            {
                Actor->ApplyState(ELFFCueState::Before);
            }
            continue;
        }

        const int32* First = FirstCueIndex.Find(Actor->CueId);
        const ELFFCueState State = LFFJourneyRules::ResolveCueState(bStarted, Index, First != nullptr ? *First : INDEX_NONE);
        // Calm Mode means no spectacle at all: not even shown already done.
        if (Actor->bSpectacle && Settings.bCalmMode)
        {
            Actor->ApplyState(ELFFCueState::Before);
            continue;
        }
        switch (Reason)
        {
        case ELFFCueReason::StepChanged:
            // On its own step it waits for the camera, unless less movement shows it already done.
            Actor->ApplyState(State == ELFFCueState::Play ? (bInstant ? ELFFCueState::After : ELFFCueState::Before) : State);
            if (State == ELFFCueState::Play && bInstant)
            {
                Shown.Add(Actor->CueId);
            }
            break;
        case ELFFCueReason::Arrived:
            if (State == ELFFCueState::Play && !bInstant)
            {
                Actor->Play(false);
                Played.Add(Actor->CueId);
            }
            break;
        case ELFFCueReason::SettingsChanged:
            // Spectacle comes back (already done) when Calm Mode is turned off on its step.
            if (State == ELFFCueState::Play && (bInstant || (Actor->bSpectacle && !Actor->IsPlaying())))
            {
                Actor->ApplyState(ELFFCueState::After);
            }
            break;
        }
    }

    for (const FName& Cue : Played)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Scene cue %s playing."), *Cue.ToString());
    }
    for (const FName& Cue : Shown)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Scene cue %s shown already done (less movement)."), *Cue.ToString());
    }
}

void ULFFJourneySubsystem::ApplyPreviewCues()
{
    const bool bWant = bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Anthem
        && LFFJourneyRules::CanPlayPreview(ELFFIntensityPreview::Anthem, Settings);
    if (bWant == bPreviewCuesPlaying)
    {
        return;
    }

    bPreviewCuesPlaying = bWant;
    // Soft replays the same sequence the step plays by itself.
    const float Longest = bWant ? StartAnthemSequence() : 0.0f;
    if (!bWant)
    {
        StopAnthemSequence(true);
    }

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    Timers.ClearTimer(PreviewCueTimer);
    if (bWant)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Anthem preview playing: %.1f s, one pass."), Longest);
        Timers.SetTimer(PreviewCueTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::FinishPreviewCues), FMath::Max(Longest, 0.5f) + 0.2f, false);
    }
    else
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Anthem preview off."));
    }
}

void ULFFJourneySubsystem::FinishPreviewCues()
{
    // One pass, then the button goes back to Off so it can be seen again.
    UE_LOG(LogVirtualStadium, Log, TEXT("Anthem preview finished."));
    if (bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Anthem)
    {
        bPreviewOn = false;
    }
    ApplySensory();
    Broadcast();
}

const ALFFCameraPath* ULFFJourneySubsystem::FindPath(const FName ToTargetId) const
{
    if (Rig == nullptr || ToTargetId.IsNone())
    {
        return nullptr;
    }

    for (TActorIterator<ALFFCameraPath> It(GetWorld()); It; ++It)
    {
        const ALFFCameraPath* Path = *It;
        if (Path->ToTargetId != ToTargetId || Path->Waypoints.IsEmpty())
        {
            continue;
        }

        if (Path->FromTargetId.IsNone())
        {
            return Path;
        }

        // Only from where it was authored to start: going back, or jumping in, moves the usual way.
        const ALFFCameraTarget* From = FindTarget(Path->FromTargetId);
        if (From != nullptr && FVector::Dist(From->GetCameraTransform().GetLocation(), Rig->GetActorLocation()) < 100.0f)
        {
            return Path;
        }
    }
    return nullptr;
}

float ULFFJourneySubsystem::StartAnthemSequence()
{
    StopAnthemSequence(false);
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (Step == nullptr || GameMode == nullptr || Settings.bCalmMode)
    {
        return 0.0f;
    }

    ALFFCueActor* Eagle = nullptr;
    float Fireworks = 0.0f;
    for (ALFFCueActor* Actor : CueActors)
    {
        if (!IsValid(Actor) || Actor->Trigger != ELFFCueTrigger::Preview || !Step->SceneCues.Contains(Actor->CueId))
        {
            continue;
        }

        if (Actor->IsA<ALFFFireworksActor>())
        {
            Fireworks = FMath::Max(Fireworks, Actor->GetTotalSeconds());
        }
        else if (Eagle == nullptr)
        {
            Eagle = Actor;
        }
    }

    bAnthemSequence = true;
    AnthemEagle = Eagle;
    const float Flight = Eagle != nullptr ? Eagle->GetTotalSeconds() : 0.0f;
    const bool bFollow = Eagle != nullptr && Rig != nullptr && !Settings.IsReducedMotionEffective();
    if (Eagle != nullptr)
    {
        Eagle->Play(false);
    }

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (bFollow)
    {
        const float JoinAt = Eagle->PlayDelaySeconds + GameMode->AnthemFollowStart * Eagle->PlaySeconds;
        // Leave the bird a moment before it is gone, so the camera turns home with it still in frame.
        AnthemFlightLeft = FMath::Max(Flight - JoinAt - 0.6f, 0.5f);
        Timers.SetTimer(AnthemTimer, this, &ULFFJourneySubsystem::JoinAnthemEagle, FMath::Max(JoinAt, 0.1f), false);
    }
    else
    {
        Timers.SetTimer(AnthemTimer, this, &ULFFJourneySubsystem::PlayAnthemFireworks, FMath::Max(Flight, 0.1f), false);
    }

    const float Total = Flight + (bFollow ? GameMode->AnthemReturnSeconds - 0.6f : 0.0f) + Fireworks;
    UE_LOG(LogVirtualStadium, Log, TEXT("Anthem: eagle %s (%.1f s), then fireworks; about %.1f s."),
        bFollow ? TEXT("with the camera close beside it") : TEXT("seen from the seat"), Flight, Total);
    return Total;
}

void ULFFJourneySubsystem::JoinAnthemEagle()
{
    ALFFCueActor* Eagle = AnthemEagle.Get();
    if (!bAnthemSequence || Eagle == nullptr || Rig == nullptr || GameMode == nullptr)
    {
        return;
    }

    Rig->StartFollow(Eagle, GameMode->AnthemFollowOffset, GameMode->AnthemFollowFieldOfView);
    GetWorld()->GetTimerManager().SetTimer(AnthemTimer, this, &ULFFJourneySubsystem::ReturnFromAnthemEagle, AnthemFlightLeft, false);
}

void ULFFJourneySubsystem::ReturnFromAnthemEagle()
{
    if (!bAnthemSequence || Rig == nullptr || GameMode == nullptr)
    {
        return;
    }

    Rig->StopFollow();
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (const ALFFCameraTarget* Target = Step != nullptr ? FindTarget(Step->CameraTargetId) : nullptr)
    {
        Rig->EaseQuietly(Target->GetCameraTransform(), Target->FieldOfView, GameMode->AnthemReturnSeconds);
    }
    GetWorld()->GetTimerManager().SetTimer(AnthemTimer, this, &ULFFJourneySubsystem::PlayAnthemFireworks, GameMode->AnthemReturnSeconds, false);
}

void ULFFJourneySubsystem::PlayAnthemFireworks()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (!bAnthemSequence || Step == nullptr)
    {
        return;
    }

    float Longest = 0.0f;
    for (ALFFCueActor* Actor : CueActors)
    {
        if (IsValid(Actor) && Actor->IsA<ALFFFireworksActor>() && Actor->Trigger == ELFFCueTrigger::Preview && Step->SceneCues.Contains(Actor->CueId))
        {
            Actor->Play(false);
            Longest = FMath::Max(Longest, Actor->GetTotalSeconds());
        }
    }

    GetWorld()->GetTimerManager().SetTimer(AnthemTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        bAnthemSequence = false;
        AnthemEagle.Reset();
    }), FMath::Max(Longest, 0.1f) + 0.2f, false);
}

void ULFFJourneySubsystem::StopAnthemSequence(const bool bRestoreCamera)
{
    if (!bAnthemSequence)
    {
        return;
    }

    bAnthemSequence = false;
    AnthemEagle.Reset();
    GetWorld()->GetTimerManager().ClearTimer(AnthemTimer);

    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    for (ALFFCueActor* Actor : CueActors)
    {
        if (IsValid(Actor) && Actor->Trigger == ELFFCueTrigger::Preview && Step != nullptr && Step->SceneCues.Contains(Actor->CueId))
        {
            Actor->ApplyState(ELFFCueState::Before);
        }
    }

    if (Rig != nullptr)
    {
        Rig->StopFollow();
        if (bRestoreCamera)
        {
            // A cut, not a move: this only happens when the visitor asked for less (Calm, less movement, pause).
            SnapCameraToCurrentStep();
        }
    }
}

void ULFFJourneySubsystem::ScheduleVoice()
{
    StopVoice();
    if (!bArrived || Machine.IsPaused() || IsExploring() || !LFFJourneyRules::ShouldPlayVoice(Settings, Machine.HasStarted(), bPreviewOn))
    {
        return;
    }

    const float Delay = GameMode != nullptr ? GameMode->VoiceStartDelaySeconds : 0.5f;
    GetWorld()->GetTimerManager().SetTimer(VoiceTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::StartVoice), FMath::Max(Delay, 0.01f), false);
}

void ULFFJourneySubsystem::StartVoice()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (Step == nullptr || GameMode == nullptr || IsExploring() || !LFFJourneyRules::ShouldPlayVoice(Settings, Machine.HasStarted(), bPreviewOn))
    {
        return;
    }

    USoundBase* Line = Step->VoiceWave.IsNull() ? nullptr : Step->VoiceWave.LoadSynchronous();
    if (Line == nullptr)
    {
        // A missing recording is never an error for the visitor: the card is the caption and stays on screen.
        UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: no voice wave at '%s'; skipped, the card stays."),
            *Step->StepId.ToString(), *Step->VoiceWave.ToString());
        return;
    }

    VoiceAudio = UGameplayStatics::CreateSound2D(this, Line, GameMode->VoiceVolume, 1.0f, 0.0f, nullptr, false, false);
    if (VoiceAudio == nullptr)
    {
        return;
    }

    VoiceAudio->OnAudioFinished.AddDynamic(this, &ULFFJourneySubsystem::HandleVoiceFinished);
    // The word cues drive the card's highlight; the engine's own subtitle line stays off screen.
    VoiceAudio->bSuppressSubtitles = true;
    VoiceWordTimes.Reset();
    if (const USoundWave* Wave = Cast<USoundWave>(Line))
    {
        for (const FSubtitleCue& Cue : Wave->Subtitles)
        {
            VoiceWordTimes.Add(Cue.Time);
        }
    }
    if (VoiceWordTimes.Num() > 0)
    {
        VoiceAudio->OnAudioPlaybackPercentNative.AddUObject(this, &ULFFJourneySubsystem::HandleVoicePlaybackPercent);
    }
    VoiceAudio->Play();
    bVoicePlaying = true;
    UE_LOG(LogVirtualStadium, Log, TEXT("Step %s: voice playing %s (%.1f s) at volume %.2f; bed ducked %.0f dB."),
        *Step->StepId.ToString(), *Line->GetName(), Line->GetDuration(), GameMode->VoiceVolume, GameMode->VoiceDuckDecibels);
    ApplyAudio();
    Broadcast();
}

void ULFFJourneySubsystem::StopVoice(const bool bReapplyAudio)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(VoiceTimer);
    }

    if (IsValid(VoiceAudio))
    {
        VoiceAudio->OnAudioFinished.RemoveDynamic(this, &ULFFJourneySubsystem::HandleVoiceFinished);
        VoiceAudio->OnAudioPlaybackPercentNative.RemoveAll(this);
        // Immediately, not a fade: the next card must never be read over the last one.
        VoiceAudio->Stop();
        UE_LOG(LogVirtualStadium, Log, TEXT("Voice stopped."));
    }
    VoiceAudio = nullptr;
    SetVoiceWord(INDEX_NONE);

    if (bVoicePlaying)
    {
        bVoicePlaying = false;
        if (bReapplyAudio)
        {
            ApplyAudio();
        }
    }
}

void ULFFJourneySubsystem::HandleVoicePlaybackPercent(const UAudioComponent* Component, const USoundWave* Wave, const float Percent)
{
    if (Wave == nullptr || Component != VoiceAudio)
    {
        return;
    }
    const float Seconds = Percent * Wave->Duration;
    // The last word that has started; before the first one, nothing is lit.
    const int32 Index = Algo::UpperBound(VoiceWordTimes, Seconds) - 1;
    SetVoiceWord(Index >= 0 ? Index : INDEX_NONE);
}

void ULFFJourneySubsystem::SetVoiceWord(const int32 Index)
{
    if (Index != VoiceWordIndex)
    {
        VoiceWordIndex = Index;
        OnVoiceWord.Broadcast(VoiceWordIndex);
    }
}

void ULFFJourneySubsystem::HandleVoiceFinished()
{
    UE_LOG(LogVirtualStadium, Log, TEXT("Voice finished."));
    SetVoiceWord(INDEX_NONE);
    VoiceAudio = nullptr;
    bVoicePlaying = false;
    ApplyAudio();
    Broadcast();
}

void ULFFJourneySubsystem::SnapCameraToCurrentStep()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    ALFFCameraTarget* Target = Step != nullptr ? FindTarget(Step->CameraTargetId) : nullptr;
    bTargetMissing = Step != nullptr && Target == nullptr;

    if (Target != nullptr && Rig != nullptr)
    {
        Rig->SnapTo(Target->GetCameraTransform(), Target->FieldOfView);
    }
}

void ULFFJourneySubsystem::ApplyLabels()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();

    // Labels are checked against the step's framed view, not wherever the camera is mid-move.
    const ALFFCameraTarget* Target = Step != nullptr ? FindTarget(Step->CameraTargetId) : nullptr;
    float Aspect = 16.0f / 9.0f;
    if (const UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        FVector2D Size;
        Viewport->GetViewportSize(Size);
        if (Size.X > 1.0f && Size.Y > 1.0f)
        {
            Aspect = Size.X / Size.Y;
        }
    }

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor->ActorHasTag(LabelTag))
        {
            continue;
        }

        bool bShow = false;
        if (Step != nullptr)
        {
            for (const FName& Label : Step->LabelsToShow)
            {
                if (Actor->ActorHasTag(Label))
                {
                    bShow = true;
                    break;
                }
            }
        }

        if (bShow && Target != nullptr)
        {
            // Keep the lower third (the caption card and the rail) clear of world labels.
            FVector2D Fraction;
            const bool bInFront = ProjectToView(Target->GetCameraTransform(), Target->FieldOfView, Aspect, Actor->GetActorLocation(), Fraction);
            if (LFFJourneyRules::ShouldHideLabelAt(bInFront, Fraction))
            {
                UE_LOG(LogVirtualStadium, Verbose, TEXT("Label %s hidden: it would sit at (%.2f, %.2f) in the frame."), *Actor->GetActorNameOrLabel(), Fraction.X, Fraction.Y);
                bShow = false;
            }
        }

        Actor->SetActorHiddenInGame(!bShow);
    }
}

void ULFFJourneySubsystem::ApplySensory()
{
    ApplyAudio();
    ApplyCrowd();
    ApplyPreviewCues();

    if (Rig != nullptr && GameMode != nullptr)
    {
        // Standard brightens the bright-lights step by itself (the same slow sine, never a flash). Calm stays flat.
        const FLFFJourneyStep* LightsStep = Machine.GetCurrentStep();
        const bool bAutoBrighten = LightsStep != nullptr && LightsStep->StepId == FName(TEXT("bright-lights")) && !Settings.bCalmMode;
        const bool bPulse = bAutoBrighten || (bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Lights
            && LFFJourneyRules::CanPlayPreview(ELFFIntensityPreview::Lights, Settings));
        if (bPulse != bLoggedPulse)
        {
            UE_LOG(LogVirtualStadium, Log, TEXT("Lights preview %s (%.1f s cycle, up to +%.1f stops)."),
                bPulse ? TEXT("on") : TEXT("off"), GameMode->LightPulseSeconds, GameMode->LightPulseStops);
            bLoggedPulse = bPulse;
        }
        Rig->SetLightPulse(bPulse, GameMode->LightPulseSeconds, GameMode->LightPulseStops);
    }
}

void ULFFJourneySubsystem::ApplyCrowd()
{
    const bool bShown = Settings.IsCrowdShown();
    int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(CrowdTag))
        {
            It->SetActorHiddenInGame(!bShown);
            if (ALFFCrowdFanActor* Fan = Cast<ALFFCrowdFanActor>(*It))
            {
                Fan->SetCrowdActive(bShown, !Settings.IsReducedMotionEffective());
            }
            ++Count;
            // The bowl crowd is Mass entities, not actors: despawn it for Crowd Off or Calm Mode, spawn it back after.
            if (AMassSpawner* Spawner = Cast<AMassSpawner>(*It); Spawner != nullptr && bShown != bCrowdSpawnersShown)
            {
                bShown ? Spawner->DoSpawning() : Spawner->DoDespawning();
            }
        }
    }

    bCrowdSpawnersShown = bShown;

    if (bShown != bLoggedCrowdShown)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Crowd %s: %d stand-in people %s."), bShown ? TEXT("Low") : TEXT("Off"), Count,
            bShown ? TEXT("shown") : TEXT("hidden"));
        bLoggedCrowdShown = bShown;
    }

    if (Settings.bCalmMode != bLoggedCalm)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Calm Mode %s: %d crowd actors %s."), Settings.bCalmMode ? TEXT("on") : TEXT("off"),
            Count, bShown ? TEXT("shown") : TEXT("hidden"));
        bLoggedCalm = Settings.bCalmMode;
    }
}

void ULFFJourneySubsystem::UpdateExteriorVisibility()
{
    const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (Camera == nullptr)
    {
        return;
    }

    const FVector Location = Camera->GetCameraLocation();
    bool bInside = false;
    for (const UBoxComponent* Box : InteriorBoxes)
    {
        if (Box != nullptr)
        {
            const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Location);
            const FVector Extent = Box->GetUnscaledBoxExtent();
            if (FBox(-Extent, Extent).IsInsideOrOn(Local))
            {
                bInside = true;
                break;
            }
        }
    }

    if (bInside == bExteriorHidden)
    {
        return;
    }

    bExteriorHidden = bInside;
    for (AActor* Actor : ExteriorActors)
    {
        if (Actor != nullptr)
        {
            Actor->SetActorHiddenInGame(bInside);
        }
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Camera %s the stadium: %d exterior actors (parking lot) %s."), bInside ? TEXT("inside") : TEXT("outside"),
        ExteriorActors.Num(), bInside ? TEXT("hidden") : TEXT("shown"));
}

#if !UE_BUILD_SHIPPING
namespace
{
    constexpr float ShotDelaySeconds = 9.0f;
    FAutoConsoleCommandWithWorldAndArgs GLFFShotsCommand(
        TEXT("LFF.Shots"),
        TEXT("LFF.Shots [welcome] <step>[@seconds] ... [large] [explore] [tour] ... [quit]: screenshot the welcome screen, steps (1-based; @ sets the wait, to catch the voice mid-line), Large text (toggles it) and the modes (dev only)."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (ULFFJourneySubsystem* Journey = World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr)
            {
                Journey->StartShots(Args);
            }
        }));
}

void ULFFJourneySubsystem::StartShots(const TArray<FString>& Args)
{
    ShotQueue.Reset();
    bShotsQuit = false;
    for (const FString& Arg : Args)
    {
        // A step may carry its own wait: 5@4.5.
        FString StepPart = Arg;
        Arg.Split(TEXT("@"), &StepPart, nullptr);
        if (Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase))
        {
            bShotsQuit = true;
        }
        else if (StepPart.IsNumeric() || Arg.Equals(TEXT("welcome"), ESearchCase::IgnoreCase) || Arg.Equals(TEXT("explore"), ESearchCase::IgnoreCase)
            || Arg.Equals(TEXT("tour"), ESearchCase::IgnoreCase) || Arg.Equals(TEXT("fly"), ESearchCase::IgnoreCase)
            || Arg.Equals(TEXT("large"), ESearchCase::IgnoreCase) || Arg.StartsWith(TEXT("level:"), ESearchCase::IgnoreCase) || Arg.StartsWith(TEXT("pick:"), ESearchCase::IgnoreCase))
        {
            ShotQueue.Add(Arg.ToLower());
        }
    }
    // Give the journey (and the tiles) time to come up: -ExecCmds can run before the first tick.
    bShotTaken = true;
    GetWorld()->GetTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::NextShot), 12.0f, false);
    UE_LOG(LogVirtualStadium, Log, TEXT("Shots: %d shots."), ShotQueue.Num());
}

void ULFFJourneySubsystem::NextShot()
{
    if (!bShotTaken)
    {
        FScreenshotRequest::RequestScreenshot(ShotName, true, false);
        UE_LOG(LogVirtualStadium, Log, TEXT("Shots: %s, step %d (%s), captured."), *ShotName, Machine.GetIndex() + 1,
            *UEnum::GetDisplayValueAsText(Machine.GetMode()).ToString());
        bShotTaken = true;
        GetWorld()->GetTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::NextShot), 1.0f, false);
        return;
    }
    if (ShotQueue.Num() == 0)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Shots: DONE."));
        if (bShotsQuit)
        {
            FPlatformMisc::RequestExit(false, TEXT("LFF.Shots"));
        }
        return;
    }

    FString Token = ShotQueue[0];
    ShotQueue.RemoveAt(0);
    float Delay = ShotDelaySeconds;
    FString Wait;
    if (Token.Split(TEXT("@"), &Token, &Wait))
    {
        Delay = FMath::Max(FCString::Atof(*Wait), 0.5f);
    }
    if (Token == TEXT("welcome"))
    {
        ShotName = TEXT("LFF_welcome");
    }
    else if (Token == TEXT("explore"))
    {
        SwitchToMode(ELFFSimulationMode::FreeExplore);
        ShotName = FString::Printf(TEXT("LFF_explore_from_step_%02d"), Machine.GetIndex() + 1);
    }
    else if (Token == TEXT("tour"))
    {
        SwitchToMode(ELFFSimulationMode::GuidedTour);
        ShotName = FString::Printf(TEXT("LFF_tour_resumed_step_%02d"), Machine.GetIndex() + 1);
    }
    else if (Token == TEXT("large"))
    {
        ToggleLargeText();
        ShotName = FString::Printf(TEXT("LFF_large_text_%s_step_%02d"), Settings.bLargeText ? TEXT("on") : TEXT("off"), Machine.GetIndex() + 1);
    }
    else if (Token == TEXT("fly"))
    {
        ShotFlySecondsLeft = 1.5f;
        ShotName = TEXT("LFF_explore_flown");
    }
    else if (Token.StartsWith(TEXT("level:")))
    {
        const FString Name = Token.RightChop(6);
        const int64 Value = StaticEnum<ELFFMapLevel>()->GetValueByNameString(Name);
        if (Value != INDEX_NONE)
        {
            SetMapLevel(static_cast<ELFFMapLevel>(Value));
        }
        ShotName = FString::Printf(TEXT("LFF_level_%s"), *Name);
    }
    else if (Token.StartsWith(TEXT("pick:")))
    {
        SelectAmenity(FName(*Token.RightChop(5)));
        ShotName = FString::Printf(TEXT("LFF_pick_%s"), *Token.RightChop(5));
    }
    else
    {
        if (!Machine.HasStarted() && !IsExploring())
        {
            Next();
        }
        JumpToStep(FCString::Atoi(*Token) - 1);
        ShotName = Wait.IsEmpty() ? FString::Printf(TEXT("LFF_step_%02d"), Machine.GetIndex() + 1)
                                  : FString::Printf(TEXT("LFF_step_%02d_at_%ss"), Machine.GetIndex() + 1, *Wait.Replace(TEXT("."), TEXT("_")));
    }
    bShotTaken = false;
    GetWorld()->GetTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::NextShot), Delay, false);
}
#endif

void ULFFJourneySubsystem::ResetPreview()
{
    if (!bPreviewOn)
    {
        return;
    }

    bPreviewOn = false;
    ApplySensory();
}

ELFFIntensityPreview ULFFJourneySubsystem::GetCurrentPreviewKind() const
{
    const FLFFJourneyStep* Step = Machine.HasStarted() && !IsExploring() ? Machine.GetCurrentStep() : nullptr;
    return Step != nullptr ? Step->IntensityPreview : ELFFIntensityPreview::None;
}

bool ULFFJourneySubsystem::IsPathBlocked(const FVector& From, const FVector& To) const
{
    if (From.Equals(To, 1.0f))
    {
        return false;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(LFFCameraPath), true);
    if (Rig != nullptr)
    {
        Params.AddIgnoredActor(Rig.Get());
    }

    // Both directions: a trace that starts inside a mesh doesn't report the face it exits through.
    FHitResult Hit;
    const FCollisionShape Probe = FCollisionShape::MakeSphere(CameraProbeRadius);
    return GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Visibility, Probe, Params)
        || GetWorld()->SweepSingleByChannel(Hit, To, From, FQuat::Identity, ECC_Visibility, Probe, Params);
}

void ULFFJourneySubsystem::HandlePreviewAudioFinished()
{
    // The sample is short and plays once; the button goes back to Off so it can be heard again.
    UE_LOG(LogVirtualStadium, Log, TEXT("Sound preview finished."));
    PreviewAudio = nullptr;
    if (bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Sound)
    {
        bPreviewOn = false;
        ApplySensory();
        Broadcast();
    }
}

void ULFFJourneySubsystem::ApplyAudio()
{
    if (GameMode == nullptr)
    {
        return;
    }

    // Sound Off is silence: the whole device is muted, so no UI or engine sound can slip through either.
    const bool bSound = Settings.IsSoundEffective();
    SetPrimaryVolume(GetWorld(), bSound ? 1.0f : 0.0f);
    if (bSound != bLoggedSound)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("%s"), bSound ? TEXT("Sound on: primary volume 1, ambient bed only.")
            : TEXT("Sound off: primary volume 0, every sound muted."));
        bLoggedSound = bSound;
    }
    if (!bSound)
    {
        StopVoice(false);
        StopAudio();
        return;
    }

    const bool bPreviewSound = bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Sound
        && LFFJourneyRules::CanPlayPreview(ELFFIntensityPreview::Sound, Settings);

    // Sound On is one low ambient bed, on the splash too. A step may name its own.
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    USoundBase* Bed = (Step != nullptr && !Step->AudioBed.IsNull()) ? Step->AudioBed.LoadSynchronous() : GameMode->AmbientBed.LoadSynchronous();
    // The bed dips under the preview sample and under the voice, so each stays the one clear sound.
    const float VoiceDuck = bVoicePlaying ? LFFJourneyRules::DecibelsToGain(GameMode->VoiceDuckDecibels) : 1.0f;
    const float BedVolume = GameMode->BedVolume * (bPreviewSound ? BedDuckWhilePreview : 1.0f) * VoiceDuck;

    if (Bed == nullptr)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Sound is on but no ambient bed was found at %s."), *GameMode->AmbientBed.ToString());
    }
    else if (IsValid(BedAudio) && BedAudio->Sound == Bed)
    {
        BedAudio->AdjustVolume(0.5f, BedVolume);
    }
    else
    {
        // A different bed (plaza murmur to seat cheer) cross-fades; turning Sound on fades in over 2 s.
        const bool bCrossfade = IsValid(BedAudio);
        if (bCrossfade)
        {
            BedAudio->FadeOut(GameMode->BedCrossfadeSeconds, 0.0f);
        }
        // Looping comes from the asset.
        BedAudio = UGameplayStatics::CreateSound2D(this, Bed, BedVolume, 1.0f, 0.0f, nullptr, false, true);
        if (BedAudio != nullptr)
        {
            BedAudio->FadeIn(bCrossfade ? GameMode->BedCrossfadeSeconds : 2.0f, BedVolume);
            UE_LOG(LogVirtualStadium, Log, TEXT("Ambient bed playing: %s at volume %.2f (looping=%d)."), *Bed->GetName(), BedVolume, Bed->IsLooping());
        }
    }

    if (!bPreviewSound)
    {
        StopPreviewAudio();
        return;
    }

    if (!IsValid(PreviewAudio))
    {
        USoundBase* Sample = (Step != nullptr && !Step->PreviewSound.IsNull()) ? Step->PreviewSound.LoadSynchronous() : GameMode->PreviewSound.LoadSynchronous();
        if (Sample == nullptr)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Sound preview requested but no sample was found at %s."), *GameMode->PreviewSound.ToString());
            return;
        }

        PreviewAudio = UGameplayStatics::CreateSound2D(this, Sample, GameMode->PreviewVolume, 1.0f, 0.0f, nullptr, false, false);
        if (PreviewAudio != nullptr)
        {
            PreviewAudio->OnAudioFinished.AddDynamic(this, &ULFFJourneySubsystem::HandlePreviewAudioFinished);
            PreviewAudio->FadeIn(1.0f, GameMode->PreviewVolume);
            UE_LOG(LogVirtualStadium, Log, TEXT("Sound preview playing: %s (%.1f s) at volume %.2f; bed ducked to %.2f."),
                *Sample->GetName(), Sample->GetDuration(), GameMode->PreviewVolume, BedVolume);
        }
    }
}

void ULFFJourneySubsystem::StopAudio()
{
    if (IsValid(BedAudio))
    {
        BedAudio->Stop();
    }

    BedAudio = nullptr;
    StopPreviewAudio();
}

void ULFFJourneySubsystem::StopPreviewAudio()
{
    if (IsValid(PreviewAudio))
    {
        PreviewAudio->OnAudioFinished.RemoveDynamic(this, &ULFFJourneySubsystem::HandlePreviewAudioFinished);
        // A short fade, never a hard cut.
        PreviewAudio->FadeOut(0.4f, 0.0f);
    }

    PreviewAudio = nullptr;
}

void ULFFJourneySubsystem::Broadcast()
{
    FLFFJourneySnapshot NewSnapshot;
    NewSnapshot.Phase = Machine.GetPhase();
    NewSnapshot.Mode = Machine.GetMode();
    NewSnapshot.TourStep = Machine.GetTourStep();
    NewSnapshot.TourStepFirstSteps = Machine.GetTourStepFirstSteps();
    NewSnapshot.bCanResumeTour = CanUseMap();
    NewSnapshot.MapLevel = MapLevel;
    NewSnapshot.bLevelHighlight = IsExploring() && bLevelHighlight;
    NewSnapshot.AmenityMask = AmenityMask;
    NewSnapshot.SelectedAmenityId = SelectedAmenityId;
    NewSnapshot.ViewTargetId = ViewTargetId;
    if (GameMode != nullptr)
    {
        NewSnapshot.MySeatTargetId = GameMode->MySeatTargetId;
        NewSnapshot.MySeatLevel = GameMode->MySeatLevel;
    }
    NewSnapshot.StepIndex = Machine.GetIndex();
    NewSnapshot.StepCount = Machine.Num();
    NewSnapshot.bStarted = Machine.HasStarted();
    NewSnapshot.bPaused = Machine.IsPaused();
    NewSnapshot.bAtEnd = Machine.IsAtEnd();
    NewSnapshot.bCanNext = !IsModalOpen() && !IsExploring()
        && (Machine.GetPhase() == ELFFJourneyPhase::Intro || Machine.CanNext() || (NewSnapshot.bAtEnd && !NewSnapshot.bPaused));
    const FLFFJourneyStep* CurrentStep = Machine.GetCurrentStep();
    const bool bBackToIntro = LFFJourneyRules::ShouldBackReturnToIntro(
        NewSnapshot.bStarted, NewSnapshot.StepIndex, NewSnapshot.bPaused, CurrentStep != nullptr && CurrentStep->bBackEnabled);
    NewSnapshot.bCanBack = !IsModalOpen() && !IsExploring() && (Machine.CanBack() || bBackToIntro);
    NewSnapshot.bSettingsOpen = bSettingsOpen;
    NewSnapshot.bRestartConfirmOpen = bRestartConfirmOpen;
    NewSnapshot.bTargetMissing = bTargetMissing;
    NewSnapshot.Settings = Settings;
    NewSnapshot.IntensityPreview = GetCurrentPreviewKind();
    NewSnapshot.bShowPreviewControls = LFFJourneyRules::ShouldShowPreviewControls(NewSnapshot.IntensityPreview, Settings);
    NewSnapshot.bPreviewOn = bPreviewOn;
    NewSnapshot.bPreviewAvailable = LFFJourneyRules::CanPlayPreview(NewSnapshot.IntensityPreview, Settings);
    NewSnapshot.TimelineMarks = TimelineMarks;
    NewSnapshot.bVoicePlaying = bVoicePlaying;

    if (GameMode != nullptr)
    {
        NewSnapshot.PrototypeBadgeText = GameMode->PrototypeBadgeText;
    }

    if (const FLFFJourneyStep* Step = Machine.GetCurrentStep())
    {
        NewSnapshot.StepId = Step->StepId;
        NewSnapshot.DisclaimerLevel = Step->DisclaimerLevel;
        NewSnapshot.PreviewImage = Step->PreviewImage;

        if (NewSnapshot.bStarted)
        {
            NewSnapshot.Title = Step->Title;
            NewSnapshot.Body = Step->Body;
            NewSnapshot.ClockText = Step->ClockText;
            NewSnapshot.bShowTicketPhone = Step->bShowTicketPhone;
            if (!IsExploring())
            {
                NewSnapshot.TourPlaces = Step->PlacesToShow;
            }
            NewSnapshot.ProgressText = FText::Format(LOCTEXT("Progress", "Step {0} of {1}"), FText::AsNumber(NewSnapshot.StepIndex + 1), FText::AsNumber(NewSnapshot.StepCount));
        }
    }

    if (!NewSnapshot.bStarted && GameMode != nullptr)
    {
        NewSnapshot.Title = GameMode->IntroTitle;
        NewSnapshot.Body = bJourneyMissing ? GameMode->JourneyMissingText : GameMode->IntroBody;
    }

    // The banner shows unless the location is Verified. The intro always shows it.
    NewSnapshot.bShowDisclaimer = !NewSnapshot.bStarted || LFFJourneyRules::ShouldShowDisclaimer(NewSnapshot.DisclaimerLevel);
    if (GameMode != nullptr)
    {
        NewSnapshot.DisclaimerText = GameMode->GetDisclaimerText(NewSnapshot.bStarted ? NewSnapshot.DisclaimerLevel : ELFFDisclaimerLevel::Illustrative);
        NewSnapshot.DisclaimerLine = GameMode->GetDisclaimerLine(NewSnapshot.bStarted ? NewSnapshot.DisclaimerLevel : ELFFDisclaimerLevel::Illustrative);
    }

    UE_LOG(LogVirtualStadium, Verbose, TEXT("Journey: mode=%s started=%d step=%d/%d (%s) paused=%d settings=%d restartConfirm=%d canBack=%d canNext=%d sound=%d calm=%d lessMovement=%d previewControls=%d preview=%d voice=%d/%d crowd=%d"),
        *UEnum::GetDisplayValueAsText(NewSnapshot.Mode).ToString(),
        NewSnapshot.bStarted, NewSnapshot.StepIndex + 1, NewSnapshot.StepCount, *NewSnapshot.StepId.ToString(), NewSnapshot.bPaused,
        NewSnapshot.bSettingsOpen, NewSnapshot.bRestartConfirmOpen, NewSnapshot.bCanBack, NewSnapshot.bCanNext,
        Settings.IsSoundEffective(), Settings.bCalmMode, Settings.IsReducedMotionEffective(), NewSnapshot.bShowPreviewControls, bPreviewOn,
        Settings.IsVoiceEffective(), bVoicePlaying, Settings.IsCrowdShown());

    Snapshot = NewSnapshot;
    OnJourneyChanged.Broadcast(Snapshot);
}

#undef LOCTEXT_NAMESPACE
