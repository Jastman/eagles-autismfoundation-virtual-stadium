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

    /** Cancels all travel at the current pose, including a held fade. Used when the visitor takes control in Explore. */
    void HoldPosition();

    /**
     * Follows Points (world space) from the current pose, eased in and out over Seconds, looking along the route.
     * Ends on TargetTransform, or with bCutAtEnd fades and cuts to it from the last point. Reports like an Ease.
     */
    void BeginPathTransition(
        const TArray<FVector>& Points,
        const FTransform& TargetTransform,
        float TargetFieldOfView,
        float Seconds,
        bool bCutAtEnd,
        float FadeSeconds,
        const FLinearColor& FadeColor,
        uint32 TransitionId);

    /**
     * Keeps the camera close beside Target (Offset in the target's own frame), lagging softly behind it and looking
     * at it. Runs until StopFollow or any new transition. Reports nothing.
     */
    void StartFollow(AActor* Target, const FVector& Offset, float FieldOfView);

    void StopFollow();

    /** An Ease back to a pose that reports nothing (the journey is already at its step). */
    void EaseQuietly(const FTransform& TargetTransform, float TargetFieldOfView, float Seconds);

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

    /** A move, fade-cut, orbit or follow is running: free flight waits until it's done. */
    bool IsBusy() const;

    /** Adds Blendable to the camera's post-process (once) at Weight; 0 leaves it in place but off. */
    void SetPostProcessBlendable(UObject* Blendable, float Weight);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StopActiveTransition();
    void FinishTransition(uint32 TransitionId);
    void StartFade(float ToAlpha, float Seconds, const FLinearColor& Color, bool bHoldWhenFinished);
    void ClearFade(float Seconds, const FLinearColor& Color);
    void TickLightPulse(float DeltaSeconds);
    void TickOrbit(float DeltaSeconds);
    void TickPath(float DeltaSeconds);
    void TickFollow(float DeltaSeconds);
    void ApplyOrbitAngle(float AngleDegrees);
    void UpdateTickEnabled();

    FTransform StartTransform = FTransform::Identity;
    FTransform EndTransform = FTransform::Identity;
    float StartFieldOfView = 70.0f;
    float EndFieldOfView = 70.0f;
    float Elapsed = 0.0f;
    float Duration = 0.0f;
    uint32 ActiveTransitionId = 0;
    uint32 TransitionSerial = 0;
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

    /** The live route of a path transition, world space (absolute, so it doesn't ride along with the camera). */
    UPROPERTY(Transient)
    TObjectPtr<class USplineComponent> PathSpline;
    bool bPathing = false;
    bool bPathCutAtEnd = false;
    FTransform PathEndTransform = FTransform::Identity;
    float PathEndFieldOfView = 70.0f;
    float PathFadeSeconds = 0.35f;
    FLinearColor PathFadeColor = FLinearColor::Black;

    TWeakObjectPtr<AActor> FollowTarget;
    FVector FollowOffset = FVector::ZeroVector;
    float FollowFieldOfView = 60.0f;
    bool bFollowing = false;
    static constexpr float FollowMaxSpeed = 1400.0f;

    FTimerHandle FadeOutTimer;
    FTimerHandle FadeInTimer;
};
