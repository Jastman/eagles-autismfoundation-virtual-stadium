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
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
