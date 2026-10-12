#include "Camera/LFFCameraRig.h"
#include "Camera/LFFCameraTarget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Core/LFFGameMode.h"
#include "Core/LFFPlayerController.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "Journey/LFFJourneySubsystem.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "World/LFFCueActor.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FLFFJourneyRuntimeTestAccess
{
    static void Configure(ULFFJourneySubsystem& Journey, ALFFGameMode* Mode, ALFFCameraRig* Rig)
    {
        Journey.GameMode = Mode;
        Journey.Rig = Rig;
        Journey.Settings = Mode->DefaultVisitorSettings;
        Journey.Settings.bSoundEnabled = false;
        Journey.Settings.bVoiceEnabled = false;
        Mode->AmbientBed.Reset();
        Mode->StadiumAmbientBed.Reset();
        Mode->AnthemSound.Reset();
        Mode->UIClickSound.Reset();
        TArray<FLFFJourneyStep> Steps;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FLFFJourneyStep Step;
            Step.StepId = FName(*FString::Printf(TEXT("runtime-%d"), Index));
            Step.CameraTargetId = TEXT("CT_RuntimeTest");
            Step.TransitionProfile = ELFFTransitionProfile::ReducedMotion;
            if (Index == 1)
            {
                Step.IntensityPreview = ELFFIntensityPreview::Anthem;
                Step.SceneCues.Add(TEXT("Anthem"));
            }
            Steps.Add(Step);
        }
        Journey.Machine.Load(Steps);
        Journey.Machine.Start();
    }

    static FLFFJourneyStateMachine& Machine(ULFFJourneySubsystem& Journey) { return Journey.Machine; }
    static void Arrive(ULFFJourneySubsystem& Journey)
    {
        Journey.Machine.NotifyTransitionFinished(Journey.Machine.GetTransitionId());
        Journey.bArrived = true;
    }
    static void Modal(ULFFJourneySubsystem& Journey, const bool bOpen) { Journey.bSettingsOpen = bOpen; }
    static FLFFVisitorSettings& Settings(ULFFJourneySubsystem& Journey) { return Journey.Settings; }
    static void AddQuietBox(ULFFJourneySubsystem& Journey, UBoxComponent* Box) { Journey.QuietBoxes.Add(Box); }
    static float RoomGain(const ULFFJourneySubsystem& Journey) { return Journey.GetRoomCrowdGain(); }
    static bool HasArrived(const ULFFJourneySubsystem& Journey) { return Journey.bArrived; }
    static bool HasClick(const ULFFJourneySubsystem& Journey) { return Journey.UIClickAudio != nullptr; }

    static void StartAnthem(ULFFJourneySubsystem& Journey, ALFFCueActor* Eagle)
    {
        Journey.Settings.bSoundEnabled = true;
        Journey.Machine.JumpTo(1);
        Journey.Machine.NotifyTransitionFinished(Journey.Machine.GetTransitionId());
        Journey.bArrived = true;
        Journey.CueActors.Add(Eagle);
        Journey.StartAnthemSequence();
        Journey.AnthemAudio = NewObject<UAudioComponent>(Journey.Rig);
    }

    static bool HasAnthem(const ULFFJourneySubsystem& Journey) { return Journey.AnthemAudio != nullptr; }
    static bool HasAnthemStart(const ULFFJourneySubsystem& Journey)
    {
        return Journey.GetWorld()->GetTimerManager().TimerExists(Journey.AnthemAudioStartTimer);
    }
    static bool HasAnthemAudioTimers(const ULFFJourneySubsystem& Journey)
    {
        const FTimerManager& Timers = Journey.GetWorld()->GetTimerManager();
        return Timers.TimerExists(Journey.AnthemAudioStartTimer) || Timers.TimerExists(Journey.AnthemAudioEndTimer)
            || Timers.TimerExists(Journey.AnthemAudioWatchTimer);
    }
    static void StopAnthem(ULFFJourneySubsystem& Journey) { Journey.StopAnthemSequence(true); }
    static void CheckAnthemFlight(ULFFJourneySubsystem& Journey) { Journey.UpdateAnthemAudio(); }
    static void LeaveAnthemStep(ULFFJourneySubsystem& Journey)
    {
        Journey.Machine.Next();
        Journey.StopAnthemSequence(false);
    }
    static void SeedAudio(ULFFJourneySubsystem& Journey)
    {
        Journey.BedAudio = NewObject<UAudioComponent>(Journey.Rig);
        Journey.FadingBedAudio = NewObject<UAudioComponent>(Journey.Rig);
        Journey.PreviewAudio = NewObject<UAudioComponent>(Journey.Rig);
        Journey.UIClickAudio = NewObject<UAudioComponent>(Journey.Rig);
        Journey.VoiceAudio = NewObject<UAudioComponent>(Journey.Rig);
        Journey.bVoicePlaying = true;
    }
    static bool HasAnyAudio(const ULFFJourneySubsystem& Journey)
    {
        return Journey.BedAudio != nullptr || Journey.FadingBedAudio != nullptr || Journey.PreviewAudio != nullptr
            || Journey.UIClickAudio != nullptr || Journey.VoiceAudio != nullptr || Journey.AnthemAudio != nullptr;
    }
};

struct FLFFPlayerControllerTestAccess
{
    static void Build(ALFFPlayerController& Controller) { Controller.BuildInputObjects(); }
    static bool HasLook(const ALFFPlayerController& Controller, const bool bFlight)
    {
        const UInputMappingContext* Context = bFlight ? Controller.FlyContext.Get() : Controller.JourneyContext.Get();
        return Context->GetMappings().ContainsByPredicate([&Controller](const FEnhancedActionKeyMapping& Mapping)
        {
            return Mapping.Action == Controller.FlyLookAction && Mapping.Key == EKeys::Mouse2D;
        });
    }
};

namespace
{
    struct FRuntimeWorld
    {
        UWorld* World = nullptr;
        ULFFJourneySubsystem* Journey = nullptr;
        ALFFGameMode* Mode = nullptr;
        ALFFCameraRig* Rig = nullptr;

        FRuntimeWorld()
        {
            const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
                .RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
                .ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            Mode = World->SpawnActor<ALFFGameMode>();
            Rig = World->SpawnActor<ALFFCameraRig>();
            Journey = NewObject<ULFFJourneySubsystem>(World);
            FLFFJourneyRuntimeTestAccess::Configure(*Journey, Mode, Rig);
            ALFFCameraTarget* Target = World->SpawnActor<ALFFCameraTarget>();
            Target->TargetId = TEXT("CT_RuntimeTest");
            Target->SetActorLocationAndRotation(FVector(1000, -300, 500), FRotator(-10, 15, 0));
            Rig->SnapTo(Target->GetCameraTransform(), Target->FieldOfView);
        }

        ~FRuntimeWorld()
        {
            Journey->Deinitialize();
            World->DestroyWorld(false);
        }

        ALFFCueActor* Eagle()
        {
            ALFFCueActor* Cue = World->SpawnActor<ALFFCueActor>();
            Cue->CueId = TEXT("Anthem");
            Cue->Trigger = ELFFCueTrigger::Preview;
            Cue->PlayDelaySeconds = 1.0f;
            Cue->PlaySeconds = 2.0f;
            Cue->bHiddenBefore = true;
            Cue->bHiddenAfter = true;
            return Cue;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyCameraRuntimeTest, "LFF.Journey.Runtime.CameraControl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyCameraRuntimeTest::RunTest(const FString& Parameters)
{
    FRuntimeWorld Fixture;
    ULFFJourneySubsystem& Journey = *Fixture.Journey;
    FLFFJourneyStateMachine& Machine = FLFFJourneyRuntimeTestAccess::Machine(Journey);
    Machine.Next();
    const uint32 TourMove = Machine.GetTransitionId();
    const FTransform Before = Fixture.Rig->GetActorTransform();
    APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
    if (Fixture.World->GetFirstPlayerController() != Controller)
    {
        Fixture.World->AddController(Controller);
    }
    if (Controller->PlayerCameraManager == nullptr)
    {
        Controller->PlayerCameraManager = Fixture.World->SpawnActor<APlayerCameraManager>();
        Controller->PlayerCameraManager->InitializeFor(Controller);
    }
    APlayerCameraManager* Camera = Controller->PlayerCameraManager;
    if (!TestNotNull(TEXT("Minimal world has a real camera manager"), Camera))
    {
        return false;
    }
    Camera->SetManualCameraFade(1.0f, FLinearColor::Black, false);
    Fixture.Rig->BeginTransition(FTransform(FRotator(-80, 0, 0), FVector(5000, 4000, 2000)), 60,
        ELFFTransitionProfile::ReducedMotion, 2.5f, 0.35f, FLinearColor::Black, TourMove);
    Journey.SwitchToMode(ELFFSimulationMode::FreeExplore);
    TestTrue(TEXT("First Explore keeps the live pose"), Fixture.Rig->GetActorTransform().Equals(Before));
    TestFalse(TEXT("Explore cancels pending travel/fade"), Fixture.Rig->IsBusy());
    TestFalse(TEXT("Cancelling a held fade restores the picture"), static_cast<bool>(Camera->bEnableFading));
    TestEqual(TEXT("The cleared fade cannot be re-enabled at black by another move"), Camera->FadeAmount, 0.0f);
    Journey.HandleRigTransitionFinished(TourMove);
    TestFalse(TEXT("Old tour completion cannot start arrival cues"), FLFFJourneyRuntimeTestAccess::HasArrived(Journey));

    const FVector Start(0, 0, 1000);
    Fixture.Rig->SnapTo(FTransform(FRotator::ZeroRotator, Start), 70);
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Journey.FlyTick(FVector2D(0, 1), 0, FVector2D::ZeroVector, 0, false, 1.0f / 60.0f);
    }
    TestTrue(TEXT("Cruise travels over 28m in one second with a comfortable start"), Fixture.Rig->GetActorLocation().X > 2800);
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(100, 0), 0, false, 1.0f / 60.0f);
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f / 60.0f);
    }
    TestTrue(TEXT("100px look drag turns 28 degrees"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Yaw, 28.0f, 0.01f));

    const FTransform Overview(FRotator(-30, 20, 0), FVector(70000, 0, 40000));
    Fixture.Rig->HoldPosition();
    Fixture.Rig->SnapTo(Overview, 60);
    FLFFJourneyRuntimeTestAccess::Settings(Journey).bReducedMotion = true;
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f / 60.0f);
    TestTrue(TEXT("Idle flight cannot pull a fitted overview into bounds"), Fixture.Rig->GetActorTransform().Equals(Overview));
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(10, 0), 0, false, 1.0f / 60.0f);
    TestTrue(TEXT("Look-only input also preserves fitted location"), Fixture.Rig->GetActorLocation().Equals(Overview.GetLocation()));
    Journey.FlyTick(FVector2D(0, -1), 0, FVector2D::ZeroVector, 0, false, 1.0f / 60.0f);
    TestTrue(TEXT("Moving from an outside overview never snaps to the flight ceiling"),
        FVector::Dist(Fixture.Rig->GetActorLocation(), Overview.GetLocation()) < 100);
    const FVector Stopped = Fixture.Rig->GetActorLocation();
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f / 60.0f);
    TestTrue(TEXT("Less movement has no drift after releasing input"), Fixture.Rig->GetActorLocation().Equals(Stopped));

    Fixture.Rig->BeginTransition(FTransform(FRotator::ZeroRotator, FVector(0, 0, 1000)), 70,
        ELFFTransitionProfile::Instant, 1, 0.35f, FLinearColor::Black, 42);
    const FTransform NewEnd(FRotator(-10, 10, 0), FVector(10000, 0, 1000));
    Fixture.Rig->BeginTransition(NewEnd, 70, ELFFTransitionProfile::Ease, 2, 0.35f, FLinearColor::Black, 42);
    Fixture.World->GetTimerManager().Tick(1.0f / 60.0f);
    TestTrue(TEXT("Same-id retarget cannot be finished by an old next-tick callback"), Fixture.Rig->IsBusy());
    TestFalse(TEXT("Retarget waits for its own motion rather than teleporting"), Fixture.Rig->GetActorTransform().Equals(NewEnd));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyDragLookRuntimeTest, "LFF.Journey.Runtime.DragLook",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyDragLookRuntimeTest::RunTest(const FString& Parameters)
{
    FRuntimeWorld Fixture;
    ULFFJourneySubsystem& Journey = *Fixture.Journey;
    FLFFJourneyStateMachine& Machine = FLFFJourneyRuntimeTestAccess::Machine(Journey);
    ALFFPlayerController* Controller = Fixture.World->SpawnActor<ALFFPlayerController>();
    FLFFPlayerControllerTestAccess::Build(*Controller);
    TestTrue(TEXT("Tour has the mouse-look mapping"), FLFFPlayerControllerTestAccess::HasLook(*Controller, false));
    TestFalse(TEXT("Explore does not double-map mouse look"), FLFFPlayerControllerTestAccess::HasLook(*Controller, true));
    UEnhancedPlayerInput* PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
    PlayerInput->ForceRebuildingKeyMaps();
    FInputAxisProperties MouseProperties;
    TestTrue(TEXT("Mouse2D has explicit axis properties"), PlayerInput->GetAxisProperties(EKeys::Mouse2D, MouseProperties));
    TestEqual(TEXT("Inherited legacy scaling cannot shrink mouse pixel deltas"), MouseProperties.Sensitivity, 1.0f);
    TestEqual(TEXT("Mouse pixel deltas have no dead zone"), MouseProperties.DeadZone, 0.0f);
    TestEqual(TEXT("Mouse pixel deltas stay linear"), MouseProperties.Exponent, 1.0f);
    TestFalse(TEXT("Before arrival the tour owns the view"), Journey.CanLookAround());
    FLFFJourneyRuntimeTestAccess::Arrive(Journey);
    const FRotator IdlePose(-89, 15, 4);
    Fixture.Rig->SetActorRotation(IdlePose);
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f);
    TestTrue(TEXT("Idle tour ticks preserve the authored pose"), Fixture.Rig->GetActorRotation().Equals(IdlePose));
    Fixture.Rig->SetActorRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Arrived tour steps allow look"), Journey.CanLookAround());
    FLFFJourneyRuntimeTestAccess::SeedAudio(Journey);
    const FVector TourLocation = Fixture.Rig->GetActorLocation();
    const uint32 TransitionId = Machine.GetTransitionId();
    const FLFFJourneySnapshot Before = Journey.GetSnapshot();
    Journey.FlyTick(FVector2D(1, 1), 1, FVector2D(100, 50), 2, true, 1.0f);
    TestTrue(TEXT("Tour drag turns the view"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Yaw, 28.0, 0.01));
    TestTrue(TEXT("Tour drag pitches the view"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Pitch, 14.0, 0.01));
    TestTrue(TEXT("Tour cannot fly, rise, or dolly"), Fixture.Rig->GetActorLocation().Equals(TourLocation));
    TestEqual(TEXT("Look does not change the tour step"), Machine.GetIndex(), 0);
    TestEqual(TEXT("Look does not restart tour travel"), Machine.GetTransitionId(), TransitionId);
    TestTrue(TEXT("Look stays in Guided Tour"), Machine.GetMode() == ELFFSimulationMode::GuidedTour);
    const FLFFJourneySnapshot After = Journey.GetSnapshot();
    TestTrue(TEXT("Look preserves narration and preview state"), Before.bVoicePlaying == After.bVoicePlaying
        && Before.bPreviewOn == After.bPreviewOn);

    for (const int32 Fps : {30, 60, 120})
    {
        Fixture.Rig->SetActorRotation(FRotator::ZeroRotator);
        Journey.ResetLookInput();
        const float Dt = 1.0f / Fps;
        Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(100, 0), 0, false, Dt);
        TestTrue(TEXT("Burst input is filtered, not applied as a 28-degree jump"), Fixture.Rig->GetActorRotation().Yaw > 0.0
            && Fixture.Rig->GetActorRotation().Yaw < 16.0);
        for (int32 Frame = 1; Frame < Fps; ++Frame)
        {
            Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, Dt);
            if (Frame == Fps / 10 - 1)
            {
                TestTrue(TEXT("The response after 100ms is independent of frame rate"), FMath::IsNearlyEqual(
                    Fixture.Rig->GetActorRotation().Yaw, 28.0 * (1.0 - FMath::Exp(-0.1 / 0.045)), 0.001));
            }
        }
        TestTrue(TEXT("Sensitivity preserves total pixels at every frame rate"), FMath::IsNearlyEqual(
            Fixture.Rig->GetActorRotation().Yaw, 28.0, 0.001));
    }

    Fixture.Rig->SetActorRotation(FRotator::ZeroRotator);
    Journey.ResetLookInput();
    double PreviousYaw = 0.0;
    double LargestFrameTurn = 0.0;
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        // The same cursor speed delivered in alternating bursts and empty frames.
        Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(Frame % 2 == 0 ? 20 : 0, 0), 0, false, 1.0f / 60.0f);
        const double Yaw = Fixture.Rig->GetActorRotation().Yaw;
        TestTrue(TEXT("Uneven samples never reverse camera motion"), Yaw >= PreviousYaw);
        LargestFrameTurn = FMath::Max(LargestFrameTurn, Yaw - PreviousYaw);
        PreviousYaw = Yaw;
    }
    TestTrue(TEXT("Alternating samples reduce the largest raw 5.6-degree jump"), LargestFrameTurn < 3.4);
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(0, 10000), 0, false, 1.0f);
    TestTrue(TEXT("Upward look stays within the pitch limit"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Pitch, 70.0, 0.001));
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(0, -10000), 0, false, 1.0f);
    TestTrue(TEXT("Downward look stays within the pitch limit"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Pitch, -85.0, 0.001));
    TestEqual(TEXT("Look never rolls"), Fixture.Rig->GetActorRotation().Roll, 0.0);

    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(50, 0), 0, false, 1.0f / 60.0f);
    Journey.ResetLookInput();
    const FRotator Released = Fixture.Rig->GetActorRotation();
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f);
    TestTrue(TEXT("Capture loss or release leaves no trailing rotation"), Fixture.Rig->GetActorRotation().Equals(Released, 0.001f));
    FLFFJourneyRuntimeTestAccess::Modal(Journey, true);
    TestFalse(TEXT("Settings block tour look"), Journey.CanLookAround());
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(500, 500), 0, false, 1.0f);
    TestTrue(TEXT("Modal input never moves the view"), Fixture.Rig->GetActorRotation().Equals(Released));
    FLFFJourneyRuntimeTestAccess::Modal(Journey, false);
    Machine.Pause();
    TestFalse(TEXT("Pause blocks tour look"), Journey.CanLookAround());
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(500, 500), 0, false, 1.0f);
    TestTrue(TEXT("Paused input never moves the view"), Fixture.Rig->GetActorRotation().Equals(Released));
    Machine.Resume();

    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(100, 0), 0, false, 1.0f / 60.0f);
    Journey.Next();
    TestFalse(TEXT("Next step owns its authored transition"), Journey.CanLookAround());
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(500, 500), 0, false, 1.0f);
    Fixture.Rig->HoldPosition();
    const FRotator Authored(0, 45, 0);
    Fixture.Rig->SetActorRotation(Authored);
    FLFFJourneyRuntimeTestAccess::Arrive(Journey);
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f);
    TestTrue(TEXT("No pending drag carries into the next step"), Fixture.Rig->GetActorRotation().Equals(Authored));
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(50, 0), 0, false, 1.0f / 60.0f);
    Fixture.Rig->SetActorRotation(Authored);
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D::ZeroVector, 0, false, 1.0f);
    TestTrue(TEXT("An externally restored pose discards old look"), Fixture.Rig->GetActorRotation().Equals(Authored));
    AActor* FollowTarget = Fixture.World->SpawnActor<AActor>();
    Fixture.Rig->StartFollow(FollowTarget, FVector(0, 0, 200), 70);
    TestFalse(TEXT("Cinematic follow retains camera ownership"), Journey.CanLookAround());
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(500, 500), 0, false, 1.0f);
    TestTrue(TEXT("Drag never competes with a cinematic follow"), Fixture.Rig->GetActorRotation().Equals(Authored));
    Fixture.Rig->StopFollow();

    FLFFJourneyRuntimeTestAccess::Settings(Journey).bCalmMode = true;
    Journey.FlyTick(FVector2D::ZeroVector, 0, FVector2D(100, 0), 0, false, 1.0f);
    TestTrue(TEXT("Calm Mode still permits deliberate look"), FMath::IsNearlyEqual(Fixture.Rig->GetActorRotation().Yaw, 73.0, 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyContrastRuntimeTest, "LFF.Journey.Runtime.HighContrastState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyContrastRuntimeTest::RunTest(const FString& Parameters)
{
    FRuntimeWorld Fixture;
    FLFFJourneyStateMachine& Machine = FLFFJourneyRuntimeTestAccess::Machine(*Fixture.Journey);
    Machine.Next();
    const uint32 Move = Machine.GetTransitionId();
    const FTransform Pose = Fixture.Rig->GetActorTransform();
    for (int32 Press = 0; Press < 10; ++Press)
    {
        Fixture.Journey->ToggleHighContrast();
        const FLFFJourneySnapshot Snapshot = Fixture.Journey->GetSnapshot();
        TestTrue(TEXT("Contrast is only a display flag"), Snapshot.Settings.bHighContrast == (Press % 2 == 0));
        TestTrue(TEXT("Tour stays in the same transition"), Snapshot.Phase == ELFFJourneyPhase::Transitioning);
        TestFalse(TEXT("Contrast does not open a modal"), Snapshot.bSettingsOpen || Snapshot.bRestartConfirmOpen);
        TestEqual(TEXT("Contrast does not invalidate travel"), Machine.GetTransitionId(), Move);
        TestTrue(TEXT("Contrast never changes camera pose"), Fixture.Rig->GetActorTransform().Equals(Pose));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyRoomAudioRuntimeTest, "LFF.Journey.Runtime.RoomAudio",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyRoomAudioRuntimeTest::RunTest(const FString& Parameters)
{
    FRuntimeWorld Fixture;
    AActor* Room = Fixture.World->SpawnActor<AActor>();
    Room->Tags.Add(TEXT("LFF_SensoryAudioZone"));
    UBoxComponent* Box = NewObject<UBoxComponent>(Room);
    Room->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(400, 600, 180));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Box->RegisterComponent();
    Room->SetActorLocationAndRotation(FVector(1000, 2000, 500), FRotator(0, -6.5f, 0));
    FLFFJourneyRuntimeTestAccess::AddQuietBox(*Fixture.Journey, Box);
    Fixture.Rig->SetActorLocation(Room->GetActorLocation());
    TestEqual(TEXT("Real room interior strongly attenuates crowd"), FLFFJourneyRuntimeTestAccess::RoomGain(*Fixture.Journey),
        Fixture.Mode->SensoryRoomCrowdGain);
    Fixture.Journey->SwitchToMode(ELFFSimulationMode::FreeExplore);
    TestEqual(TEXT("Room stays quiet in Explore, independent of the tour bookmark"), FLFFJourneyRuntimeTestAccess::RoomGain(*Fixture.Journey),
        Fixture.Mode->SensoryRoomCrowdGain);
    Fixture.Rig->SetActorLocation(Box->GetComponentTransform().TransformPosition(FVector(440, 0, 0)));
    const float EdgeGain = FLFFJourneyRuntimeTestAccess::RoomGain(*Fixture.Journey);
    TestTrue(TEXT("80cm boundary is a smooth return, not an abrupt jump"), EdgeGain > Fixture.Mode->SensoryRoomCrowdGain && EdgeGain < 1);
    Fixture.Rig->SetActorLocation(Box->GetComponentTransform().TransformPosition(FVector(500, 0, 0)));
    TestEqual(TEXT("Leaving the actual room restores crowd, despite the bookmark"), FLFFJourneyRuntimeTestAccess::RoomGain(*Fixture.Journey), 1.0f);
    Fixture.Rig->SetActorLocation(Box->GetComponentTransform().TransformPosition(FVector(0, 0, 500)));
    TestEqual(TEXT("Flying above a room is not being inside it"), FLFFJourneyRuntimeTestAccess::RoomGain(*Fixture.Journey), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyAnthemRuntimeTest, "LFF.Journey.Runtime.AnthemCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyAnthemRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Pathway = 0; Pathway < 9; ++Pathway)
    {
        FRuntimeWorld Fixture;
        ALFFCueActor* Eagle = Fixture.Eagle();
        FLFFJourneyRuntimeTestAccess::StartAnthem(*Fixture.Journey, Eagle);
        TestTrue(TEXT("Anthem audio waits for flight, not the cue delay"), FLFFJourneyRuntimeTestAccess::HasAnthemStart(*Fixture.Journey));
        switch (Pathway)
        {
        case 0: Fixture.Journey->TogglePause(); break;
        case 1: Fixture.Journey->ToggleSettings(); break;
        case 2: Fixture.Journey->Cancel(); break;
        case 3: Fixture.Journey->SwitchToMode(ELFFSimulationMode::FreeExplore); break;
        case 4: Fixture.Journey->ToggleCalmMode(); break;
        case 5: Fixture.Journey->ToggleReducedMotion(); break;
        case 6: Fixture.Journey->RequestRestart(); break;
        case 7: FLFFJourneyRuntimeTestAccess::LeaveAnthemStep(*Fixture.Journey); break;
        case 8:
            Eagle->ApplyState(ELFFCueState::After);
            FLFFJourneyRuntimeTestAccess::CheckAnthemFlight(*Fixture.Journey);
            break;
        }
        TestFalse(FString::Printf(TEXT("Path %d stops anthem audio"), Pathway), FLFFJourneyRuntimeTestAccess::HasAnthem(*Fixture.Journey));
        TestFalse(FString::Printf(TEXT("Path %d cannot leave a delayed anthem restart"), Pathway),
            FLFFJourneyRuntimeTestAccess::HasAnthemAudioTimers(*Fixture.Journey));
        TestFalse(FString::Printf(TEXT("Path %d ends eagle motion"), Pathway), Eagle->IsPlaying());
    }

    FRuntimeWorld Fixture;
    ALFFCueActor* Eagle = Fixture.Eagle();
    FLFFJourneyRuntimeTestAccess::StartAnthem(*Fixture.Journey, Eagle);
    FLFFJourneyRuntimeTestAccess::SeedAudio(*Fixture.Journey);
    Fixture.Journey->ToggleSound();
    TestFalse(TEXT("Sound off clears current, outgoing, voice, preview, anthem and UI audio"),
        FLFFJourneyRuntimeTestAccess::HasAnyAudio(*Fixture.Journey));
    TestFalse(TEXT("Sound off cancels anthem audio timers"), FLFFJourneyRuntimeTestAccess::HasAnthemAudioTimers(*Fixture.Journey));
    TestTrue(TEXT("Sound off preserves the visual flight"), Eagle->IsPlaying());
    Fixture.Journey->PlayUIClick();
    TestFalse(TEXT("Sound off cannot create button audio"), FLFFJourneyRuntimeTestAccess::HasClick(*Fixture.Journey));
    Fixture.Journey->ToggleSound();
    TestFalse(TEXT("Unmuting cannot restart an already cancelled anthem"), FLFFJourneyRuntimeTestAccess::HasAnthem(*Fixture.Journey));
    Fixture.Journey->ToggleCalmMode();
    Fixture.Journey->PlayUIClick();
    TestFalse(TEXT("Calm mode cannot create button audio"), FLFFJourneyRuntimeTestAccess::HasClick(*Fixture.Journey));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFJourneyAudioDefaultsTest, "LFF.Journey.Runtime.AudioDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFJourneyAudioDefaultsTest::RunTest(const FString& Parameters)
{
    const ALFFGameMode* Mode = GetDefault<ALFFGameMode>();
    TestEqual(TEXT("Normal crowd recording configured"), Mode->AmbientBed.ToString(),
        FString(TEXT("/Game/LFF/Audio/SW_LFF_AmbientCrowd1.SW_LFF_AmbientCrowd1")));
    TestEqual(TEXT("Game crowd recording configured"), Mode->StadiumAmbientBed.ToString(),
        FString(TEXT("/Game/LFF/Audio/SW_LFF_AmbientCrowd2.SW_LFF_AmbientCrowd2")));
    TestEqual(TEXT("Supplied anthem configured"), Mode->AnthemSound.ToString(),
        FString(TEXT("/Game/LFF/Audio/SW_LFF_AnthemLondon.SW_LFF_AnthemLondon")));
    TestEqual(TEXT("Subtle button recording configured"), Mode->UIClickSound.ToString(),
        FString(TEXT("/Game/LFF/Audio/SW_LFF_UIClick.SW_LFF_UIClick")));
    TestTrue(TEXT("Normalized narrator remains below unity"), Mode->VoiceVolume > 0.75f && Mode->VoiceVolume <= 1.0f);
    TestTrue(TEXT("Low-peak UI click is not made inaudible by excessive runtime attenuation"), Mode->UIClickVolume >= 0.9f);
    const float SourcePeak = LFFJourneyRules::DecibelsToGain(-1.5f);
    const float Duck = LFFJourneyRules::DecibelsToGain(Mode->VoiceDuckDecibels);
    TestTrue(TEXT("Anthem has prominent foreground gain"), Mode->AnthemVolume >= 0.8f);
    const float VoiceMixPeak = SourcePeak * (Mode->VoiceVolume * 0.7f + Mode->BedVolume * 0.2f * Duck
        + Mode->AnthemVolume * 0.55f) + 0.037f * Mode->UIClickVolume;
    const float FlightMixPeak = SourcePeak * (Mode->BedVolume * 0.2f + Mode->AnthemVolume) + 0.037f * Mode->UIClickVolume;
    TestTrue(TEXT("Normalized voice plus ducked crowd/anthem/click has headroom"), VoiceMixPeak < 1.0f);
    TestTrue(TEXT("Normalized crowd plus anthem/click has headroom"), FlightMixPeak < 1.0f);
    TestEqual(TEXT("Quiet blend is normal outside the zone"), LFFJourneyRules::QuietZoneGain(100, 80, 0.035f), 1.0f);
    TestEqual(TEXT("Quiet blend is fully attenuated inside"), LFFJourneyRules::QuietZoneGain(0, 80, 0.035f), 0.035f);
    return true;
}

#endif
