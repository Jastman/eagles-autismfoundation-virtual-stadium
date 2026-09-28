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
    VIRTUALSTADIUM_API float ClampDuration(float AuthoredSeconds, float DefaultSeconds, float MinSeconds, float MaxSeconds);
    VIRTUALSTADIUM_API bool ShouldShowDisclaimer(ELFFDisclaimerLevel Level);
    /** The visitor toggles are offered on the intro and whenever Settings is open (one set of buttons, LEDGER D13). */
    VIRTUALSTADIUM_API bool ShouldShowToggles(bool bStarted, bool bSettingsOpen);
}
