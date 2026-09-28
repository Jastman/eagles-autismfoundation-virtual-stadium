#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Journey/LFFJourneyStateMachine.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFJourneySubsystem.generated.h"

class ALFFCameraRig;
class ALFFCameraTarget;
class ALFFGameMode;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLFFOnJourneyChanged, const FLFFJourneySnapshot&, Snapshot);

/**
 * Owns the guided journey for one world: state machine, visitor settings, camera rig, labels and audio.
 * Active only when the world's game mode is ALFFGameMode; other maps are unaffected.
 * Input and UI only call commands here and read the snapshot; they hold no journey state.
 */
UCLASS()
class VIRTUALSTADIUM_API ULFFJourneySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** Intro: start. Last step: back to the intro. Otherwise: next step. Ignored while a question is open. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Next();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Back();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void TogglePause();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Resume();

    /** Asks "Start again?" rather than restarting straight away. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void RequestRestart();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ConfirmRestart();

    /** Answers "yes" to the open question; with none open, same as Next. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Confirm();

    /** Closes the open question or settings; otherwise resumes if paused. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Cancel();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleSettings();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleReducedMotion();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleSound();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleCalmMode();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleLargeText();

    UFUNCTION(BlueprintPure, Category = "Journey")
    FLFFJourneySnapshot GetSnapshot() const { return Snapshot; }

    UPROPERTY(BlueprintAssignable, Category = "Journey")
    FLFFOnJourneyChanged OnJourneyChanged;

    /** Called by ALFFCameraRig. Stale ids are ignored by the state machine. */
    void HandleRigTransitionFinished(uint32 TransitionId);

private:
    void InitializeJourney();
    bool LoadSteps(TArray<FLFFJourneyStep>& OutSteps) const;
    ALFFCameraRig* FindOrSpawnRig();
    ALFFCameraTarget* FindTarget(FName TargetId) const;

    /** Moves the camera to the current step. bForceFade is used for restart so the jump back is calm. */
    void MoveCameraToCurrentStep(bool bForceFade);
    void SnapCameraToCurrentStep();
    void ApplyLabels();
    void ApplyAudio();
    void StopAudio();
    void Broadcast();
    bool IsModalOpen() const { return bSettingsOpen || bRestartConfirmOpen; }

    FLFFJourneyStateMachine Machine;
    FLFFVisitorSettings Settings;
    FLFFJourneySnapshot Snapshot;
    bool bSettingsOpen = false;
    bool bRestartConfirmOpen = false;
    bool bTargetMissing = false;
    bool bJourneyMissing = false;

    UPROPERTY(Transient)
    TObjectPtr<ALFFGameMode> GameMode;

    UPROPERTY(Transient)
    TObjectPtr<ALFFCameraRig> Rig;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BedAudio;
};
