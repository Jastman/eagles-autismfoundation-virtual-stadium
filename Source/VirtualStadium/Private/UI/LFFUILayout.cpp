#include "UI/LFFUILayout.h"

float LFFUILayout::ViewportInset(const FVector2D& Viewport)
{
    return FMath::Clamp(static_cast<float>(FMath::Min(Viewport.X, Viewport.Y) / 45.0), 12.0f, 24.0f);
}

float LFFUILayout::WelcomeBottomInset(const float ViewportWidth)
{
    // Keep the full narrow-window Settings clearance until the cards fit, then release the extra space gradually.
    return FMath::Lerp(208.0f, 112.0f, FMath::Clamp((ViewportWidth - 1460.0f) / 200.0f, 0.0f, 1.0f));
}

LFFUILayout::FCornerPanels LFFUILayout::CornerPanels(const FVector2D& Viewport, const bool bLargeText)
{
    FCornerPanels Result;
    Result.EdgeInset = ViewportInset(Viewport);
    const float CaptionScale = bLargeText ? 1.25f : 1.0f;
    Result.InformationWidth = FMath::Min(480.0f * CaptionScale, static_cast<float>(Viewport.X * 0.43));
    Result.ControlWidth = FMath::Min(540.0f, static_cast<float>(Viewport.X * 0.43));
    Result.MaximumHeight = FMath::Max(64.0f, static_cast<float>(Viewport.Y) - Result.EdgeInset * 2.0f - 64.0f);
    Result.NarrativeHeight = FMath::Max(64.0f, static_cast<float>(Viewport.Y) - Result.EdgeInset * 2.0f - 180.0f);
    Result.CaptionWidth = FMath::Max(1.0f, (Result.InformationWidth - 40.0f) / CaptionScale);
    return Result;
}

float LFFUILayout::TimelineMarkWidth(const float AvailableWidth, const float MinimumWidth, const float Gap)
{
    const int32 Columns = TimelineColumns(AvailableWidth, MinimumWidth, Gap);
    // Leave a subpixel rounding allowance so Slate never sends an exactly fitted final mark to another row.
    return FMath::Max(1.0f, (AvailableWidth - FMath::Max(0.0f, Gap) * (Columns - 1) - 0.01f) / Columns);
}

int32 LFFUILayout::TimelineColumns(const float AvailableWidth, const float MarkWidth, const float Gap)
{
    return FMath::Max(1, FMath::FloorToInt((FMath::Max(0.0f, AvailableWidth) + FMath::Max(0.0f, Gap))
        / FMath::Max(1.0f, MarkWidth + FMath::Max(0.0f, Gap))));
}

FBox2D LFFUILayout::TicketBounds(const FVector2D& Viewport, const FBox2D& Information,
    const FBox2D& Controls, const FVector2D& NaturalSize)
{
    const double Inset = ViewportInset(Viewport);
    if (Viewport.X <= Inset * 2.0 || Viewport.Y <= Inset * 2.0 || NaturalSize.X <= 0.0 || NaturalSize.Y <= 0.0)
    {
        return FBox2D(ForceInit);
    }

    const double Left = FMath::Max(Viewport.X * 0.5, Information.bIsValid ? Information.Max.X + Inset : Inset);
    const FBox2D Beside(FVector2D(Left, Inset),
        FVector2D(Controls.bIsValid ? Controls.Min.X - Inset : Viewport.X - Inset, Viewport.Y - Inset));
    const FBox2D Below(FVector2D(Left, Controls.bIsValid ? Controls.Max.Y + Inset : Inset),
        Viewport - FVector2D(Inset, Inset));

    auto FitScale = [&NaturalSize](const FBox2D& Area)
    {
        const FVector2D Size = Area.Max - Area.Min;
        return FMath::Max(0.0, FMath::Min3(1.0, Size.X / NaturalSize.X, Size.Y / NaturalSize.Y));
    };
    const double BesideScale = FitScale(Beside);
    const double BelowScale = FitScale(Below);
    double Scale = BesideScale;
    double BelowTravel = 0.0;
    double RightTravel = 0.0;
    if (BelowScale > 0.0 && BesideScale < BelowScale)
    {
        // As the lane narrows, move down while still beside the dock, then grow into the space below it.
        // This rounds the dock's corner without a layout-mode jump or passing through its controls.
        BelowTravel = FMath::Clamp(2.0 * (1.0 - BesideScale / BelowScale), 0.0, 1.0);
        if (BesideScale < BelowScale * 0.5)
        {
            Scale = BelowScale - BesideScale;
            RightTravel = 1.0 - 2.0 * BesideScale / BelowScale;
        }
    }
    if (Scale <= 0.0)
    {
        return FBox2D(ForceInit);
    }
    const FVector2D Size = NaturalSize * Scale;
    const double BesideY = Beside.Min.Y + (Beside.Max.Y - Beside.Min.Y - Size.Y) * 0.5;
    const double BelowY = Below.Min.Y + (Below.Max.Y - Below.Min.Y - Size.Y) * 0.5;
    const double X = RightTravel > 0.0
        ? Left + RightTravel * (Below.Max.X - Left - Size.X)
        : Beside.Max.X - Size.X;
    const FVector2D Position(X, FMath::Lerp(BesideY, BelowY, BelowTravel));
    return FBox2D(Position, Position + Size);
}
