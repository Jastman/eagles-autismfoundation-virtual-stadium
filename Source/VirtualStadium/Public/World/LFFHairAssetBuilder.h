#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LFFHairAssetBuilder.generated.h"

class USkeletalMesh;

/** Editor adapter for the installed MetaHuman Crowd groom fitting pipeline. No runtime assembly dependencies. */
UCLASS()
class VIRTUALSTADIUM_API ULFFHairAssetBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Creates an unsaved, head-weighted card asset fitted to this exact face. Errors never substitute geometry. */
    UFUNCTION(BlueprintCallable, Category = "LFF|Editor|Hair", meta = (DevelopmentOnly))
    static USkeletalMesh* BuildFittedCards(UObject* WardrobeItem, USkeletalMesh* TargetFace,
        const FString& AssetPackage, FString& Error);

    /** Read-only preflight: actual binding/groom, card geometry, compact textures, materials and target skeleton. */
    UFUNCTION(BlueprintCallable, Category = "LFF|Editor|Hair", meta = (DevelopmentOnly))
    static FString InspectSource(UObject* WardrobeItem, USkeletalMesh* TargetFace);

    /** Read-only generated-asset evidence, including weighted vertices and every rendered LOD. */
    UFUNCTION(BlueprintCallable, Category = "LFF|Editor|Hair", meta = (DevelopmentOnly))
    static FString InspectCards(USkeletalMesh* Cards, USkeletalMesh* TargetFace);
};
