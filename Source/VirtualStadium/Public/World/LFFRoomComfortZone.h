#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFFRoomComfortZone.generated.h"

class UBoxComponent;
class UPostProcessComponent;

/** A room-local comfort zone; its measured box drives both postprocess blending and sensory audio attenuation. */
UCLASS()
class VIRTUALSTADIUM_API ALFFRoomComfortZone : public AActor
{
    GENERATED_BODY()

public:
    ALFFRoomComfortZone();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
    TObjectPtr<UBoxComponent> RoomBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
    TObjectPtr<UPostProcessComponent> RoomPostProcess;
};
