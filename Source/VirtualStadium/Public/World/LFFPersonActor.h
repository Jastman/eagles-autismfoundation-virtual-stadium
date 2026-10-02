#pragma once

#include "CoreMinimal.h"
#include "World/LFFCueActor.h"
#include "LFFPersonActor.generated.h"

class UChaosClothAssetBase;
class UChaosClothComponent;
class UMaterialInterface;
class USkeletalMesh;

/** One outfit piece: a MetaHuman Crowd Sample cloth asset and, optionally, one material for all of its slots. */
USTRUCT(BlueprintType)
struct FLFFOutfitPiece
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<UChaosClothAssetBase> Cloth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<UMaterialInterface> Material;

    /** Horizontal scale about the body's axis. The cloth isn't simulated, so a touch over 1 keeps skin from showing through. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person", meta = (ClampMin = "1.0", ClampMax = "1.2"))
    float Inflate = 1.0f;
};

/**
 * A clothed person: a MetaHuman Crowd Sample body and face, and outfit pieces (Tools/Editor/build_lff_standins.py),
 * assembled on construction so they show in the editor too. The face and clothes follow the body's pose; the body
 * loops StillAnimation, and MovingAnimation while its cue moves it. The cloth isn't simulated, only skinned: enough at
 * these distances, and it keeps a line of people cheap.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFPersonActor : public ALFFCueActor
{
    GENERATED_BODY()

public:
    ALFFPersonActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> BodyMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> FaceMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TArray<FLFFOutfitPiece> Outfit;

    /**
     * Hidden mesh the animations play on, on their own skeleton (SKEL_MetaHumanCrowd); the body, face and clothes
     * copy its pose by bone name. The crowd clips don't play directly on the bodies' metahuman_base_skel.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> PoseMesh;

protected:
    virtual void BeginPlay() override;
    virtual USkeletalMeshComponent* GetAnimatedMesh() const override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Pose;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Face;

private:
    /** Meshes, the outfit components and the pose links. Safe to run again (construction reruns in the editor). */
    void Assemble();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UChaosClothComponent>> Clothes;
};
