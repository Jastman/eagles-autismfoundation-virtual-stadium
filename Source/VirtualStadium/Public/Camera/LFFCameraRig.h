#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFCameraRig.generated.h"

/**
 * Applies camera motion only; owns no journey state. Spawned by ULFFJourneySubsystem if the level has none.
 * Ticks only while an Ease transition, the splash orbit or the Lights preview is running. Reports completion to the
 * subsystem by transition id.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFCameraRig : public ACameraActor
{
    GENERATED_BODY()

public:
    ALFFCameraRig();

    virtual void Tick(float DeltaSeconds) override;

    void BeginTransition(
        const FTransform& TargetTransform,
        float TargetFieldOfView,
        ELFFTransitionProfile Profile,
        float EaseSeconds,
        float FadeSeconds,
        const FLinearColor& FadeColor,
        uint32 TransitionId);

    void SnapTo(const FTransform& TargetTransform, float TargetFieldOfView);

    /** Freezes an Ease move in place. Fades are short and are allowed to finish. */
    void SetTransitionPaused(bool bPaused);

    /**
     * Splash: circles Center at a fixed radius and height, always looking at LookAt, one lap per PeriodSeconds.
     * Starts from StartAngleDegrees (0 = +X from Center). A new transition stops it.
     */
    void StartOrbit(const FVector& Center, float Radius, float Height, const FVector& LookAt, float PeriodSeconds,
        float FieldOfView, float StartAngleDegrees);

    /** Holds the camera where it is (Calm Mode or less movement on the splash). */
    void StopOrbit();

    bool IsOrbiting() const { return bOrbiting; }

    /** Angle of the camera around Center, in the same convention as StartOrbit. */
    float GetOrbitAngle(const FVector& Center) const;

    /**
     * Glides along the orbit (the short way round) to TargetAngleDegrees and stops there, so leaving the splash
     * never cuts across the stadium. Reports completion by TransitionId like any other move.
     */
    void EaseOrbitTo(float TargetAngleDegrees, float Seconds, uint32 TransitionId);

    /**
     * Lights preview: exposure rises and falls as a smooth sine (PeakStops at the top, one cycle per PeriodSeconds).
     * Stopping ramps back to normal over RampSeconds, so the picture never jumps or flickers.
     */
    void SetLightPulse(bool bOn, float PeriodSeconds, float PeakStops);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StopActiveTransition();
    void FinishTransition(uint32 TransitionId);
    void StartFade(float ToAlpha, float Seconds, const FLinearColor& Color, bool bHoldWhenFinished);
    void ClearFade(float Seconds, const FLinearColor& Color);
    void TickLightPulse(float DeltaSeconds);
    void TickOrbit(float DeltaSeconds);
    void ApplyOrbitAngle(float AngleDegrees);
    void UpdateTickEnabled();

    FTransform StartTransform = FTransform::Identity;
    FTransform EndTransform = FTransform::Identity;
    float StartFieldOfView = 70.0f;
    float EndFieldOfView = 70.0f;
    float Elapsed = 0.0f;
    float Duration = 0.0f;
    uint32 ActiveTransitionId = 0;
    bool bEasing = false;
    bool bEasePaused = false;

    bool bOrbiting = false;
    bool bOrbitEasing = false;
    FVector OrbitCenter = FVector::ZeroVector;
    FVector OrbitLookAt = FVector::ZeroVector;
    float OrbitRadius = 0.0f;
    float OrbitHeight = 0.0f;
    float OrbitPeriod = 36.0f;
    float OrbitFieldOfView = 70.0f;
    float OrbitAngle = 0.0f;
    float OrbitEaseFrom = 0.0f;
    float OrbitEaseTo = 0.0f;

    bool bPulseOn = false;
    float PulseElapsed = 0.0f;
    float PulsePeriod = 4.0f;
    float PulsePeakStops = 0.0f;
    float PulseStops = 0.0f;
    static constexpr float PulseRampSeconds = 0.8f;

    FTimerHandle FadeOutTimer;
    FTimerHandle FadeInTimer;
};
