#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/StadiumScheduleStop.h"
#include "StadiumScheduleDataAsset.generated.h"

UCLASS(BlueprintType)
class VIRTUALSTADIUM_API UStadiumScheduleDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
    TArray<FStadiumScheduleStop> Stops;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    int32 GetStopCount() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FStadiumScheduleStop GetStopAt(int32 Index) const;
};

