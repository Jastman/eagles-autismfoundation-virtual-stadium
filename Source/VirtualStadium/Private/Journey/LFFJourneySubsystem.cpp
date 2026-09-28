#include "Journey/LFFJourneySubsystem.h"

#include "Camera/LFFCameraRig.h"
#include "Camera/LFFCameraTarget.h"
#include "Components/AudioComponent.h"
#include "Core/LFFGameMode.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "VirtualStadium.h"

#define LOCTEXT_NAMESPACE "LFFJourney"

namespace
{
    const FName LabelTag(TEXT("LFF_Label"));
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
    StopAudio();
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

    Rig = FindOrSpawnRig();
    if (Rig != nullptr)
    {
        if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
        {
            PlayerController->SetViewTarget(Rig.Get());
        }
    }

    // The intro sits on the first step's view, so pressing Start never moves the camera.
    SnapCameraToCurrentStep();
    ApplyLabels();
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
        Machine.Start();
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

    Broadcast();
}

void ULFFJourneySubsystem::Resume()
{
    Machine.Resume();
    if (Rig != nullptr)
    {
        Rig->SetTransitionPaused(false);
    }

    Broadcast();
}

void ULFFJourneySubsystem::RequestRestart()
{
    if (!Machine.HasStarted() || bSettingsOpen)
    {
        return;
    }

    bRestartConfirmOpen = true;
    Broadcast();
}

void ULFFJourneySubsystem::ConfirmRestart()
{
    bRestartConfirmOpen = false;
    bSettingsOpen = false;

    if (Machine.Restart() == ELFFCommandResult::Accepted)
    {
        if (Rig != nullptr)
        {
            Rig->SetTransitionPaused(false);
        }

        MoveCameraToCurrentStep(true);
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
    Settings.bReducedMotion = !Settings.bReducedMotion;
    Broadcast();
}

void ULFFJourneySubsystem::ToggleSound()
{
    Settings.bSoundEnabled = !Settings.bSoundEnabled;
    ApplyAudio();
    Broadcast();
}

void ULFFJourneySubsystem::ToggleCalmMode()
{
    Settings.bCalmMode = !Settings.bCalmMode;
    ApplyAudio();
    Broadcast();
}

void ULFFJourneySubsystem::ToggleLargeText()
{
    Settings.bLargeText = !Settings.bLargeText;
    Broadcast();
}

void ULFFJourneySubsystem::HandleRigTransitionFinished(const uint32 TransitionId)
{
    if (Machine.NotifyTransitionFinished(TransitionId))
    {
        Broadcast();
    }
}

void ULFFJourneySubsystem::MoveCameraToCurrentStep(const bool bForceFade)
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    ALFFCameraTarget* Target = Step != nullptr ? FindTarget(Step->CameraTargetId) : nullptr;
    bTargetMissing = Target == nullptr;

    ApplyLabels();
    ApplyAudio();

    if (Target == nullptr || Rig == nullptr || GameMode == nullptr)
    {
        if (Step != nullptr)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Step %s: camera target '%s' not found. The camera stays put."), *Step->StepId.ToString(), *Step->CameraTargetId.ToString());
        }

        // Fail closed: don't move, don't leave the journey stuck mid-transition.
        Machine.NotifyTransitionFinished(Machine.GetTransitionId());
        return;
    }

    const ELFFTransitionProfile Profile = bForceFade
        ? ELFFTransitionProfile::ReducedMotion
        : LFFJourneyRules::ResolveTransitionProfile(Step->TransitionProfile, Settings.IsReducedMotionEffective());

    const float EaseSeconds = LFFJourneyRules::ClampDuration(
        Step->TransitionSeconds, GameMode->DefaultEaseSeconds, GameMode->MinTransitionSeconds, GameMode->MaxTransitionSeconds);

    Rig->BeginTransition(
        Target->GetCameraTransform(),
        Target->FieldOfView,
        Profile,
        EaseSeconds,
        GameMode->ReducedMotionFadeSeconds,
        GameMode->FadeColor,
        Machine.GetTransitionId());
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

        Actor->SetActorHiddenInGame(!bShow);
    }
}

void ULFFJourneySubsystem::ApplyAudio()
{
    const FLFFJourneyStep* Step = Machine.GetCurrentStep();
    USoundBase* Bed = (Settings.bSoundEnabled && Step != nullptr) ? Step->AudioBed.LoadSynchronous() : nullptr;

    if (Bed == nullptr)
    {
        StopAudio();
        return;
    }

    const float Volume = GameMode != nullptr
        ? GameMode->BedVolume * (Settings.bCalmMode ? GameMode->CalmVolumeMultiplier : 1.0f)
        : 0.3f;

    if (BedAudio != nullptr && BedAudio->Sound == Bed)
    {
        BedAudio->SetVolumeMultiplier(Volume);
        return;
    }

    StopAudio();
    BedAudio = UGameplayStatics::SpawnSound2D(this, Bed, Volume, 1.0f, 0.0f, nullptr, false, true);
}

void ULFFJourneySubsystem::StopAudio()
{
    if (IsValid(BedAudio))
    {
        BedAudio->Stop();
    }

    BedAudio = nullptr;
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
    NewSnapshot.bCanBack = !IsModalOpen() && Machine.CanBack();
    NewSnapshot.bSettingsOpen = bSettingsOpen;
    NewSnapshot.bRestartConfirmOpen = bRestartConfirmOpen;
    NewSnapshot.bTargetMissing = bTargetMissing;
    NewSnapshot.Settings = Settings;

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
    }

    Snapshot = NewSnapshot;
    OnJourneyChanged.Broadcast(Snapshot);
}

#undef LOCTEXT_NAMESPACE
