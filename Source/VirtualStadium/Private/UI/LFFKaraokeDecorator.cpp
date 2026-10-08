#include "UI/LFFKaraokeDecorator.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/RichTextBlock.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"

namespace
{
    struct FLFFKaraokeLook
    {
        FLinearColor HighlightColor = FLinearColor::Transparent;
        FLinearColor UnderlineColor = FLinearColor::Transparent;
        float UnderlineThickness = 4.0f;
        FVector2f Padding = FVector2f::ZeroVector;
        float CornerRadius = 6.0f;
    };

    /**
     * The spoken word as an ordinary text run in the card's own style, so the line breaks exactly as it does without the
     * highlight (an inline widget is measured on its own and moves line breaks around it, which made the card reflow as
     * the voice read). Only the paint differs: the highlight behind the word and the underline across its foot.
     */
    class FLFFKaraokeRun : public FSlateTextRun
    {
    public:
        static TSharedRef<FLFFKaraokeRun> Create(const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle,
            const FTextRange& InRange, const FLFFKaraokeLook& InLook)
        {
            return MakeShareable(new FLFFKaraokeRun(InRunInfo, InText, InStyle, InRange, InLook));
        }

        virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            // Block offsets and sizes are pre-scaled, as in FSlateTextRun::OnPaint.
            const float InverseScale = Inverse(AllottedGeometry.Scale);
            const FVector2f Offset(TransformPoint(InverseScale, TextArgs.Block->GetLocationOffset()));
            const FVector2f Size(TransformVector(InverseScale, TextArgs.Block->GetSize()));
            const FVector2f BoxSize(Size.X + Look.Padding.X * 2.0f, Size.Y + Look.Padding.Y * 2.0f);
            const FVector2f BoxOffset = Offset - Look.Padding;
            const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
            const ESlateDrawEffect Effects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

            FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxOffset)),
                &HighlightBrush, Effects, HighlightBrush.GetTint(InWidgetStyle) * Tint);
            // Across the bottom of the highlight, below the descenders.
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(FVector2f(BoxSize.X, Look.UnderlineThickness),
                    FSlateLayoutTransform(FVector2f(BoxOffset.X, BoxOffset.Y + BoxSize.Y - Look.UnderlineThickness))),
                &UnderlineBrush, Effects, UnderlineBrush.GetTint(InWidgetStyle) * Tint);

            return FSlateTextRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
        }

        virtual TSharedRef<IRun> Clone() const override
        {
            return MakeShareable(new FLFFKaraokeRun(*this));
        }

    private:
        FLFFKaraokeRun(const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange,
            const FLFFKaraokeLook& InLook)
            : FSlateTextRun(InRunInfo, InText, InStyle, InRange)
            , Look(InLook)
            , HighlightBrush(InLook.HighlightColor, InLook.CornerRadius)
            // Square ends: the bar reads as one solid line under the word.
            , UnderlineBrush(InLook.UnderlineColor, FMath::Min(InLook.UnderlineThickness * 0.5f, 2.0f))
        {
        }

        FLFFKaraokeRun(const FLFFKaraokeRun& Other) = default;

        FLFFKaraokeLook Look;
        FSlateRoundedBoxBrush HighlightBrush;
        FSlateRoundedBoxBrush UnderlineBrush;
    };

    class FLFFKaraokeTextDecorator : public ITextDecorator
    {
    public:
        FLFFKaraokeTextDecorator(URichTextBlock* InOwner, const FLFFKaraokeLook& InLook)
            : Owner(InOwner)
            , Look(InLook)
        {
        }

        virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
        {
            return RunParseResult.Name == TEXT("w");
        }

        virtual TSharedRef<ISlateRun> Create(const TSharedRef<FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText,
            const TSharedRef<FString>& InOutModelText, const ISlateStyle* Style) override
        {
            // The word itself goes into the line's text, as plain text would, in the card's default style.
            FTextRange ModelRange;
            ModelRange.BeginIndex = InOutModelText->Len();
            *InOutModelText += OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.Len());
            ModelRange.EndIndex = InOutModelText->Len();
            const URichTextBlock* Block = Owner.Get();
            const FTextBlockStyle& TextStyle = Block != nullptr ? Block->GetCurrentDefaultTextStyle() : FTextBlockStyle::GetDefault();
            return FLFFKaraokeRun::Create(FRunInfo(RunParseResult.Name), InOutModelText, TextStyle, ModelRange, Look);
        }

    private:
        TWeakObjectPtr<URichTextBlock> Owner;
        FLFFKaraokeLook Look;
    };
}

TSharedPtr<ITextDecorator> ULFFKaraokeDecorator::CreateDecorator(URichTextBlock* InOwner)
{
    FLFFKaraokeLook Look;
    Look.HighlightColor = HighlightColor;
    Look.UnderlineColor = UnderlineColor;
    Look.UnderlineThickness = UnderlineThickness;
    Look.Padding = FVector2f(HighlightPadding);
    Look.CornerRadius = CornerRadius;
    return MakeShared<FLFFKaraokeTextDecorator>(InOwner, Look);
}
