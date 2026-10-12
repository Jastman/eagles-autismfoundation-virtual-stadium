#include "World/LFFRoomComfortZone.h"

#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"

ALFFRoomComfortZone::ALFFRoomComfortZone()
{
    PrimaryActorTick.bCanEverTick = false;
    RoomBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBounds"));
    RoomBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RoomBounds->SetGenerateOverlapEvents(false);
    RoomBounds->SetBoxExtent(FVector(1.0));
    RootComponent = RoomBounds;

    RoomPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("RoomPostProcess"));
    RoomPostProcess->SetupAttachment(RoomBounds);
    RoomPostProcess->bUnbound = false;
    RoomPostProcess->Priority = 5.0f;
    RoomPostProcess->BlendRadius = 80.0f;
    RoomPostProcess->BlendWeight = 1.0f;
    FPostProcessSettings& Settings = RoomPostProcess->Settings;
    Settings.bOverride_AutoExposureSpeedUp = true;
    Settings.bOverride_AutoExposureSpeedDown = true;
    Settings.AutoExposureSpeedUp = 6.0f;
    Settings.AutoExposureSpeedDown = 6.0f;
    // Only neutralize the local cast, not contrast, luminance range, exposure bias or the stadium's global rendering.
    Settings.bOverride_WhiteTint = true;
    Settings.WhiteTint = 0.0f;
    Settings.bOverride_SceneColorTint = true;
    Settings.SceneColorTint = FLinearColor::White;
}
