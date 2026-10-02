#include "World/LFFPersonActor.h"

#include "Animation/AnimationAsset.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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
}

void ALFFPersonActor::Assemble()
{
    Pose->SetSkeletalMesh(PoseMesh);
    Body->SetSkeletalMesh(BodyMesh);
    Face->SetSkeletalMesh(FaceMesh);
    // A leader can't follow another leader, so everything follows the pose mesh directly.
    USkeletalMeshComponent* Leader = GetAnimatedMesh();
    Body->SetLeaderPoseComponent(Leader == Body ? nullptr : Leader);
    Face->SetLeaderPoseComponent(Leader);

    for (UChaosClothComponent* Old : Clothes)
    {
        if (Old != nullptr)
        {
            Old->DestroyComponent();
        }
    }
    Clothes.Reset();

    for (const FLFFOutfitPiece& Piece : Outfit)
    {
        if (Piece.Cloth == nullptr)
        {
            continue;
        }
        UChaosClothComponent* Cloth = NewObject<UChaosClothComponent>(this, NAME_None, RF_Transient);
        Cloth->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        Cloth->SetupAttachment(Body);
        Cloth->SetRelativeScale3D(FVector(Piece.Inflate, Piece.Inflate, 1.0f));
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
    }
}
