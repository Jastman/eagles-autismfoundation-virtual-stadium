#include "World/LFFTileStreamingSubsystem.h"
#include "Cesium3DTileset.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFTileStreamingTest, "LFF.World.TileStreaming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFTileStreamingTest::RunTest(const FString& Parameters)
{
    // Panning must never reveal holes: tiles around the camera stay loaded and loading tiles fall back to their parent.
    const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    ACesium3DTileset* Tileset = World->SpawnActor<ACesium3DTileset>();
    if (!TestNotNull(TEXT("Tileset fixture spawned"), Tileset))
    {
        World->DestroyWorld(false);
        return false;
    }
    ULFFTileStreamingSubsystem::ConfigureTileset(*Tileset);
    TestFalse(TEXT("No frustum culling"), Tileset->EnableFrustumCulling);
    TestTrue(TEXT("Off-screen tiles keep a minimum quality"), Tileset->EnforceCulledScreenSpaceError);
    TestTrue(TEXT("Off-screen error is coarser than on-screen"), Tileset->CulledScreenSpaceError > Tileset->GetMaximumScreenSpaceError());
    TestTrue(TEXT("Holes are forbidden"), Tileset->ForbidHoles);
    TestTrue(TEXT("Cache holds at least 1 GB"), Tileset->MaximumCachedBytes >= 1024ll * 1024 * 1024);
    World->DestroyWorld(false);

    // The status line keeps moving and ends on the cheer.
    const FString First = ULFFTileStreamingSubsystem::GetStatusText(0.0f, 0.0f).ToString();
    TestNotEqual(TEXT("Status changes as loading progresses"), ULFFTileStreamingSubsystem::GetStatusText(60.0f, 0.0f).ToString(), First);
    TestNotEqual(TEXT("Status changes over time even when progress stalls"), ULFFTileStreamingSubsystem::GetStatusText(0.0f, 10.0f).ToString(), First);
    TestEqual(TEXT("Done says Fly, Eagles, Fly!"), ULFFTileStreamingSubsystem::GetStatusText(100.0f, 0.0f).ToString(), FString(TEXT("Fly, Eagles, Fly!")));
    TestFalse(TEXT("Never shows the cheer early"), ULFFTileStreamingSubsystem::GetStatusText(99.9f, 1000.0f).ToString().Contains(TEXT("Fly, Eagles")));
    return true;
}

#endif
