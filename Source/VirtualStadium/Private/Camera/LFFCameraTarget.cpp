#include "Camera/LFFCameraTarget.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

ALFFCameraTarget::ALFFCameraTarget()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
    DirectionArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    if (DirectionArrow != nullptr)
    {
        DirectionArrow->SetupAttachment(SceneRoot);
        DirectionArrow->ArrowSize = 2.0f;
    }
#endif
}

FTransform ALFFCameraTarget::GetCameraTransform() const
{
    FRotator Rotation = GetActorRotation();
    Rotation.Roll = 0.0f;
    return FTransform(Rotation, GetActorLocation());
}
