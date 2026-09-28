#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StadiumScheduleStop.h"
#include "StadiumStopMarker.generated.h"

class UBillboardComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class VIRTUALSTADIUM_API AStadiumStopMarker : public AActor
{
    GENERATED_BODY()

public:
    AStadiumStopMarker();

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void ConfigureFromStop(const FStadiumScheduleStop& Stop);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBillboardComponent* BillboardComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UTextRenderComponent* LabelComponent;
};

