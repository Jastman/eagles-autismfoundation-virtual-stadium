#include "World/LFFCrowdFanActor.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FLFFCrowdSpacingTestAccess
{
    static const TArray<FVector>& Loop(const ALFFCrowdFanActor& Fan) { return Fan.PassingLoop; }
    static bool Waiting(const ALFFCrowdFanActor& Fan) { return Fan.bWaitingForSpace; }
    static bool Shares(const ALFFCrowdFanActor& Fan, const ALFFPersonActor& Other) { return Fan.SharesPersonRoute(Other); }
};

namespace
{
    struct FMovementWorld
    {
        UWorld* World;
        UAnimSequence* Idle;
        UAnimSequence* Walk;

        FMovementWorld()
        {
            const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
                .ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Stand_Idle_Loop"));
            Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Walk_Loop_F"));
        }

        ~FMovementWorld() { World->DestroyWorld(false); }

        AActor* Box(const FVector& Center, const FVector& Extent)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Shape);
            Shape->SetMobility(EComponentMobility::Static);
            Shape->SetBoxExtent(Extent);
            Shape->SetCollisionObjectType(ECC_WorldStatic);
            Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Shape->SetCollisionResponseToAllChannels(ECR_Block);
            Shape->SetWorldLocation(Center);
            Shape->RegisterComponent();
            return Actor;
        }

        ALFFCrowdFanActor* Fan(const TArray<FVector>& Route, const float Phase, const float Speed)
        {
            ALFFCrowdFanActor* Actor = World->SpawnActor<ALFFCrowdFanActor>();
            Actor->WalkRoute = Route;
            Actor->RoutePhase = Phase;
            Actor->WalkSpeed = Speed;
            Actor->StillAnimation = Idle;
            Actor->MovingAnimation = Walk;
            Actor->DispatchBeginPlay();
            Actor->SetCrowdActive(true, true);
            return Actor;
        }

        ALFFPersonActor* Person(const FVector& End, const FVector& Offset)
        {
            ALFFPersonActor* Actor = World->SpawnActor<ALFFPersonActor>();
            Actor->CueId = TEXT("ConcourseWalk");
            Actor->Tags.AddUnique(TEXT("LFF_Crowd"));
            Actor->SetActorLocation(End);
            Actor->StartOffset = Offset;
            Actor->StillAnimation = Idle;
            Actor->MovingAnimation = Walk;
            Actor->bLinear = true;
            Actor->PlaySeconds = 30.0f;
            Actor->PlayDelaySeconds = 1.0f;
            Actor->DispatchBeginPlay();
            Actor->ApplyState(ELFFCueState::Before);
            return Actor;
        }
    };

    double DistanceToPolyline(const FVector& Location, const TArray<FVector>& Points)
    {
        double Result = TNumericLimits<double>::Max();
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            Result = FMath::Min(Result, FVector::Distance(Location,
                FMath::ClosestPointOnSegment(Location, Points[Index - 1], Points[Index])));
        }
        return Result;
    }

    bool CheckPeople(FAutomationTestBase& Test, const TArray<ALFFPersonActor*>& People, double& Minimum)
    {
        for (int32 A = 0; A < People.Num(); ++A)
        {
            for (int32 B = A + 1; B < People.Num(); ++B)
            {
                if (!People[A]->IsHidden() && !People[B]->IsHidden())
                {
                    const double Distance = FVector::Distance(People[A]->GetActorLocation(), People[B]->GetActorLocation());
                    Minimum = FMath::Min(Minimum, Distance);
                    if (Distance < LFFCirculationRules::MinimumSeparation - 0.01)
                    {
                        Test.AddError(FString::Printf(TEXT("Visible people intersected during movement: %s / %s, %.3f cm"),
                            *People[A]->GetName(), *People[B]->GetName(), Distance));
                        return false;
                    }
                }
            }
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFPassingGeometryTest, "LFF.Crowd.Movement.PassingGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFPassingGeometryTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> Route = { FVector(0, 0, 0), FVector(300, 0, 15), FVector(300, 400, 35) };
    TArray<FVector> Loop;
    if (!TestTrue(TEXT("Detailed graded route gets a bounded passing circuit"), LFFCirculationRules::BuildPassingLoop(Route, Loop)))
    {
        return false;
    }
    TestTrue(TEXT("Lane change closes the circuit without an endpoint teleport"), Loop[0].Equals(Loop.Last()));
    TestEqual(TEXT("Both lanes retain every authored corner"), Loop.Num(), Route.Num() * 2 + 1);
    TestEqual(TEXT("Outbound feet keep the measured corner elevation"), Loop[1].Z, Route[1].Z);
    TestEqual(TEXT("Return feet keep the measured corner elevation"), Loop[4].Z, Route[1].Z);
    TestTrue(TEXT("+X travel keeps right, +Y"), Loop[0].Equals(FVector(0, 75, 0)));
    TestTrue(TEXT("-X travel keeps right, -Y"), Loop[5].Equals(FVector(0, -75, 0)));
    const double Length = LFFCirculationRules::RouteLength(Loop);
    // Examine the WHOLE circuit and wrap, not only start positions.
    for (double Distance = 0; Distance < Length * 3; Distance += 2.0)
    {
        const auto Pose = LFFCirculationRules::SampleLoop(Loop, Distance);
        if (!Pose.bValid || DistanceToPolyline(Pose.Location, Loop) > 0.001)
        {
            AddError(TEXT("Passing sampler left its detailed closed route"));
            return false;
        }
    }
    TestTrue(TEXT("Loop seam is continuous"), FVector::Distance(
        LFFCirculationRules::SampleLoop(Loop, Length - 0.001).Location,
        LFFCirculationRules::SampleLoop(Loop, Length + 0.001).Location) < 0.003);
    TestFalse(TEXT("Hairpins do not invent unsafe oversized lanes"), LFFCirculationRules::BuildPassingLoop(
        { FVector::ZeroVector, FVector(300, 0, 0), FVector::ZeroVector }, Loop));
    TestFalse(TEXT("Vertical-only routes do not invent horizontal navigation"), LFFCirculationRules::BuildPassingLoop(
        { FVector::ZeroVector, FVector(0, 0, 300) }, Loop));
    TestFalse(TEXT("Swept reservation prevents tunnelling through a person with a clear destination"),
        LFFCirculationRules::HasSpace(FVector(-300, 0, 0), FVector(300, 0, 0), { FVector::ZeroVector }, false));
    TestFalse(TEXT("Placement rejects an overlapping start"),
        LFFCirculationRules::HasSpace(FVector::ZeroVector, FVector(30, 0, 0), { FVector::ZeroVector }, true));
    TestTrue(TEXT("Other levels do not block a route"),
        LFFCirculationRules::HasSpace(FVector(-300, 0, 0), FVector(300, 0, 0), { FVector(0, 0, 300) }, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdFullRouteSpacingTest, "LFF.Crowd.Movement.FullRouteSpacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdFullRouteSpacingTest::RunTest(const FString& Parameters)
{
    FMovementWorld Fixture;
    if (!TestNotNull(TEXT("Installed walk animation"), Fixture.Walk) || !TestNotNull(TEXT("Installed idle animation"), Fixture.Idle))
    {
        return false;
    }
    Fixture.Box(FVector(0, 0, -10), FVector(4000, 4000, 10));
    const TArray<FVector> Route = { FVector::ZeroVector, FVector(1000, 0, 0), FVector(1000, 700, 0) };
    TArray<ALFFCrowdFanActor*> Fans = { Fixture.Fan(Route, 0.11f, 55.0f),
        Fixture.Fan(Route, 0.89f, 105.0f), Fixture.Fan(Route, 0.37f, 155.0f) };
    // One-way legacy cue crosses BOTH ambient lanes, at a different speed and after a delay.
    ALFFPersonActor* Legacy = Fixture.Person(FVector(500, 500, 0), FVector(0, -1000, 0));
    Legacy->Play(false);
    const TArray<ALFFPersonActor*> People = { Fans[0], Fans[1], Fans[2], Legacy };
    for (const ALFFPersonActor* Person : People)
    {
        AddInfo(FString::Printf(TEXT("Initial %s id=%u location=%s"), *Person->GetName(), Person->GetUniqueID(),
            *Person->GetActorLocation().ToString()));
    }
    AddInfo(FString::Printf(TEXT("Same-route reservations: %d / %d"),
        FLFFCrowdSpacingTestAccess::Shares(*Fans[0], *Fans[1]),
        FLFFCrowdSpacingTestAccess::Shares(*Fans[0], *Fans[2])));
    for (const ALFFCrowdFanActor* Fan : Fans)
    {
        if (!TestTrue(TEXT("Safe floor enables measured right-hand lanes"), !FLFFCrowdSpacingTestAccess::Loop(*Fan).IsEmpty()))
        {
            return false;
        }
    }
    double Minimum = TNumericLimits<double>::Max();
    double Travel[3] = {};
    double MinX[3] = { 1e9, 1e9, 1e9 };
    double MaxX[3] = {};
    double MaxY[3] = {};
    bool bSawWait = false;
    if (!CheckPeople(*this, People, Minimum))
    {
        return false;
    }
    // Four minutes covers several COMPLETE trips even at the slowest speed. Check after EVERY actor advance.
    // Alternate tick order; freeze/resume a visible walker, hide/reveal another, and exercise a frame hitch.
    for (int32 Frame = 0; Frame < 2400; ++Frame)
    {
        if (Frame == 500) { Fans[0]->SetCrowdActive(true, false); }
        if (Frame == 550) { Fans[0]->SetCrowdActive(true, true); }
        if (Frame == 800) { Fans[1]->SetCrowdActive(false, true); }
        if (Frame == 850) { Fans[1]->SetCrowdActive(true, true); }
        Legacy->Tick(Frame == 200 ? 3.5f : 0.1f);
        if (!CheckPeople(*this, People, Minimum)) { return false; }
        for (int32 Order = 0; Order < Fans.Num(); ++Order)
        {
            const int32 Index = Frame % 2 == 0 ? Order : Fans.Num() - 1 - Order;
            ALFFCrowdFanActor* Fan = Fans[Index];
            const FVector Before = Fan->GetActorLocation();
            Fan->Tick(Frame == 200 ? 3.5f : 0.1f);
            const FVector After = Fan->GetActorLocation();
            if (Frame == 525 && Index == 0)
            {
                TestTrue(TEXT("Reduced motion freezes the route pose"), Before.Equals(After));
                TestFalse(TEXT("Reduced motion disables route tick"), Fan->IsActorTickEnabled());
            }
            if (!Fan->IsHidden())
            {
                // Ignore the intentional hidden-to-visible slot placement when accumulating movement.
                if (Frame != 850) { Travel[Index] += FVector::Distance(Before, After); }
                MinX[Index] = FMath::Min(MinX[Index], After.X);
                MaxX[Index] = FMath::Max(MaxX[Index], After.X);
                MaxY[Index] = FMath::Max(MaxY[Index], After.Y);
                if (DistanceToPolyline(After, FLFFCrowdSpacingTestAccess::Loop(*Fan)) > 0.01)
                {
                    AddError(TEXT("Walker drifted off its floor-measured detailed route"));
                    return false;
                }
                if (FMath::Abs(After.Z) >= 0.01)
                {
                    AddError(TEXT("Measured feet left the floor"));
                    return false;
                }
            }
            bSawWait |= FLFFCrowdSpacingTestAccess::Waiting(*Fan);
            if (!CheckPeople(*this, People, Minimum)) { return false; }
        }
    }
    TestTrue(TEXT("Spacing was measured through full trips, including lane changes and a legacy crossing"),
        Minimum >= LFFCirculationRules::MinimumSeparation - 0.01);
    AddInfo(FString::Printf(TEXT("Full-route minimum spacing %.3f cm; legacy ended at %s (playing=%d)"),
        Minimum, *Legacy->GetActorLocation().ToString(), Legacy->IsPlaying()));
    TestTrue(TEXT("Different speeds actually exercised yielding, not only initial spacing"), bSawWait);
    TestFalse(TEXT("A blocked legacy cue retains time and eventually finishes when space opens"), Legacy->IsPlaying());
    for (int32 Index = 0; Index < Fans.Num(); ++Index)
    {
        AddInfo(FString::Printf(TEXT("Walker %d: travel=%.3f cm, circuits=%.3f, x=%.3f..%.3f, maxY=%.3f, final=%s"),
            Index, Travel[Index], Travel[Index] / LFFCirculationRules::RouteLength(FLFFCrowdSpacingTestAccess::Loop(*Fans[Index])),
            MinX[Index], MaxX[Index], MaxY[Index], *Fans[Index]->GetActorLocation().ToString()));
        TestTrue(TEXT("Each walker completes multiple circuits, rather than deadlocking or standing still"),
            Travel[Index] > LFFCirculationRules::RouteLength(FLFFCrowdSpacingTestAccess::Loop(*Fans[Index])) * 2.0);
        TestTrue(TEXT("Each walker visits both ends and the far detailed corner"),
            MinX[Index] < 1.0 && MaxX[Index] > 1074.0 && MaxY[Index] > 699.0);
        TestTrue(TEXT("Authored route detail is untouched"), Fans[Index]->WalkRoute == Route);
        Fans[Index]->ForEachComponent<USkeletalMeshComponent>(false, [this, &Fixture](USkeletalMeshComponent* Component)
        {
            if (Component->GetFName() == TEXT("Pose") && Component->GetSingleNodeInstance() != nullptr)
            {
                TestTrue(TEXT("Full-route movement retains the authored walking clip"),
                    Component->GetSingleNodeInstance()->GetAnimationAsset() == Fixture.Walk);
            }
        });
        TInlineComponentArray<UPrimitiveComponent*> Components(Fans[Index]);
        for (const UPrimitiveComponent* Component : Components)
        {
            TestEqual(TEXT("People still cannot push/block the tour camera"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFLegacyPersonSpacingTest, "LFF.Crowd.Movement.LegacySpacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFLegacyPersonSpacingTest::RunTest(const FString& Parameters)
{
    FMovementWorld Fixture;
    if (!TestNotNull(TEXT("Installed legacy walk animation"), Fixture.Walk)) { return false; }
    ALFFPersonActor* Leader = Fixture.Person(FVector(1200, 0, 0), FVector(-1200, 0, 0));
    ALFFPersonActor* Follower = Fixture.Person(FVector(1600, 0, 0), FVector(-1800, 0, 0));
    TestEqual(TEXT("Legacy fixtures exercise the actual shared ConcourseWalk cue identity"), Leader->CueId, FName(TEXT("ConcourseWalk")));
    TestEqual(TEXT("Both concourse people share that cue identity"), Follower->CueId, Leader->CueId);
    Leader->PlaySeconds = 20.0f;
    Follower->PlaySeconds = 10.0f;
    Leader->Play(false);
    Follower->Play(false);
    double Minimum = TNumericLimits<double>::Max();
    const TArray<ALFFPersonActor*> People = { Leader, Follower };
    bool bSawWaiting = false;
    for (int32 Frame = 0; Frame < 700; ++Frame)
    {
        if (Frame == 290)
        {
            TestTrue(TEXT("Follower remains playing while a standing endpoint blocks its route"), Follower->IsPlaying());
            TestFalse(TEXT("Leader has finished its one-way route"), Leader->IsPlaying());
        }
        if (Frame == 300) { Leader->SetActorHiddenInGame(true); }
        Leader->Tick(0.1f);
        if (!CheckPeople(*this, People, Minimum)) { return false; }
        const FVector Before = Follower->GetActorLocation();
        Follower->Tick(Frame == 80 ? 2.0f : 0.1f);
        if (Frame > 20 && Follower->IsPlaying() && Before.Equals(Follower->GetActorLocation()))
        {
            bSawWaiting = true;
            Follower->ForEachComponent<USkeletalMeshComponent>(false, [this](USkeletalMeshComponent* Component)
            {
                if (Component->GetFName() == TEXT("Pose") && Component->GetSingleNodeInstance() != nullptr)
                {
                    TestFalse(TEXT("Waiting does not animate walking in place"), Component->GetSingleNodeInstance()->IsPlaying());
                }
            });
        }
        if (!CheckPeople(*this, People, Minimum)) { return false; }
        if (FMath::Abs(Follower->GetActorLocation().Y) > 0.01 || FMath::Abs(Follower->GetActorLocation().Z) > 0.01)
        {
            AddError(TEXT("Legacy person left the authored one-way route"));
            return false;
        }
    }
    TestTrue(TEXT("Faster legacy follower exercised spacing over time"), bSawWaiting);
    TestFalse(TEXT("A blocked cue eventually completes after the endpoint clears"), Follower->IsPlaying());
    TestTrue(TEXT("Cue reaches its exact authored end pose"), Follower->GetActorLocation().Equals(FVector(1600, 0, 0), 0.01));

    // Instant/reduced-motion placement must also reserve space; it must not expose two people at the same end.
    ALFFPersonActor* Instant = Fixture.Person(FVector(1600, 0, 0), FVector(-1800, 0, 0));
    Instant->Play(true);
    const TArray<ALFFPersonActor*> WithInstant = { Follower, Instant };
    TestFalse(TEXT("Instant cue is stationary"), Instant->IsPlaying());
    TestFalse(TEXT("A nearby free route slot remains visible"), Instant->IsHidden());
    TestTrue(TEXT("Instant placement remains on the measured line"),
        FMath::Abs(Instant->GetActorLocation().Y) < 0.01 && FMath::Abs(Instant->GetActorLocation().Z) < 0.01);
    return CheckPeople(*this, WithInstant, Minimum);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdUnsafeLaneTest, "LFF.Crowd.Movement.UnsafeLaneFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdUnsafeLaneTest::RunTest(const FString& Parameters)
{
    FMovementWorld Fixture;
    if (!TestNotNull(TEXT("Installed walk animation"), Fixture.Walk)) { return false; }
    // A narrow measured strip is NOT permission to move feet onto missing ground beside it.
    Fixture.Box(FVector(500, 0, -10), FVector(600, 30, 10));
    // A roof far above the measured feet must not be mistaken for walkable ground.
    Fixture.Box(FVector(500, 0, 400), FVector(600, 400, 10));
    const TArray<FVector> Route = { FVector::ZeroVector, FVector(500, 0, 0), FVector(1000, 0, 0) };
    ALFFCrowdFanActor* A = Fixture.Fan(Route, 0.1f, 160.0f);
    ALFFCrowdFanActor* B = Fixture.Fan(Route, 0.6f, 80.0f);
    TestTrue(TEXT("Unmeasured lateral ground rejects lane navigation"), FLFFCrowdSpacingTestAccess::Loop(*A).IsEmpty());
    double Minimum = TNumericLimits<double>::Max();
    const TArray<ALFFPersonActor*> People = { A, B };
    for (int32 Frame = 0; Frame < 1200; ++Frame)
    {
        A->Tick(0.1f);
        if (!CheckPeople(*this, People, Minimum)) { return false; }
        B->Tick(0.1f);
        if (!CheckPeople(*this, People, Minimum)) { return false; }
        if (DistanceToPolyline(A->GetActorLocation(), Route) > 0.01
            || DistanceToPolyline(B->GetActorLocation(), Route) > 0.01)
        {
            AddError(TEXT("Fallback left the measured route or climbed a roof"));
            return false;
        }
    }
    TestTrue(TEXT("Single-width head-on walkers wait rather than intersect"),
        FLFFCrowdSpacingTestAccess::Waiting(*A) && FLFFCrowdSpacingTestAccess::Waiting(*B));

    FMovementWorld MirroredFixture;
    ALFFCrowdFanActor* MirroredA = MirroredFixture.Fan(Route, 0.25f, 85.0f);
    ALFFCrowdFanActor* MirroredB = MirroredFixture.Fan(Route, 0.75f, 85.0f);
    // These phases have the SAME initial position on the original ping-pong route.
    TestFalse(TEXT("Mirrored-phase visitors can both remain visible"), MirroredA->IsHidden() || MirroredB->IsHidden());
    TestTrue(TEXT("Runtime placement repairs coincident mirrored phases"),
        FVector::Distance(MirroredA->GetActorLocation(), MirroredB->GetActorLocation()) >= LFFCirculationRules::MinimumSeparation - 0.01);

    // Even with a wide floor, a low wall/rail in a proposed lane must reject it.
    FMovementWorld WallFixture;
    WallFixture.Box(FVector(500, 0, -10), FVector(700, 500, 10));
    // Bottom is above the short floor probe, so only the body-clearance sweep can reject this rail.
    WallFixture.Box(FVector(500, 75, 45), FVector(20, 20, 15));
    ALFFCrowdFanActor* WallFan = WallFixture.Fan(Route, 0.0f, 85.0f);
    TestTrue(TEXT("Capsule clearance catches a low barrier, not only head-height walls"),
        FLFFCrowdSpacingTestAccess::Loop(*WallFan).IsEmpty());
    return true;
}

#endif
