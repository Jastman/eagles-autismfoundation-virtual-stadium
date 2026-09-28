#include "Journey/LFFJourneyStateMachine.h"

void FLFFJourneyStateMachine::Load(const TArray<FLFFJourneyStep>& InSteps)
{
    Steps = InSteps;
    bPaused = false;
    ++TransitionId;

    if (Steps.Num() == 0)
    {
        Index = INDEX_NONE;
        Phase = ELFFJourneyPhase::Empty;
        return;
    }

    Index = 0;
    Phase = ELFFJourneyPhase::Intro;
}

ELFFCommandResult FLFFJourneyStateMachine::Start()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (Phase != ELFFJourneyPhase::Intro)
    {
        return ELFFCommandResult::RejectedAlreadyStarted;
    }

    Index = 0;
    Phase = ELFFJourneyPhase::AtStep;
    return ELFFCommandResult::Accepted;
}

ELFFCommandResult FLFFJourneyStateMachine::Next()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (Phase == ELFFJourneyPhase::Intro)
    {
        return ELFFCommandResult::RejectedNotStarted;
    }

    if (bPaused)
    {
        return ELFFCommandResult::RejectedPaused;
    }

    if (Index >= Steps.Num() - 1)
    {
        return ELFFCommandResult::RejectedAtEnd;
    }

    if (!Steps[Index].bNextEnabled)
    {
        return ELFFCommandResult::RejectedDisabledByStep;
    }

    return BeginMoveTo(Index + 1);
}

ELFFCommandResult FLFFJourneyStateMachine::Back()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (Phase == ELFFJourneyPhase::Intro)
    {
        return ELFFCommandResult::RejectedNotStarted;
    }

    if (bPaused)
    {
        return ELFFCommandResult::RejectedPaused;
    }

    if (Index <= 0)
    {
        return ELFFCommandResult::RejectedAtStart;
    }

    if (!Steps[Index].bBackEnabled)
    {
        return ELFFCommandResult::RejectedDisabledByStep;
    }

    return BeginMoveTo(Index - 1);
}

ELFFCommandResult FLFFJourneyStateMachine::Restart()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    Index = 0;
    Phase = ELFFJourneyPhase::Intro;
    bPaused = false;
    ++TransitionId;
    return ELFFCommandResult::Accepted;
}

ELFFCommandResult FLFFJourneyStateMachine::Pause()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (Phase == ELFFJourneyPhase::Intro)
    {
        return ELFFCommandResult::RejectedNotStarted;
    }

    bPaused = true;
    return ELFFCommandResult::Accepted;
}

ELFFCommandResult FLFFJourneyStateMachine::Resume()
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    bPaused = false;
    return ELFFCommandResult::Accepted;
}

bool FLFFJourneyStateMachine::NotifyTransitionFinished(const uint32 Id)
{
    if (Phase != ELFFJourneyPhase::Transitioning || Id != TransitionId)
    {
        return false;
    }

    Phase = ELFFJourneyPhase::AtStep;
    return true;
}

bool FLFFJourneyStateMachine::HasStarted() const
{
    return Phase == ELFFJourneyPhase::Transitioning || Phase == ELFFJourneyPhase::AtStep;
}

bool FLFFJourneyStateMachine::CanNext() const
{
    return HasStarted() && !bPaused && Index < Steps.Num() - 1 && Steps[Index].bNextEnabled;
}

bool FLFFJourneyStateMachine::CanBack() const
{
    return HasStarted() && !bPaused && Index > 0 && Steps[Index].bBackEnabled;
}

bool FLFFJourneyStateMachine::IsAtEnd() const
{
    return HasStarted() && Index == Steps.Num() - 1;
}

const FLFFJourneyStep* FLFFJourneyStateMachine::GetCurrentStep() const
{
    return Steps.IsValidIndex(Index) ? &Steps[Index] : nullptr;
}

ELFFCommandResult FLFFJourneyStateMachine::BeginMoveTo(const int32 NewIndex)
{
    Index = NewIndex;
    Phase = ELFFJourneyPhase::Transitioning;
    ++TransitionId;
    return ELFFCommandResult::Accepted;
}

namespace LFFJourneyRules
{
    ELFFTransitionProfile ResolveTransitionProfile(const ELFFTransitionProfile Authored, const bool bReducedMotion)
    {
        if (bReducedMotion && Authored == ELFFTransitionProfile::Ease)
        {
            return ELFFTransitionProfile::ReducedMotion;
        }

        return Authored;
    }

    float ClampDuration(const float AuthoredSeconds, const float DefaultSeconds, const float MinSeconds, const float MaxSeconds)
    {
        const float Seconds = AuthoredSeconds > 0.0f ? AuthoredSeconds : DefaultSeconds;
        return FMath::Clamp(Seconds, MinSeconds, FMath::Max(MinSeconds, MaxSeconds));
    }

    bool ShouldShowDisclaimer(const ELFFDisclaimerLevel Level)
    {
        return Level != ELFFDisclaimerLevel::Verified;
    }
}
