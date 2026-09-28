#include "Tour/StadiumTourCamera.h"

#include "Blueprint/UserWidget.h"
#include "Data/StadiumScheduleDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tour/StadiumStopMarker.h"
#include "UI/StadiumTourWidget.h"

AStadiumTourCamera::AStadiumTourCamera()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStadiumTourCamera::BeginPlay()
{
    Super::BeginPlay();

    if (GetStopCount() > 0)
    {
        CurrentStopIndex = FMath::Clamp(StartingIndex, 0, GetStopCount() - 1);
        ApplyStop(CurrentStopIndex, false);

        if (bAutoSpawnMarkers)
        {
            SpawnMarkers();
        }
    }

    if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
    {
        PlayerController->SetViewTarget(this);
        EnableInput(PlayerController);

        if (InputComponent != nullptr)
        {
            InputComponent->BindAction(TEXT("TourPrevious"), IE_Pressed, this, &AStadiumTourCamera::PreviousStop);
            InputComponent->BindAction(TEXT("TourNext"), IE_Pressed, this, &AStadiumTourCamera::NextStop);
            InputComponent->BindAction(TEXT("TourRestart"), IE_Pressed, this, &AStadiumTourCamera::RestartTour);
        }

        if (TourWidgetClass != nullptr)
        {
            TourWidget = CreateWidget<UStadiumTourWidget>(PlayerController, TourWidgetClass);
            if (TourWidget != nullptr)
            {
                TourWidget->AddToViewport();
                TourWidget->InitializeFromTour(this);
            }
        }
    }
}

void AStadiumTourCamera::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bIsTransitioning)
    {
        return;
    }

    TransitionElapsed += DeltaSeconds;
    const float Alpha = TransitionDuration <= KINDA_SMALL_NUMBER ? 1.0f : FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);
    const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

    SetActorLocationAndRotation(
        FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, EasedAlpha),
        FQuat::Slerp(TransitionStartRotation, TransitionTargetRotation, EasedAlpha)
    );

    if (Alpha >= 1.0f)
    {
        FinishTransition();
    }
}

void AStadiumTourCamera::NextStop()
{
    if (GetStopCount() > 0)
    {
        ApplyStop((CurrentStopIndex + 1) % GetStopCount(), true);
    }
}

void AStadiumTourCamera::PreviousStop()
{
    if (GetStopCount() > 0)
    {
        ApplyStop((CurrentStopIndex - 1 + GetStopCount()) % GetStopCount(), true);
    }
}

void AStadiumTourCamera::RestartTour()
{
    if (GetStopCount() > 0)
    {
        ApplyStop(0, true);
    }
}

void AStadiumTourCamera::GoToStopIndex(const int32 NewIndex)
{
    ApplyStop(NewIndex, true);
}

int32 AStadiumTourCamera::GetCurrentStopIndex() const
{
    return CurrentStopIndex;
}

int32 AStadiumTourCamera::GetStopCount() const
{
    return ScheduleData != nullptr ? ScheduleData->GetStopCount() : 0;
}

FStadiumScheduleStop AStadiumTourCamera::GetCurrentStop() const
{
    return ScheduleData != nullptr ? ScheduleData->GetStopAt(CurrentStopIndex) : FStadiumScheduleStop();
}

void AStadiumTourCamera::ApplyStop(const int32 NewIndex, const bool bAnimate)
{
    if (ScheduleData == nullptr || !ScheduleData->Stops.IsValidIndex(NewIndex))
    {
        return;
    }

    const FStadiumScheduleStop& Stop = ScheduleData->Stops[NewIndex];
    CurrentStopIndex = NewIndex;

    FTransform CurrentTransform = GetActorTransform();
    if (bIsTransitioning)
    {
        const float Alpha = TransitionDuration <= KINDA_SMALL_NUMBER ? 1.0f : FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);
        const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
        const FVector CurrentLocation = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, EasedAlpha);
        const FQuat CurrentRotation = FQuat::Slerp(TransitionStartRotation, TransitionTargetRotation, EasedAlpha);

        SetActorLocationAndRotation(CurrentLocation, CurrentRotation);
        CurrentTransform = FTransform(CurrentRotation, CurrentLocation);
    }

    const FTransform TargetTransform(Stop.CameraRotation, Stop.WorldLocation);

    if (!bAnimate)
    {
        SetActorLocationAndRotation(TargetTransform.GetLocation(), TargetTransform.GetRotation());
        bIsTransitioning = false;
        BroadcastCurrentStop();
        return;
    }

    TransitionStartLocation = CurrentTransform.GetLocation();
    TransitionStartRotation = CurrentTransform.GetRotation();
    TransitionTargetLocation = TargetTransform.GetLocation();
    TransitionTargetRotation = TargetTransform.GetRotation();
    TransitionElapsed = 0.0f;
    TransitionDuration = FMath::Max(Stop.FlyToDuration, 0.01f);
    bIsTransitioning = true;
    BroadcastCurrentStop();
}

void AStadiumTourCamera::FinishTransition()
{
    SetActorLocationAndRotation(TransitionTargetLocation, TransitionTargetRotation);
    bIsTransitioning = false;
}

void AStadiumTourCamera::SpawnMarkers()
{
    if (MarkerClass == nullptr || ScheduleData == nullptr)
    {
        return;
    }

    for (AStadiumStopMarker* ExistingMarker : SpawnedMarkers)
    {
        if (ExistingMarker != nullptr)
        {
            ExistingMarker->Destroy();
        }
    }
    SpawnedMarkers.Reset();

    for (const FStadiumScheduleStop& Stop : ScheduleData->Stops)
    {
        AStadiumStopMarker* Marker = GetWorld()->SpawnActor<AStadiumStopMarker>(MarkerClass, Stop.WorldLocation, FRotator::ZeroRotator);
        if (Marker != nullptr)
        {
            Marker->ConfigureFromStop(Stop);
            SpawnedMarkers.Add(Marker);
        }
    }
}

void AStadiumTourCamera::BroadcastCurrentStop()
{
    if (ScheduleData != nullptr && ScheduleData->Stops.IsValidIndex(CurrentStopIndex))
    {
        OnStopChanged.Broadcast(CurrentStopIndex, ScheduleData->Stops[CurrentStopIndex]);
    }
}
