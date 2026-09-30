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

ELFFCommandResult FLFFJourneyStateMachine::JumpTo(const int32 NewIndex)
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

    if (!Steps.IsValidIndex(NewIndex))
    {
        return NewIndex < 0 ? ELFFCommandResult::RejectedAtStart : ELFFCommandResult::RejectedAtEnd;
    }

    if (NewIndex == Index)
    {
        return ELFFCommandResult::RejectedDisabledByStep;
    }

    return BeginMoveTo(NewIndex);
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

    ELFFTransitionProfile ResolveMove(const ELFFTransitionProfile Authored, const FLFFVisitorSettings& Settings, const bool bPathBlocked)
    {
        if (Settings.bCalmMode)
        {
            return ELFFTransitionProfile::Instant;
        }

        return ResolveTransitionProfile(Authored, Settings.IsReducedMotionEffective() || bPathBlocked);
    }

    bool ShouldShowPreviewControls(const ELFFIntensityPreview Kind, const FLFFVisitorSettings& Settings)
    {
        return Kind != ELFFIntensityPreview::None && !Settings.bCalmMode;
    }

    bool CanPlayPreview(const ELFFIntensityPreview Kind, const FLFFVisitorSettings& Settings)
    {
        if (!ShouldShowPreviewControls(Kind, Settings))
        {
            return false;
        }

        // Lights and Anthem are visual only; the Sound sample needs Sound on.
        return Kind != ELFFIntensityPreview::Sound || Settings.IsSoundEffective();
    }

    bool ShouldHideLabelAt(const bool bInFront, const FVector2D& ScreenFraction)
    {
        return !bInFront || ScreenFraction.Y > 2.0f / 3.0f;
    }

    float ClampDuration(const float AuthoredSeconds, const float DefaultSeconds, const float MinSeconds, const float MaxSeconds)
    {
        const float Seconds = AuthoredSeconds > 0.0f ? AuthoredSeconds : DefaultSeconds;
        return FMath::Clamp(Seconds, MinSeconds, FMath::Max(MinSeconds, MaxSeconds));
    }

    bool ShouldBackReturnToIntro(const bool bStarted, const int32 Index, const bool bPaused, const bool bStepAllowsBack)
    {
        return bStarted && Index == 0 && !bPaused && bStepAllowsBack;
    }

    bool ShouldShowDisclaimer(const ELFFDisclaimerLevel Level)
    {
        return Level != ELFFDisclaimerLevel::Verified;
    }

    ELFFCueState ResolveCueState(const bool bStarted, const int32 CurrentIndex, const int32 FirstCueIndex)
    {
        if (!bStarted || FirstCueIndex == INDEX_NONE || CurrentIndex < FirstCueIndex)
        {
            return ELFFCueState::Before;
        }

        return CurrentIndex == FirstCueIndex ? ELFFCueState::Play : ELFFCueState::After;
    }

    bool ShouldOrbitSplash(const FLFFVisitorSettings& Settings)
    {
        return !Settings.IsReducedMotionEffective();
    }

    bool ShouldPlayVoice(const FLFFVisitorSettings& Settings, const bool bStarted, const bool bPreviewOn)
    {
        return bStarted && Settings.IsVoiceEffective() && !bPreviewOn;
    }

    float DecibelsToGain(const float Decibels)
    {
        return FMath::Pow(10.0f, Decibels / 20.0f);
    }
}
