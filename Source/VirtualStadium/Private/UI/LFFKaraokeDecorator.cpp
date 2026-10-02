#include "UI/LFFKaraokeDecorator.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/RichTextBlock.h"
#include "Framework/Text/SlateTextRun.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    /** The word as plain text, with the highlight painted around it and the underline across its foot. */
    class SLFFKaraokeWord : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SLFFKaraokeWord) {}
            SLATE_DEFAULT_SLOT(FArguments, Content)
            SLATE_ARGUMENT(FLinearColor, HighlightColor)
            SLATE_ARGUMENT(FLinearColor, UnderlineColor)
            SLATE_ARGUMENT(float, UnderlineThickness)
            SLATE_ARGUMENT(FVector2D, Padding)
            SLATE_ARGUMENT(float, CornerRadius)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Padding = FVector2f(InArgs._Padding);
            UnderlineThickness = InArgs._UnderlineThickness;
            HighlightBrush = FSlateRoundedBoxBrush(InArgs._HighlightColor, InArgs._CornerRadius);
            // Square ends: the bar reads as one solid line under the word.
            UnderlineBrush = FSlateRoundedBoxBrush(InArgs._UnderlineColor, FMath::Min(InArgs._UnderlineThickness * 0.5f, 2.0f));
            ChildSlot[InArgs._Content.Widget];
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            const FVector2f Size = AllottedGeometry.GetLocalSize();
            const FVector2f BoxSize(Size.X + Padding.X * 2.0f, Size.Y + Padding.Y * 2.0f);
            const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

            FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(FVector2f(-Padding.X, -Padding.Y))),
                &HighlightBrush, ESlateDrawEffect::None, HighlightBrush.GetTint(InWidgetStyle) * Tint);

            // Across the bottom of the highlight, below the descenders.
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(FVector2f(BoxSize.X, UnderlineThickness),
                    FSlateLayoutTransform(FVector2f(-Padding.X, Size.Y + Padding.Y - UnderlineThickness))),
                &UnderlineBrush, ESlateDrawEffect::None, UnderlineBrush.GetTint(InWidgetStyle) * Tint);

            return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
        }

    private:
        FVector2f Padding = FVector2f::ZeroVector;
        float UnderlineThickness = 4.0f;
        FSlateRoundedBoxBrush HighlightBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
        FSlateRoundedBoxBrush UnderlineBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
    };

    class FLFFKaraokeTextDecorator : public FRichTextDecorator
    {
    public:
        FLFFKaraokeTextDecorator(URichTextBlock* InOwner, const ULFFKaraokeDecorator* InSettings)
            : FRichTextDecorator(InOwner)
            , Settings(InSettings)
        {
        }

        virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
        {
            return RunParseResult.Name == TEXT("w");
        }

    protected:
        virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& DefaultTextStyle) const override
        {
            const ULFFKaraokeDecorator* S = Settings.Get();
            if (S == nullptr)
            {
                return TSharedPtr<SWidget>();
            }

            // The same font and colour as the words around it, so the line keeps its exact width.
            return SNew(SLFFKaraokeWord)
                .HighlightColor(S->HighlightColor)
                .UnderlineColor(S->UnderlineColor)
                .UnderlineThickness(S->UnderlineThickness)
                .Padding(S->HighlightPadding)
                .CornerRadius(S->CornerRadius)
                [
                    SNew(STextBlock)
                    .Text(RunInfo.Content)
                    .Font(DefaultTextStyle.Font)
                    .ColorAndOpacity(DefaultTextStyle.ColorAndOpacity)
                ];
        }

    private:
        TWeakObjectPtr<const ULFFKaraokeDecorator> Settings;
    };
}

TSharedPtr<ITextDecorator> ULFFKaraokeDecorator::CreateDecorator(URichTextBlock* InOwner)
{
    return MakeShared<FLFFKaraokeTextDecorator>(InOwner, this);
}
