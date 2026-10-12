#pragma once

#include "CoreMinimal.h"

/** Layout values are logical UMG units, after Unreal's viewport DPI scaling. */
namespace LFFUILayout
{
    struct VIRTUALSTADIUM_API FCornerPanels
    {
        float InformationWidth = 0.0f;
        float ControlWidth = 0.0f;
        float MaximumHeight = 0.0f;
        float NarrativeHeight = 0.0f;
        float CaptionWidth = 0.0f;
        float EdgeInset = 24.0f;
    };

    VIRTUALSTADIUM_API float ViewportInset(const FVector2D& Viewport);
    VIRTUALSTADIUM_API float WelcomeBottomInset(float ViewportWidth);
    VIRTUALSTADIUM_API FCornerPanels CornerPanels(const FVector2D& Viewport, bool bLargeText);
    VIRTUALSTADIUM_API int32 TimelineColumns(float AvailableWidth, float MarkWidth, float Gap);
    VIRTUALSTADIUM_API float TimelineMarkWidth(float AvailableWidth, float MinimumWidth, float Gap);
    VIRTUALSTADIUM_API FBox2D TicketBounds(const FVector2D& Viewport, const FBox2D& Information,
        const FBox2D& Controls, const FVector2D& NaturalSize);
}
