#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/StadiumScheduleStop.h"
#include "StadiumTourWidget.generated.h"

class AStadiumTourCamera;
class UButton;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class VIRTUALSTADIUM_API UStadiumTourWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Schedule")
    void InitializeFromTour(AStadiumTourCamera* InTourCamera);

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FText GetCurrentTitle() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FText GetCurrentDescription() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FText GetProgressText() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    float GetProgressFraction() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    FStadiumScheduleStop GetCurrentStop() const;

    UFUNCTION(BlueprintPure, Category = "Schedule")
    TSoftObjectPtr<UTexture2D> GetCurrentPreviewImage() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandlePreviousClicked();

    UFUNCTION()
    void HandleNextClicked();

    UFUNCTION()
    void HandleRestartClicked();

    UFUNCTION()
    void HandleStopChanged(int32 NewIndex, FStadiumScheduleStop NewStop);

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* PreviousButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* NextButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* RestartButton;

    UPROPERTY(BlueprintReadOnly, Category = "Schedule")
    TObjectPtr<AStadiumTourCamera> TourCamera;

    UPROPERTY(BlueprintReadOnly, Category = "Schedule")
    FStadiumScheduleStop CachedStop;

    UPROPERTY(BlueprintReadOnly, Category = "Schedule")
    int32 CachedIndex = 0;
};
