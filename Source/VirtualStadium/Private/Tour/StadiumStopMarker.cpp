#include "Tour/StadiumStopMarker.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

AStadiumStopMarker::AStadiumStopMarker()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    BillboardComponent->SetupAttachment(SceneRoot);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
    MeshComponent->SetupAttachment(SceneRoot);

    LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    LabelComponent->SetupAttachment(SceneRoot);
    LabelComponent->SetHorizontalAlignment(EHTA_Center);
    LabelComponent->SetWorldSize(64.0f);
    LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
}

void AStadiumStopMarker::ConfigureFromStop(const FStadiumScheduleStop& Stop)
{
    SetActorLocation(Stop.WorldLocation);
    LabelComponent->SetText(FText::FromString(Stop.PlaceholderLabel.IsEmpty() ? Stop.Title.ToString() : Stop.PlaceholderLabel));

    BillboardComponent->SetHiddenInGame(!Stop.bShowBillboard);
    if (UTexture2D* Sprite = Stop.BillboardSprite.LoadSynchronous())
    {
        BillboardComponent->SetSprite(Sprite);
    }

    if (UStaticMesh* PlaceholderMesh = Stop.PlaceholderMesh.LoadSynchronous())
    {
        MeshComponent->SetStaticMesh(PlaceholderMesh);
        MeshComponent->SetVisibility(true);
        MeshComponent->SetWorldScale3D(Stop.PlaceholderScale);
    }
    else
    {
        MeshComponent->SetVisibility(false);
    }
}

