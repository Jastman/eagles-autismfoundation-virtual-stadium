#include "Camera/LFFCameraRig.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Journey/LFFJourneySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ALFFCameraRig::ALFFCameraRig()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    if (UCameraComponent* Camera = GetCameraComponent())
    {
        Camera->SetConstraintAspectRatio(false);
        Camera->SetFieldOfView(70.0f);
    }
}

void ALFFCameraRig::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bEasing || bEasePaused)
    {
        return;
    }

    Elapsed += DeltaSeconds;
    const float Alpha = Duration <= KINDA_SMALL_NUMBER ? 1.0f : FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
    const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

    FRotator Rotation = FQuat::Slerp(StartTransform.GetRotation(), EndTransform.GetRotation(), Eased).Rotator();
    Rotation.Roll = 0.0f;
    SetActorLocationAndRotation(FMath::Lerp(StartTransform.GetLocation(), EndTransform.GetLocation(), Eased), Rotation);

    if (UCameraComponent* Camera = GetCameraComponent())
    {
        Camera->SetFieldOfView(FMath::Lerp(StartFieldOfView, EndFieldOfView, Eased));
    }

    if (Alpha >= 1.0f)
    {
        FinishTransition(ActiveTransitionId);
    }
}

void ALFFCameraRig::BeginTransition(
    const FTransform& TargetTransform,
    const float TargetFieldOfView,
    const ELFFTransitionProfile Profile,
    const float EaseSeconds,
    const float FadeSeconds,
    const FLinearColor& FadeColor,
    const uint32 TransitionId)
{
    StopActiveTransition();
    ActiveTransitionId = TransitionId;

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    switch (Profile)
    {
    case ELFFTransitionProfile::Ease:
    {
        // Start from wherever the camera is now, so retargeting mid-move never jumps.
        StartTransform = GetActorTransform();
        EndTransform = TargetTransform;
        StartFieldOfView = GetCameraComponent() != nullptr ? GetCameraComponent()->FieldOfView : TargetFieldOfView;
        EndFieldOfView = TargetFieldOfView;
        Elapsed = 0.0f;
        Duration = EaseSeconds;
        bEasing = true;
        SetActorTickEnabled(true);
        break;
    }

    case ELFFTransitionProfile::ReducedMotion:
    {
        // Fade to a neutral colour, cut while hidden, fade back. No camera path is ever visible.
        const float Fade = FMath::Max(FadeSeconds, 0.05f);
        StartFade(1.0f, Fade, FadeColor, true);

        const FTransform CutTransform = TargetTransform;
        const float CutFieldOfView = TargetFieldOfView;
        World->GetTimerManager().SetTimer(
            FadeOutTimer,
            FTimerDelegate::CreateWeakLambda(this, [this, CutTransform, CutFieldOfView, Fade, FadeColor, TransitionId]()
            {
                if (TransitionId != ActiveTransitionId)
                {
                    return;
                }

                SnapTo(CutTransform, CutFieldOfView);
                StartFade(0.0f, Fade, FadeColor, false);

                if (UWorld* InnerWorld = GetWorld())
                {
                    InnerWorld->GetTimerManager().SetTimer(
                        FadeInTimer,
                        FTimerDelegate::CreateWeakLambda(this, [this, TransitionId]() { FinishTransition(TransitionId); }),
                        Fade,
                        false);
                }
            }),
            Fade,
            false);
        break;
    }

    case ELFFTransitionProfile::Instant:
    default:
    {
        SnapTo(TargetTransform, TargetFieldOfView);
        // Report on the next tick so callers never re-enter the subsystem mid-command.
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(this, [this, TransitionId]() { FinishTransition(TransitionId); }));
        break;
    }
    }
}

void ALFFCameraRig::SnapTo(const FTransform& TargetTransform, const float TargetFieldOfView)
{
    FRotator Rotation = TargetTransform.Rotator();
    Rotation.Roll = 0.0f;
    SetActorLocationAndRotation(TargetTransform.GetLocation(), Rotation);

    if (UCameraComponent* Camera = GetCameraComponent())
    {
        Camera->SetFieldOfView(TargetFieldOfView);
    }
}

void ALFFCameraRig::SetTransitionPaused(const bool bPaused)
{
    bEasePaused = bPaused;
}

void ALFFCameraRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopActiveTransition();
    Super::EndPlay(EndPlayReason);
}

void ALFFCameraRig::StopActiveTransition()
{
    bEasing = false;
    bEasePaused = false;
    SetActorTickEnabled(false);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FadeOutTimer);
        World->GetTimerManager().ClearTimer(FadeInTimer);
    }
}

void ALFFCameraRig::FinishTransition(const uint32 TransitionId)
{
    // A newer transition has replaced this one; its own completion will report.
    if (TransitionId != ActiveTransitionId)
    {
        return;
    }

    if (bEasing)
    {
        SnapTo(EndTransform, EndFieldOfView);
    }

    bEasing = false;
    SetActorTickEnabled(false);

    if (UWorld* World = GetWorld())
    {
        if (ULFFJourneySubsystem* Journey = World->GetSubsystem<ULFFJourneySubsystem>())
        {
            Journey->HandleRigTransitionFinished(ActiveTransitionId);
        }
    }
}

void ALFFCameraRig::StartFade(const float ToAlpha, const float Seconds, const FLinearColor& Color, const bool bHoldWhenFinished)
{
    if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        // Start from the current fade amount so an interrupted fade never pops.
        CameraManager->StartCameraFade(CameraManager->FadeAmount, ToAlpha, Seconds, Color, false, bHoldWhenFinished);
    }
}
