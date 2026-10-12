#include "UI/SLFFLoadingScreen.h"

#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // Eagles midnight green, #004C54.
    const FLinearColor MidnightGreen = FLinearColor::FromSRGBColor(FColor(0x00, 0x4C, 0x54));
}

FSlateBrush SLFFLoadingScreen::MakeLogoBrush(UTexture2D* Texture)
{
    FSlateBrush Brush;
    if (Texture == nullptr)
    {
        Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
        return Brush;
    }
    Brush.SetResourceObject(Texture);
    const float Width = static_cast<float>(FMath::Max(Texture->GetSizeX(), 1));
    const float Height = static_cast<float>(FMath::Max(Texture->GetSizeY(), 1));
    // Same height for both marks; very wide logos are capped so the pair stays centred on a 1280-wide window.
    const float DrawHeight = FMath::Min(LogoHeight, 420.0f * Height / Width);
    Brush.ImageSize = FVector2D(DrawHeight * Width / Height, DrawHeight);
    return Brush;
}

FSlateBrush SLFFLoadingScreen::MakeRoundedBrush(const FLinearColor& Color)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Color);
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
    return Brush;
}

void SLFFLoadingScreen::Construct(const FArguments& InArgs)
{
    LeftLogoBrush = MakeLogoBrush(InArgs._LeftLogo);
    RightLogoBrush = MakeLogoBrush(InArgs._RightLogo);
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
    BackgroundBrush.TintColor = FSlateColor(MidnightGreen);
    TrackBrush = MakeRoundedBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));
    FillBrush = MakeRoundedBrush(FLinearColor::White);
    DividerBrush.DrawAs = ESlateBrushDrawType::Box;
    DividerBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));

    const UObject* Font = InArgs._Font;
    auto MakeFont = [Font](const int32 Size, const TCHAR* Typeface)
    {
        return Font != nullptr ? FSlateFontInfo(Font, Size, FName(Typeface)) : FCoreStyle::GetDefaultFontStyle(FName(Typeface), Size);
    };

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&BackgroundBrush)
        .Padding(0.0f)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SImage).Image(&LeftLogoBrush)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(56.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(2.0f)
                    .HeightOverride(LogoHeight * 0.8f)
                    [
                        SNew(SImage).Image(&DividerBrush)
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SImage).Image(&RightLogoBrush)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 72.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(BarWidth)
                .HeightOverride(BarHeight)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot()
                    [
                        SNew(SImage).Image(&TrackBrush)
                    ]
                    + SOverlay::Slot()
                    .HAlign(HAlign_Left)
                    [
                        SNew(SBox)
                        .WidthOverride_Lambda([this]() { return FOptionalSize(FMath::Max(BarHeight, BarWidth * Percent / 100.0f)); })
                        [
                            SNew(SImage).Image(&FillBrush)
                        ]
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 24.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font(MakeFont(30, TEXT("Bold")))
                .ColorAndOpacity(FLinearColor::White)
                .Text_Lambda([this]() { return FText::AsPercent(FMath::FloorToFloat(Percent) / 100.0f); })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font(MakeFont(20, TEXT("Regular")))
                .ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f))
                .Justification(ETextJustify::Center)
                .Text_Lambda([this]() { return Status; })
            ]
        ]
    ];
}

void SLFFLoadingScreen::SetProgress(const float InPercent, const FText& InStatus)
{
    Percent = FMath::Clamp(InPercent, 0.0f, 100.0f);
    Status = InStatus;
}
