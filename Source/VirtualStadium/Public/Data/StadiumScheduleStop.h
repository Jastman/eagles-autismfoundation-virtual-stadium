#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "StadiumScheduleStop.generated.h"

class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct VIRTUALSTADIUM_API FStadiumScheduleStop : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FName StopId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule")
    FRotator CameraRotation = FRotator(-12.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule", meta = (ClampMin = "0.0"))
    float FlyToDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    TSoftObjectPtr<UTexture2D> PreviewImage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
    bool bShowBillboard = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
    FString PlaceholderLabel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
    TSoftObjectPtr<UTexture2D> BillboardSprite;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
    TSoftObjectPtr<UStaticMesh> PlaceholderMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
    FVector PlaceholderScale = FVector(1.0f, 1.0f, 1.0f);
};

