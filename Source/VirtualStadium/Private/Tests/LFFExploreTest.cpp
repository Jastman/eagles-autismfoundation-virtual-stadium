#include "Core/LFFGameMode.h"
#include "Dom/JsonObject.h"
#include "Journey/LFFJourneyStateMachine.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFExploreRulesTest, "LFF.Explore.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFExploreRulesTest::RunTest(const FString& Parameters)
{
    using namespace LFFJourneyRules;

    // Free flight stays in the site's flyable space.
    const FVector Inside(1000.0, -2000.0, 500.0);
    TestTrue(TEXT("Inside the space is left alone"), ClampFlyPosition(Inside, 60000.0f, 150.0f, 30000.0f).Equals(Inside));
    const FVector Far = ClampFlyPosition(FVector(90000.0, 0.0, 500.0), 60000.0f, 150.0f, 30000.0f);
    TestTrue(TEXT("Too far out comes back to the edge"), FMath::IsNearlyEqual(Far.X, 60000.0, 0.01) && FMath::IsNearlyEqual(Far.Y, 0.0, 0.01));
    TestEqual(TEXT("Below the floor comes up to it"), ClampFlyPosition(FVector(0.0, 0.0, -400.0), 60000.0f, 150.0f, 30000.0f).Z, 150.0);
    TestEqual(TEXT("Above the ceiling comes down to it"), ClampFlyPosition(FVector(0.0, 0.0, 99999.0), 60000.0f, 150.0f, 30000.0f).Z, 30000.0);

    // Each level's overview sits above its floor and looks straight at the floor's centre.
    const ALFFGameMode* Defaults = GetDefault<ALFFGameMode>();
    const TArray<FLFFLevelView>& Views = Defaults->LevelViews;
    TestEqual(TEXT("One view per level"), Views.Num(), LFFMapLevelCount);
    for (int32 Index = 0; Index < Views.Num(); ++Index)
    {
        const FLFFLevelView& View = Views[Index];
        const FTransform Pose = LevelOverviewPose(View);
        const FVector Target(0.0, 0.0, View.FloorZ);
        TestTrue(FString::Printf(TEXT("Level %d: the overview is above its floor"), Index), Pose.GetLocation().Z > View.FloorZ);
        TestTrue(FString::Printf(TEXT("Level %d: it looks at the floor's centre"), Index),
            (Pose.GetLocation() + Pose.GetRotation().Vector() * View.ViewDistance).Equals(Target, 1.0));
        TestTrue(FString::Printf(TEXT("Level %d: no roll"), Index), FMath::IsNearlyZero(Pose.Rotator().Roll, 0.01f));
        TestTrue(FString::Printf(TEXT("Level %d: its floor is in its band"), Index), View.FloorZ >= View.MinZ && View.FloorZ <= View.MaxZ);
        if (Index > 0)
        {
            TestEqual(FString::Printf(TEXT("Level %d: its band starts where the one below ends"), Index), View.MinZ, Views[Index - 1].MaxZ);
        }
    }

    // Assert the actual perspective projection, not a cone plus an approximate sidebar yaw.
    {
        const TArray<FVector> Spread = { FVector(-9168.0, 6000.0, 3470.0), FVector(8248.0, 7995.0, 3470.0), FVector(-7416.0, -6290.0, 310.0),
            FVector(-8003.0, 4251.0, 310.0) };
        TArray<FVector> Anchors = Spread;
        for (const FVector& Point : Spread)
        {
            Anchors.Add(Point + FVector(0.0, 0.0, 2500.0));
        }
        for (const FVector2D Size : { FVector2D(1920, 1080), FVector2D(1280, 720), FVector2D(2560, 1080), FVector2D(900, 1000) })
        {
            const TArray<FBox2D> Panels = {
                FBox2D(FVector2D(24, 24), FVector2D(Size.X * 0.30, 120)),
                FBox2D(FVector2D(Size.X * 0.65, 24), FVector2D(Size.X - 24, Size.Y * 0.72)) };
            const FBox2D Pixels = UnobstructedViewport(Size, Panels, 48.0f, 112.0f);
            TestTrue(TEXT("Corner panels leave a real safe area"), Pixels.bIsValid && Pixels.GetArea() > 0.0);
            const FBox2D Safe(Pixels.Min / Size, Pixels.Max / Size);
            const float Aspect = Size.X / Size.Y;
            for (const float Yaw : { 20.0f, -140.0f })
            {
                const FTransform Frame = FramePointsPose(Anchors, -50.0f, Yaw, 60.0f, Aspect, Safe, 400.0f, 6000.0f);
                const double TanH = FMath::Tan(FMath::DegreesToRadians(30.0));
                for (const FVector& Point : Anchors)
                {
                    const FVector Camera = Frame.InverseTransformPositionNoScale(Point);
                    TestTrue(TEXT("Ground and stem are in front"), Camera.X > 0.0);
                    const FVector2D Projected(0.5 + Camera.Y / (2.0 * Camera.X * TanH),
                        0.5 - Camera.Z * Aspect / (2.0 * Camera.X * TanH));
                    TestTrue(FString::Printf(TEXT("%.0fx%.0f: ground/head project inside unobstructed HUD area"), Size.X, Size.Y),
                        Safe.IsInsideOrOn(Projected));
                }
            }
        }
        const FVector One(100.0, 200.0, 300.0);
        const FBox2D Full(FVector2D(0, 0), FVector2D(1, 1));
        TestTrue(TEXT("One place uses the minimum distance"), FMath::IsNearlyEqual(
            FVector::Dist(FramePointsPose({ One }, -50.0f, 20.0f, 60.0f, 16.0f / 9.0f, Full, 0.0f, 6000.0f).GetLocation(), One), 6000.0, 0.01));
        const FVector2D Window(1280, 720);
        TestFalse(TEXT("A fully covered viewport has no fictional safe area"),
            UnobstructedViewport(Window, { FBox2D(FVector2D::ZeroVector, Window) }, 48.0f, 112.0f).bIsValid);
    }

    // Every place in the data sits in its own level's band, so it's highlighted with its level and found where it's listed.
    FString Json;
    TArray<TSharedPtr<FJsonValue>> Rows;
    const FString Path = FPaths::ProjectDir() / TEXT("SourceData/Journey/LFF_Amenities.json");
    if (!TestTrue(TEXT("LFF_Amenities.json reads"), FFileHelper::LoadFileToString(Json, *Path)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Rows)))
    {
        return false;
    }
    const UEnum* Levels = StaticEnum<ELFFMapLevel>();
    TSet<FString> Names;
    for (const TSharedPtr<FJsonValue>& Value : Rows)
    {
        const TSharedPtr<FJsonObject> Row = Value->AsObject();
        const FString Name = Row->GetStringField(TEXT("Name"));
        Names.Add(Name);
        const int32 Level = static_cast<int32>(Levels->GetValueByNameString(Row->GetStringField(TEXT("Level"))));
        const double Z = Row->GetObjectField(TEXT("Location"))->GetNumberField(TEXT("Z"));
        if (TestTrue(FString::Printf(TEXT("%s names a level"), *Name), Views.IsValidIndex(Level)))
        {
            TestTrue(FString::Printf(TEXT("%s is inside its level's band"), *Name), Z >= Views[Level].MinZ && Z <= Views[Level].MaxZ);
        }
    }

    // Every place a tour step names on the model is a place in the data, so a step never points at nothing.
    FString StepsJson;
    TArray<TSharedPtr<FJsonValue>> Steps;
    const FString StepsPath = FPaths::ProjectDir() / TEXT("SourceData/Journey/LFF_GoldenPath.json");
    if (!TestTrue(TEXT("LFF_GoldenPath.json reads"), FFileHelper::LoadFileToString(StepsJson, *StepsPath)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(StepsJson), Steps)))
    {
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : Steps)
    {
        const TSharedPtr<FJsonObject> Step = Value->AsObject();
        const TArray<TSharedPtr<FJsonValue>>* Places = nullptr;
        if (Step->TryGetArrayField(TEXT("PlacesToShow"), Places))
        {
            for (const TSharedPtr<FJsonValue>& Place : *Places)
            {
                TestTrue(FString::Printf(TEXT("Step %s shows %s, a place in LFF_Amenities.json"), *Step->GetStringField(TEXT("Name")), *Place->AsString()),
                    Names.Contains(Place->AsString()));
            }
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
