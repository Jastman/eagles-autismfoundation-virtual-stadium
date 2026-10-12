#include "World/LFFCrowdFanActor.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkinnedMeshComponent.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/PlayerController.h"
#include "Journey/LFFJourneySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VirtualStadium.h"

double LFFCirculationRules::RouteLength(const TArray<FVector>& Points)
{
    double Length = 0.0;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        if (Points[Index].ContainsNaN())
        {
            return 0.0;
        }
        if (Index > 0)
        {
            Length += FVector::Distance(Points[Index - 1], Points[Index]);
        }
    }
    return FMath::IsFinite(Length) ? Length : 0.0;
}

LFFCirculationRules::FRoutePose LFFCirculationRules::SampleRoute(const TArray<FVector>& Points, const double Distance)
{
    FRoutePose Result;
    const double Length = RouteLength(Points);
    if (Length <= UE_SMALL_NUMBER || !FMath::IsFinite(Distance))
    {
        return Result;
    }
    double Phase = FMath::Fmod(Distance, Length * 2.0);
    if (Phase < 0.0)
    {
        Phase += Length * 2.0;
    }
    const bool bReturning = Phase >= Length;
    double Remaining = bReturning ? Length * 2.0 - Phase : Phase;
    for (int32 Index = 1; Index < Points.Num(); ++Index)
    {
        const FVector Segment = Points[Index] - Points[Index - 1];
        const double SegmentLength = Segment.Size();
        if (SegmentLength <= UE_SMALL_NUMBER)
        {
            continue;
        }
        if (Remaining <= SegmentLength)
        {
            Result.Location = FMath::Lerp(Points[Index - 1], Points[Index], Remaining / SegmentLength);
            Result.Direction = Segment / SegmentLength * (bReturning ? -1.0 : 1.0);
            Result.bValid = true;
            return Result;
        }
        Remaining -= SegmentLength;
    }
    return Result;
}

LFFCirculationRules::FRoutePose LFFCirculationRules::SampleLoop(const TArray<FVector>& Points, const double Distance)
{
    const double Length = RouteLength(Points);
    if (Length <= UE_SMALL_NUMBER || !FMath::IsFinite(Distance) || !Points[0].Equals(Points.Last()))
    {
        return {};
    }
    double Phase = FMath::Fmod(Distance, Length);
    if (Phase < 0.0)
    {
        Phase += Length;
    }
    // SampleRoute's outbound half is also an exact polyline sampler.
    return SampleRoute(Points, Phase);
}

bool LFFCirculationRules::BuildPassingLoop(const TArray<FVector>& Points, TArray<FVector>& OutLoop)
{
    OutLoop.Reset();
    if (RouteLength(Points) <= UE_SMALL_NUMBER)
    {
        return false;
    }
    TArray<FVector> Centers;
    for (const FVector& Point : Points)
    {
        if (Centers.IsEmpty() || !Point.Equals(Centers.Last()))
        {
            Centers.Add(Point);
        }
    }
    TArray<FVector> Right;
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        const FVector Before = (Centers[Index] - Centers[FMath::Max(0, Index - 1)]).GetSafeNormal2D();
        const FVector After = (Centers[FMath::Min(Index + 1, Centers.Num() - 1)] - Centers[Index]).GetSafeNormal2D();
        const FVector Tangent = (Before + After).GetSafeNormal2D();
        const FVector Normal = FVector::CrossProduct(FVector::UpVector, Tangent);
        const FVector SegmentNormal = FVector::CrossProduct(FVector::UpVector, Index == 0 ? After : Before);
        const double Dot = FVector::DotProduct(Normal, SegmentNormal);
        // No guessed lane around hairpins/vertical paths or huge miters near a wall.
        if (Dot < 0.5)
        {
            OutLoop.Reset();
            return false;
        }
        Right.Add(Normal * (LaneOffset / Dot));
        OutLoop.Add(Centers[Index] + Right.Last());
    }
    for (int32 Index = Centers.Num() - 1; Index >= 0; --Index)
    {
        OutLoop.Add(Centers[Index] - Right[Index]);
    }
    // TArray rejects Add with a reference into itself, even if spare capacity is available.
    const FVector LoopStart = OutLoop[0];
    OutLoop.Add(LoopStart);
    return true;
}

bool LFFCirculationRules::HasSpace(const FVector& From, const FVector& To, const TArray<FVector>& People, const bool bPlacement)
{
    if (From.ContainsNaN() || To.ContainsNaN())
    {
        return false;
    }
    const FVector Travel = To - From;
    const double LengthSquared = Travel.SizeSquared();
    const double RequiredSquared = FMath::Square(MinimumSeparation);
    for (const FVector& Person : People)
    {
        const double Along = bPlacement || LengthSquared <= UE_SMALL_NUMBER ? 1.0
            : FMath::Clamp(FVector::DotProduct(Person - From, Travel) / LengthSquared, 0.0, 1.0);
        if (FVector::DistSquared(From + Travel * Along, Person) < RequiredSquared - UE_SMALL_NUMBER)
        {
            return false;
        }
    }
    return true;
}

bool LFFCirculationRules::ShouldAnimate(const bool bVisible, const bool bMotionEnabled, const bool bInRange)
{
    return bVisible && bMotionEnabled && bInRange;
}

void ALFFCrowdFanActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(TEXT("LFF_Crowd"));
    Variation.Initialize(VariationSeed);
    IdleRate = Variation.FRandRange(0.85f, 1.15f);
    TInlineComponentArray<USkinnedMeshComponent*> Meshes(this);
    for (USkinnedMeshComponent* Mesh : Meshes)
    {
        const USkinnedAsset* Asset = Mesh->GetSkinnedAsset();
        // Fitted necklines, cuffs and hair cards retain near detail; screen-size LOD still reduces them at distance.
        const bool bFitSensitive = Mesh->GetFName() == TEXT("Face") || Mesh->ComponentHasTag(TEXT("LFF_Hair"))
            || Mesh->IsA<UChaosClothComponent>();
        Mesh->OverrideMinLOD(bFitSensitive ? 0
            : Asset != nullptr ? FMath::Min(2, FMath::Max(0, Asset->GetLODNum() - 1)) : 0);
        Mesh->SetCastShadow(false);
    }
    RouteAnchor = RootComponent->GetRelativeTransform();
    const double Length = LFFCirculationRules::RouteLength(WalkRoute);
    bWalking = Length > UE_SMALL_NUMBER && MovingAnimation != nullptr && MovingAnimation->GetPlayLength() > 0.0f;
    if (WalkRoute.Num() > 0 && !bWalking)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd fan %s: invalid walking route or missing moving animation; visitor will wait."), *GetName());
    }
    if (bWalking && !MeasurePassingLoop())
    {
        UE_LOG(LogVirtualStadium, Display, TEXT("Crowd fan %s: passing lanes lack measured floor/clearance; using the authored route with stop-and-wait spacing."), *GetName());
    }
    const double Cycle = PassingLoop.IsEmpty() ? Length * 2.0 : LFFCirculationRules::RouteLength(PassingLoop);
    WalkDistance = Cycle * FMath::Clamp(RoutePhase, 0.0f, 1.0f);
    bRoutePlacementBlocked = bWalking && !PlaceWalker();
    if (const ULFFJourneySubsystem* Journey = GetWorld()->GetSubsystem<ULFFJourneySubsystem>())
    {
        const FLFFVisitorSettings& Settings = Journey->GetSnapshot().Settings;
        SetCrowdActive(Settings.IsCrowdShown(), !Settings.IsReducedMotionEffective());
    }
    else
    {
        SetCrowdActive(true, true);
    }
    // A visible frozen visitor still needs one evaluated pose, not a T-pose.
    if (!bPoseRunning)
    {
        PlayIdle();
        UpdateActivity();
    }
}

void ALFFCrowdFanActor::Tick(const float DeltaSeconds)
{
    // CueActor disables its own tick when no one-way cue is playing. Ambient walkers have an independent lifecycle.
    AActor::Tick(DeltaSeconds);
    if (bWalking && bPoseRunning)
    {
        const double Candidate = WalkDistance + FMath::Clamp(DeltaSeconds, 0.0f, 0.1f) * FMath::Clamp(WalkSpeed, 20.0f, 160.0f);
        bWaitingForSpace = !ApplyWalkPose(Candidate);
        if (!bWaitingForSpace)
        {
            const double Cycle = PassingLoop.IsEmpty() ? LFFCirculationRules::RouteLength(WalkRoute) * 2.0
                : LFFCirculationRules::RouteLength(PassingLoop);
            WalkDistance = FMath::Fmod(Candidate, Cycle);
        }
        if (UAnimSingleNodeInstance* Instance = GetAnimatedMesh()->GetSingleNodeInstance())
        {
            Instance->SetPlaying(!bWaitingForSpace);
        }
    }
}

bool ALFFCrowdFanActor::ApplyWalkPose(const double Distance, const bool bPlacement)
{
    if (!bWalking)
    {
        return false;
    }
    const auto PoseAtDistance = PassingLoop.IsEmpty() ? LFFCirculationRules::SampleRoute(WalkRoute, Distance)
        : LFFCirculationRules::SampleLoop(PassingLoop, Distance);
    if (PoseAtDistance.bValid)
    {
        return TrySetPersonPose(RouteAnchor.TransformPosition(PoseAtDistance.Location),
            RouteAnchor.GetRotation() * PoseAtDistance.Direction.Rotation().Quaternion(), bPlacement);
    }
    return false;
}

bool ALFFCrowdFanActor::PlaceWalker()
{
    const double Cycle = PassingLoop.IsEmpty() ? LFFCirculationRules::RouteLength(WalkRoute) * 2.0
        : LFFCirculationRules::RouteLength(PassingLoop);
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(Cycle / 25.0), 1, 4096);
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const double Candidate = WalkDistance + Cycle * Step / Steps;
        if (ApplyWalkPose(Candidate, true))
        {
            WalkDistance = FMath::Fmod(Candidate, Cycle);
            bWaitingForSpace = false;
            return true;
        }
    }
    return false;
}

bool ALFFCrowdFanActor::SharesPersonRoute(const ALFFPersonActor& Other) const
{
    const ALFFCrowdFanActor* Fan = Cast<ALFFCrowdFanActor>(&Other);
    // A procession negotiating its own end turn is NOT crossing traffic. Otherwise a higher-priority
    // follower can reserve the leader's turn, stopping both permanently at the end of the route.
    return Fan != nullptr && WalkRoute == Fan->WalkRoute
        && (RouteAnchor * PersonParentTransform()).Equals(Fan->RouteAnchor * Fan->PersonParentTransform());
}

void ALFFCrowdFanActor::PredictPersonRoute(TArray<FVector>& OutWorldPoints) const
{
    OutWorldPoints.Reset();
    if (!bWalking || !bPoseRunning || IsHidden())
    {
        return;
    }
    OutWorldPoints.Add(GetActorLocation());
    const FTransform Frame = RouteAnchor * PersonParentTransform();
    for (int32 Sample = 1; Sample <= 10; ++Sample)
    {
        const double Distance = WalkDistance + Sample * 0.25 * FMath::Clamp(WalkSpeed, 20.0f, 160.0f);
        const auto RoutePose = PassingLoop.IsEmpty() ? LFFCirculationRules::SampleRoute(WalkRoute, Distance)
            : LFFCirculationRules::SampleLoop(PassingLoop, Distance);
        if (RoutePose.bValid)
        {
            OutWorldPoints.Add(Frame.TransformPosition(RoutePose.Location));
        }
    }
}

bool ALFFCrowdFanActor::MeasurePassingLoop()
{
    PassingLoop.Reset();
    TArray<FVector> Proposed;
    if (!LFFCirculationRules::BuildPassingLoop(WalkRoute, Proposed))
    {
        return false;
    }
    const FTransform Frame = RouteAnchor * PersonParentTransform();
    const FVector Up = Frame.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
    const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LFFCrowdPassingFloor), true, this);
    // Short probes at the measured feet, not a roof-to-ground trace. A missing/ambiguous floor rejects the lane.
    const auto FloorAt = [&](const FVector& Expected, FHitResult& Hit)
    {
        return GetWorld()->LineTraceSingleByObjectType(Hit, Expected + Up * 25.0, Expected - Up * 40.0, Objects, Query)
            && FVector::DotProduct(Hit.ImpactNormal, Up) >= 0.8;
    };
    FHitResult CenterFloor;
    const FVector Center = Frame.TransformPosition(WalkRoute[0]);
    if (!FloorAt(Center, CenterFloor))
    {
        return false;
    }
    const double FeetLift = FVector::DotProduct(Center - CenterFloor.ImpactPoint, Up);
    if (FeetLift < -2.0 || FeetLift > 20.0)
    {
        return false;
    }
    TArray<FVector> Measured;
    for (int32 Segment = 1; Segment < Proposed.Num(); ++Segment)
    {
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Distance(Proposed[Segment - 1], Proposed[Segment]) / 25.0));
        // Keep malformed/very long authored paths bounded; fallback still retains their original detail.
        if (Measured.Num() + Steps + 1 > 4096)
        {
            return false;
        }
        for (int32 Step = Segment == 1 ? 0 : 1; Step <= Steps; ++Step)
        {
            const FVector Expected = Frame.TransformPosition(FMath::Lerp(Proposed[Segment - 1], Proposed[Segment],
                static_cast<double>(Step) / Steps));
            FHitResult Floor;
            if (!FloorAt(Expected, Floor)
                || FMath::Abs(FVector::DotProduct(Expected - Floor.ImpactPoint, Up) - FeetLift) > 12.0)
            {
                return false;
            }
            const FVector Feet = Floor.ImpactPoint + Up * FeetLift;
            const FVector Across = FVector::CrossProduct(Up,
                Frame.TransformVectorNoScale(Proposed[Segment] - Proposed[Segment - 1])).GetSafeNormal();
            for (const double Side : { -20.0, 20.0 })
            {
                FHitResult FootFloor;
                if (!FloorAt(Feet + Across * Side, FootFloor)
                    || FMath::Abs(FVector::DotProduct(Feet - FootFloor.ImpactPoint, Up) - FeetLift) > 12.0)
                {
                    return false;
                }
            }
            // Feet-to-head clearance on every short segment, including low rails (not only a torso-height ray).
            const FVector Previous = Measured.IsEmpty() ? Feet : Frame.TransformPosition(Measured.Last());
            FHitResult Obstacle;
            if (GetWorld()->SweepSingleByObjectType(Obstacle, Previous + Up * 95.0, Feet + Up * 95.0,
                FQuat::FindBetweenNormals(FVector::UpVector, Up), Objects, FCollisionShape::MakeCapsule(35.0f, 90.0f), Query))
            {
                return false;
            }
            Measured.Add(Frame.InverseTransformPosition(Feet));
        }
    }
    // The measured closing point must be exact to avoid a seam/teleport on wrap.
    Measured.Last() = Measured[0];
    PassingLoop = MoveTemp(Measured);
    return true;
}

void ALFFCrowdFanActor::PlayIdle()
{
    USkeletalMeshComponent* Mesh = GetAnimatedMesh();
    UAnimationAsset* Clip = bWalking ? MovingAnimation.Get() : StillAnimation.Get();
    if (Clip == nullptr || Mesh == nullptr || Clip->GetPlayLength() <= 0.0f)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd fan %s has no usable idle animation."), *GetName());
        return;
    }
    Mesh->PlayAnimation(Clip, true);
    Mesh->SetPlayRate(IdleRate);
    Mesh->SetPosition(Variation.FRandRange(0.0f, Clip->GetPlayLength()), false);
    Mesh->TickAnimation(0.0f, false);
    Mesh->RefreshBoneTransforms();
    ScheduleGesture();
}

void ALFFCrowdFanActor::ScheduleGesture()
{
    if (bPoseRunning && !bWalking && Gestures.Num() > 0)
    {
        GetWorldTimerManager().SetTimer(GestureTimer, this, &ALFFCrowdFanActor::PlayGesture, Variation.FRandRange(9.0f, 28.0f), false);
    }
}

void ALFFCrowdFanActor::PlayGesture()
{
    if (!bPoseRunning || bWalking || Gestures.Num() == 0)
    {
        return;
    }
    UAnimSequence* Clip = Gestures[Variation.RandHelper(Gestures.Num())];
    if (Clip == nullptr || Clip->GetPlayLength() <= 0.0f)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd fan %s has an invalid gesture."), *GetName());
        PlayIdle();
        return;
    }
    const float Rate = Variation.FRandRange(0.9f, 1.1f);
    GetAnimatedMesh()->PlayAnimation(Clip, false);
    GetAnimatedMesh()->SetPlayRate(Rate);
    GetWorldTimerManager().SetTimer(GestureTimer, this, &ALFFCrowdFanActor::PlayIdle, Clip->GetPlayLength() / Rate, false);
}

void ALFFCrowdFanActor::SetCrowdActive(const bool bVisible, const bool bAnimate)
{
    if (bVisible && bWalking && (IsHidden() || bRoutePlacementBlocked))
    {
        bRoutePlacementBlocked = !PlaceWalker();
    }
    bCrowdVisible = bVisible && !bRoutePlacementBlocked;
    bCrowdAnimated = bAnimate;
    SetActorHiddenInGame(!bCrowdVisible);
    GetWorldTimerManager().ClearTimer(ProximityTimer);
    if (bCrowdVisible && bAnimate)
    {
        RefreshProximity();
        GetWorldTimerManager().SetTimer(ProximityTimer, this, &ALFFCrowdFanActor::RefreshProximity, 1.0f, true,
            Variation.FRandRange(0.1f, 1.0f));
    }
    UpdateActivity();
}

void ALFFCrowdFanActor::RefreshProximity()
{
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        bInAnimationRange = FVector::DistSquared(ViewLocation, GetActorLocation())
            <= FMath::Square(FMath::Clamp(AnimationDistance, 500.0f, 10000.0f));
    }
    UpdateActivity();
}

void ALFFCrowdFanActor::UpdateActivity()
{
    const bool bRunning = LFFCirculationRules::ShouldAnimate(bCrowdVisible, bCrowdAnimated, bInAnimationRange);
    if (bRunning != bPoseRunning)
    {
        bPoseRunning = bRunning;
        GetWorldTimerManager().ClearTimer(GestureTimer);
        if (bRunning)
        {
            PlayIdle();
        }
        SetActorTickEnabled(bRunning && bWalking);
    }
    if (UAnimSingleNodeInstance* Instance = GetAnimatedMesh()->GetSingleNodeInstance())
    {
        Instance->SetPlaying(bRunning && !bWaitingForSpace);
    }
    TInlineComponentArray<USkinnedMeshComponent*> Meshes(this);
    for (USkinnedMeshComponent* Mesh : Meshes)
    {
        Mesh->SetComponentTickEnabled(bRunning);
    }
}

void ALFFCrowdFanActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(GestureTimer);
    GetWorldTimerManager().ClearTimer(ProximityTimer);
    Super::EndPlay(EndPlayReason);
}
