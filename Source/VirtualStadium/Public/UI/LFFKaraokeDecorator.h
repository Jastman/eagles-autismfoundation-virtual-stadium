#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "LFFKaraokeDecorator.generated.h"

/**
 * The spoken word on the caption card: <w>word</> in the card's rich text. The word keeps the card's own font and
 * colour (so nothing reflows and the text contrast never drops) and gets a soft tinted highlight behind it plus a solid
 * underline across its foot, so the marker never depends on colour alone. No animation: it moves word to word.
 */
UCLASS()
class VIRTUALSTADIUM_API ULFFKaraokeDecorator : public URichTextBlockDecorator
{
    GENERATED_BODY()

public:
    virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

    /** Behind the word. Kept soft so the card text on top keeps its full contrast. */
    UPROPERTY(EditAnywhere, Category = "Karaoke")
    FLinearColor HighlightColor = FLinearColor(0.141f, 0.514f, 0.021f, 0.32f);

    UPROPERTY(EditAnywhere, Category = "Karaoke")
    FLinearColor UnderlineColor = FLinearColor(0.141f, 0.514f, 0.021f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "Karaoke", meta = (ClampMin = "2.0", ClampMax = "8.0"))
    float UnderlineThickness = 4.0f;

    /** How far the highlight reaches past the word on each side. It is drawn outside the word, so nothing moves. */
    UPROPERTY(EditAnywhere, Category = "Karaoke")
    FVector2D HighlightPadding = FVector2D(5.0, 1.0);

    UPROPERTY(EditAnywhere, Category = "Karaoke", meta = (ClampMin = "0.0", ClampMax = "16.0"))
    float CornerRadius = 6.0f;
};
