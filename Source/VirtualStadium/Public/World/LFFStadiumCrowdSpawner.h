#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawner.h"
#include "LFFStadiumCrowdSpawner.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Spawn points for the bowl crowd: one per seat of the stadium model, read from a seat list
 * (Tools/Pipeline/extract_seats.py: Content/LFF/Crowd/seats.json, [x, y, floor z, yaw] in the LFF_SiteRoot frame, cm).
 * Each fan stands on the tread in front of their seat, facing the field. Seats near the camera seats are left empty,
 * so nobody stands in the shot.
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
    float FrontOffset = 25.0f;

    /** Camera targets whose surroundings stay empty (the seat views): nobody within ClearRadius, or ahead of them within ClearAhead. */
    UPROPERTY(EditAnywhere, Category = "Seats")
    TArray<FName> ClearAroundTargets = { TEXT("CT_SeatFieldView") };

    UPROPERTY(EditAnywhere, Category = "Seats")
    float ClearRadius = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Seats")
    float ClearAhead = 1200.0f;

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
     * Where the fans stand (world, facing the field), in a fixed shuffle: Near for MetaHumans, Far for billboards.
     * Seats kept clear for the seat views are in neither.
     */
    void CollectStandPoints(UWorld* World, TArray<FTransform>& Near, TArray<FTransform>& Far) const;

private:
    bool LoadSeats() const;

    mutable TArray<FVector4f> Seats;
    mutable bool bLoaded = false;
};

/**
 * The bowl crowd. Around the visitor's seat: the MetaHuman Crowd Sample's people (its Mass crowd config and character
 * collection). Everywhere else: one upright billboard per fan, an instance of a plane per seat, spread over
 * BillboardMaterials (Tools/Editor/build_crowd_billboards.py). Both fill Occupancy of their seats; the MetaHumans are
 * at most MaxPeople. Tagged LFF_Crowd: Crowd Off and Calm Mode despawn the MetaHumans and hide the billboards
 * (ULFFJourneySubsystem::ApplyCrowd).
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

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Instanced, Category = "Crowd")
    TObjectPtr<ULFFSeatSpawnDataGenerator> SeatGenerator;

private:
    void BuildBillboards(const TArray<FTransform>& Far);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Billboards;
};
