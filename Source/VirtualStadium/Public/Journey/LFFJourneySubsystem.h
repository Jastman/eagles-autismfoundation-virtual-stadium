#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Journey/LFFJourneyStateMachine.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFJourneySubsystem.generated.h"

class ALFFCameraPath;
class ALFFCameraRig;
class ALFFCameraTarget;
class ALFFCueActor;
class ALFFGameMode;
class UAudioComponent;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLFFOnJourneyChanged, const FLFFJourneySnapshot&, Snapshot);
/** The word of the card (title, then body, split at spaces) the voice is reading, or INDEX_NONE when it isn't. */
DECLARE_MULTICAST_DELEGATE_OneParam(FLFFOnVoiceWord, int32);

/** Why the scene cues are being re-applied. */
enum class ELFFCueReason : uint8
{
    StepChanged,
    Arrived,
    SettingsChanged
};

/**
 * Owns the guided journey for one world: state machine, visitor settings, camera rig (splash orbit included),
 * labels, audio (beds, previews, voiceover), scene cues and the optional intensity previews.
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

    /** Previous step. On the first step, back to the splash. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Back();

    /** A timeline mark: straight to that step. Ignored if the step doesn't exist, before Start, paused or in a modal. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void JumpToStep(int32 StepIndex);

    /** Voice reads each card (Title, then Body). Needs Sound on; ignored while Sound is off or Calm Mode is on. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleVoice();
    /** The voice button on each card: Voice off, or Voice on (turning Sound on with it if needed). Nothing in Calm Mode. */
    void ToggleVoiceFromCard();
    int32 GetVoiceWordIndex() const { return VoiceWordIndex; }

    /** Crowd: Low (a few stand-in people) or Off. Ignored while Calm Mode is on (it hides them anyway). */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleCrowd();

    /** "Learn More" on the last step: opens the Eagles Autism Foundation page in the browser. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void OpenLearnMore();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void TogglePause();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void Resume();

    /** Asks "Start again?" rather than restarting straight away. From Settings, the question replaces the panel. */
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

    /** Ignored while Calm Mode is on (Calm Mode already means less movement). */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleReducedMotion();

    /** Off mutes every sound, UI included. On plays only the low ambient bed. Ignored while Calm Mode is on. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleSound();

    /** On: less movement, sound off, crowd hidden, previews locked off, camera cuts only. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleCalmMode();

    /** Standard | Calm (splash choice, top bar). Standard turns the full day on: sound, voice, movement, crowd. */
    void SetExperienceMode(bool bCalm);

    /** "Preview this? Off / Soft" on a loud or bright step. Ignored elsewhere, while paused or in Calm Mode. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void TogglePreview();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleLargeText();

    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ToggleHighContrast();

    UFUNCTION(BlueprintPure, Category = "Journey")
    FLFFJourneySnapshot GetSnapshot() const { return Snapshot; }

    UPROPERTY(BlueprintAssignable, Category = "Journey")
    FLFFOnJourneyChanged OnJourneyChanged;

    /** Karaoke: the card highlights this word while the voice reads it (word timings: the voice SoundWave's subtitle cues). */
    FLFFOnVoiceWord OnVoiceWord;

#if !UE_BUILD_SHIPPING
    /** Dev only (console LFF.Shots 1 2 18 [quit]): starts the journey, visits those steps (1-based) and saves a
     *  screenshot of each, HUD included, ShotDelaySeconds after the jump. For offscreen checks of the scene. */
    void StartShots(const TArray<FString>& Args);
#endif

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
    /** Start: glides along the splash orbit into the first step's wide shot (or moves there like any step). */
    void MoveCameraToFirstStep();
    /** The splash camera: a slow orbit, or a still wide shot with less movement. bCut fades there first (restart). */
    void EnterSplash(bool bCut);
    void StartSplashOrbit();
    /** The orbit through SplashOrbitTargetId around LFF_SiteRoot. False if the target or root is missing. */
    bool GetSplashOrbit(FVector& OutCenter, float& OutRadius, float& OutHeight, FVector& OutLookAt, float& OutFov,
        float& OutAnchorAngle, FTransform& OutWide) const;
    /** The camera has arrived at the current step: play its arrival cues and read the card. */
    void OnArrived();
    void ApplyCues(ELFFCueReason Reason);
    /** The anthem preview's cues (the eagle, fireworks) play while Soft is picked and end the preview when done. */
    void ApplyPreviewCues();
    void FinishPreviewCues();

    /** An authored route to ToTargetId that starts where the camera is now (its From target), if any. */
    const ALFFCameraPath* FindPath(FName ToTargetId) const;

    /**
     * Anthem, Standard: the eagle flies and the camera joins it close beside, then eases back to the seat and the
     * gold and white fireworks burst above the rim. Less movement keeps the camera at the seat. Calm Mode: none of it.
     * Returns the whole sequence's length, s.
     */
    float StartAnthemSequence();
    void JoinAnthemEagle();
    void ReturnFromAnthemEagle();
    void PlayAnthemFireworks();
    /** Stops the sequence; bRestoreCamera cuts back to the step's pose (Calm Mode or pause turned on mid-flight). */
    void StopAnthemSequence(bool bRestoreCamera);
    void ScheduleVoice();
    void StartVoice();
    /** Stops the voice at once. bReapplyAudio restores the bed from its duck. */
    void StopVoice(bool bReapplyAudio = true);
    UFUNCTION()
    void HandleVoiceFinished();
    void HandleVoicePlaybackPercent(const UAudioComponent* Component, const USoundWave* Wave, float Percent);
    void SetVoiceWord(int32 Index);
    void ApplyLabels();
    /** Master mute, ambient bed and the Sound preview. */
    void ApplyAudio();
    void StopAudio();
    void StopPreviewAudio();
    /** Everything Calm Mode and the previews touch: audio, the Lights pulse and crowd actors. */
    void ApplySensory();
    void ApplyCrowd();
    /**
     * The parking lot's cars (actors tagged LFF_Exterior) are hidden while the camera is inside the stadium (the box of
     * any actor tagged LFF_StadiumInterior), so nothing renders them from the seats and concourse. Checked 5 times a
     * second against the live camera, so glides in and out switch them where the walls already hide the lot.
     */
    void UpdateExteriorVisibility();
    /** Every step arrives with its preview Off. */
    void ResetPreview();
    bool IsPathBlocked(const FVector& From, const FVector& To) const;
    ELFFIntensityPreview GetCurrentPreviewKind() const;

    UFUNCTION()
    void HandlePreviewAudioFinished();
    void Broadcast();
    bool IsModalOpen() const { return bSettingsOpen || bRestartConfirmOpen; }

    FLFFJourneyStateMachine Machine;
    FLFFVisitorSettings Settings;
    FLFFJourneySnapshot Snapshot;
    bool bSettingsOpen = false;
    bool bRestartConfirmOpen = false;
    bool bTargetMissing = false;
    bool bJourneyMissing = false;
    bool bPreviewOn = false;
    /** The camera has reached the current step (arrival cues and the voice wait for this). */
    bool bArrived = false;
    bool bVoicePlaying = false;
    /** Start time of each card word in the current voice line (Tools/Audio/make_vo_words.py). */
    TArray<float> VoiceWordTimes;
    int32 VoiceWordIndex = INDEX_NONE;
    bool bPreviewCuesPlaying = false;
    bool bAnthemSequence = false;
    TWeakObjectPtr<ALFFCueActor> AnthemEagle;
    float AnthemFlightLeft = 0.0f;
    FTimerHandle AnthemTimer;
    bool bLoggedCrowdShown = true;
    bool bExteriorHidden = false;
    /** The crowd spawners auto-spawn on BeginPlay, so they start shown. */
    bool bCrowdSpawnersShown = true;
    // Last states written to the log, so each change is logged once (evidence for TEST_PLAN T2).
    bool bLoggedSound = true;
    bool bLoggedPulse = false;
    bool bLoggedCalm = false;

    UPROPERTY(Transient)
    TObjectPtr<ALFFGameMode> GameMode;

    UPROPERTY(Transient)
    TObjectPtr<ALFFCameraRig> Rig;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BedAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PreviewAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> VoiceAudio;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ALFFCueActor>> CueActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> ExteriorActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> InteriorBoxes;

    /** The first step listing each cue id: its gate is closed before that step and open after it. */
    TMap<FName, int32> FirstCueIndex;
    TArray<FLFFTimelineMark> TimelineMarks;
    FTimerHandle VoiceTimer;
    FTimerHandle PreviewCueTimer;
    FTimerHandle ExteriorTimer;
#if !UE_BUILD_SHIPPING
    void NextShot();
    TArray<int32> ShotSteps;
    bool bShotsQuit = false;
    bool bShotTaken = false;
    FTimerHandle ShotTimer;
#endif
};
