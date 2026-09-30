#pragma once

#include "CoreMinimal.h"
#include "Journey/LFFJourneyTypes.h"

/**
 * Pure journey state: step index, phase, pause and transition serial.
 * No world, no time, no UObjects, so it can be unit tested headless.
 * Never wraps: Next on the last step and Back on the first step are rejected.
 */
struct VIRTUALSTADIUM_API FLFFJourneyStateMachine
{
    void Load(const TArray<FLFFJourneyStep>& InSteps);

    /** Intro -> first step. The camera is already at the first step, so no transition starts. */
    ELFFCommandResult Start();
    ELFFCommandResult Next();
    ELFFCommandResult Back();
    /** Straight to any step (a timeline mark). Rejected before Start, while paused, out of range, or to the same step. */
    ELFFCommandResult JumpTo(int32 NewIndex);
    /** Any state -> Intro at the first step, unpaused. Invalidates any transition in flight. */
    ELFFCommandResult Restart();
    ELFFCommandResult Pause();
    ELFFCommandResult Resume();

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
    uint32 GetTransitionId() const { return TransitionId; }
    const FLFFJourneyStep* GetCurrentStep() const;

private:
    ELFFCommandResult BeginMoveTo(int32 NewIndex);

    TArray<FLFFJourneyStep> Steps;
    int32 Index = INDEX_NONE;
    ELFFJourneyPhase Phase = ELFFJourneyPhase::Empty;
    bool bPaused = false;
    uint32 TransitionId = 0;
};

namespace LFFJourneyRules
{
    VIRTUALSTADIUM_API ELFFTransitionProfile ResolveTransitionProfile(ELFFTransitionProfile Authored, bool bReducedMotion);
    /**
     * The move actually used: Calm Mode cuts; less movement, or a path that would pass through a wall or prop,
     * fades and cuts; otherwise the authored profile. The camera never flies through geometry.
     */
    VIRTUALSTADIUM_API ELFFTransitionProfile ResolveMove(ELFFTransitionProfile Authored, const FLFFVisitorSettings& Settings, bool bPathBlocked);
    /** "Preview this?" shows on intensity steps unless Calm Mode is on. */
    VIRTUALSTADIUM_API bool ShouldShowPreviewControls(ELFFIntensityPreview Kind, const FLFFVisitorSettings& Settings);
    /** Soft can play: never in Calm Mode, and a Sound preview only when Sound is on. */
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
}
