#include "World/LFFStadiumCrowdSpawner.h"
#include "World/LFFCrowdFanActor.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"

#include "Camera/LFFCameraTarget.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
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

LFFCrowdRules::FCounts LFFCrowdRules::RepresentationCounts(const int32 DetailedSeats, const int32 NearSeats, const int32 FarSeats,
    const float Occupancy, const int32 MassLimit)
{
    FCounts Result;
    const float Fraction = FMath::Clamp(Occupancy, 0.0f, 1.0f);
    Result.Detailed = FMath::RoundToInt(FMath::Max(0, DetailedSeats) * Fraction);
    const int32 NearOccupied = FMath::RoundToInt(FMath::Max(0, NearSeats) * Fraction);
    Result.Mass = FMath::Min(FMath::Max(0, MassLimit), NearOccupied);
    Result.Overflow = NearOccupied - Result.Mass;
    Result.Far = FMath::RoundToInt(FMath::Max(0, FarSeats) * Fraction) + Result.Overflow;
    return Result;
}

bool LFFCrowdRules::IsCameraSpace(const FVector& Point, const FTransform& View, const float Radius, const float Ahead)
{
    if (FVector::DistSquared2D(Point, View.GetLocation()) < FMath::Square(Radius))
    {
        return true;
    }
    const FVector Relative = View.InverseTransformPositionNoScale(Point);
    return Ahead > 0.0f && Relative.X > 0.0f && Relative.X < Ahead && FMath::Abs(Relative.Y) < Relative.X * 0.8f + Radius;
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

    FString TreadText;
    TSharedPtr<FJsonObject> TreadRoot;
    const TArray<TSharedPtr<FJsonValue>>* Floors = nullptr;
    TArray<uint8> SeatBytes;
    if (!FFileHelper::LoadFileToArray(SeatBytes, *Path)
        || !FFileHelper::LoadFileToString(TreadText, *(FPaths::ProjectContentDir() / TreadsFile))
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TreadText), TreadRoot)
        || !TreadRoot.IsValid() || !TreadRoot->TryGetArrayField(TEXT("tread_z_cm"), Floors) || Floors->Num() != List->Num())
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: missing or invalid measured treads; run Tools/Pipeline/build_seat_treads.py."));
        return false;
    }
    FString SourceHash;
    double StandingOffset = 0.0, SittingOffset = 0.0, LateralOffset = 0.0, Version = 0.0;
    if (!TreadRoot->TryGetStringField(TEXT("seats_sha1"), SourceHash)
        || !SourceHash.Equals(FSHA1::HashBuffer(SeatBytes.GetData(), SeatBytes.Num()).ToString(), ESearchCase::IgnoreCase)
        || !TreadRoot->TryGetNumberField(TEXT("version"), Version) || Version != 1.0
        || !TreadRoot->TryGetNumberField(TEXT("standing_offset_cm"), StandingOffset)
        || !FMath::IsNearlyEqual(StandingOffset, static_cast<double>(FrontOffset), 0.01)
        || !TreadRoot->TryGetNumberField(TEXT("seated_offset_cm"), SittingOffset)
        || !TreadRoot->TryGetNumberField(TEXT("seated_lateral_offset_cm"), LateralOffset))
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: treads do not match the seat file/foot offsets; regenerate the sidecar."));
        return false;
    }

    TArray<FVector4f> LoadedSeats;
    TArray<FVector2D> LoadedTreads;
    int32 Unmeasured = 0;
    for (int32 Index = 0; Index < List->Num(); ++Index)
    {
        const TArray<TSharedPtr<FJsonValue>>* Seat = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Floor = nullptr;
        double Values[4];
        double StandingZ = 0.0, SeatedZ = 0.0;
        if (!(*List)[Index]->TryGetArray(Seat) || Seat->Num() != 4
            || !(*Floors)[Index]->TryGetArray(Floor) || Floor->Num() != 2)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: malformed seat/tread row %d."), Index);
            return false;
        }
        for (int32 Column = 0; Column < 4; ++Column)
        {
            if (!(*Seat)[Column]->TryGetNumber(Values[Column]) || !FMath::IsFinite(Values[Column]))
            {
                UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: invalid coordinate in seat %d."), Index);
                return false;
            }
        }
        if ((*Floor)[0]->IsNull() || (*Floor)[1]->IsNull())
        {
            ++Unmeasured;
            continue;
        }
        if (!(*Floor)[0]->TryGetNumber(StandingZ) || !(*Floor)[1]->TryGetNumber(SeatedZ)
            || !FMath::IsFinite(StandingZ) || !FMath::IsFinite(SeatedZ)
            || StandingZ > Values[2] || SeatedZ > Values[2])
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: invalid floor in seat %d."), Index);
            return false;
        }
        LoadedSeats.Emplace(Values[0], Values[1], Values[2], Values[3]);
        LoadedTreads.Emplace(StandingZ, SeatedZ);
    }
    Seats = MoveTemp(LoadedSeats);
    Treads = MoveTemp(LoadedTreads);
    SeatedOffset = SittingOffset;
    SeatedLateralOffset = LateralOffset;
    if (Unmeasured > 0)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Crowd: excluded %d seats without measured treads; no guessed floor heights."), Unmeasured);
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Crowd: %d seats with measured treads from %s."), Seats.Num(), *SeatsFile);
    return Seats.Num() > 0;
}

int32 ULFFSeatSpawnDataGenerator::GetSeatCount() const
{
    LoadSeats();
    return Seats.Num();
}

void ULFFSeatSpawnDataGenerator::CollectStandPoints(UWorld* World, TArray<FTransform>& Near, TArray<FTransform>& Far,
    TArray<FLFFCrowdSeat>& Detailed) const
{
    Near.Reset();
    Far.Reset();
    Detailed.Reset();
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
    for (int32 Index = 0; Index < Seats.Num(); ++Index)
    {
        const FVector4f& Seat = Seats[Index];
        const FRotator Yaw(0.0f, Seat.W, 0.0f);
        const FVector Local = FVector(Seat.X, Seat.Y, Treads[Index].X) + Yaw.Vector() * FrontOffset;
        const FVector Sitting = FVector(Seat.X, Seat.Y, Treads[Index].Y) + Yaw.RotateVector(FVector(SeatedOffset, SeatedLateralOffset, 0.0f));
        const FVector WorldLocation = Site.TransformPosition(Local);
        bool bClear = true;
        for (const FTransform& View : ClearViews)
        {
            if (LFFCrowdRules::IsCameraSpace(WorldLocation, View, ClearRadius, ClearAhead))
            {
                bClear = false;
                break;
            }
        }
        if (!bClear)
        {
            continue;
        }
        double Nearest = TNumericLimits<double>::Max();
        for (const FVector& Centre : MetaHumanCentres)
        {
            Nearest = FMath::Min(Nearest, FVector::DistSquared(Centre, WorldLocation));
        }
        const FQuat Rotation = Site.TransformRotation(Yaw.Quaternion());
        if (bUseDetailedFans && Nearest < FMath::Square(DetailedRadius))
        {
            Detailed.Add({ FTransform(Rotation, WorldLocation), FTransform(Rotation, Site.TransformPosition(Sitting)), Nearest });
        }
        else
        {
            (Nearest < FMath::Square(MetaHumanRadius) ? Near : Far).Emplace(Rotation, WorldLocation);
        }
    }
    Detailed.Sort([](const FLFFCrowdSeat& A, const FLFFCrowdSeat& B) { return A.DistanceSquared < B.DistanceSquared; });
    const int32 Limit = FMath::Clamp(MaxDetailedFans, 0, 96);
    for (int32 Index = Limit; Index < Detailed.Num(); ++Index)
    {
        (Detailed[Index].DistanceSquared < FMath::Square(MetaHumanRadius) ? Near : Far).Add(Detailed[Index].Standing);
    }
    Detailed.SetNum(FMath::Min(Limit, Detailed.Num()));

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
    TArray<FLFFCrowdSeat> Detailed;
    CollectStandPoints(QueryOwner.GetWorld(), Points, Far, Detailed);
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
        TArray<FLFFCrowdSeat> Detailed;
        SeatGenerator->bUseDetailedFans = DetailedLooks.Num() > 0 && !DetailedLooks.ContainsByPredicate([](const FLFFCrowdAppearance& Look)
            { return Look.BodyMesh == nullptr || Look.FaceMesh == nullptr || Look.Outfit.Num() == 0
                || Look.Outfit.ContainsByPredicate([](const FLFFOutfitPiece& Piece) { return Piece.Cloth == nullptr; }); })
            && SeatedIdle != nullptr && SeatedClap != nullptr
            && StandingIdle != nullptr && StandingCheer != nullptr && StandingClap != nullptr;
        if (!SeatGenerator->bUseDetailedFans)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Crowd: detailed looks/clips missing; configure the spawner with configure_stadium_crowd.py."));
        }
        SeatGenerator->CollectStandPoints(GetWorld(), Near, Far, Detailed);
        const LFFCrowdRules::FCounts Counts = LFFCrowdRules::RepresentationCounts(Detailed.Num(), Near.Num(), Far.Num(), Occupancy, MaxPeople);
        Count = Counts.Mass;
        Far.SetNum(Counts.Far - Counts.Overflow);
        for (int32 Index = Count; Index < Count + Counts.Overflow; ++Index)
        {
            Far.Add(Near[Index]);
        }
        BuildBillboards(Far);
        BuildDetailedFans(Detailed, Counts.Detailed);
    }
    Super::BeginPlay();
}

void ALFFStadiumCrowdSpawner::BuildDetailedFans(const TArray<FLFFCrowdSeat>& Points, const int32 FanCount)
{
    for (int32 Index = 0; Index < FanCount; ++Index)
    {
        const FLFFCrowdAppearance& Look = DetailedLooks[Index % DetailedLooks.Num()];
        if (Look.BodyMesh == nullptr || Look.FaceMesh == nullptr || Look.Outfit.Num() == 0)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: invalid detailed appearance %d; configure the spawner before play."), Index % DetailedLooks.Num());
            continue;
        }
        const bool bStanding = Index % 7 == 0;
        const FTransform& Transform = bStanding ? Points[Index].Standing : Points[Index].Seated;
        ALFFCrowdFanActor* Fan = GetWorld()->SpawnActorDeferred<ALFFCrowdFanActor>(ALFFCrowdFanActor::StaticClass(), Transform, this, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Fan == nullptr)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("Crowd: could not spawn detailed fan %d."), Index);
            continue;
        }
        Fan->BodyMesh = Look.BodyMesh;
        Fan->FaceMesh = Look.FaceMesh;
        Fan->HairStyle = Look.HairStyle;
        Fan->Outfit = Look.Outfit;
        Fan->BodyMaterialOverrides = Look.BodyMaterialOverrides;
        Fan->FaceMaterialOverrides = Look.FaceMaterialOverrides;
        Fan->ExposedBodyMaterialSlots = Look.ExposedBodyMaterialSlots;
        Fan->StillAnimation = bStanding ? StandingIdle : SeatedIdle;
        Fan->Gestures = bStanding ? TArray<TObjectPtr<UAnimSequence>>{ StandingCheer, StandingClap }
            : TArray<TObjectPtr<UAnimSequence>>{ SeatedClap };
        Fan->VariationSeed = 1018 + Index * 7919;
        Fan->FinishSpawning(Transform);
        DetailedFans.Add(Fan);
    }
    UE_LOG(LogVirtualStadium, Log, TEXT("Crowd: %d detailed neighbours; independent phases/rates and occasional gestures."), DetailedFans.Num());
}

void ALFFStadiumCrowdSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (ALFFCrowdFanActor* Fan : DetailedFans)
    {
        if (IsValid(Fan))
        {
            Fan->Destroy();
        }
    }
    DetailedFans.Reset();
    Super::EndPlay(EndPlayReason);
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
