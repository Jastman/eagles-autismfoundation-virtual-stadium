#include "UI/LFFCalmToggle.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "VirtualStadium.h"

void ULFFCalmToggle::NativeOnSnapshot(const FLFFJourneySnapshot& Snapshot)
{
    Super::NativeOnSnapshot(Snapshot);

    const bool bChanged = bCalmMode != Snapshot.Settings.bCalmMode;
    bCalmMode = Snapshot.Settings.bCalmMode;
    const ESlateVisibility SceneVisibility = Snapshot.Settings.bHighContrast
        ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
    CalmModeActiveScene->SetVisibility(SceneVisibility);
    CalmModeQuietScene->SetVisibility(SceneVisibility);

    // Only this short, deliberate toggle transition is allowed; Calm Mode still stops stadium motion immediately.
    const bool bInstant = !bVisualStateInitialized || Snapshot.Settings.bReducedMotion
        || Snapshot.bStarted || Snapshot.Mode == ELFFSimulationMode::FreeExplore
        || Snapshot.bSettingsOpen || Snapshot.bPaused || Snapshot.bRestartConfirmOpen;
    if (CalmTransition == nullptr)
    {
        if (!bVisualStateInitialized)
        {
            UE_LOG(LogVirtualStadium, Error, TEXT("%s is missing its CalmTransition UMG animation."), *GetName());
        }
    }
    if (bInstant || CalmTransition == nullptr)
    {
        StopAnimation(CalmTransition);
        CalmModeThumb->SetRenderTranslation(FVector2D(bCalmMode ? 172.0f : 0.0f, 0.0f));
        CalmModeQuietScene->SetRenderOpacity(bCalmMode ? 1.0f : 0.0f);
    }
    else if (bChanged)
    {
        if (bCalmMode)
        {
            PlayAnimationForward(CalmTransition, 1.0f, false);
        }
        else
        {
            PlayAnimationReverse(CalmTransition, 1.0f, false);
        }
    }
    bVisualStateInitialized = true;
}

void ULFFCalmToggle::NativeDestruct()
{
    bVisualStateInitialized = false;
    Super::NativeDestruct();
}
