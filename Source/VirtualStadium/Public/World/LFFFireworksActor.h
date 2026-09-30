#pragma once

#include "CoreMinimal.h"
#include "World/LFFCueActor.h"
#include "LFFFireworksActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Soft fireworks for the anthem preview: a few slow bursts of glowing sparks that open, drift down and shrink away.
 * Nothing flashes: every spark keeps one steady brightness from start to end, and bursts are seconds apart
 * (far below any flash threshold). Plays only after the visitor picks Soft; hidden in Calm Mode with the preview.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFFireworksActor : public ALFFCueActor
{
    GENERATED_BODY()

public:
    ALFFFireworksActor();

    /** Burst centres in the actor's frame, cm. One burst each, BurstInterval apart. */
    UPROPERTY(EditAnywhere, Category = "Fireworks")
    TArray<FVector> BurstOffsets;

    /** One colour per burst, cycled. Steady glow, no flicker. */
    UPROPERTY(EditAnywhere, Category = "Fireworks")
    TArray<FLinearColor> BurstColors;

    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "8", ClampMax = "120"))
    int32 SparksPerBurst = 60;

    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "100.0"))
    float BurstRadius = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "1.5", ClampMax = "8.0"))
    float BurstSeconds = 3.5f;

    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "1.0", ClampMax = "5.0"))
    float BurstInterval = 1.6f;

    /** Spark diameter at its biggest, cm. About 0.6 degrees from the seats, 120 m away. */
    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "10.0"))
    float SparkSize = 130.0f;

    /** Emissive material with BaseTint and EmissiveStrength parameters (M_LFF_Stadium instance). */
    UPROPERTY(EditAnywhere, Category = "Fireworks")
    TSoftObjectPtr<UMaterialInterface> SparkMaterial;

    /**
     * Emissive strength (cd/m2 on a white tint). It has to be in the range of the 1:00 PM sky (a few thousand) to
     * read at all under daylight exposure; it stays steady for the spark's whole life, so it never flashes.
     */
    UPROPERTY(EditAnywhere, Category = "Fireworks", meta = (ClampMin = "0.0", ClampMax = "20000.0"))
    float SparkGlow = 4000.0f;

protected:
    virtual void ApplyAlpha(float Alpha, float ElapsedSeconds) override;
    virtual void OnPlayStarted() override;
    virtual void OnStopped() override;

private:
    void EnsureBursts();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bursts;

    /** Unit directions, the same for every burst (a Fibonacci sphere), so a burst is round and even. */
    TArray<FVector> Directions;
};
