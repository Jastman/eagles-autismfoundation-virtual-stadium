#include "Journey/LFFJourneyStateMachine.h"

void FLFFJourneyStateMachine::Load(const TArray<FLFFJourneyStep>& InSteps)
{
    Steps = InSteps;
    bPaused = false;
    Mode = ELFFSimulationMode::GuidedTour;
    Bookmark = FLFFTourBookmark();
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

    if (Mode == ELFFSimulationMode::FreeExplore)
    {
        return ELFFCommandResult::RejectedInExplore;
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

    if (Mode == ELFFSimulationMode::FreeExplore)
    {
        return ELFFCommandResult::RejectedInExplore;
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

    if (Mode == ELFFSimulationMode::FreeExplore)
    {
        return ELFFCommandResult::RejectedInExplore;
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

    if (Mode == ELFFSimulationMode::FreeExplore)
    {
        return ELFFCommandResult::RejectedInExplore;
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
    Mode = ELFFSimulationMode::GuidedTour;
    Bookmark = FLFFTourBookmark();
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

ELFFCommandResult FLFFJourneyStateMachine::SwitchToMode(const ELFFSimulationMode NewMode)
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (NewMode == Mode)
    {
        return ELFFCommandResult::RejectedSameMode;
    }

    // Paused means nothing changes until Resume, the mode included.
    if (bPaused)
    {
        return ELFFCommandResult::RejectedPaused;
    }

    if (NewMode == ELFFSimulationMode::GuidedTour)
    {
        return ResumeTour();
    }

    // Index is never written in Free explore. The bookmark makes the place explicit, and Resume tour reads it, so a
    // move from before the switch (it lands on this same step) can't change where the tour picks up.
    Bookmark.StepIndex = Index;
    Bookmark.TourStep = TourStepOf(Index);
    Bookmark.bTourStarted = HasStarted();
    Mode = ELFFSimulationMode::FreeExplore;
    return ELFFCommandResult::Accepted;
}

ELFFCommandResult FLFFJourneyStateMachine::ResumeTour(const int32 AtIndex)
{
    if (Phase == ELFFJourneyPhase::Empty)
    {
        return ELFFCommandResult::RejectedEmpty;
    }

    if (Mode != ELFFSimulationMode::FreeExplore)
    {
        return ELFFCommandResult::RejectedSameMode;
    }

    if (bPaused)
    {
        return ELFFCommandResult::RejectedPaused;
    }

    // The intro sits on step 1, so a tour that never started resumes at the first step.
    const int32 Target = AtIndex != INDEX_NONE ? AtIndex : (Bookmark.IsSet() ? Bookmark.StepIndex : 0);
    if (!Steps.IsValidIndex(Target))
    {
        return Target < 0 ? ELFFCommandResult::RejectedAtStart : ELFFCommandResult::RejectedAtEnd;
    }

    Mode = ELFFSimulationMode::GuidedTour;
    Bookmark = FLFFTourBookmark();
    // A move even to the same step: the camera is wherever Free explore left it. The new id retires any older move.
    return BeginMoveTo(Target);
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
    return Mode == ELFFSimulationMode::GuidedTour && HasStarted() && !bPaused && Index < Steps.Num() - 1 && Steps[Index].bNextEnabled;
}

bool FLFFJourneyStateMachine::CanBack() const
{
    return Mode == ELFFSimulationMode::GuidedTour && HasStarted() && !bPaused && Index > 0 && Steps[Index].bBackEnabled;
}

bool FLFFJourneyStateMachine::IsAtEnd() const
{
    return HasStarted() && Index == Steps.Num() - 1;
}

ELFFTourStep FLFFJourneyStateMachine::GetTourStep() const
{
    return Mode == ELFFSimulationMode::FreeExplore && Bookmark.IsSet() ? Bookmark.TourStep : TourStepOf(Index);
}

ELFFTourStep FLFFJourneyStateMachine::TourStepOf(const int32 StepIndex) const
{
    return LFFJourneyRules::ResolveTourStep(StepIndex, TourStepFirstSteps);
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
    FVector ClampFlyPosition(const FVector& SiteLocation, const float Radius, const float MinZ, const float MaxZ)
    {
        FVector2D Across(SiteLocation.X, SiteLocation.Y);
        if (Across.SizeSquared() > FMath::Square(Radius))
        {
            Across = Across.GetSafeNormal() * Radius;
        }
        return FVector(Across.X, Across.Y, FMath::Clamp(SiteLocation.Z, MinZ, FMath::Max(MinZ, MaxZ)));
    }

    FTransform LevelOverviewPose(const FLFFLevelView& View)
    {
        const FRotator Look(View.ViewPitchDegrees, View.ViewYawDegrees, 0.0f);
        const FVector Target(0.0, 0.0, View.FloorZ);
        return FTransform(Look, Target - Look.Vector() * View.ViewDistance);
    }

    FBox2D UnobstructedViewport(const FVector2D& Size, const TArray<FBox2D>& Panels, const float Inset, const float BottomInset)
    {
        const FBox2D Bounds(FVector2D(Inset, Inset), Size - FVector2D(Inset, BottomInset));
        if (Bounds.GetSize().X <= 1.0 || Bounds.GetSize().Y <= 1.0)
        {
            return FBox2D(ForceInit);
        }
        TArray<FBox2D> Covered;
        TArray<double> X = { Bounds.Min.X, Bounds.Max.X }, Y = { Bounds.Min.Y, Bounds.Max.Y };
        for (const FBox2D& Panel : Panels)
        {
            if (!Panel.bIsValid)
            {
                continue;
            }
            const FBox2D Grown(Panel.Min - FVector2D(Inset, Inset), Panel.Max + FVector2D(Inset, Inset));
            Covered.Add(Grown);
            X.AddUnique(FMath::Clamp(Grown.Min.X, Bounds.Min.X, Bounds.Max.X));
            X.AddUnique(FMath::Clamp(Grown.Max.X, Bounds.Min.X, Bounds.Max.X));
            Y.AddUnique(FMath::Clamp(Grown.Min.Y, Bounds.Min.Y, Bounds.Max.Y));
            Y.AddUnique(FMath::Clamp(Grown.Max.Y, Bounds.Min.Y, Bounds.Max.Y));
        }
        X.Sort();
        Y.Sort();
        FBox2D Best(ForceInit);
        double BestArea = 0.0;
        for (int32 Left = 0; Left < X.Num(); ++Left)
        for (int32 Right = Left + 1; Right < X.Num(); ++Right)
        for (int32 Top = 0; Top < Y.Num(); ++Top)
        for (int32 Bottom = Top + 1; Bottom < Y.Num(); ++Bottom)
        {
            const FBox2D Candidate(FVector2D(X[Left], Y[Top]), FVector2D(X[Right], Y[Bottom]));
            const double Area = Candidate.GetArea();
            const bool bCovered = Covered.ContainsByPredicate([&Candidate](const FBox2D& Panel)
            {
                return Candidate.Min.X < Panel.Max.X - 0.01 && Candidate.Max.X > Panel.Min.X + 0.01
                    && Candidate.Min.Y < Panel.Max.Y - 0.01 && Candidate.Max.Y > Panel.Min.Y + 0.01;
            });
            if (Area > BestArea && !bCovered)
            {
                Best = Candidate;
                BestArea = Area;
            }
        }
        return Best;
    }

    FTransform FramePointsPose(const TArray<FVector>& Points, const float PitchDegrees, const float YawDegrees, const float HorizontalFov,
        const float Aspect, const FBox2D& SafeRect, const float Margin, const float MinDistance)
    {
        if (!ensureMsgf(Points.Num() > 0 && Aspect > 0.0f && HorizontalFov > 0.0f && HorizontalFov < 180.0f
            && SafeRect.bIsValid && SafeRect.GetSize().X > 0.0 && SafeRect.GetSize().Y > 0.0, TEXT("Invalid map framing inputs")))
        {
            return FTransform::Identity;
        }
        FBox Box(ForceInit);
        for (const FVector& Point : Points)
        {
            Box += Point;
        }
        const FVector Centre = Box.GetCenter();
        const FRotator Look(PitchDegrees, YawDegrees, 0.0f);
        const FQuat Rotation = Look.Quaternion();
        const double TanH = FMath::Tan(FMath::DegreesToRadians(HorizontalFov * 0.5f));
        const double TanV = TanH / Aspect;
        const double Left = (2.0 * SafeRect.Min.X - 1.0) * TanH, Right = (2.0 * SafeRect.Max.X - 1.0) * TanH;
        const double Bottom = (1.0 - 2.0 * SafeRect.Max.Y) * TanV, Top = (1.0 - 2.0 * SafeRect.Min.Y) * TanV;
        const double CX = (Left + Right) * 0.5, CY = (Bottom + Top) * 0.5;
        double Distance = MinDistance;
        for (const FVector& Point : Points)
        {
            const FVector Offset = Rotation.UnrotateVector(Point - Centre);
            Distance = FMath::Max(Distance, -Offset.X + Margin + 1.0);
            // Each inequality is a frustum plane, including a spherical world-space margin.
            Distance = FMath::Max(Distance, (Left * Offset.X - Offset.Y + Margin * FMath::Sqrt(1.0 + Left * Left)) / (CX - Left));
            Distance = FMath::Max(Distance, (Offset.Y - Right * Offset.X + Margin * FMath::Sqrt(1.0 + Right * Right)) / (Right - CX));
            Distance = FMath::Max(Distance, (Bottom * Offset.X - Offset.Z + Margin * FMath::Sqrt(1.0 + Bottom * Bottom)) / (CY - Bottom));
            Distance = FMath::Max(Distance, (Offset.Z - Top * Offset.X + Margin * FMath::Sqrt(1.0 + Top * Top)) / (Top - CY));
        }
        return FTransform(Rotation, Centre - Rotation.RotateVector(FVector(Distance, CX * Distance, CY * Distance)));
    }

    ELFFTourStep ResolveTourStep(const int32 StepIndex, const TArray<int32>& TourStepFirstSteps)
    {
        int32 Found = 0;
        for (int32 Chapter = 0; Chapter < FMath::Min(TourStepFirstSteps.Num(), LFFTourStepCount); ++Chapter)
        {
            if (StepIndex >= TourStepFirstSteps[Chapter])
            {
                Found = Chapter;
            }
        }
        return static_cast<ELFFTourStep>(Found);
    }

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
