#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawner.h"
#include "World/LFFPersonActor.h"
#include "LFFStadiumCrowdSpawner.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UAnimSequence;
class ALFFCrowdFanActor;

USTRUCT(BlueprintType)
struct FLFFCrowdAppearance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TObjectPtr<USkeletalMesh> BodyMesh;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TObjectPtr<USkeletalMesh> FaceMesh;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    FLFFHairPiece HairStyle;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TArray<FLFFOutfitPiece> Outfit;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TMap<FName, TObjectPtr<UMaterialInterface>> BodyMaterialOverrides;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TMap<FName, TObjectPtr<UMaterialInterface>> FaceMaterialOverrides;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TArray<FName> ExposedBodyMaterialSlots = { TEXT("head"), TEXT("face"), TEXT("hands") };
};

struct FLFFCrowdSeat
{
    FTransform Standing;
    FTransform Seated;
    double DistanceSquared = 0.0;
};

namespace LFFCrowdRules
{
    struct FCounts
    {
        int32 Detailed = 0;
        int32 Mass = 0;
        int32 Far = 0;
        int32 Overflow = 0;
    };
    VIRTUALSTADIUM_API FCounts RepresentationCounts(int32 DetailedSeats, int32 NearSeats, int32 FarSeats, float Occupancy, int32 MassLimit);
    VIRTUALSTADIUM_API bool IsCameraSpace(const FVector& Point, const FTransform& View, float Radius, float Ahead);
}

/**
 * Spawn points for the bowl crowd: one per seat of the stadium model, read from a seat list
 * (Tools/Pipeline/extract_seats.py: [x, y, chair underside z, yaw] in the LFF_SiteRoot frame, cm).
 * build_seat_treads.py measures the actual floor separately. Only the visitor's immediate space stays empty.
 */
UCLASS(EditInlineNew)
class VIRTUALSTADIUM_API ULFFSeatSpawnDataGenerator : public UMassEntitySpawnDataGeneratorBase
{
    GENERATED_BODY()

public:
    ULFFSeatSpawnDataGenerator();

    virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count,
        FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

    /** Relative to the project's Content folder (staged with the build: DefaultGame.ini). */
    UPROPERTY(EditAnywhere, Category = "Seats")
    FString SeatsFile = TEXT("LFF/Crowd/seats.json");

    /** How far in front of the seat's centre a fan stands (cm). */
    UPROPERTY(EditAnywhere, Category = "Seats")
    float FrontOffset = 70.0f;

    UPROPERTY(EditAnywhere, Category = "Seats")
    FString TreadsFile = TEXT("LFF/Crowd/seat_treads.json");

    /** Camera targets whose surroundings stay empty (the seat views): nobody within ClearRadius, or ahead of them within ClearAhead. */
    UPROPERTY(EditAnywhere, Category = "Seats")
    TArray<FName> ClearAroundTargets = { TEXT("CT_SeatFieldView") };

    UPROPERTY(EditAnywhere, Category = "Seats")
    float ClearRadius = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Seats")
    float ClearAhead = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Seats", meta = (ClampMin = "0.0"))
    float DetailedRadius = 700.0f;

    UPROPERTY(EditAnywhere, Category = "Seats", meta = (ClampMin = "0", ClampMax = "96"))
    int32 MaxDetailedFans = 48;

    /** Set before spawn selection; a missing appearance/clip is explicitly reported and uses the efficient crowd. */
    bool bUseDetailedFans = false;

    /**
     * Camera targets the MetaHumans sit around (the visitor's seat): only seats within MetaHumanRadius of one get a
     * MetaHuman. Every other seat is a billboard (ALFFStadiumCrowdSpawner): the GPU skinning buffer can't hold a
     * whole bowl of MetaHumans, and past the near rows a fan is a few pixels anyway.
     */
    UPROPERTY(EditAnywhere, Category = "Seats")
    TArray<FName> MetaHumanAroundTargets = { TEXT("CT_SeatFieldView") };

    UPROPERTY(EditAnywhere, Category = "Seats", meta = (ClampMin = "0.0"))
    float MetaHumanRadius = 2000.0f;

    /** Seat count in the list (loaded on first use). */
    int32 GetSeatCount() const;

    /**
     * Floor-based transforms (world, facing the field): Detailed for clothed neighbours, Near for Mass, Far for billboards.
     * Seats kept clear for the seat views are in neither.
     */
    void CollectStandPoints(UWorld* World, TArray<FTransform>& Near, TArray<FTransform>& Far, TArray<FLFFCrowdSeat>& Detailed) const;

private:
    bool LoadSeats() const;

    mutable TArray<FVector4f> Seats;
    mutable TArray<FVector2D> Treads;
    mutable float SeatedOffset = 45.0f;
    mutable float SeatedLateralOffset = 13.0f;
    mutable bool bLoaded = false;
};

/**
 * The bowl crowd: capped clothed neighbours with seated/standing clips, middle-distance Mass crowd, distant instanced
 * billboards. Each representation fills Occupancy; Mass-cap overflow stays occupied as billboards. Measured floor data
 * replaces chair heights. Crowd Off and Calm Mode hide detailed actors, stop their animation work and despawn Mass;
 * Less movement freezes detailed fans (ULFFJourneySubsystem::ApplyCrowd).
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFStadiumCrowdSpawner : public AMassSpawner
{
    GENERATED_BODY()

public:
    ALFFStadiumCrowdSpawner();

    UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Occupancy = 0.8f;

    UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0"))
    int32 MaxPeople = 6000;

    /** One standing fan each (masked); seats take them in turn. None: no billboards. */
    UPROPERTY(EditAnywhere, Category = "Crowd")
    TArray<TObjectPtr<UMaterialInterface>> BillboardMaterials;

    UPROPERTY(EditAnywhere, Category = "Crowd")
    TObjectPtr<UStaticMesh> BillboardMesh;

    /** Billboard width and height (cm): the texture is 1:4, one fan about 1.75 m tall standing at its bottom. */
    UPROPERTY(EditAnywhere, Category = "Crowd")
    FVector2D BillboardSize = FVector2D(60.0f, 240.0f);

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TArray<FLFFCrowdAppearance> DetailedLooks;

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TObjectPtr<UAnimSequence> SeatedIdle;

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TObjectPtr<UAnimSequence> SeatedClap;

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TObjectPtr<UAnimSequence> StandingCheer;

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TObjectPtr<UAnimSequence> StandingClap;

    UPROPERTY(EditAnywhere, Category = "Crowd|Detailed")
    TObjectPtr<UAnimSequence> StandingIdle;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, Instanced, Category = "Crowd")
    TObjectPtr<ULFFSeatSpawnDataGenerator> SeatGenerator;

private:
    void BuildBillboards(const TArray<FTransform>& Far);
    void BuildDetailedFans(const TArray<FLFFCrowdSeat>& Seats, int32 FanCount);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Billboards;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ALFFCrowdFanActor>> DetailedFans;
};
