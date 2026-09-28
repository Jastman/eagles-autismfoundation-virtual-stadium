#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Data/StadiumScheduleStop.h"
#include "StadiumTourCamera.generated.h"

class AStadiumStopMarker;
class UStadiumScheduleDataAsset;
class UStadiumTourWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTourStopChanged, int32, CurrentIndex, FStadiumScheduleStop, CurrentStop);

UCLASS(Blueprintable)
class VIRTUALSTADIUM_API AStadiumTourCamera : public ACameraActor
{
    GENERATED_BODY()

public:
    AStadiumTourCamera();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
    UStadiumScheduleDataAsset* ScheduleData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
    int32 StartingIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers")
    bool bAutoSpawnMarkers = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers")
    TSubclassOf<AStadiumStopMarker> MarkerClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UStadiumTourWidget> TourWidgetClass;

    UPROPERTY(BlueprintAssignable, Category = "Schedule")
    FOnTourStopChanged OnStopChanged;

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void NextStop();

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void PreviousStop();

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void RestartTour();

    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void GoToStopIndex(int32 NewIndex);

    UFUNCTION(BlueprintPure, Category = "Schedule")
    int32 GetCurrentStopIndex() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    int32 GetStopCount() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FStadiumScheduleStop GetCurrentStop() const;

protected:
    void ApplyStop(int32 NewIndex, bool bAnimate);
    void FinishTransition();
    void SpawnMarkers();
    void BroadcastCurrentStop();

    UPROPERTY(Transient)
    UStadiumTourWidget* TourWidget;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AStadiumStopMarker>> SpawnedMarkers;

    FVector TransitionStartLocation = FVector::ZeroVector;
    FQuat TransitionStartRotation = FQuat::Identity;
    FVector TransitionTargetLocation = FVector::ZeroVector;
    FQuat TransitionTargetRotation = FQuat::Identity;
    float TransitionElapsed = 0.0f;
    float TransitionDuration = 0.0f;
    bool bIsTransitioning = false;
    int32 CurrentStopIndex = INDEX_NONE;
};

