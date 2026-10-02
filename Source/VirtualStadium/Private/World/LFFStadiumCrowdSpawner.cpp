#include "World/LFFStadiumCrowdSpawner.h"

#include "Camera/LFFCameraTarget.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MassSpawnLocationProcessor.h"
#include "Materials/MaterialInterface.h"
#include "MassSpawnerTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "VirtualStadium.h"

namespace
{
    const FName CrowdSiteRootTag(TEXT("LFF_SiteRoot"));
}

ULFFSeatSpawnDataGenerator::ULFFSeatSpawnDataGenerator()
{
    bUseCustomSeed = true;
    RandomSelectionSeed = 1018;
}

bool ULFFSeatSpawnDataGenerator::LoadSeats() const
{
    if (bLoaded)
    {
        return Seats.Num() > 0;
    }
    bLoaded = true;

    const FString Path = FPaths::ProjectContentDir() / SeatsFile;
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Crowd: no seat list at %s; nobody in the seats."), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("seats"), List))
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Crowd: %s is not a seat list."), *Path);
        return false;
    }

    Seats.Reserve(List->Num());
    for (const TSharedPtr<FJsonValue>& Value : *List)
    {
        const TArray<TSharedPtr<FJsonValue>>& Seat = Value->AsArray();
        if (Seat.Num() >= 4)
        {
            Seats.Emplace(Seat[0]->AsNumber(), Seat[1]->AsNumber(), Seat[2]->AsNumber(), Seat[3]->AsNumber());
        }
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Crowd: %d seats from %s."), Seats.Num(), *SeatsFile);
    return Seats.Num() > 0;
}

int32 ULFFSeatSpawnDataGenerator::GetSeatCount() const
{
    LoadSeats();
    return Seats.Num();
}

void ULFFSeatSpawnDataGenerator::CollectStandPoints(UWorld* World, TArray<FTransform>& Near, TArray<FTransform>& Far) const
{
    Near.Reset();
    Far.Reset();
    if (World == nullptr || !LoadSeats())
    {
        return;
    }

    // Seats are in the site frame; LFF_SiteRoot places it on the globe.
    FTransform Site = FTransform::Identity;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(CrowdSiteRootTag))
        {
            Site = It->GetActorTransform();
            break;
        }
    }

    // Empty seats around the seat cameras, and in front of them, so no one stands in their shot.
    TArray<FTransform> ClearViews;
    TArray<FVector> MetaHumanCentres;
    for (TActorIterator<ALFFCameraTarget> It(World); It; ++It)
    {
        if (ClearAroundTargets.Contains(It->TargetId))
        {
            ClearViews.Add(It->GetCameraTransform());
        }
        if (MetaHumanAroundTargets.Contains(It->TargetId))
        {
            MetaHumanCentres.Add(It->GetCameraTransform().GetLocation());
        }
    }

    Near.Reserve(Seats.Num() / 8);
    Far.Reserve(Seats.Num());
    for (const FVector4f& Seat : Seats)
    {
        const FRotator Yaw(0.0f, Seat.W, 0.0f);
        const FVector Local = FVector(Seat.X, Seat.Y, Seat.Z) + Yaw.Vector() * FrontOffset;
        const FVector WorldLocation = Site.TransformPosition(Local);
        bool bClear = true;
        for (const FTransform& View : ClearViews)
        {
            const FVector Rel = View.InverseTransformPositionNoScale(WorldLocation);
            if (Rel.Size2D() < ClearRadius || (Rel.X > 0.0f && Rel.X < ClearAhead && FMath::Abs(Rel.Y) < Rel.X * 0.8f + ClearRadius))
            {
                bClear = false;
                break;
            }
        }
        if (!bClear)
        {
            continue;
        }
        bool bNear = false;
        for (const FVector& Centre : MetaHumanCentres)
        {
            if (FVector::DistSquared(Centre, WorldLocation) < FMath::Square(MetaHumanRadius))
            {
                bNear = true;
                break;
            }
        }
        (bNear ? Near : Far).Emplace(Site.TransformRotation(Yaw.Quaternion()), WorldLocation);
    }

    // A fixed shuffle, so the same seats fill every run and partial occupancy is spread over the whole bowl.
    FRandomStream Random(GetRandomSelectionSeed());
    for (TArray<FTransform>* Points : { &Near, &Far })
    {
        for (int32 I = Points->Num() - 1; I > 0; --I)
        {
            Points->Swap(I, Random.RandHelper(I + 1));
        }
    }
}

void ULFFSeatSpawnDataGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, const int32 Count,
    FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
    TArray<FMassEntitySpawnDataGeneratorResult> Results;
    TArray<FTransform> Points;
    TArray<FTransform> Far;
    CollectStandPoints(QueryOwner.GetWorld(), Points, Far);
    if (Count <= 0 || Points.Num() == 0)
    {
        FinishedGeneratingSpawnPointsDelegate.Execute(Results);
        return;
    }

    const int32 Spawn = FMath::Min(Count, Points.Num());
    BuildResultsFromEntityTypes(Spawn, EntityTypes, Results);
    int32 Next = 0;
    for (FMassEntitySpawnDataGeneratorResult& Result : Results)
    {
        Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
        Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
        FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();
        Transforms.bRandomize = false;
        Transforms.Transforms.Reserve(Result.NumEntities);
        for (int32 I = 0; I < Result.NumEntities && Next < Points.Num(); ++I)
        {
            Transforms.Transforms.Add(Points[Next++]);
        }
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Crowd: %d MetaHumans in %d seats within %.0f m of the visitor's seat."), Spawn, Points.Num(),
        MetaHumanRadius / 100.0f);
    FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}

ALFFStadiumCrowdSpawner::ALFFStadiumCrowdSpawner()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    BillboardMesh = Plane.Object;

    SeatGenerator = CreateDefaultSubobject<ULFFSeatSpawnDataGenerator>(TEXT("Seats"));
    FMassSpawnDataGenerator& Generator = SpawnDataGenerators.AddDefaulted_GetRef();
    Generator.GeneratorInstance = SeatGenerator;
    Generator.Proportion = 1.0f;
    Tags.Add(TEXT("LFF_Crowd"));
}

void ALFFStadiumCrowdSpawner::BeginPlay()
{
    // Count before the base class spawns on BeginPlay.
    if (SeatGenerator != nullptr)
    {
        for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
        {
            if (Generator.GeneratorInstance == nullptr)
            {
                Generator.GeneratorInstance = SeatGenerator;
            }
        }
        TArray<FTransform> Near;
        TArray<FTransform> Far;
        SeatGenerator->CollectStandPoints(GetWorld(), Near, Far);
        Count = FMath::Min(MaxPeople, FMath::RoundToInt(Near.Num() * Occupancy));
        Far.SetNum(FMath::RoundToInt(Far.Num() * Occupancy));
        BuildBillboards(Far);
    }
    Super::BeginPlay();
}

void ALFFStadiumCrowdSpawner::BuildBillboards(const TArray<FTransform>& Far)
{
    for (UInstancedStaticMeshComponent* Old : Billboards)
    {
        if (Old != nullptr)
        {
            Old->DestroyComponent();
        }
    }
    Billboards.Reset();

    TArray<UMaterialInterface*> Materials;
    for (UMaterialInterface* Material : BillboardMaterials)
    {
        if (Material != nullptr)
        {
            Materials.Add(Material);
        }
    }
    if (BillboardMesh == nullptr || Materials.Num() == 0 || Far.Num() == 0)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Crowd: no billboards (%d far seats, %d materials); run Tools/Editor/build_crowd_billboards.py."),
            Far.Num(), Materials.Num());
        return;
    }

    for (UMaterialInterface* Material : Materials)
    {
        UInstancedStaticMeshComponent* Cards = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
        Cards->SetupAttachment(GetRootComponent());
        Cards->SetStaticMesh(BillboardMesh);
        Cards->SetMaterial(0, Material);
        Cards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Cards->SetCastShadow(false);
        Cards->SetUsingAbsoluteLocation(true);
        Cards->SetUsingAbsoluteRotation(true);
        Cards->SetUsingAbsoluteScale(true);
        Cards->RegisterComponent();
        Billboards.Add(Cards);
    }

    // The engine plane is 100 cm square, facing +Z: stand it up, facing the seat's +X (the field), on the tread.
    const FTransform PlaneToCard(FRotator(0.0f, -90.0f, 90.0f).Quaternion(), FVector::ZeroVector,
        FVector(BillboardSize.X / 100.0f, BillboardSize.Y / 100.0f, 1.0f));
    TArray<TArray<FTransform>> PerMaterial;
    PerMaterial.SetNum(Billboards.Num());
    for (int32 I = 0; I < Far.Num(); ++I)
    {
        const FTransform Stand(Far[I].GetRotation(), Far[I].GetLocation() + FVector(0.0f, 0.0f, BillboardSize.Y * 0.5f));
        PerMaterial[I % Billboards.Num()].Add(PlaneToCard * Stand);
    }
    for (int32 M = 0; M < Billboards.Num(); ++M)
    {
        Billboards[M]->AddInstances(PerMaterial[M], false, true);
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Crowd: %d billboard fans in the rest of the bowl (%d looks)."), Far.Num(), Billboards.Num());
}
