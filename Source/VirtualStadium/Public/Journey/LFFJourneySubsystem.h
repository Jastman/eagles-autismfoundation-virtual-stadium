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
class APostProcessVolume;
class UAudioComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;

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

    /**
     * A timeline mark or arc node: straight to that step. Ignored if the step doesn't exist, before Start, paused or in
     * a modal. In Free explore it resumes the tour there, or at the bookmark if the node is the chapter you left.
     */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void JumpToStep(int32 StepIndex);

    /**
     * The Guided tour / Explore switch (keys T and E). Free explore keeps the camera where it is, stops the tour's voice
     * and preview, cancels any tour move at the current pose, and bookmarks the step. Guided tour is ResumeTour(), or Start on the welcome screen.
     * Ignored in a modal or while paused.
     */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void SwitchToMode(ELFFSimulationMode NewMode);

    /** Free explore -> the bookmarked step, with a fade-cut (a cut in Calm Mode); the card is read again on arrival. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void ResumeTour();

    /** Free explore (key H): the map turns to the seat's level and the camera fade-cuts to the seat's view. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void GoToMySeat();

    /** Free explore's Show me: the camera fade-cuts to a camera target (a cut in Calm Mode). The tour's place doesn't change. */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ShowTarget(FName CameraTargetId);

    /** Uses the authored interior view when present, otherwise frames this mapped place. Never fabricates an interior. */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void FocusAmenity(FName AmenityId);

    /** Actual viewport and opaque corner-panel geometry, supplied by the HUD after layout. */
    void SetMapViewportFrame(const FVector2D& Size, const FBox2D& SafeRect, float StemHeight);

    /**
     * Free explore's level buttons: picks the level, highlights its height band and fade-cuts to an oblique overview
     * (a cut in Calm Mode). Picking it again re-frames it.
     */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void SetMapLevel(ELFFMapLevel Level);

    /** Free explore: shows or hides one kind of marker. */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ToggleAmenity(ELFFAmenity Amenity);

    UFUNCTION(BlueprintCallable, Category = "Map")
    void HideAllAmenities();

    /** Free explore: picks a marker (its card shows Show me). None clears the pick. */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void SelectAmenity(FName AmenityId);

    ELFFSimulationMode GetMode() const { return Machine.GetMode(); }

    /** LFF_SiteRoot's world transform (read at start and on each visit to Free explore): the frame places are written in. */
    const FTransform& GetSiteFrame() const { return SiteFrame; }

    /** Free explore's places from DT_LFF_Amenities, in table order (unverified ones dropped in packaged builds). */
    const TArray<FLFFAmenityMarker>& GetAmenities() const { return Amenities; }

    /** A place by id, the seat included (MySeatAmenityId, at CT_SeatFieldView). False if there's no such place. */
    bool GetAmenity(FName AmenityId, FLFFAmenityMarker& OutAmenity) const;

    /** The seat's id among the places: its callout and card work like any other's, and it can't be toggled off. */
    static const FName MySeatAmenityId;

    /**
     * Camera input, once a frame from the player controller. Move is (right, forward) from -1 to 1, Rise is
     * up or down, LookDelta is the mouse drag in pixels (only while a button is held over the 3D view), Dolly is wheel
     * notches. Waits while a move or fade is running; eases into and out of moving (at once with less movement). Stays in
     * the site's flyable space. Tour steps accept look only, once their authored camera motion has finished.
     * Mouse look is smoothed without multiplying pixel deltas by frame time. No roll, ever.
     */
    void FlyTick(const FVector2D& Move, float Rise, const FVector2D& LookDelta, float Dolly, bool bFast, float DeltaSeconds);

    bool CanLookAround() const;
    /** Capture loss and step changes discard pending look rather than moving a later view. */
    void ResetLookInput();

    /** Button feedback only. Sound off / Calm mode are silent; repeated presses never stack click sounds. */
    UFUNCTION(BlueprintCallable, Category = "Journey")
    void PlayUIClick();

    /** Free flight can take input now (Free explore, no modal, not paused). */
    bool CanFly() const { return CanUseMap(); }

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

    /** Off mutes every sound, UI included. On restores ambience and enabled narration, not a cancelled anthem. */
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
    /** Dev only (console LFF.Shots welcome 1 9 explore tour 18 [quit]): saves a screenshot, HUD included, after each token,
     *  ShotDelaySeconds later: "welcome" as the app opens (first only), a number visits that step (1-based; starts the tour
     *  if needed), "explore" switches to Free explore, "tour" resumes the tour. "size:1920x1080" resizes the live viewport
     *  without changing its step or mode. For offscreen checks of the scene and HUD. */
    void StartShots(const TArray<FString>& Args);
#endif

    /** Called by ALFFCameraRig. Stale ids are ignored by the state machine. */
    void HandleRigTransitionFinished(uint32 TransitionId);

private:
    void InitializeJourney();
    bool LoadSteps(TArray<FLFFJourneyStep>& OutSteps) const;
    ALFFCameraRig* FindOrSpawnRig();
    ALFFCameraTarget* FindTarget(FName TargetId) const;

    /** Moves the camera to the current step. bForceFade is used for restart and Resume tour so the jump back is calm. */
    void MoveCameraToCurrentStep(bool bForceFade);
    /** Resume tour at the bookmark (INDEX_NONE) or at a step (an arc node). */
    void ResumeTourAt(int32 StepIndex);
    bool IsExploring() const { return Machine.GetMode() == ELFFSimulationMode::FreeExplore; }
    /** The map's own commands: Free explore only, never in a modal or while paused. */
    bool CanUseMap() const { return IsExploring() && !IsModalOpen() && !Machine.IsPaused(); }
    void ResetMap();
    void LoadAmenities();
    void RefreshSiteFrame();
    /** The picked level's post-process highlight: on in Free explore after a level button, off otherwise. */
    void ApplyLevelHighlight();
    /** Fade-cuts to a level's oblique overview (FLFFLevelView), looking at its floor. */
    void FlyToLevel(ELFFMapLevel Level);
    /** Fade-cuts to an oblique fit of every visible place, within the actual HUD-safe rectangle. */
    void FramePlacesShown();
    void FrameMapPoints(const TArray<FVector>& Points);
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
    void StartAnthemAudio();
    void StopAnthemAudio();
    void UpdateAnthemAudio();
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
    /** Room ambience follows the actual camera, never the bookmarked step or a selected marker. */
    void UpdateRoomAudio();
    float GetRoomCrowdGain() const;
    void StopActivePreview();
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
    /** The Free explore map: kept across mode switches, reset by Start again. */
    ELFFMapLevel MapLevel = ELFFMapLevel::LowerConcourse;
    int32 AmenityMask = LFFDefaultAmenityMask;
    FName SelectedAmenityId = NAME_None;
    /** False until the first visit to Free explore, which opens the map on the seat's level. */
    bool bMapOpened = false;
    bool bLevelHighlight = false;
    TArray<FLFFAmenityMarker> Amenities;
    FTransform SiteFrame = FTransform::Identity;
    FVector2D MapViewportSize = FVector2D::ZeroVector;
    FBox2D MapSafeRect = FBox2D(ForceInit);
    float MapStemHeight = 2500.0f;
    bool bMapAutoFramed = false;
    bool bLoggedFramingUnavailable = false;
    FVector FlyVelocity = FVector::ZeroVector;
    FVector2D PendingLookDegrees = FVector2D::ZeroVector;
    FRotator LastLookRotation = FRotator::ZeroRotator;
    bool bLookSmoothingActive = false;
    /** Wheel distance still to travel, cm (eased out over a few frames). */
    float FlyDollyLeft = 0.0f;
    bool bLoggedHighlightMissing = false;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> LevelHighlight;
    /** The last camera target the camera went to: the current step's, My seat or a Show me. */
    FName ViewTargetId = NAME_None;
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
    FTimerHandle AnthemAudioStartTimer;
    FTimerHandle AnthemAudioEndTimer;
    FTimerHandle AnthemAudioWatchTimer;
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

    /** Retain the outgoing bed so a mute, pause teardown or rapid retarget can stop it too. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> FadingBedAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PreviewAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> VoiceAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> AnthemAudio;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> UIClickAudio;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ALFFCueActor>> CueActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> ExteriorActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> InteriorBoxes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<APostProcessVolume>> QuietVolumes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> QuietBoxes;

    float RoomCrowdGain = 1.0f;
    float AppliedBedVolume = -1.0f;
    float FadingBedBaseVolume = 1.0f;
    float AppliedAnthemVolume = -1.0f;
    /** The first step listing each cue id: its gate is closed before that step and open after it. */
    TMap<FName, int32> FirstCueIndex;
    TArray<FLFFTimelineMark> TimelineMarks;
    FTimerHandle VoiceTimer;
    FTimerHandle PreviewCueTimer;
    FTimerHandle ExteriorTimer;
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FLFFJourneyRuntimeTestAccess;
#endif
#if !UE_BUILD_SHIPPING
    void NextShot();
    TArray<FString> ShotQueue;
    FString ShotName;
    /** "fly": a scripted push forward through the same FlyTick path, for the offscreen checks. */
    float ShotFlySecondsLeft = 0.0f;
    bool bShotsQuit = false;
    bool bShotTaken = false;
    FTimerHandle ShotTimer;
#endif
};
