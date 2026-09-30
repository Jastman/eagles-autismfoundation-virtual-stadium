#include "World/LFFFireworksActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    const TCHAR* SparkMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    /** The engine sphere is 100 cm across. */
    constexpr float SphereDiameter = 100.0f;
    /** How far a spark falls over a burst, cm (a slow drift, not a drop). */
    constexpr float SparkDrop = 350.0f;
}

ALFFFireworksActor::ALFFFireworksActor()
{
    Trigger = ELFFCueTrigger::Preview;
    bHiddenBefore = true;
    bHiddenAfter = true;
    bLinear = true;
    BurstOffsets = { FVector(0.0f, -2600.0f, 0.0f), FVector(0.0f, 0.0f, 700.0f), FVector(0.0f, 2600.0f, 100.0f) };
    // Soft white-gold, Eagles green, silver: bright enough to read in daylight, never white-hot.
    BurstColors = { FLinearColor(1.0f, 0.78f, 0.45f), FLinearColor(0.25f, 0.85f, 0.55f), FLinearColor(0.8f, 0.85f, 0.95f) };
}

void ALFFFireworksActor::EnsureBursts()
{
    if (Directions.Num() != SparksPerBurst)
    {
        Directions.Reset();
        const float Golden = UE_PI * (3.0f - FMath::Sqrt(5.0f));
        for (int32 Index = 0; Index < SparksPerBurst; ++Index)
        {
            const float Z = 1.0f - 2.0f * (Index + 0.5f) / SparksPerBurst;
            const float Ring = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
            Directions.Add(FVector(FMath::Cos(Golden * Index) * Ring, FMath::Sin(Golden * Index) * Ring, Z));
        }
    }

    if (Bursts.Num() == BurstOffsets.Num())
    {
        return;
    }

    for (UInstancedStaticMeshComponent* Burst : Bursts)
    {
        if (Burst != nullptr)
        {
            Burst->DestroyComponent();
        }
    }
    Bursts.Reset();

    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SparkMeshPath);
    UMaterialInterface* Material = SparkMaterial.LoadSynchronous();
    for (int32 Index = 0; Index < BurstOffsets.Num(); ++Index)
    {
        UInstancedStaticMeshComponent* Burst = NewObject<UInstancedStaticMeshComponent>(this);
        Burst->SetMobility(EComponentMobility::Movable);
        Burst->SetupAttachment(RootComponent);
        Burst->SetStaticMesh(Sphere);
        Burst->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Burst->SetCastShadow(false);
        Burst->RegisterComponent();
        Burst->SetRelativeLocation(BurstOffsets[Index]);
        if (Material != nullptr)
        {
            UMaterialInstanceDynamic* Glow = Burst->CreateDynamicMaterialInstance(0, Material);
            if (Glow != nullptr && BurstColors.Num() > 0)
            {
                Glow->SetVectorParameterValue(TEXT("BaseTint"), BurstColors[Index % BurstColors.Num()]);
                Glow->SetScalarParameterValue(TEXT("EmissiveStrength"), SparkGlow);
            }
        }
        for (int32 Spark = 0; Spark < SparksPerBurst; ++Spark)
        {
            Burst->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.0f)));
        }
        Bursts.Add(Burst);
    }
}

void ALFFFireworksActor::OnPlayStarted()
{
    EnsureBursts();
    PlaySeconds = FMath::Max(1.0f, (BurstOffsets.Num() - 1) * BurstInterval + BurstSeconds);
}

void ALFFFireworksActor::OnStopped()
{
    for (UInstancedStaticMeshComponent* Burst : Bursts)
    {
        if (Burst != nullptr)
        {
            Burst->SetVisibility(false);
        }
    }
}

void ALFFFireworksActor::ApplyAlpha(const float Alpha, const float ElapsedSeconds)
{
    // The actor itself never moves; each burst is drawn from its own clock.
    if (!IsPlaying())
    {
        return;
    }

    for (int32 Index = 0; Index < Bursts.Num(); ++Index)
    {
        UInstancedStaticMeshComponent* Burst = Bursts[Index];
        if (Burst == nullptr)
        {
            continue;
        }

        const float T = (ElapsedSeconds - Index * BurstInterval) / BurstSeconds;
        const bool bVisible = T > 0.0f && T < 1.0f;
        Burst->SetVisibility(bVisible);
        if (!bVisible)
        {
            continue;
        }

        // Opens quickly then slows (ease-out), drifts down, and every spark shrinks to nothing in the last half.
        const float Open = 1.0f - FMath::Pow(1.0f - FMath::Clamp(T / 0.55f, 0.0f, 1.0f), 3.0f);
        const float Shrink = 1.0f - FMath::SmoothStep(0.45f, 1.0f, T);
        const float Grow = FMath::SmoothStep(0.0f, 0.12f, T);
        const float Scale = SparkSize / SphereDiameter * Shrink * Grow;
        const float Drop = SparkDrop * T * T;

        TArray<FTransform> Transforms;
        Transforms.Reserve(Directions.Num());
        for (const FVector& Direction : Directions)
        {
            Transforms.Add(FTransform(FQuat::Identity, Direction * BurstRadius * Open - FVector(0.0f, 0.0f, Drop), FVector(Scale)));
        }
        Burst->BatchUpdateInstancesTransforms(0, Transforms, false, true);
    }
}
