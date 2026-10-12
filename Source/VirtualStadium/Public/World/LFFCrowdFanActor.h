#pragma once

#include "CoreMinimal.h"
#include "World/LFFPersonActor.h"
#include "TimerManager.h"
#include "LFFCrowdFanActor.generated.h"

class UAnimSequence;

namespace LFFCirculationRules
{
    struct FRoutePose
    {
        FVector Location = FVector::ZeroVector;
        FVector Direction = FVector::ForwardVector;
        bool bValid = false;
    };
    VIRTUALSTADIUM_API double RouteLength(const TArray<FVector>& Points);
    /** Ping-pong on measured authored paths: no endpoint teleport, extrapolation or root-motion drift. */
    VIRTUALSTADIUM_API FRoutePose SampleRoute(const TArray<FVector>& Points, double Distance);
    /** Closed, measured passing circuit, including the lane changes at both ends. */
    VIRTUALSTADIUM_API FRoutePose SampleLoop(const TArray<FVector>& Points, double Distance);
    /** Geometry only; callers must measure the floor and check clearance before using these lanes. */
    VIRTUALSTADIUM_API bool BuildPassingLoop(const TArray<FVector>& Points, TArray<FVector>& OutLoop);
    /** Swept person-only reservation. Checking just the destination permits tunnelling at crossings. */
    VIRTUALSTADIUM_API bool HasSpace(const FVector& From, const FVector& To, const TArray<FVector>& People, bool bPlacement);
    constexpr double MinimumSeparation = 125.0;
    constexpr double LaneOffset = 75.0;
    VIRTUALSTADIUM_API bool ShouldAnimate(bool bVisible, bool bMotionEnabled, bool bInRange);
}

/** A bounded seat neighbour, waiting visitor or path walker. Crowd Off/Calm stop all work; distant visitors freeze. */
UCLASS()
class VIRTUALSTADIUM_API ALFFCrowdFanActor : public ALFFPersonActor
{
    GENERATED_BODY()

public:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TArray<TObjectPtr<UAnimSequence>> Gestures;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    int32 VariationSeed = 1018;

    /** Ground-measured points in the initial actor frame. Empty means a waiting/seated visitor. */
    UPROPERTY(EditAnywhere, Category = "Crowd|Walking")
    TArray<FVector> WalkRoute;

    UPROPERTY(EditAnywhere, Category = "Crowd|Walking", meta = (ClampMin = "20.0", ClampMax = "160.0"))
    float WalkSpeed = 85.0f;

    /** Fraction of the complete outbound/return trip; runtime reservation also rejects coincident mirrored phases. */
    UPROPERTY(EditAnywhere, Category = "Crowd|Walking", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RoutePhase = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "500.0", ClampMax = "10000.0"))
    float AnimationDistance = 6500.0f;

    /** Hidden fans do no pose or gesture work; reduced motion leaves visible fans in a still pose. */
    void SetCrowdActive(bool bVisible, bool bAnimate);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PredictPersonRoute(TArray<FVector>& OutWorldPoints) const override;
    virtual bool SharesPersonRoute(const ALFFPersonActor& Other) const override;
    virtual bool IsWaitingForPersonSpace() const override { return bWaitingForSpace; }

private:
    friend struct FLFFCrowdSpacingTestAccess;
    void PlayIdle();
    void PlayGesture();
    void ScheduleGesture();
    void RefreshProximity();
    void UpdateActivity();
    bool ApplyWalkPose(double Distance, bool bPlacement = false);
    bool PlaceWalker();
    bool MeasurePassingLoop();

    FRandomStream Variation;
    FTimerHandle GestureTimer;
    FTimerHandle ProximityTimer;
    FTransform RouteAnchor = FTransform::Identity;
    /** Runtime-only floor-checked lanes. The authored route and its detailed corners are never rewritten. */
    TArray<FVector> PassingLoop;
    double WalkDistance = 0.0;
    float IdleRate = 1.0f;
    bool bCrowdVisible = true;
    bool bCrowdAnimated = true;
    bool bInAnimationRange = true;
    bool bPoseRunning = false;
    bool bWalking = false;
    bool bWaitingForSpace = false;
    bool bRoutePlacementBlocked = false;
};
