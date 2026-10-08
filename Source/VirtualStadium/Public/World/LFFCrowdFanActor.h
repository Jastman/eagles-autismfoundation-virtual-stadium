#pragma once

#include "CoreMinimal.h"
#include "World/LFFPersonActor.h"
#include "TimerManager.h"
#include "LFFCrowdFanActor.generated.h"

class UAnimSequence;

/** A bounded, detailed seat neighbour. Timer-driven gestures never interfere with the cue actor's tick lifecycle. */
UCLASS()
class VIRTUALSTADIUM_API ALFFCrowdFanActor : public ALFFPersonActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Crowd")
    TArray<TObjectPtr<UAnimSequence>> Gestures;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    int32 VariationSeed = 1018;

    /** Hidden fans do no pose or gesture work; reduced motion leaves visible fans in a still pose. */
    void SetCrowdActive(bool bVisible, bool bAnimate);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PlayIdle();
    void PlayGesture();
    void ScheduleGesture();

    FRandomStream Variation;
    FTimerHandle GestureTimer;
    float IdleRate = 1.0f;
    bool bCrowdVisible = true;
    bool bCrowdAnimated = true;
};
