#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFFTileStreamingSubsystem.generated.h"

class ACesium3DTileset;
class SLFFLoadingScreen;
class SWidget;
class UTexture2D;

/**
 * Keeps the Google photorealistic tiles from showing blue holes:
 *  - Tunes every Cesium tileset in the level so tiles around the camera stay loaded (no frustum culling, a coarser
 *    screen-space error off-screen) and a not-yet-loaded tile shows its parent instead of a hole (ForbidHoles).
 *  - On the first level of each launch, covers the screen with SLFFLoadingScreen until the tiles have finished loading
 *    (or LoadingTimeoutSeconds passes). -LFFNoLoadingScreen skips the cover.
 */
UCLASS()
class VIRTUALSTADIUM_API ULFFTileStreamingSubsystem final : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    /** The streaming settings applied to every tileset (exposed for tests). */
    static void ConfigureTileset(ACesium3DTileset& Tileset);

    /** Status line for a given displayed percent and seconds on screen. */
    static FText GetStatusText(float Percent, float ElapsedSeconds);

    bool IsLoadingScreenVisible() const { return LoadingScreen.IsValid(); }

    static constexpr float MinimumLoadingSeconds = 2.5f;
    static constexpr float LoadingTimeoutSeconds = 45.0f;
    static constexpr float SettledSeconds = 1.0f;
    static constexpr float LoadedPercent = 99.5f;
    static constexpr float FadeSeconds = 0.6f;

private:
    float GetTilesetProgress() const;
    void ShowLoadingScreen();
    void HideLoadingScreen();
    void UnfreezeTime();

    TArray<TWeakObjectPtr<ACesium3DTileset>> Tilesets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> LogoTextures;

    TSharedPtr<SLFFLoadingScreen> LoadingScreen;
    TSharedPtr<SWidget> LoadingScreenRoot;
    float Elapsed = 0.0f;
    float DisplayedPercent = 0.0f;
    float SettledFor = 0.0f;
    float FadeElapsed = -1.0f;
    bool bSawIncompleteLoad = false;
    bool bTimeFrozen = false;
    float SavedTimeDilation = 1.0f;

    static bool bShownThisLaunch;
};
