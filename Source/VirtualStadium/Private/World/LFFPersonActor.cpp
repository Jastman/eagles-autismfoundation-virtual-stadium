#include "World/LFFPersonActor.h"
#include "World/LFFCrowdFanActor.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Journey/LFFJourneySubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ConstructorHelpers.h"
#include "VirtualStadium.h"

bool LFFClothingRules::CanHideSlot(const FName Slot, const TArray<FName>& Covered, const TArray<FName>& Exposed)
{
    return !Slot.IsNone() && Covered.Contains(Slot) && !Exposed.Contains(Slot);
}

bool LFFClothingRules::IsHairFitValid(const USkeletalMesh* FittedFace, const USkeletalMesh* ActualFace)
{
    return FittedFace != nullptr && ActualFace != nullptr && FittedFace == ActualFace;
}

ALFFPersonActor::ALFFPersonActor()
{
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CrowdPoseMesh(
        TEXT("/MetaHumanCrowdContent/Animations/SKM_MetaHumanCrowd.SKM_MetaHumanCrowd"));
    PoseMesh = CrowdPoseMesh.Object;

    Pose = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Pose"));
    Pose->SetupAttachment(SceneRoot);
    Pose->SetMobility(EComponentMobility::Movable);
    Pose->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pose->SetVisibility(false);
    Pose->CastShadow = false;
    // Hidden, so it has to be told to keep posing: everything visible follows it.
    Pose->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(SceneRoot);
    Body->SetMobility(EComponentMobility::Movable);
    // MetaHuman meshes face +Y; the actor's front is +X (the cue moves it forward).
    Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->bCastDynamicShadow = true;

    Face = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
    Face->SetupAttachment(Body);
    Face->SetMobility(EComponentMobility::Movable);
    Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Hair = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hair"));
    Hair->SetupAttachment(Body);
    Hair->SetMobility(EComponentMobility::Movable);
    Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hair->SetGenerateOverlapEvents(false);
    Hair->SetCastShadow(false);
    Hair->ComponentTags.Add(TEXT("LFF_Hair"));
}

void ALFFPersonActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Assemble();
    // The editor shows the standing pose (in game, ALFFCueActor plays it from BeginPlay).
    if (StillAnimation != nullptr)
    {
        GetAnimatedMesh()->OverrideAnimationData(StillAnimation, true, true);
    }
}

USkeletalMeshComponent* ALFFPersonActor::GetAnimatedMesh() const
{
    return PoseMesh != nullptr ? Pose.Get() : Body.Get();
}

void ALFFPersonActor::BeginPlay()
{
    Assemble();
    Super::BeginPlay();
    if (USkeletalMeshComponent* Animated = GetAnimatedMesh())
    {
        Animated->TickAnimation(0.0f, false);
        Animated->RefreshBoneTransforms();
    }
    SetActorHiddenInGame(IsHidden());
}

void ALFFPersonActor::SetActorHiddenInGame(const bool bNewHidden)
{
    bool bCrowdHidden = false;
    bool bReducedCrowdMotion = false;
    if (ActorHasTag(TEXT("LFF_Crowd")) && GetWorld() != nullptr && GetWorld()->IsGameWorld())
    {
        if (const ULFFJourneySubsystem* Journey = GetWorld()->GetSubsystem<ULFFJourneySubsystem>())
        {
            const FLFFVisitorSettings& Settings = Journey->GetSnapshot().Settings;
            bCrowdHidden = !Settings.IsCrowdShown();
            bReducedCrowdMotion = Settings.IsReducedMotionEffective();
        }
    }
    if (!bNewHidden && !bCrowdHidden && !bSpaceHidden && !StartOffset.IsNearlyZero()
        && !IsA<ALFFCrowdFanActor>() && GetWorld() != nullptr && GetWorld()->IsGameWorld() && RootComponent != nullptr)
    {
        // Legacy cues can be revealed before their first ApplyState (or after an ambient walker has moved here).
        // Ambient actors claim their own route slot in SetCrowdActive instead.
        bSpaceHidden = !TrySetPersonPose(RootComponent->GetRelativeLocation(), RootComponent->GetRelativeRotation().Quaternion(), true);
    }
    // A one-way concourse cue must not reveal an authored crowd member while Crowd Off or Calm is selected.
    Super::SetActorHiddenInGame(bNewHidden || bCrowdHidden || bSpaceHidden);
    const bool bAnimate = !IsHidden() && !bReducedCrowdMotion;
    TInlineComponentArray<USkinnedMeshComponent*> Meshes(this);
    for (USkinnedMeshComponent* Mesh : Meshes)
    {
        Mesh->SetComponentTickEnabled(bAnimate);
        if (USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Mesh))
        {
            if (UAnimSingleNodeInstance* Instance = Skeletal->GetSingleNodeInstance())
            {
                Instance->SetPlaying(bAnimate);
            }
        }
    }
}

FTransform ALFFPersonActor::PersonParentTransform() const
{
    const USceneComponent* Parent = RootComponent != nullptr ? RootComponent->GetAttachParent() : nullptr;
    return Parent != nullptr ? Parent->GetComponentTransform() : FTransform::Identity;
}

bool ALFFPersonActor::TrySetPersonPose(const FVector& RelativeLocation, const FQuat& RelativeRotation, const bool bPlacement)
{
    if (RootComponent == nullptr || GetWorld() == nullptr || RelativeLocation.ContainsNaN() || RelativeRotation.ContainsNaN())
    {
        return false;
    }
    const FVector Destination = PersonParentTransform().TransformPosition(RelativeLocation);
    TArray<FVector> People;
    TArray<ALFFPersonActor*> CrossingPeople;
    for (TActorIterator<ALFFPersonActor> It(GetWorld()); It; ++It)
    {
        // Pending actors do not yet own a route position. Each claims a slot when it begins/reappears.
        if (*It != this && It->HasActorBegunPlay() && !It->IsHidden() && !It->IsActorBeingDestroyed())
        {
            People.Add(It->GetActorLocation());
            if (!bPlacement && !SharesPersonRoute(**It))
            {
                CrossingPeople.Add(*It);
            }
        }
    }
    if (!LFFCirculationRules::HasSpace(GetActorLocation(), Destination, People, bPlacement))
    {
        return false;
    }
    // A crossing already in progress must clear before an approaching person reaches its hard spacing
    // boundary. Otherwise a slow legacy walker can be trapped BETWEEN the outbound and return lanes.
    // Priority resolves approaching traffic; an early stop grants an occupied crossing time to clear.
    // Waiting actors do not retain a moving reservation that prevents the other person from clearing.
    const FVector TravelDirection = (Destination - GetActorLocation()).GetSafeNormal();
    TArray<FVector> OwnPrediction;
    for (const ALFFPersonActor* Other : CrossingPeople)
    {
        TArray<FVector> Prediction;
        Other->PredictPersonRoute(Prediction);
        if (Prediction.Num() < 2
            || FVector::DotProduct(TravelDirection, (Prediction[1] - Prediction[0]).GetSafeNormal()) >= 0.5)
        {
            continue;
        }
        if (Other->GetUniqueID() < GetUniqueID())
        {
            if (!Other->IsWaitingForPersonSpace())
            {
                for (int32 Index = 1; Index < Prediction.Num(); ++Index)
                {
                    if (!LFFCirculationRules::HasSpace(Prediction[Index - 1], Prediction[Index], { Destination }, false))
                    {
                        return false;
                    }
                }
            }
        }
        else
        {
            if (OwnPrediction.IsEmpty())
            {
                PredictPersonRoute(OwnPrediction);
            }
            for (int32 Index = 1; Index < OwnPrediction.Num(); ++Index)
            {
                if (!LFFCirculationRules::HasSpace(OwnPrediction[Index - 1], OwnPrediction[Index],
                    { Other->GetActorLocation() }, false))
                {
                    return false;
                }
            }
        }
    }
    RootComponent->SetRelativeLocationAndRotation(RelativeLocation, RelativeRotation);
    return true;
}

void ALFFPersonActor::PredictPersonRoute(TArray<FVector>& OutWorldPoints) const
{
    OutWorldPoints.Reset();
    if (!IsPlaying() || StartOffset.IsNearlyZero() || IsHidden())
    {
        return;
    }
    OutWorldPoints.Add(GetActorLocation());
    const FTransform Parent = PersonParentTransform();
    for (int32 Sample = 1; Sample <= 10; ++Sample)
    {
        const float Moving = GetCueElapsed() + Sample * 0.25f - PlayDelaySeconds;
        const float Linear = FMath::Clamp(Moving / FMath::Max(PlaySeconds, 0.01f), 0.0f, 1.0f);
        const float Alpha = bLinear ? Linear : 1.0f - FMath::Pow(1.0f - Linear, 3.0f);
        OutWorldPoints.Add(Parent.TransformPosition(PersonCuePose(Alpha).GetLocation()));
    }
}

FTransform ALFFPersonActor::PersonCuePose(const float Alpha) const
{
    const FTransform& Rest = GetRestRelativePose();
    const FTransform Start = FTransform(StartRotation, StartOffset) * Rest;
    return FTransform(FQuat::Slerp(Start.GetRotation(), Rest.GetRotation(), Alpha),
        FMath::Lerp(Start.GetLocation(), Rest.GetLocation(), Alpha), Rest.GetScale3D());
}

void ALFFPersonActor::PausePersonWalk(const bool bPause)
{
    if (USkeletalMeshComponent* Mesh = GetAnimatedMesh())
    {
        if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
        {
            Instance->SetPlaying(!bPause && !IsHidden());
        }
    }
}

bool ALFFPersonActor::TryAdvanceCue(const float Alpha, const float ElapsedSeconds)
{
    if (StartOffset.IsNearlyZero())
    {
        return Super::TryAdvanceCue(Alpha, ElapsedSeconds);
    }
    const FTransform PoseAtAlpha = PersonCuePose(Alpha);
    const bool bAccepted = TrySetPersonPose(PoseAtAlpha.GetLocation(), PoseAtAlpha.GetRotation(), bSpaceHidden);
    if (bAccepted && bSpaceHidden)
    {
        bSpaceHidden = false;
        SetActorHiddenInGame(false);
    }
    bPersonWaitingForSpace = !bAccepted;
    PausePersonWalk(!bAccepted);
    return bAccepted;
}

void ALFFPersonActor::ApplyAlpha(const float Alpha, const float ElapsedSeconds)
{
    bPersonWaitingForSpace = false;
    if (StartOffset.IsNearlyZero())
    {
        Super::ApplyAlpha(Alpha, ElapsedSeconds);
        return;
    }
    // State changes/instant reduced-motion cues reserve a stationary slot on the SAME line, not a sideways detour.
    // Search nearest to the requested pose; ordinary movement uses the swept guard above and never jumps slots.
    const double Length = PersonParentTransform().TransformVector(
        GetRestRelativePose().TransformVector(StartOffset)).Size();
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(Length / 25.0), 1, 4096);
    bSpaceHidden = false;
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        for (const float Sign : { -1.0f, 1.0f })
        {
            const float Candidate = Alpha + Sign * static_cast<float>(Step) / Steps;
            if (Candidate < 0.0f || Candidate > 1.0f)
            {
                continue;
            }
            const FTransform PoseAtAlpha = PersonCuePose(Candidate);
            if (TrySetPersonPose(PoseAtAlpha.GetLocation(), PoseAtAlpha.GetRotation(), true))
            {
                return;
            }
        }
    }
    // An over-capacity authored line must not expose intersecting people. Retry on the next cue/state application.
    bSpaceHidden = true;
    SetActorHiddenInGame(true);
}

void ALFFPersonActor::Assemble()
{
    Pose->SetSkeletalMesh(PoseMesh);
    Body->SetSkeletalMesh(BodyMesh);
    Face->SetSkeletalMesh(FaceMesh);
    // A leader can't follow another leader, so everything follows the pose mesh directly.
    USkeletalMeshComponent* Leader = GetAnimatedMesh();
    Body->SetLeaderPoseComponent(Leader == Body ? nullptr : Leader);
    const bool bFacialFits = FacialAnimation != nullptr && FaceMesh != nullptr
        && FacialAnimation->GetSkeleton() == FaceMesh->GetSkeleton();
    if (FacialAnimation != nullptr && !bFacialFits)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Person %s: facial clip %s does not use face %s's skeleton."),
            *GetActorNameOrLabel(), *GetNameSafe(FacialAnimation), *GetNameSafe(FaceMesh));
    }
    Face->SetLeaderPoseComponent(bFacialFits ? nullptr : Leader);
    // The stock facial post-process rebuilds joints from RigLogic curves, overwriting a baked joint clip.
    Face->SetDisablePostProcessBlueprint(bFacialFits);
    // Lower Crowd face LODs remove the joints needed for the close conversational expression.
    Face->SetForcedLOD(bFacialFits ? 1 : 0);
    if (bFacialFits)
    {
        Face->PlayAnimation(FacialAnimation, true);
        Face->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
    const bool bHairFits = HairStyle.CardMesh != nullptr
        && LFFClothingRules::IsHairFitValid(HairStyle.FittedFaceMesh, FaceMesh);
    Hair->SetSkeletalMesh(bHairFits ? HairStyle.CardMesh.Get() : nullptr);
    Hair->SetRelativeTransform(FTransform::Identity);
    Hair->SetLeaderPoseComponent(Leader);
    if (HairStyle.CardMesh != nullptr && !bHairFits)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Person %s: hair cards %s were not fitted to face %s (expected %s)."),
            *GetActorNameOrLabel(), *GetNameSafe(HairStyle.CardMesh), *GetNameSafe(FaceMesh), *GetNameSafe(HairStyle.FittedFaceMesh));
    }
    if (bHairFits)
    {
        const TArray<FSkeletalMaterial>& HairMaterials = HairStyle.CardMesh->GetMaterials();
        for (int32 Index = 0; Index < HairMaterials.Num(); ++Index)
        {
            const auto* Override = HairStyle.MaterialOverrides.Find(HairMaterials[Index].MaterialSlotName);
            Hair->SetMaterial(Index, Override != nullptr && *Override != nullptr
                ? Override->Get() : HairMaterials[Index].MaterialInterface.Get());
        }
    }

    for (UChaosClothComponent* Old : Clothes)
    {
        if (Old != nullptr)
        {
            Old->DestroyComponent();
        }
    }
    Clothes.Reset();

    TArray<FName> CoveredSlots;
    bool bCompleteOutfit = Outfit.Num() > 0;
    for (const FLFFOutfitPiece& Piece : Outfit)
    {
        if (Piece.Cloth == nullptr || (Piece.FittedBodyMesh != nullptr && Piece.FittedBodyMesh != BodyMesh))
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Person %s: missing garment or fitted body mismatch (%s, expected %s, actual %s)."),
                *GetActorNameOrLabel(), *GetNameSafe(Piece.Cloth), *GetNameSafe(Piece.FittedBodyMesh), *GetNameSafe(BodyMesh));
            bCompleteOutfit = false;
            continue;
        }
        UChaosClothComponent* Cloth = NewObject<UChaosClothComponent>(this, NAME_None, RF_Transient);
        Cloth->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        Cloth->SetupAttachment(Body);
        Cloth->SetRelativeScale3D(FVector::OneVector);
        Cloth->SetMobility(EComponentMobility::Movable);
        Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Cloth->SetEnableSimulation(false);
        Cloth->SetAsset(Piece.Cloth);
        Cloth->RegisterComponent();
        Cloth->SetLeaderPoseComponent(Leader);
        if (Piece.Material != nullptr)
        {
            for (int32 Slot = 0; Slot < Cloth->GetNumMaterials(); ++Slot)
            {
                Cloth->SetMaterial(Slot, Piece.Material);
            }
        }
        Clothes.Add(Cloth);
        for (const FName Slot : Piece.CoveredBodyMaterialSlots)
        {
            CoveredSlots.AddUnique(Slot);
        }
    }
    ApplyBodyCoverage(CoveredSlots, bCompleteOutfit);
}

void ALFFPersonActor::ApplySkinMaterialOverrides(USkeletalMeshComponent* Component,
    const TMap<FName, TObjectPtr<UMaterialInterface>>& Overrides, const bool bCompleteOutfit)
{
    const USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset();
    if (Mesh == nullptr)
    {
        return;
    }
    const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        const auto* Override = Overrides.Find(Materials[Index].MaterialSlotName);
        Component->SetMaterial(Index, bCompleteOutfit && Override != nullptr && *Override != nullptr
            ? Override->Get() : Materials[Index].MaterialInterface.Get());
    }
    for (const auto& Override : Overrides)
    {
        if (!Materials.ContainsByPredicate([&Override](const FSkeletalMaterial& Material)
            { return Material.MaterialSlotName == Override.Key; }))
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Person %s: %s mask names nonexistent material slot %s."),
                *GetActorNameOrLabel(), *Component->GetName(), *Override.Key.ToString());
        }
    }
}

void ALFFPersonActor::ApplyBodyCoverage(const TArray<FName>& CoveredSlots, const bool bCompleteOutfit)
{
    ApplySkinMaterialOverrides(Body, BodyMaterialOverrides, bCompleteOutfit);
    ApplySkinMaterialOverrides(Face, FaceMaterialOverrides, bCompleteOutfit);
    if (BodyMesh == nullptr)
    {
        return;
    }
    const TArray<FSkeletalMaterial>& Materials = BodyMesh->GetMaterials();
    const FSkeletalMeshRenderData* RenderData = BodyMesh->GetResourceForRendering();
    if (RenderData == nullptr)
    {
        return;
    }
    for (int32 LOD = 0; LOD < RenderData->LODRenderData.Num(); ++LOD)
    {
        Body->ShowAllMaterialSections(LOD);
        const FSkeletalMeshLODInfo* Info = BodyMesh->GetLODInfo(LOD);
        const auto& Sections = RenderData->LODRenderData[LOD].RenderSections;
        for (int32 Section = 0; Section < Sections.Num(); ++Section)
        {
            int32 MaterialIndex = Sections[Section].MaterialIndex;
            if (Info != nullptr && Info->LODMaterialMap.IsValidIndex(Section) && Info->LODMaterialMap[Section] != INDEX_NONE)
            {
                MaterialIndex = Info->LODMaterialMap[Section];
            }
            if (Materials.IsValidIndex(MaterialIndex)
                && LFFClothingRules::CanHideSlot(Materials[MaterialIndex].MaterialSlotName, CoveredSlots, ExposedBodyMaterialSlots))
            {
                Body->ShowMaterialSection(Sections[Section].MaterialIndex, Section, false, LOD);
            }
        }
    }
}
