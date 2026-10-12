#pragma once

#include "CoreMinimal.h"
#include "World/LFFCueActor.h"
#include "LFFPersonActor.generated.h"

class UChaosClothAssetBase;
class UChaosClothComponent;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;

namespace LFFClothingRules
{
    /** Coverage is explicitly authored: never hide a whole skin slot just because its name contains "body". */
    VIRTUALSTADIUM_API bool CanHideSlot(FName Slot, const TArray<FName>& Covered, const TArray<FName>& Exposed);
    VIRTUALSTADIUM_API bool IsHairFitValid(const USkeletalMesh* FittedFace, const USkeletalMesh* ActualFace);
}

/** Proper MetaHuman hair-card geometry, fitted to the authored face. No strand simulation or scalp-scale workaround. */
USTRUCT(BlueprintType)
struct FLFFHairPiece
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
    TObjectPtr<USkeletalMesh> CardMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
    TObjectPtr<USkeletalMesh> FittedFaceMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
    TMap<FName, TObjectPtr<UMaterialInterface>> MaterialOverrides;
};

/** One outfit piece: a MetaHuman Crowd Sample cloth asset and, optionally, one material for all of its slots. */
USTRUCT(BlueprintType)
struct FLFFOutfitPiece
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<UChaosClothAssetBase> Cloth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<UMaterialInterface> Material;

    /** Optional fit contract, set by the outfit authoring script to the body this garment was fitted to. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> FittedBodyMesh;

    /** Only fully covered, separate body slots. For a shared torso/hands slot use an authored body-mask material instead. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TArray<FName> CoveredBodyMaterialSlots;

    /** Serialized compatibility only. Garments always use their fitted size, never an inflation workaround. */
    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use a fitted garment and body coverage, not scale inflation."))
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
    virtual void SetActorHiddenInGame(bool bNewHidden) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> BodyMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> FaceMesh;

    /** Optional compatible facial loop; separate from the body's idle or greeting animation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<UAnimSequence> FacialAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    FLFFHairPiece HairStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TArray<FLFFOutfitPiece> Outfit;

    /** Per-slot materials containing the combined garment masks, retaining exposed hands/arms/neck and the original skin shading. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TMap<FName, TObjectPtr<UMaterialInterface>> BodyMaterialOverrides;

    /** Shipped garment masks for the face mesh's lower neck/shoulders, preserving the exposed face. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TMap<FName, TObjectPtr<UMaterialInterface>> FaceMaterialOverrides;

    /** Additional explicit protection for separately authored exposed sections. The separate face is never hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TArray<FName> ExposedBodyMaterialSlots = { TEXT("head"), TEXT("face"), TEXT("hands") };

    /**
     * Hidden mesh the animations play on, on their own skeleton (SKEL_MetaHumanCrowd); the body, face and clothes
     * copy its pose by bone name. The crowd clips don't play directly on the bodies' metahuman_base_skel.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
    TObjectPtr<USkeletalMesh> PoseMesh;

protected:
    virtual void BeginPlay() override;
    virtual USkeletalMeshComponent* GetAnimatedMesh() const override;
    virtual void ApplyAlpha(float Alpha, float ElapsedSeconds) override;
    virtual bool TryAdvanceCue(float Alpha, float ElapsedSeconds) override;

    /** Only other visible people participate; never enable physics collision against the visitor/camera. */
    bool TrySetPersonPose(const FVector& RelativeLocation, const FQuat& RelativeRotation, bool bPlacement = false);
    FTransform PersonParentTransform() const;
    /** Short, route-constrained reservations for crossing traffic, not navigation/avoidance steering. */
    virtual void PredictPersonRoute(TArray<FVector>& OutWorldPoints) const;
    virtual bool SharesPersonRoute(const ALFFPersonActor& Other) const { return false; }
    virtual bool IsWaitingForPersonSpace() const { return bPersonWaitingForSpace; }

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Pose;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Face;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> Hair;

private:
    FTransform PersonCuePose(float Alpha) const;
    void PausePersonWalk(bool bPause);
    bool bSpaceHidden = false;
    bool bPersonWaitingForSpace = false;
    void ApplySkinMaterialOverrides(USkeletalMeshComponent* Component,
        const TMap<FName, TObjectPtr<UMaterialInterface>>& Overrides, bool bCompleteOutfit);
    void ApplyBodyCoverage(const TArray<FName>& CoveredSlots, bool bCompleteOutfit);

    /** Meshes, the outfit components and the pose links. Safe to run again (construction reruns in the editor). */
    void Assemble();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UChaosClothComponent>> Clothes;
};
