#include "Journey/LFFJourneyStateMachine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    TArray<FLFFJourneyStep> MakeSteps(const int32 Count)
    {
        TArray<FLFFJourneyStep> Steps;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FLFFJourneyStep Step;
            Step.StepId = FName(*FString::Printf(TEXT("step-%d"), Index));
            Step.CameraTargetId = FName(*FString::Printf(TEXT("CT_%d"), Index));
            Steps.Add(Step);
        }
        return Steps;
    }
}

// Run headless: UnrealEditor-Cmd VirtualStadium.uproject -ExecCmds="Automation RunTests LFF.;Quit" -unattended -nullrhi -nopause -log
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyStateMachineNavigationTest, "LFF.Journey.StateMachine.Navigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyStateMachineNavigationTest::RunTest(const FString& Parameters)
{
    FLFFJourneyStateMachine Machine;
    Machine.Load(MakeSteps(4));

    TestTrue(TEXT("Loads into the intro"), Machine.GetPhase() == ELFFJourneyPhase::Intro);
    TestEqual(TEXT("Intro sits on the first step"), Machine.GetIndex(), 0);
    TestTrue(TEXT("Next before Start is rejected"), Machine.Next() == ELFFCommandResult::RejectedNotStarted);
    TestTrue(TEXT("Pause before Start is rejected"), Machine.Pause() == ELFFCommandResult::RejectedNotStarted);

    TestTrue(TEXT("Start is accepted"), Machine.Start() == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Start lands at a step without moving"), Machine.GetPhase() == ELFFJourneyPhase::AtStep);
    TestTrue(TEXT("Second Start is rejected"), Machine.Start() == ELFFCommandResult::RejectedAlreadyStarted);
    TestTrue(TEXT("Back at the first step is rejected"), Machine.Back() == ELFFCommandResult::RejectedAtStart);

    for (int32 Expected = 1; Expected < 4; ++Expected)
    {
        TestTrue(TEXT("Next is accepted"), Machine.Next() == ELFFCommandResult::Accepted);
        TestTrue(TEXT("Next begins a transition"), Machine.GetPhase() == ELFFJourneyPhase::Transitioning);
        TestTrue(TEXT("Current transition finishes"), Machine.NotifyTransitionFinished(Machine.GetTransitionId()));
        TestEqual(TEXT("Index advances by one"), Machine.GetIndex(), Expected);
    }

    TestTrue(TEXT("At the end"), Machine.IsAtEnd());
    TestFalse(TEXT("CanNext is false at the end"), Machine.CanNext());
    TestTrue(TEXT("Next at the end is rejected (no wrap)"), Machine.Next() == ELFFCommandResult::RejectedAtEnd);
    TestEqual(TEXT("Index unchanged after rejected Next"), Machine.GetIndex(), 3);

    TestTrue(TEXT("Back is accepted"), Machine.Back() == ELFFCommandResult::Accepted);
    TestEqual(TEXT("Back moves one step"), Machine.GetIndex(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyStateMachinePauseRestartTest, "LFF.Journey.StateMachine.PauseRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyStateMachinePauseRestartTest::RunTest(const FString& Parameters)
{
    FLFFJourneyStateMachine Machine;
    Machine.Load(MakeSteps(4));
    Machine.Start();
    Machine.Next();

    const uint32 FirstMove = Machine.GetTransitionId();
    TestTrue(TEXT("Pause is accepted"), Machine.Pause() == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Next while paused is rejected"), Machine.Next() == ELFFCommandResult::RejectedPaused);
    TestTrue(TEXT("Back while paused is rejected"), Machine.Back() == ELFFCommandResult::RejectedPaused);
    TestFalse(TEXT("CanNext is false while paused"), Machine.CanNext());
    Machine.Resume();

    TestTrue(TEXT("Next during a transition retargets"), Machine.Next() == ELFFCommandResult::Accepted);
    TestFalse(TEXT("Stale transition id is ignored"), Machine.NotifyTransitionFinished(FirstMove));
    TestTrue(TEXT("Still transitioning after a stale finish"), Machine.GetPhase() == ELFFJourneyPhase::Transitioning);

    Machine.Pause();
    TestTrue(TEXT("Restart is accepted while paused"), Machine.Restart() == ELFFCommandResult::Accepted);
    TestFalse(TEXT("Restart unpauses"), Machine.IsPaused());
    TestTrue(TEXT("Restart returns to the intro"), Machine.GetPhase() == ELFFJourneyPhase::Intro);
    TestEqual(TEXT("Restart returns to the first step"), Machine.GetIndex(), 0);
    TestFalse(TEXT("A transition from before Restart is ignored"), Machine.NotifyTransitionFinished(Machine.GetTransitionId() - 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyStateMachineEdgeCaseTest, "LFF.Journey.StateMachine.EdgeCases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyStateMachineEdgeCaseTest::RunTest(const FString& Parameters)
{
    FLFFJourneyStateMachine Empty;
    Empty.Load(TArray<FLFFJourneyStep>());
    TestTrue(TEXT("Empty: Start"), Empty.Start() == ELFFCommandResult::RejectedEmpty);
    TestTrue(TEXT("Empty: Next"), Empty.Next() == ELFFCommandResult::RejectedEmpty);
    TestTrue(TEXT("Empty: Back"), Empty.Back() == ELFFCommandResult::RejectedEmpty);
    TestTrue(TEXT("Empty: Restart"), Empty.Restart() == ELFFCommandResult::RejectedEmpty);
    TestNull(TEXT("Empty: no current step"), Empty.GetCurrentStep());

    TArray<FLFFJourneyStep> Steps = MakeSteps(3);
    Steps[0].bNextEnabled = false;
    Steps[2].bBackEnabled = false;

    FLFFJourneyStateMachine Gated;
    Gated.Load(Steps);
    Gated.Start();
    TestTrue(TEXT("Step with Next disabled rejects Next"), Gated.Next() == ELFFCommandResult::RejectedDisabledByStep);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyRulesTest, "LFF.Journey.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyRulesTest::RunTest(const FString& Parameters)
{
    using namespace LFFJourneyRules;

    TestTrue(TEXT("Ease stays Ease"), ResolveTransitionProfile(ELFFTransitionProfile::Ease, false) == ELFFTransitionProfile::Ease);
    TestTrue(TEXT("Ease becomes a fade with reduced motion"), ResolveTransitionProfile(ELFFTransitionProfile::Ease, true) == ELFFTransitionProfile::ReducedMotion);
    TestTrue(TEXT("Instant stays Instant"), ResolveTransitionProfile(ELFFTransitionProfile::Instant, true) == ELFFTransitionProfile::Instant);
    TestTrue(TEXT("ReducedMotion stays ReducedMotion"), ResolveTransitionProfile(ELFFTransitionProfile::ReducedMotion, false) == ELFFTransitionProfile::ReducedMotion);

    TestEqual(TEXT("0 uses the default"), ClampDuration(0.0f, 3.0f, 0.5f, 6.0f), 3.0f);
    TestEqual(TEXT("Too long is clamped"), ClampDuration(20.0f, 3.0f, 0.5f, 6.0f), 6.0f);
    TestEqual(TEXT("Too short is clamped"), ClampDuration(0.1f, 3.0f, 0.5f, 6.0f), 0.5f);

    TestTrue(TEXT("Illustrative shows the disclaimer"), ShouldShowDisclaimer(ELFFDisclaimerLevel::Illustrative));
    TestTrue(TEXT("Approximate shows the disclaimer"), ShouldShowDisclaimer(ELFFDisclaimerLevel::Approximate));
    TestFalse(TEXT("Verified hides the disclaimer"), ShouldShowDisclaimer(ELFFDisclaimerLevel::Verified));

    TestTrue(TEXT("Back on step 1 returns to the splash"), ShouldBackReturnToIntro(true, 0, false, true));
    TestFalse(TEXT("Back on the splash does nothing"), ShouldBackReturnToIntro(false, 0, false, true));
    TestFalse(TEXT("Back on step 2 is a normal Back"), ShouldBackReturnToIntro(true, 1, false, true));
    TestFalse(TEXT("Back while paused does nothing"), ShouldBackReturnToIntro(true, 0, true, true));
    TestFalse(TEXT("A step that disables Back keeps it disabled"), ShouldBackReturnToIntro(true, 0, false, false));

    FLFFVisitorSettings Standard;
    FLFFVisitorSettings LessMovement;
    LessMovement.bReducedMotion = true;
    FLFFVisitorSettings Calm;
    Calm.bCalmMode = true;
    FLFFVisitorSettings SoundOn;
    SoundOn.bSoundEnabled = true;
    FLFFVisitorSettings CalmWithSound = Calm;
    CalmWithSound.bSoundEnabled = true;

    TestTrue(TEXT("A clear path eases"), ResolveMove(ELFFTransitionProfile::Ease, Standard, false) == ELFFTransitionProfile::Ease);
    TestTrue(TEXT("A blocked path fades and cuts"), ResolveMove(ELFFTransitionProfile::Ease, Standard, true) == ELFFTransitionProfile::ReducedMotion);
    TestTrue(TEXT("Less movement fades and cuts"), ResolveMove(ELFFTransitionProfile::Ease, LessMovement, false) == ELFFTransitionProfile::ReducedMotion);
    TestTrue(TEXT("Calm Mode cuts"), ResolveMove(ELFFTransitionProfile::Ease, Calm, false) == ELFFTransitionProfile::Instant);
    TestTrue(TEXT("Calm Mode cuts even an authored fade"), ResolveMove(ELFFTransitionProfile::ReducedMotion, Calm, true) == ELFFTransitionProfile::Instant);

    TestTrue(TEXT("Calm Mode is silent"), !CalmWithSound.IsSoundEffective() && SoundOn.IsSoundEffective());
    TestTrue(TEXT("Calm Mode means less movement"), Calm.IsReducedMotionEffective());

    TestFalse(TEXT("No preview on ordinary steps"), ShouldShowPreviewControls(ELFFIntensityPreview::None, Standard));
    TestTrue(TEXT("Preview offered on a loud step"), ShouldShowPreviewControls(ELFFIntensityPreview::Sound, Standard));
    TestFalse(TEXT("Calm Mode hides the preview"), ShouldShowPreviewControls(ELFFIntensityPreview::Lights, Calm));
    TestTrue(TEXT("Lights preview plays with sound off"), CanPlayPreview(ELFFIntensityPreview::Lights, Standard));
    TestFalse(TEXT("Sound preview needs Sound on"), CanPlayPreview(ELFFIntensityPreview::Sound, Standard));
    TestTrue(TEXT("Sound preview plays with Sound on"), CanPlayPreview(ELFFIntensityPreview::Sound, SoundOn));
    TestFalse(TEXT("Calm Mode locks previews off"), CanPlayPreview(ELFFIntensityPreview::Sound, CalmWithSound));

    TestFalse(TEXT("A label above the POI shows"), ShouldHideLabelAt(true, FVector2D(0.3f, 0.4f)));
    TestTrue(TEXT("A label in the lower third hides"), ShouldHideLabelAt(true, FVector2D(0.3f, 0.75f)));
    TestTrue(TEXT("A label behind the camera hides"), ShouldHideLabelAt(false, FVector2D(0.5f, 0.5f)));

    // Voice: off by default, needs Sound, Calm Mode forces it off; never over a preview; no voice on the splash.
    FLFFVisitorSettings VoiceOn = SoundOn;
    VoiceOn.bVoiceEnabled = true;
    FLFFVisitorSettings VoiceNoSound;
    VoiceNoSound.bVoiceEnabled = true;
    FLFFVisitorSettings VoiceCalm = VoiceOn;
    VoiceCalm.bCalmMode = true;
    TestFalse(TEXT("Voice is off by default"), Standard.IsVoiceEffective());
    TestTrue(TEXT("Voice and Sound on reads the card"), ShouldPlayVoice(VoiceOn, true, false));
    TestFalse(TEXT("Sound Off mutes the voice"), ShouldPlayVoice(VoiceNoSound, true, false));
    TestFalse(TEXT("Calm Mode forces Voice off"), ShouldPlayVoice(VoiceCalm, true, false));
    TestFalse(TEXT("No voice on the splash"), ShouldPlayVoice(VoiceOn, false, false));
    TestFalse(TEXT("No voice over a preview"), ShouldPlayVoice(VoiceOn, true, true));
    TestTrue(TEXT("-8 dB duck is about 0.4"), FMath::IsNearlyEqual(DecibelsToGain(-8.0f), 0.398f, 0.002f));

    // Crowd: Low by default, Off on request, hidden in Calm Mode.
    FLFFVisitorSettings CrowdOff;
    CrowdOff.bCrowdEnabled = false;
    TestTrue(TEXT("Crowd Low by default"), Standard.IsCrowdShown());
    TestFalse(TEXT("Crowd Off hides it"), CrowdOff.IsCrowdShown());
    TestFalse(TEXT("Calm Mode hides the crowd"), Calm.IsCrowdShown());

    // The anthem can still be seen with Sound off; its separate audio is muted, and Calm Mode locks both off.
    TestTrue(TEXT("Anthem preview plays with sound off"), CanPlayPreview(ELFFIntensityPreview::Anthem, Standard));
    TestFalse(TEXT("Calm Mode locks the anthem preview off"), CanPlayPreview(ELFFIntensityPreview::Anthem, Calm));

    // Arrival cues: closed before their step, open on it, stay open after; the splash is always closed.
    TestTrue(TEXT("Cue on the splash is Before"), ResolveCueState(false, 5, 5) == ELFFCueState::Before);
    TestTrue(TEXT("Cue before its step is Before"), ResolveCueState(true, 3, 5) == ELFFCueState::Before);
    TestTrue(TEXT("Cue on its step plays"), ResolveCueState(true, 5, 5) == ELFFCueState::Play);
    TestTrue(TEXT("Cue after its step is After"), ResolveCueState(true, 9, 5) == ELFFCueState::After);
    TestTrue(TEXT("An unused cue is Before"), ResolveCueState(true, 9, INDEX_NONE) == ELFFCueState::Before);

    TestTrue(TEXT("The splash orbits by default"), ShouldOrbitSplash(Standard));
    TestFalse(TEXT("Less movement holds the splash still"), ShouldOrbitSplash(LessMovement));
    TestFalse(TEXT("Calm Mode holds the splash still"), ShouldOrbitSplash(Calm));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyStateMachineJumpTest, "LFF.Journey.StateMachine.Jump",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyStateMachineJumpTest::RunTest(const FString& Parameters)
{
    FLFFJourneyStateMachine Machine;
    Machine.Load(MakeSteps(6));
    TestTrue(TEXT("Jump before Start is rejected"), Machine.JumpTo(3) == ELFFCommandResult::RejectedNotStarted);

    Machine.Start();
    TestTrue(TEXT("Jump forward is accepted"), Machine.JumpTo(4) == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Jump begins a transition"), Machine.GetPhase() == ELFFJourneyPhase::Transitioning);
    TestEqual(TEXT("Jump lands on the mark's step"), Machine.GetIndex(), 4);
    TestTrue(TEXT("Jump back is accepted"), Machine.JumpTo(1) == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Jump to a missing step is rejected"), Machine.JumpTo(6) == ELFFCommandResult::RejectedAtEnd);
    TestTrue(TEXT("Jump below zero is rejected"), Machine.JumpTo(-1) == ELFFCommandResult::RejectedAtStart);
    TestTrue(TEXT("Jump to the current step does nothing"), Machine.JumpTo(1) != ELFFCommandResult::Accepted);
    TestEqual(TEXT("Index unchanged after rejected jumps"), Machine.GetIndex(), 1);

    Machine.Pause();
    TestTrue(TEXT("Jump while paused is rejected"), Machine.JumpTo(5) == ELFFCommandResult::RejectedPaused);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyStateMachineModesTest, "LFF.Journey.StateMachine.Modes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyStateMachineModesTest::RunTest(const FString& Parameters)
{
    // The real day: 23 steps in five chapters (ALFFGameMode::TourStepFirstSteps).
    const TArray<int32> FirstSteps = { 0, 4, 7, 10, 14 };
    FLFFJourneyStateMachine Machine;
    Machine.Load(MakeSteps(23));
    Machine.SetTourSteps(FirstSteps);

    using LFFJourneyRules::ResolveTourStep;
    TestTrue(TEXT("Step 1 is Arrival"), ResolveTourStep(0, FirstSteps) == ELFFTourStep::Arrival);
    TestTrue(TEXT("Step 5 opens Gates"), ResolveTourStep(4, FirstSteps) == ELFFTourStep::Gates);
    TestTrue(TEXT("Step 7 is still Gates"), ResolveTourStep(6, FirstSteps) == ELFFTourStep::Gates);
    TestTrue(TEXT("Step 9 is Plaza"), ResolveTourStep(8, FirstSteps) == ELFFTourStep::Plaza);
    TestTrue(TEXT("Breaks and help belong to Game"), ResolveTourStep(22, FirstSteps) == ELFFTourStep::Game);
    TestTrue(TEXT("No chapters: everything is Arrival"), ResolveTourStep(12, TArray<int32>()) == ELFFTourStep::Arrival);

    // Tour to step 9 (Popcorn for the People), then Next towards step 10 and switch while that move is in flight.
    Machine.Start();
    Machine.JumpTo(8);
    Machine.NotifyTransitionFinished(Machine.GetTransitionId());
    Machine.Next();
    const uint32 InFlight = Machine.GetTransitionId();
    TestTrue(TEXT("Free explore is accepted mid-move"), Machine.SwitchToMode(ELFFSimulationMode::FreeExplore) == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Now in Free explore"), Machine.GetMode() == ELFFSimulationMode::FreeExplore);
    TestEqual(TEXT("The bookmark is the step the tour was on"), Machine.GetBookmark().StepIndex, 9);
    TestTrue(TEXT("The bookmark keeps its chapter"), Machine.GetBookmark().TourStep == ELFFTourStep::Plaza);
    TestTrue(TEXT("The bookmark knows the tour had started"), Machine.GetBookmark().bTourStarted);
    TestTrue(TEXT("Switching to the same mode is rejected"), Machine.SwitchToMode(ELFFSimulationMode::FreeExplore) == ELFFCommandResult::RejectedSameMode);
    TestFalse(TEXT("Explore retires the old arrival callback"), Machine.NotifyTransitionFinished(InFlight));
    TestTrue(TEXT("Explore is no longer stuck waiting on cancelled travel"), Machine.GetPhase() == ELFFJourneyPhase::AtStep);
    TestTrue(TEXT("A retired finish doesn't leave Free explore"), Machine.GetMode() == ELFFSimulationMode::FreeExplore);

    // Free explore can't move the step.
    TestTrue(TEXT("Next is rejected in Free explore"), Machine.Next() == ELFFCommandResult::RejectedInExplore);
    TestTrue(TEXT("Back is rejected in Free explore"), Machine.Back() == ELFFCommandResult::RejectedInExplore);
    TestTrue(TEXT("Jump is rejected in Free explore"), Machine.JumpTo(2) == ELFFCommandResult::RejectedInExplore);
    TestFalse(TEXT("CanNext is false in Free explore"), Machine.CanNext());
    TestFalse(TEXT("CanBack is false in Free explore"), Machine.CanBack());
    TestEqual(TEXT("The step is unchanged"), Machine.GetIndex(), 9);
    TestTrue(TEXT("The arc keeps the bookmarked chapter"), Machine.GetTourStep() == ELFFTourStep::Plaza);

    // Paused means nothing changes, the mode included.
    Machine.Pause();
    TestTrue(TEXT("Resume tour while paused is rejected"), Machine.ResumeTour() == ELFFCommandResult::RejectedPaused);
    Machine.Resume();

    // Resume tour: back to the bookmark with a new move (the caller fade-cuts there).
    const uint32 BeforeResume = Machine.GetTransitionId();
    TestTrue(TEXT("Resume tour is accepted"), Machine.ResumeTour() == ELFFCommandResult::Accepted);
    TestTrue(TEXT("Back in the Guided tour"), Machine.GetMode() == ELFFSimulationMode::GuidedTour);
    TestEqual(TEXT("Resumes on the bookmarked step"), Machine.GetIndex(), 9);
    TestTrue(TEXT("Resume starts a move even to the same step"), Machine.GetPhase() == ELFFJourneyPhase::Transitioning);
    TestTrue(TEXT("Resume retires older moves"), Machine.GetTransitionId() != BeforeResume);
    TestFalse(TEXT("The bookmark is cleared"), Machine.GetBookmark().IsSet());
    TestTrue(TEXT("Resume tour in the tour is rejected"), Machine.ResumeTour() == ELFFCommandResult::RejectedSameMode);
    Machine.NotifyTransitionFinished(Machine.GetTransitionId());
    TestTrue(TEXT("Next works again"), Machine.Next() == ELFFCommandResult::Accepted);
    TestEqual(TEXT("Next goes on from the bookmark"), Machine.GetIndex(), 10);

    // An arc node in Free explore resumes at that chapter instead.
    Machine.SwitchToMode(ELFFSimulationMode::FreeExplore);
    TestTrue(TEXT("Resume at a chapter is accepted"), Machine.ResumeTour(14) == ELFFCommandResult::Accepted);
    TestEqual(TEXT("Resumes at the chapter's step"), Machine.GetIndex(), 14);
    TestTrue(TEXT("That's Game"), Machine.GetTourStep() == ELFFTourStep::Game);
    Machine.SwitchToMode(ELFFSimulationMode::FreeExplore);
    TestTrue(TEXT("Resume at a missing step is rejected"), Machine.ResumeTour(23) == ELFFCommandResult::RejectedAtEnd);
    TestTrue(TEXT("A rejected resume stays in Free explore"), Machine.GetMode() == ELFFSimulationMode::FreeExplore);

    // Restart lands in the Guided tour.
    Machine.Restart();
    TestTrue(TEXT("Restart returns to the Guided tour"), Machine.GetMode() == ELFFSimulationMode::GuidedTour);
    TestFalse(TEXT("Restart clears the bookmark"), Machine.GetBookmark().IsSet());

    // Free explore straight from the welcome screen, then Resume tour starts the tour at step 1.
    TestTrue(TEXT("Free explore from the welcome screen"), Machine.SwitchToMode(ELFFSimulationMode::FreeExplore) == ELFFCommandResult::Accepted);
    TestFalse(TEXT("The tour hasn't started"), Machine.GetBookmark().bTourStarted);
    TestTrue(TEXT("Start is a tour command"), Machine.Start() == ELFFCommandResult::RejectedInExplore);
    TestTrue(TEXT("Resume tour starts the tour"), Machine.ResumeTour() == ELFFCommandResult::Accepted);
    TestTrue(TEXT("The tour has started"), Machine.HasStarted());
    TestEqual(TEXT("At step 1"), Machine.GetIndex(), 0);

    FLFFJourneyStateMachine Empty;
    Empty.Load(TArray<FLFFJourneyStep>());
    TestTrue(TEXT("Empty: Free explore"), Empty.SwitchToMode(ELFFSimulationMode::FreeExplore) == ELFFCommandResult::RejectedEmpty);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
