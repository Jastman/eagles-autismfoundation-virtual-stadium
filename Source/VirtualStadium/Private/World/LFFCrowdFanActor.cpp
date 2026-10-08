#include "World/LFFCrowdFanActor.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Journey/LFFJourneySubsystem.h"
#include "TimerManager.h"
#include "VirtualStadium.h"

void ALFFCrowdFanActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(TEXT("LFF_Crowd"));
    Variation.Initialize(VariationSeed);
    IdleRate = Variation.FRandRange(0.85f, 1.15f);
    Body->OverrideMinLOD(2);
    Face->OverrideMinLOD(2);
    Body->SetCastShadow(false);
    Face->SetCastShadow(false);
    PlayIdle();
    if (const ULFFJourneySubsystem* Journey = GetWorld()->GetSubsystem<ULFFJourneySubsystem>())
    {
        const FLFFVisitorSettings& Settings = Journey->GetSnapshot().Settings;
        SetCrowdActive(Settings.IsCrowdShown(), !Settings.IsReducedMotionEffective());
    }
}

void ALFFCrowdFanActor::PlayIdle()
{
    USkeletalMeshComponent* Mesh = GetAnimatedMesh();
    if (StillAnimation == nullptr || Mesh == nullptr || StillAnimation->GetPlayLength() <= 0.0f)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd fan %s has no usable idle animation."), *GetName());
        return;
    }
    Mesh->PlayAnimation(StillAnimation, true);
    Mesh->SetPlayRate(IdleRate);
    Mesh->SetPosition(Variation.FRandRange(0.0f, StillAnimation->GetPlayLength()), false);
    Mesh->TickAnimation(0.0f, false);
    Mesh->RefreshBoneTransforms();
    ScheduleGesture();
}

void ALFFCrowdFanActor::ScheduleGesture()
{
    if (bCrowdVisible && bCrowdAnimated && Gestures.Num() > 0)
    {
        GetWorldTimerManager().SetTimer(GestureTimer, this, &ALFFCrowdFanActor::PlayGesture, Variation.FRandRange(9.0f, 28.0f), false);
    }
}

void ALFFCrowdFanActor::PlayGesture()
{
    if (!bCrowdVisible || !bCrowdAnimated || Gestures.Num() == 0)
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
    const bool bWasRunning = bCrowdVisible && bCrowdAnimated;
    bCrowdVisible = bVisible;
    bCrowdAnimated = bAnimate;
    SetActorHiddenInGame(!bVisible);
    const bool bRunning = bVisible && bAnimate;
    if (bRunning != bWasRunning)
    {
        GetWorldTimerManager().ClearTimer(GestureTimer);
        if (bRunning)
        {
            PlayIdle();
        }
    }
    if (UAnimSingleNodeInstance* Instance = GetAnimatedMesh()->GetSingleNodeInstance())
    {
        Instance->SetPlaying(bRunning);
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
    Super::EndPlay(EndPlayReason);
}
