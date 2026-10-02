#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFCueActor.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;

/** What starts a cue: the camera arriving at its step, or the visitor picking "Soft" on its step's preview. */
UENUM(BlueprintType)
enum class ELFFCueTrigger : uint8
{
    Arrival,
    Preview
};

/**
 * One moving piece of the scene, driven by a journey step's SceneCues: a gate leaf sliding open, the arriving car,
 * the bald eagle's single pass. Placed in the level at its end pose (open, parked, end of the flight); it starts at
 * StartOffset from there and moves back to it once, never looping. Props are built from primitives on this actor
 * (Tools/Editor/build_lff_standins.py); attached actors move and hide with it.
 * Less movement and Calm Mode skip the move and show the end pose (ULFFJourneySubsystem). Ticks only while moving.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFCueActor : public AActor
{
    GENERATED_BODY()

public:
    ALFFCueActor();

    virtual void Tick(float DeltaSeconds) override;

    /** Matches a step's SceneCues entry. Several actors can share one id (the two leaves of a gate). */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cue")
    FName CueId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    ELFFCueTrigger Trigger = ELFFCueTrigger::Arrival;

    /** Start pose relative to the placed (end) pose, in the actor's own frame. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    FVector StartOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    FRotator StartRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.5", ClampMax = "20.0"))
    float PlaySeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float PlayDelaySeconds = 0.0f;

    /** Constant speed (a flight) instead of easing out (a car or a gate settling to a stop). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    bool bLinear = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    bool bHiddenBefore = false;

    /** Hidden once the move is over (the eagle leaves the frame and is gone). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    bool bHiddenAfter = false;

    /** People: looped on this actor's skeletal meshes while the move runs (a walk). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    TObjectPtr<UAnimationAsset> MovingAnimation;

    /** People: looped while standing, before and after the move or through its delay (an idle). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    TObjectPtr<UAnimationAsset> StillAnimation;

    /** Spectacle (the gate flares): never shown in Calm Mode, rather than shown at its end pose. Use with bHiddenBefore. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
    bool bSpectacle = false;

    /** Before: start pose. After: end pose. Play: start pose, waiting for Play(). Stops any move in progress. */
    void ApplyState(ELFFCueState State);

    /** Runs the move once. Instant jumps straight to the end pose. */
    void Play(bool bInstant);

    bool IsPlaying() const { return bPlaying; }
    float GetTotalSeconds() const { return PlayDelaySeconds + PlaySeconds; }

protected:
    virtual void BeginPlay() override;

    /** 0 at the start pose, 1 at the end pose. Subclasses may draw more (fireworks). */
    virtual void ApplyAlpha(float Alpha, float ElapsedSeconds);
    virtual void OnPlayStarted() {}
    virtual void OnStopped() {}
    /** The one mesh the animations play on (a person's body; the rest follow its pose). Null: every skeletal mesh. */
    virtual USkeletalMeshComponent* GetAnimatedMesh() const { return nullptr; }

    void SetCueHidden(bool bHidden);

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

private:
    void CacheRestPose();
    void Finish();
    /** Loops Animation on every skeletal mesh of this actor (no-op without one). */
    void PlayLoop(UAnimationAsset* Animation);
    /** Single-node loop on one mesh, warning in the log (with both skeletons) if the clip doesn't take. */
    void PlayLoopOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Animation);

    FTransform RestRelative = FTransform::Identity;
    bool bHasRest = false;
    bool bPlaying = false;
    bool bMovingAnimationOn = false;
    float Elapsed = 0.0f;
};
