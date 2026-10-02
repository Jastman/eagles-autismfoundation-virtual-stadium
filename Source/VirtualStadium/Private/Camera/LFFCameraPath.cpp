#include "Camera/LFFCameraPath.h"

#include "Components/SplineComponent.h"

ALFFCameraPath::ALFFCameraPath()
{
    PrimaryActorTick.bCanEverTick = false;

    Preview = CreateDefaultSubobject<USplineComponent>(TEXT("Preview"));
    SetRootComponent(Preview);
    Preview->SetMobility(EComponentMobility::Static);
    Preview->bDrawDebug = true;
    Preview->SetHiddenInGame(true);
    SetActorHiddenInGame(true);
}

void ALFFCameraPath::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (Preview != nullptr)
    {
        Preview->SetSplinePoints(Waypoints, ESplineCoordinateSpace::World, true);
    }
}
