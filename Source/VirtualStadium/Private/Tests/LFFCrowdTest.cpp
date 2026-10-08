#include "World/LFFStadiumCrowdSpawner.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdRulesTest, "LFF.Crowd.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdRulesTest::RunTest(const FString& Parameters)
{
    const FTransform Camera(FRotator(-6.0f, 0.0f, 0.0f), FVector(0.0, 0.0, 270.0));
    TestTrue(TEXT("Only the visitor's immediate space is cleared"),
        LFFCrowdRules::IsCameraSpace(FVector(20.0, 0.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("The next row remains populated"), LFFCrowdRules::IsCameraSpace(FVector(92.0, 0.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("No large empty cone in front"), LFFCrowdRules::IsCameraSpace(FVector(500.0, 20.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("Seats beside the visitor remain populated"), LFFCrowdRules::IsCameraSpace(FVector(0.0, 62.0, 0.0), Camera, 50.0f, 0.0f));
    const LFFCrowdRules::FCounts Counts = LFFCrowdRules::RepresentationCounts(48, 1000, 10000, 0.7f, 300);
    TestEqual(TEXT("Detailed fans are bounded"), Counts.Detailed, 34);
    TestEqual(TEXT("Mass fans respect the cap"), Counts.Mass, 300);
    TestEqual(TEXT("Capped near fans are not silently discarded"), Counts.Overflow, 400);
    TestEqual(TEXT("Overflow keeps its occupied seat as a billboard"), Counts.Far, 7400);
    const auto Empty = LFFCrowdRules::RepresentationCounts(48, 1000, 10000, 0.0f, 300);
    TestEqual(TEXT("Zero occupancy makes every representation empty"), Empty.Detailed + Empty.Mass + Empty.Far, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdTreadsTest, "LFF.Crowd.MeasuredTreads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdTreadsTest::RunTest(const FString& Parameters)
{
    FString SeatText, TreadText;
    TArray<uint8> Bytes;
    TSharedPtr<FJsonObject> Seats, Treads;
    const FString Folder = FPaths::ProjectContentDir() / TEXT("LFF/Crowd");
    if (!TestTrue(TEXT("Seat geometry and measured treads read"),
        FFileHelper::LoadFileToString(SeatText, *(Folder / TEXT("seats.json")))
        && FFileHelper::LoadFileToString(TreadText, *(Folder / TEXT("seat_treads.json")))
        && FFileHelper::LoadFileToArray(Bytes, *(Folder / TEXT("seats.json")))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SeatText), Seats)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TreadText), Treads)))
    {
        return false;
    }
    TestTrue(TEXT("Sidecar matches the exact seat file"),
        Treads->GetStringField(TEXT("seats_sha1")).Equals(FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString(), ESearchCase::IgnoreCase));
    const auto& Chairs = Seats->GetArrayField(TEXT("seats"));
    const auto& Floors = Treads->GetArrayField(TEXT("tread_z_cm"));
    if (!TestEqual(TEXT("One tread row per source seat"), Floors.Num(), Chairs.Num()))
    {
        return false;
    }
    int32 Measured = 0;
    for (int32 Index = 0; Index < Floors.Num(); ++Index)
    {
        const auto& Floor = Floors[Index]->AsArray();
        if (Floor[0]->IsNull() || Floor[1]->IsNull())
        {
            continue;
        }
        const double ChairZ = Chairs[Index]->AsArray()[2]->AsNumber();
        for (const auto& Z : Floor)
        {
            TestTrue(TEXT("Floor is measured below the chair, within trace depth"), Z->AsNumber() < ChairZ && ChairZ - Z->AsNumber() <= 151.0);
        }
        ++Measured;
    }
    TestTrue(TEXT("At least 95 percent have measured floors"), Measured >= Chairs.Num() * 0.95);
    ULFFSeatSpawnDataGenerator* Generator = NewObject<ULFFSeatSpawnDataGenerator>();
    TestEqual(TEXT("Runtime validates and consumes all measured seats"), Generator->GetSeatCount(), Measured);
    return true;
}

#endif
