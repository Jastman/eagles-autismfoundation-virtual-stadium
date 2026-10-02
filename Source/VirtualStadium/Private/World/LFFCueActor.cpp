#include "World/LFFCueActor.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

ALFFCueActor::ALFFCueActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Movable);
    RootComponent = SceneRoot;
}

void ALFFCueActor::BeginPlay()
{
    Super::BeginPlay();
    CacheRestPose();
    PlayLoop(StillAnimation);
}

void ALFFCueActor::CacheRestPose()
{
    if (!bHasRest && RootComponent != nullptr)
    {
        // Relative to LFF_SiteRoot, so the globe anchor can place the site without moving the story.
        RestRelative = RootComponent->GetRelativeTransform();
        bHasRest = true;
    }
}

void ALFFCueActor::ApplyState(const ELFFCueState State)
{
    CacheRestPose();
    bPlaying = false;
    SetActorTickEnabled(false);
    OnStopped();
    PlayLoop(StillAnimation);

    const bool bAfter = State == ELFFCueState::After;
    ApplyAlpha(bAfter ? 1.0f : 0.0f, 0.0f);
    SetCueHidden(bAfter ? bHiddenAfter : bHiddenBefore);
}

void ALFFCueActor::Play(const bool bInstant)
{
    CacheRestPose();
    if (bInstant)
    {
        ApplyState(ELFFCueState::After);
        return;
    }

    Elapsed = 0.0f;
    bPlaying = true;
    PlayLoop(StillAnimation);
    ApplyAlpha(0.0f, 0.0f);
    // Something that appears for its move (the eagle) stays hidden through the delay.
    SetCueHidden(bHiddenBefore && PlayDelaySeconds > 0.0f);
    OnPlayStarted();
    SetActorTickEnabled(true);
}

void ALFFCueActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPlaying)
    {
        SetActorTickEnabled(false);
        return;
    }

    Elapsed += DeltaSeconds;
    const float Moving = Elapsed - PlayDelaySeconds;
    if (Moving < 0.0f)
    {
        return;
    }

    SetCueHidden(false);
    if (!bMovingAnimationOn)
    {
        PlayLoop(MovingAnimation);
        bMovingAnimationOn = MovingAnimation != nullptr;
    }
    const float Linear = FMath::Clamp(Moving / FMath::Max(PlaySeconds, 0.01f), 0.0f, 1.0f);
    // Ease-out cubic: it arrives gently and settles, with no bounce or overshoot.
    const float Alpha = bLinear ? Linear : 1.0f - FMath::Pow(1.0f - Linear, 3.0f);
    ApplyAlpha(Alpha, Moving);

    if (Linear >= 1.0f)
    {
        Finish();
    }
}

void ALFFCueActor::Finish()
{
    bPlaying = false;
    SetActorTickEnabled(false);
    OnStopped();
    PlayLoop(StillAnimation);
    ApplyAlpha(1.0f, PlaySeconds);
    SetCueHidden(bHiddenAfter);
}

void ALFFCueActor::ApplyAlpha(const float Alpha, float /*ElapsedSeconds*/)
{
    if (!bHasRest || RootComponent == nullptr)
    {
        return;
    }

    const FTransform Start = FTransform(StartRotation, StartOffset) * RestRelative;
    const FVector Location = FMath::Lerp(Start.GetLocation(), RestRelative.GetLocation(), Alpha);
    const FQuat Rotation = FQuat::Slerp(Start.GetRotation(), RestRelative.GetRotation(), Alpha);
    RootComponent->SetRelativeLocationAndRotation(Location, Rotation);
}

void ALFFCueActor::PlayLoop(UAnimationAsset* Animation)
{
    if (Animation == nullptr)
    {
        return;
    }
    bMovingAnimationOn = Animation == MovingAnimation;
    if (USkeletalMeshComponent* Animated = GetAnimatedMesh())
    {
        PlayLoopOn(Animated, Animation);
        return;
    }
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(this);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        PlayLoopOn(Mesh, Animation);
    }
}

void ALFFCueActor::PlayLoopOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Animation)
{
    const USkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset();
    const USkeleton* MeshSkeleton = MeshAsset != nullptr ? MeshAsset->GetSkeleton() : nullptr;
    const USkeleton* AnimSkeleton = Animation->GetSkeleton();
    Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;  // the person's pose mesh is hidden
    Mesh->SetComponentTickEnabled(true);
    Mesh->PlayAnimation(Animation, true);

    const UAnimSingleNodeInstance* Player = Mesh->GetSingleNodeInstance();
    if (Player == nullptr || Player->GetAnimationAsset() != Animation)
    {
        const FString Warning = FString::Printf(TEXT("%s: %s did not start on %s (mesh skeleton %s, clip skeleton %s)"),
            *GetActorNameOrLabel(), *GetNameSafe(Animation), *GetNameSafe(MeshAsset), *GetNameSafe(MeshSkeleton),
            *GetNameSafe(AnimSkeleton));
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Warning);
    }
}

void ALFFCueActor::SetCueHidden(const bool bHidden)
{
    SetActorHiddenInGame(bHidden);

    TArray<AActor*> Attached;
    GetAttachedActors(Attached, true, true);
    for (AActor* Child : Attached)
    {
        if (Child != nullptr)
        {
            Child->SetActorHiddenInGame(bHidden);
        }
    }
}
