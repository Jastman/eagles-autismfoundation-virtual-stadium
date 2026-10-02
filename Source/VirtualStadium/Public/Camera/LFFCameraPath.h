#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFFCameraPath.generated.h"

class USplineComponent;

/**
 * An authored camera route between two steps' targets, used instead of a straight Ease when the journey moves from
 * FromTargetId to ToTargetId. The camera starts where it is, follows the waypoints (world space, cm) and either settles
 * on the target or, with bCutAtEnd, fades and cuts to it (the route stays outside and the target is behind a wall).
 * The route is checked by hand against the stadium model, which has no collision. Calm Mode cuts; less movement fades.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFCameraPath : public AActor
{
    GENERATED_BODY()

public:
    ALFFCameraPath();

    virtual void OnConstruction(const FTransform& Transform) override;

    /** The target the camera leaves from. None: any. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Journey")
    FName FromTargetId = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Journey")
    FName ToTargetId = NAME_None;

    /** World-space points after the camera's own start, in order. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    TArray<FVector> Waypoints;

    /** Whole route, start to target. Slow on purpose: about walking pace or a gentle drone. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey", meta = (ClampMin = "2.0", ClampMax = "20.0"))
    float TravelSeconds = 8.0f;

    /** Fade and cut from the last waypoint to the target instead of flying the last stretch. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journey")
    bool bCutAtEnd = false;

protected:
    /** Editor preview of the route only; the rig builds its own from the live camera position. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USplineComponent> Preview;
};
