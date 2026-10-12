#pragma once

#include "CoreMinimal.h"
#include "Journey/LFFJourneyTypes.h"

/** Where the tour was when the visitor switched to Free explore. Resume tour goes back here. */
struct FLFFTourBookmark
{
    int32 StepIndex = INDEX_NONE;
    ELFFTourStep TourStep = ELFFTourStep::Arrival;
    /** False when Free explore was picked on the welcome screen: Resume tour then starts the tour at step 1. */
    bool bTourStarted = false;

    bool IsSet() const { return StepIndex != INDEX_NONE; }
};

/**
 * Pure journey state: step index, phase, pause, transition serial and the Guided tour / Free explore mode.
 * No world, no time, no UObjects, so it can be unit tested headless.
 * Never wraps: Next on the last step and Back on the first step are rejected.
 * Free explore never writes the step: Next, Back, Start and jumps are rejected there, and Resume tour returns to the
 * bookmark taken on the way in.
 */
struct VIRTUALSTADIUM_API FLFFJourneyStateMachine
{
    void Load(const TArray<FLFFJourneyStep>& InSteps);
    /** First step (0-based) of each ELFFTourStep, in order. Steps before the first one count as Arrival. */
    void SetTourSteps(const TArray<int32>& InFirstSteps) { TourStepFirstSteps = InFirstSteps; }

    /** Intro -> first step. The camera is already at the first step, so no transition starts. */
    ELFFCommandResult Start();
    ELFFCommandResult Next();
    ELFFCommandResult Back();
    /** Straight to any step (a timeline mark). Rejected before Start, while paused, out of range, or to the same step. */
    ELFFCommandResult JumpTo(int32 NewIndex);
    /** Any state -> Intro at the first step, unpaused, in the Guided tour. Invalidates any transition in flight. */
    ELFFCommandResult Restart();
    ELFFCommandResult Pause();
    ELFFCommandResult Resume();

    /**
     * Free explore bookmarks the tour's step and chapter and holds them, retiring any tour transition so an arrival
     * callback can't restart tour cues after the visitor takes control. Guided tour is ResumeTour(). Rejected while paused.
     */
    ELFFCommandResult SwitchToMode(ELFFSimulationMode NewMode);
    /**
     * Free explore -> Guided tour at the bookmark, or at AtIndex (an arc node). Always starts a new transition, even back
     * to the same step, because the explore camera is somewhere else: the caller fade-cuts there. Starts the tour if
     * Free explore was picked on the welcome screen.
     */
    ELFFCommandResult ResumeTour(int32 AtIndex = INDEX_NONE);

    /** Returns true if this finished the current transition. Stale ids are ignored. */
    bool NotifyTransitionFinished(uint32 Id);

    bool HasStarted() const;
    bool CanNext() const;
    bool CanBack() const;
    bool IsAtEnd() const;
    bool IsPaused() const { return bPaused; }
    int32 GetIndex() const { return Index; }
    int32 Num() const { return Steps.Num(); }
    ELFFJourneyPhase GetPhase() const { return Phase; }
    ELFFSimulationMode GetMode() const { return Mode; }
    const FLFFTourBookmark& GetBookmark() const { return Bookmark; }
    /** The current step's chapter; in Free explore, the bookmarked one. */
    ELFFTourStep GetTourStep() const;
    ELFFTourStep TourStepOf(int32 StepIndex) const;
    const TArray<int32>& GetTourStepFirstSteps() const { return TourStepFirstSteps; }
    uint32 GetTransitionId() const { return TransitionId; }
    const FLFFJourneyStep* GetCurrentStep() const;

private:
    ELFFCommandResult BeginMoveTo(int32 NewIndex);

    TArray<FLFFJourneyStep> Steps;
    TArray<int32> TourStepFirstSteps;
    int32 Index = INDEX_NONE;
    ELFFJourneyPhase Phase = ELFFJourneyPhase::Empty;
    ELFFSimulationMode Mode = ELFFSimulationMode::GuidedTour;
    FLFFTourBookmark Bookmark;
    bool bPaused = false;
    uint32 TransitionId = 0;
};

namespace LFFJourneyRules
{
    /** The chapter a step belongs to: the last one whose first step is at or before it. Arrival before the first. */
    VIRTUALSTADIUM_API ELFFTourStep ResolveTourStep(int32 StepIndex, const TArray<int32>& TourStepFirstSteps);
    /** Free flight's space, in the site frame: within Radius of the centre across, and between MinZ and MaxZ. */
    VIRTUALSTADIUM_API FVector ClampFlyPosition(const FVector& SiteLocation, float Radius, float MinZ, float MaxZ);
    /** Where a level's overview camera sits and looks, in the site frame. No roll. */
    VIRTUALSTADIUM_API FTransform LevelOverviewPose(const FLFFLevelView& View);
    /** Largest unobstructed rectangle in actual HUD units, with edge/credit clearance. Invalid when no space remains. */
    VIRTUALSTADIUM_API FBox2D UnobstructedViewport(const FVector2D& Size, const TArray<FBox2D>& Panels, float Inset, float BottomInset);
    /** Exact perspective fit inside a normalized safe rectangle. Points include marker stems; Margin is world-space clearance. */
    VIRTUALSTADIUM_API FTransform FramePointsPose(const TArray<FVector>& Points, float PitchDegrees, float YawDegrees, float HorizontalFov,
        float Aspect, const FBox2D& SafeRect, float Margin, float MinDistance);
    VIRTUALSTADIUM_API ELFFTransitionProfile ResolveTransitionProfile(ELFFTransitionProfile Authored, bool bReducedMotion);
    /**
     * The move actually used: Calm Mode cuts; less movement, or a path that would pass through a wall or prop,
     * fades and cuts; otherwise the authored profile. The camera never flies through geometry.
     */
    VIRTUALSTADIUM_API ELFFTransitionProfile ResolveMove(ELFFTransitionProfile Authored, const FLFFVisitorSettings& Settings, bool bPathBlocked);
    /** "Preview this?" shows on intensity steps unless Calm Mode is on. */
    VIRTUALSTADIUM_API bool ShouldShowPreviewControls(ELFFIntensityPreview Kind, const FLFFVisitorSettings& Settings);
    /** Soft can play: never in Calm Mode, and a Sound preview only when Sound is on. Anthem visuals can stay silent. */
    VIRTUALSTADIUM_API bool CanPlayPreview(ELFFIntensityPreview Kind, const FLFFVisitorSettings& Settings);
    /**
     * World labels stay out of the caption card's way: hidden if behind the camera or in the lower third of the
     * frame. ScreenFraction is (0,0) top left to (1,1) bottom right.
     */
    VIRTUALSTADIUM_API bool ShouldHideLabelAt(bool bInFront, const FVector2D& ScreenFraction);
    VIRTUALSTADIUM_API float ClampDuration(float AuthoredSeconds, float DefaultSeconds, float MinSeconds, float MaxSeconds);
    VIRTUALSTADIUM_API bool ShouldShowDisclaimer(ELFFDisclaimerLevel Level);
    /** Back on the first step returns to the splash instead of doing nothing (LEDGER D26). */
    VIRTUALSTADIUM_API bool ShouldBackReturnToIntro(bool bStarted, int32 Index, bool bPaused, bool bStepAllowsBack);
    /**
     * An arrival cue (a gate, the arriving car) against the journey: Before until its step, Play on its step, After
     * on every later one. The splash is always Before. FirstCueIndex is the first step that lists the cue.
     */
    VIRTUALSTADIUM_API ELFFCueState ResolveCueState(bool bStarted, int32 CurrentIndex, int32 FirstCueIndex);
    /** The splash slowly orbits the stadium, unless less movement or Calm Mode asks for a still wide shot. */
    VIRTUALSTADIUM_API bool ShouldOrbitSplash(const FLFFVisitorSettings& Settings);
    /** The voice reads a step only with Voice and Sound on, never over an intensity preview. */
    VIRTUALSTADIUM_API bool ShouldPlayVoice(const FLFFVisitorSettings& Settings, bool bStarted, bool bPreviewOn);
    /** Linear gain for a level change in dB (the bed ducks under the voice by a few dB). */
    VIRTUALSTADIUM_API float DecibelsToGain(float Decibels);
    /** Full quieting inside a bounded room, smoothly returning to normal across its outside blend distance. */
    VIRTUALSTADIUM_API float QuietZoneGain(float OutsideDistance, float BlendDistance, float InsideGain);
}
