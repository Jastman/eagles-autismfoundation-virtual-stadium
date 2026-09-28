#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFFCameraTarget.generated.h"

class UArrowComponent;

/**
 * A named camera pose placed in the level, attached under LFF_SiteRoot (ADR-0001).
 * Journey steps reference it by TargetId, never by coordinates. Never ticks.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFCameraTarget : public AActor
{
    GENERATED_BODY()

public:
    ALFFCameraTarget();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Journey")
    FName TargetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey", meta = (ClampMin = "20.0", ClampMax = "120.0"))
    float FieldOfView = 70.0f;

    /** World pose with roll forced to zero (no camera roll, ACCESSIBILITY.md). */
    FTransform GetCameraTransform() const;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UArrowComponent> DirectionArrow;
#endif
};
