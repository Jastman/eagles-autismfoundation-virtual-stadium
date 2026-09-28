#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFCameraRig.generated.h"

/**
 * Applies camera motion only; owns no journey state. Spawned by ULFFJourneySubsystem if the level has none.
 * Ticks only while an Ease transition is running. Reports completion to the subsystem by transition id.
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

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StopActiveTransition();
    void FinishTransition(uint32 TransitionId);
    void StartFade(float ToAlpha, float Seconds, const FLinearColor& Color, bool bHoldWhenFinished);

    FTransform StartTransform = FTransform::Identity;
    FTransform EndTransform = FTransform::Identity;
    float StartFieldOfView = 70.0f;
    float EndFieldOfView = 70.0f;
    float Elapsed = 0.0f;
    float Duration = 0.0f;
    uint32 ActiveTransitionId = 0;
    bool bEasing = false;
    bool bEasePaused = false;

    FTimerHandle FadeOutTimer;
    FTimerHandle FadeInTimer;
};
