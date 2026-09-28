#include "UI/StadiumTourWidget.h"

#include "Components/Button.h"
#include "Tour/StadiumTourCamera.h"

void UStadiumTourWidget::InitializeFromTour(AStadiumTourCamera* InTourCamera)
{
    TourCamera = InTourCamera;

    if (TourCamera != nullptr)
    {
        TourCamera->OnStopChanged.AddDynamic(this, &UStadiumTourWidget::HandleStopChanged);
        CachedIndex = TourCamera->GetCurrentStopIndex();
        CachedStop = TourCamera->GetCurrentStop();
    }
}

FText UStadiumTourWidget::GetCurrentTitle() const
{
    return CachedStop.Title;
}

FText UStadiumTourWidget::GetCurrentDescription() const
{
    return CachedStop.Description;
}

FText UStadiumTourWidget::GetProgressText() const
{
    if (TourCamera == nullptr)
    {
        return FText::FromString(TEXT("Stop 0 of 0"));
    }

    const int32 StopCount = TourCamera->GetStopCount();
    if (StopCount <= 0)
    {
        return FText::FromString(TEXT("Stop 0 of 0"));
    }

    return FText::Format(
        NSLOCTEXT("VirtualStadium", "TourProgress", "Stop {0} of {1}"),
        FText::AsNumber(FMath::Max(CachedIndex + 1, 1)),
        FText::AsNumber(StopCount)
    );
}

float UStadiumTourWidget::GetProgressFraction() const
{
    if (TourCamera == nullptr || TourCamera->GetStopCount() <= 0 || CachedIndex < 0)
    {
        return 0.0f;
    }

    return static_cast<float>(CachedIndex + 1) / static_cast<float>(TourCamera->GetStopCount());
}

FStadiumScheduleStop UStadiumTourWidget::GetCurrentStop() const
{
    return CachedStop;
}

TSoftObjectPtr<UTexture2D> UStadiumTourWidget::GetCurrentPreviewImage() const
{
    return CachedStop.PreviewImage;
}

void UStadiumTourWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (PreviousButton != nullptr)
    {
        PreviousButton->OnClicked.AddDynamic(this, &UStadiumTourWidget::HandlePreviousClicked);
    }

    if (NextButton != nullptr)
    {
        NextButton->OnClicked.AddDynamic(this, &UStadiumTourWidget::HandleNextClicked);
    }

    if (RestartButton != nullptr)
    {
        RestartButton->OnClicked.AddDynamic(this, &UStadiumTourWidget::HandleRestartClicked);
    }
}

void UStadiumTourWidget::HandlePreviousClicked()
{
    if (TourCamera != nullptr)
    {
        TourCamera->PreviousStop();
    }
}

void UStadiumTourWidget::HandleNextClicked()
{
    if (TourCamera != nullptr)
    {
        TourCamera->NextStop();
    }
}

void UStadiumTourWidget::HandleRestartClicked()
{
    if (TourCamera != nullptr)
    {
        TourCamera->RestartTour();
    }
}

void UStadiumTourWidget::HandleStopChanged(const int32 NewIndex, const FStadiumScheduleStop NewStop)
{
    CachedIndex = NewIndex;
    CachedStop = NewStop;
}
