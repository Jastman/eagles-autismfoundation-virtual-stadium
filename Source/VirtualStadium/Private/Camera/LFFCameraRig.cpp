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

    TickLightPulse(DeltaSeconds);
    TickOrbit(DeltaSeconds);

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
        ClearFade(FadeSeconds, FadeColor);
        // Start from wherever the camera is now, so retargeting mid-move never jumps.
        StartTransform = GetActorTransform();
        EndTransform = TargetTransform;
        StartFieldOfView = GetCameraComponent() != nullptr ? GetCameraComponent()->FieldOfView : TargetFieldOfView;
        EndFieldOfView = TargetFieldOfView;
        Elapsed = 0.0f;
        Duration = EaseSeconds;
        bEasing = true;
        UpdateTickEnabled();
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
        ClearFade(FadeSeconds, FadeColor);
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

void ALFFCameraRig::StartOrbit(const FVector& Center, const float Radius, const float Height, const FVector& LookAt,
    const float PeriodSeconds, const float FieldOfView, const float StartAngleDegrees)
{
    StopActiveTransition();
    OrbitCenter = Center;
    OrbitRadius = FMath::Max(Radius, 100.0f);
    OrbitHeight = Height;
    OrbitLookAt = LookAt;
    OrbitPeriod = FMath::Max(PeriodSeconds, 10.0f);
    OrbitFieldOfView = FieldOfView;
    OrbitAngle = StartAngleDegrees;
    ClearFade(0.35f, FLinearColor(0.08f, 0.08f, 0.10f, 1.0f));
    bOrbiting = true;
    bOrbitEasing = false;
    ApplyOrbitAngle(OrbitAngle);
    UpdateTickEnabled();
}

void ALFFCameraRig::StopOrbit()
{
    bOrbiting = false;
    bOrbitEasing = false;
    UpdateTickEnabled();
}

float ALFFCameraRig::GetOrbitAngle(const FVector& Center) const
{
    const FVector Offset = GetActorLocation() - Center;
    return FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
}

void ALFFCameraRig::EaseOrbitTo(const float TargetAngleDegrees, const float Seconds, const uint32 TransitionId)
{
    if (!bOrbiting)
    {
        return;
    }

    ActiveTransitionId = TransitionId;
    OrbitEaseFrom = OrbitAngle;
    // The short way round: never more than half a lap.
    OrbitEaseTo = OrbitAngle + FMath::UnwindDegrees(TargetAngleDegrees - OrbitAngle);
    Elapsed = 0.0f;
    Duration = FMath::Max(Seconds, 0.1f);
    bOrbitEasing = true;
    bEasePaused = false;
    UpdateTickEnabled();
}

void ALFFCameraRig::TickOrbit(const float DeltaSeconds)
{
    if (!bOrbiting)
    {
        return;
    }

    if (!bOrbitEasing)
    {
        OrbitAngle = FMath::UnwindDegrees(OrbitAngle + 360.0f * DeltaSeconds / OrbitPeriod);
        ApplyOrbitAngle(OrbitAngle);
        return;
    }

    if (bEasePaused)
    {
        return;
    }

    Elapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
    OrbitAngle = FMath::Lerp(OrbitEaseFrom, OrbitEaseTo, FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f));
    ApplyOrbitAngle(OrbitAngle);

    if (Alpha >= 1.0f)
    {
        bOrbiting = false;
        bOrbitEasing = false;
        UpdateTickEnabled();
        FinishTransition(ActiveTransitionId);
    }
}

void ALFFCameraRig::ApplyOrbitAngle(const float AngleDegrees)
{
    const float Radians = FMath::DegreesToRadians(AngleDegrees);
    const FVector Location(OrbitCenter.X + OrbitRadius * FMath::Cos(Radians), OrbitCenter.Y + OrbitRadius * FMath::Sin(Radians), OrbitHeight);
    FRotator Rotation = (OrbitLookAt - Location).Rotation();
    Rotation.Roll = 0.0f;
    SetActorLocationAndRotation(Location, Rotation);

    if (UCameraComponent* Camera = GetCameraComponent())
    {
        Camera->SetFieldOfView(OrbitFieldOfView);
    }
}

void ALFFCameraRig::SetLightPulse(const bool bOn, const float PeriodSeconds, const float PeakStops)
{
    if (bOn && !bPulseOn)
    {
        PulseElapsed = 0.0f;
    }

    bPulseOn = bOn;
    // 3 s is the floor: at most one gentle rise every few seconds, nowhere near the 3 Hz flash threshold.
    PulsePeriod = FMath::Max(PeriodSeconds, 3.0f);
    PulsePeakStops = FMath::Clamp(PeakStops, 0.0f, 1.5f);
    UpdateTickEnabled();
}

void ALFFCameraRig::TickLightPulse(const float DeltaSeconds)
{
    UCameraComponent* Camera = GetCameraComponent();
    if (Camera == nullptr || (!bPulseOn && PulseStops <= 0.0f))
    {
        return;
    }

    if (bPulseOn)
    {
        PulseElapsed += DeltaSeconds;
        // 0 -> peak -> 0 as a raised cosine, so it starts from the normal picture with no step.
        PulseStops = PulsePeakStops * 0.5f * (1.0f - FMath::Cos(UE_TWO_PI * PulseElapsed / PulsePeriod));
    }
    else
    {
        PulseStops = FMath::Max(0.0f, PulseStops - DeltaSeconds * FMath::Max(PulsePeakStops, 0.1f) / PulseRampSeconds);
    }

    Camera->PostProcessSettings.bOverride_AutoExposureBias = bPulseOn || PulseStops > 0.0f;
    Camera->PostProcessSettings.AutoExposureBias = PulseStops;
    Camera->PostProcessBlendWeight = 1.0f;

    if (!bPulseOn && PulseStops <= 0.0f)
    {
        UpdateTickEnabled();
    }
}

void ALFFCameraRig::UpdateTickEnabled()
{
    SetActorTickEnabled(bEasing || bOrbiting || bPulseOn || PulseStops > 0.0f);
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
    bOrbiting = false;
    bOrbitEasing = false;
    UpdateTickEnabled();

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
    UpdateTickEnabled();

    if (UWorld* World = GetWorld())
    {
        if (ULFFJourneySubsystem* Journey = World->GetSubsystem<ULFFJourneySubsystem>())
        {
            Journey->HandleRigTransitionFinished(ActiveTransitionId);
        }
    }
}

void ALFFCameraRig::ClearFade(const float Seconds, const FLinearColor& Color)
{
    // A move that replaces a fade-and-cut halfway must never leave the picture dimmed.
    if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        if (CameraManager->FadeAmount > 0.0f)
        {
            StartFade(0.0f, FMath::Max(Seconds, 0.05f), Color, false);
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
