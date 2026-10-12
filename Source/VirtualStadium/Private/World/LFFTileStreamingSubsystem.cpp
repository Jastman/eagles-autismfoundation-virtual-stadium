#include "World/LFFTileStreamingSubsystem.h"

#include "Cesium3DTileset.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "UI/SLFFLoadingScreen.h"
#include "VirtualStadium.h"

#define LOCTEXT_NAMESPACE "LFFLoadingScreen"

bool ULFFTileStreamingSubsystem::bShownThisLaunch = false;

namespace
{
    const TCHAR* LoadingFontPath = TEXT("/Game/LFF/UI/Fonts/F_LFF_Atkinson.F_LFF_Atkinson");
    const TCHAR* EAFLogoPath = TEXT("/Game/LFF/UI/Icons/T_LFF_Logo_EAF_White.T_LFF_Logo_EAF_White");
    const TCHAR* FiveBelowLogoPath = TEXT("/Game/LFF/UI/Icons/T_LFF_Logo_FiveBelow_UI.T_LFF_Logo_FiveBelow_UI");
    constexpr int32 LoadingScreenZOrder = 10000;
}

bool ULFFTileStreamingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULFFTileStreamingSubsystem::ConfigureTileset(ACesium3DTileset& Tileset)
{
    // Turning the camera reveals tiles that were frustum-culled, so they start empty and the sky shows through.
    // Keep everything around the camera loaded, just coarser off-screen (in view stays at the tileset's own error, 16).
    Tileset.EnableFrustumCulling = false;
    Tileset.EnforceCulledScreenSpaceError = true;
    Tileset.CulledScreenSpaceError = 32.0;
    // A tile that's still loading is drawn as its (lower-detail) parent, never as a hole.
    Tileset.ForbidHoles = true;
    Tileset.PreloadAncestors = true;
    Tileset.PreloadSiblings = true;
    Tileset.MaximumSimultaneousTileLoads = FMath::Max(Tileset.MaximumSimultaneousTileLoads, 32);
    // The default 256 MB evicts the stadium's own tiles once the camera has been all the way round.
    Tileset.MaximumCachedBytes = FMath::Max<int64>(Tileset.MaximumCachedBytes, 1024ll * 1024 * 1024);
}

FText ULFFTileStreamingSubsystem::GetStatusText(const float Percent, const float ElapsedSeconds)
{
    static const FText Lines[] = {
        LOCTEXT("Status0", "Finding Lincoln Financial Field..."),
        LOCTEXT("Status1", "Mowing the grass..."),
        LOCTEXT("Status2", "Painting the end zones midnight green..."),
        LOCTEXT("Status3", "Warming up the stadium lights..."),
        LOCTEXT("Status4", "Saving your seat in Section 111..."),
        LOCTEXT("Status5", "Finding the quiet spaces..."),
        LOCTEXT("Status6", "Almost game time..."),
    };
    constexpr int32 Count = UE_ARRAY_COUNT(Lines);
    if (Percent >= 100.0f)
    {
        return LOCTEXT("StatusDone", "Fly, Eagles, Fly!");
    }
    // Follow the bar, but keep the line moving even if one stretch of loading is slow.
    const int32 ByProgress = FMath::FloorToInt(FMath::Clamp(Percent, 0.0f, 99.9f) / 100.0f * Count);
    const int32 ByTime = FMath::FloorToInt(FMath::Max(ElapsedSeconds, 0.0f) / 3.5f);
    return Lines[FMath::Clamp(FMath::Max(ByProgress, ByTime), 0, Count - 1)];
}

void ULFFTileStreamingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    for (TActorIterator<ACesium3DTileset> It(&InWorld); It; ++It)
    {
        ConfigureTileset(**It);
        Tilesets.Add(*It);
    }
    if (Tilesets.Num() > 0)
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Cesium: %d tileset(s) set to stream around the camera without holes."), Tilesets.Num());
    }

    if (!bShownThisLaunch && Tilesets.Num() > 0 && !IsRunningDedicatedServer()
        && !FParse::Param(FCommandLine::Get(), TEXT("LFFNoLoadingScreen")))
    {
        ShowLoadingScreen();
    }
}

void ULFFTileStreamingSubsystem::ShowLoadingScreen()
{
    UWorld* World = GetWorld();
    UGameViewportClient* Viewport = World != nullptr ? World->GetGameViewport() : nullptr;
    if (Viewport == nullptr || !FSlateApplication::IsInitialized())
    {
        return;
    }
    bShownThisLaunch = true;

    UTexture2D* EAF = LoadObject<UTexture2D>(nullptr, EAFLogoPath);
    UTexture2D* FiveBelow = LoadObject<UTexture2D>(nullptr, FiveBelowLogoPath);
    LogoTextures = { EAF, FiveBelow };
    if (EAF == nullptr || FiveBelow == nullptr)
    {
        UE_LOG(LogVirtualStadium, Warning, TEXT("Loading screen: a logo is missing (%s, %s)."), EAFLogoPath, FiveBelowLogoPath);
    }
    const UObject* Font = LoadObject<UObject>(nullptr, LoadingFontPath);

    LoadingScreen = SNew(SLFFLoadingScreen).LeftLogo(EAF).RightLogo(FiveBelow).Font(Font);
    LoadingScreen->SetProgress(0.0f, GetStatusText(0.0f, 0.0f));
    LoadingScreenRoot = LoadingScreen;
    Viewport->AddViewportWidgetContent(LoadingScreenRoot.ToSharedRef(), LoadingScreenZOrder);
    FSlateApplication::Get().SetAllUserFocus(LoadingScreen, EFocusCause::SetDirectly);

    // Hold the world still behind the cover: the welcome orbit moves the camera, so the tiles it needs never settle.
    // Tilesets still tick (only their delta shrinks), so loading carries on at full speed.
    if (AWorldSettings* Settings = World->GetWorldSettings())
    {
        SavedTimeDilation = Settings->TimeDilation;
        Settings->SetTimeDilation(Settings->MinGlobalTimeDilation);
        bTimeFrozen = true;
    }

    Elapsed = 0.0f;
    DisplayedPercent = 0.0f;
    SettledFor = 0.0f;
    FadeElapsed = -1.0f;
    bSawIncompleteLoad = false;
    UE_LOG(LogVirtualStadium, Log, TEXT("Loading screen: shown until the 3D tiles finish loading."));
}

void ULFFTileStreamingSubsystem::UnfreezeTime()
{
    if (!bTimeFrozen)
    {
        return;
    }
    bTimeFrozen = false;
    if (UWorld* World = GetWorld())
    {
        if (AWorldSettings* Settings = World->GetWorldSettings())
        {
            Settings->SetTimeDilation(SavedTimeDilation);
        }
    }
}

void ULFFTileStreamingSubsystem::HideLoadingScreen()
{
    UnfreezeTime();
    if (!LoadingScreenRoot.IsValid())
    {
        return;
    }
    UWorld* World = GetWorld();
    if (UGameViewportClient* Viewport = World != nullptr ? World->GetGameViewport() : nullptr)
    {
        Viewport->RemoveViewportWidgetContent(LoadingScreenRoot.ToSharedRef());
    }
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    LoadingScreen.Reset();
    LoadingScreenRoot.Reset();
    LogoTextures.Reset();
}

float ULFFTileStreamingSubsystem::GetTilesetProgress() const
{
    float Progress = 100.0f;
    bool bAny = false;
    for (const TWeakObjectPtr<ACesium3DTileset>& Tileset : Tilesets)
    {
        if (const ACesium3DTileset* Live = Tileset.Get(); Live != nullptr && !Live->IsHidden())
        {
            Progress = FMath::Min(Progress, Live->GetLoadProgress());
            bAny = true;
        }
    }
    return bAny ? Progress : 100.0f;
}

void ULFFTileStreamingSubsystem::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!LoadingScreen.IsValid())
    {
        return;
    }
    // Real time: the world's own clock is held almost still while the cover is up. Hitches shouldn't skip the bar ahead.
    const float Dt = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);
    Elapsed += Dt;

    if (FadeElapsed >= 0.0f)
    {
        FadeElapsed += Dt;
        LoadingScreen->SetRenderOpacity(1.0f - FMath::Clamp((FadeElapsed - 0.4f) / FadeSeconds, 0.0f, 1.0f));
        if (FadeElapsed >= 0.4f + FadeSeconds)
        {
            UE_LOG(LogVirtualStadium, Log, TEXT("Loading screen: done after %.1f s."), Elapsed);
            HideLoadingScreen();
        }
        return;
    }

    const float Raw = GetTilesetProgress();
    if (FMath::FloorToInt(Elapsed / 5.0f) != FMath::FloorToInt((Elapsed - Dt) / 5.0f))
    {
        UE_LOG(LogVirtualStadium, Log, TEXT("Loading screen: %.0f s, tiles %.4f%%."), Elapsed, Raw);
    }
    bSawIncompleteLoad |= Raw < LoadedPercent;
    // Cesium reports 100 for a moment before it knows which tiles it needs; trust it once it has been below.
    // 99.9999 means every tile is in and Cesium is only waiting to hide the coarser ones, so it counts as loaded.
    const bool bLoaded = Raw >= LoadedPercent && (bSawIncompleteLoad || Elapsed > 6.0f);
    SettledFor = bLoaded ? SettledFor + Dt : 0.0f;
    const bool bDone = (SettledFor >= SettledSeconds && Elapsed >= MinimumLoadingSeconds) || Elapsed >= LoadingTimeoutSeconds;

    // Monotonic and eased: Cesium's own figure dips whenever it discovers more tiles to load.
    const float Target = bDone ? 100.0f : FMath::Min(Raw, 99.0f);
    if (Target > DisplayedPercent)
    {
        DisplayedPercent = FMath::Min(Target, FMath::FInterpTo(DisplayedPercent, Target, Dt, 3.0f) + 8.0f * Dt);
    }
    LoadingScreen->SetProgress(DisplayedPercent, GetStatusText(DisplayedPercent, Elapsed));

    if (bDone && DisplayedPercent >= 100.0f)
    {
        if (Elapsed >= LoadingTimeoutSeconds)
        {
            UE_LOG(LogVirtualStadium, Warning, TEXT("Loading screen: tiles still loading after %.0f s (%.0f%%); showing the stadium anyway."),
                LoadingTimeoutSeconds, Raw);
        }
        FadeElapsed = 0.0f;
        // The orbit starts moving again under the fade.
        UnfreezeTime();
    }
    else if (FSlateApplication::IsInitialized() && !LoadingScreen->HasAnyUserFocus().IsSet())
    {
        // The HUD takes focus when it is created; keep keys from starting the tour behind the cover.
        FSlateApplication::Get().SetAllUserFocus(LoadingScreen, EFocusCause::SetDirectly);
    }
}

void ULFFTileStreamingSubsystem::Deinitialize()
{
    HideLoadingScreen();
    Tilesets.Reset();
    Super::Deinitialize();
}

TStatId ULFFTileStreamingSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULFFTileStreamingSubsystem, STATGROUP_Tickables);
}

#undef LOCTEXT_NAMESPACE
