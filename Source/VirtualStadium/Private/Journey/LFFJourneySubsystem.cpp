#include "Journey/LFFJourneySubsystem.h"

#include "Camera/LFFCameraRig.h"
#include "Camera/LFFCameraTarget.h"
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
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "VirtualStadium.h"
#include "World/LFFCueActor.h"

#define LOCTEXT_NAMESPACE "LFFJourney"

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
    if (IsModalOpen())
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
    if (IsModalOpen())
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

    // "Nothing will move until you resume": a running preview and the voice stop too.
    StopVoice();
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

void ULFFJourneySubsystem::JumpToStep(const int32 StepIndex)
{
    if (IsModalOpen())
    {
        return;
    }

    if (Machine.JumpTo(StepIndex) == ELFFCommandResult::Accepted)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Timeline: jump to step %d."), StepIndex + 1);
        MoveCameraToCurrentStep(false);
    }

    Broadcast();
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

    // A new step silences the old one at once: no voice overlaps the next card.
    StopVoice();
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
    if (Rig == nullptr || GameMode == nullptr || Machine.HasStarted())
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
    ScheduleVoice();
}

void ULFFJourneySubsystem::ApplyCues(const ELFFCueReason Reason)
{
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
            if (State == ELFFCueState::Play && bInstant)
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
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    float Longest = 0.0f;
    int32 Count = 0;
    for (ALFFCueActor* Actor : CueActors)
    {
        if (!IsValid(Actor) || Actor->Trigger != ELFFCueTrigger::Preview || Step == nullptr || !Step->SceneCues.Contains(Actor->CueId))
        {
            continue;
        }

        if (bWant)
        {
            Actor->Play(false);
            Longest = FMath::Max(Longest, Actor->GetTotalSeconds());
            ++Count;
        }
        else
        {
            Actor->ApplyState(ELFFCueState::Before);
        }
    }

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    Timers.ClearTimer(PreviewCueTimer);
    if (bWant)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Anthem preview playing: %d actors, %.1f s, one pass."), Count, Longest);
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

void ULFFJourneySubsystem::ScheduleVoice()
{
    StopVoice();
    if (!bArrived || Machine.IsPaused() || !LFFJourneyRules::ShouldPlayVoice(Settings, Machine.HasStarted(), bPreviewOn))
    {
        return;
    }

    const float Delay = GameMode != nullptr ? GameMode->VoiceStartDelaySeconds : 0.5f;
    GetWorld()->GetTimerManager().SetTimer(VoiceTimer, FTimerDelegate::CreateUObject(this, &ULFFJourneySubsystem::StartVoice), FMath::Max(Delay, 0.01f), false);
}

void ULFFJourneySubsystem::StartVoice()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    if (Step == nullptr || GameMode == nullptr || !LFFJourneyRules::ShouldPlayVoice(Settings, Machine.HasStarted(), bPreviewOn))
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
        // Immediately, not a fade: the next card must never be read over the last one.
        VoiceAudio->Stop();
        UE_LOG(LogVirtualStadium, Log, TEXT("Voice stopped."));
    }
    VoiceAudio = nullptr;

    if (bVoicePlaying)
    {
        bVoicePlaying = false;
        if (bReapplyAudio)
        {
            ApplyAudio();
        }
    }
}

void ULFFJourneySubsystem::HandleVoiceFinished()
{
    UE_LOG(LogVirtualStadium, Log, TEXT("Voice finished."));
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
        const bool bPulse = bPreviewOn && GetCurrentPreviewKind() == ELFFIntensityPreview::Lights
            && LFFJourneyRules::CanPlayPreview(ELFFIntensityPreview::Lights, Settings);
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
            ++Count;
        }
    }

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
    const FLFFJourneyStep* Step = Machine.HasStarted() ? Machine.GetCurrentStep() : nullptr;
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
    NewSnapshot.StepIndex = Machine.GetIndex();
    NewSnapshot.StepCount = Machine.Num();
    NewSnapshot.bStarted = Machine.HasStarted();
    NewSnapshot.bPaused = Machine.IsPaused();
    NewSnapshot.bAtEnd = Machine.IsAtEnd();
    NewSnapshot.bCanNext = !IsModalOpen() && (Machine.GetPhase() == ELFFJourneyPhase::Intro || Machine.CanNext() || (NewSnapshot.bAtEnd && !NewSnapshot.bPaused));
    const FLFFJourneyStep* CurrentStep = Machine.GetCurrentStep();
    const bool bBackToIntro = LFFJourneyRules::ShouldBackReturnToIntro(
        NewSnapshot.bStarted, NewSnapshot.StepIndex, NewSnapshot.bPaused, CurrentStep != nullptr && CurrentStep->bBackEnabled);
    NewSnapshot.bCanBack = !IsModalOpen() && (Machine.CanBack() || bBackToIntro);
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

    UE_LOG(LogVirtualStadium, Verbose, TEXT("Journey: started=%d step=%d/%d (%s) paused=%d settings=%d restartConfirm=%d canBack=%d canNext=%d sound=%d calm=%d lessMovement=%d previewControls=%d preview=%d voice=%d/%d crowd=%d"),
        NewSnapshot.bStarted, NewSnapshot.StepIndex + 1, NewSnapshot.StepCount, *NewSnapshot.StepId.ToString(), NewSnapshot.bPaused,
        NewSnapshot.bSettingsOpen, NewSnapshot.bRestartConfirmOpen, NewSnapshot.bCanBack, NewSnapshot.bCanNext,
        Settings.IsSoundEffective(), Settings.bCalmMode, Settings.IsReducedMotionEffective(), NewSnapshot.bShowPreviewControls, bPreviewOn,
        Settings.IsVoiceEffective(), bVoicePlaying, Settings.IsCrowdShown());

    Snapshot = NewSnapshot;
    OnJourneyChanged.Broadcast(Snapshot);
}

#undef LOCTEXT_NAMESPACE
