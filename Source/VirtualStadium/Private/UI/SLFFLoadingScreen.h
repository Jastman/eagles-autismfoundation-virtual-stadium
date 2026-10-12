#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;

/**
 * First-launch cover shown until the stadium's 3D tiles have loaded: the EAF and Five Below logos on Eagles midnight
 * green, a white progress bar, the percentage and a status line. Swallows mouse and keys so nothing starts underneath.
 * Owned by ULFFTileStreamingSubsystem, which feeds it progress.
 */
class SLFFLoadingScreen final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLFFLoadingScreen) {}
        SLATE_ARGUMENT(UTexture2D*, LeftLogo)
        SLATE_ARGUMENT(UTexture2D*, RightLogo)
        SLATE_ARGUMENT(const UObject*, Font)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Percent is 0..100 (already smoothed and monotonic). */
    void SetProgress(float InPercent, const FText& InStatus);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override { return FReply::Handled(); }
    virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }

    static constexpr float BarWidth = 560.0f;
    static constexpr float BarHeight = 10.0f;
    static constexpr float LogoHeight = 150.0f;

private:
    static FSlateBrush MakeLogoBrush(UTexture2D* Texture);
    static FSlateBrush MakeRoundedBrush(const FLinearColor& Color);

    FSlateBrush LeftLogoBrush;
    FSlateBrush RightLogoBrush;
    FSlateBrush BackgroundBrush;
    FSlateBrush TrackBrush;
    FSlateBrush FillBrush;
    FSlateBrush DividerBrush;

    float Percent = 0.0f;
    FText Status;
};
