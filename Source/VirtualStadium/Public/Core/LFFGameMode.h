#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Journey/LFFJourneyTypes.h"
#include "LFFGameMode.generated.h"

class UDataTable;
class UMaterialInterface;
class USoundBase;

/**
 * Journey configuration for the stadium map. Set as GlobalDefaultGameMode in DefaultEngine.ini.
 * No pawn is spawned; the camera rig is the view target.
 * Defaults work without a Blueprint subclass; a data-only BP may override them.
 */
UCLASS()
class VIRTUALSTADIUM_API ALFFGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALFFGameMode();

    virtual void StartPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Cooked via DirectoriesToAlwaysCook=/Game/LFF/Data (DefaultGame.ini). */
    UPROPERTY(EditDefaultsOnly, Category = "Journey")
    TSoftObjectPtr<UDataTable> JourneyTable;

    UPROPERTY(EditDefaultsOnly, Category = "Journey")
    FLFFVisitorSettings DefaultVisitorSettings;

    /**
     * First step (0-based) of each ELFFTourStep, in order: Arrival, Gates, Plaza, To my seat, Game. These are steps 1, 5,
     * 8, 11 and 15 of SourceData/Journey/LFF_GoldenPath.json; Breaks and help (19-23) are part of Game. The arc and the
     * sidebar eyebrow read these through the snapshot, so update them here if steps are added.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Journey")
    TArray<int32> TourStepFirstSteps = { 0, 4, 7, 10, 14 };

    /** Free explore's My seat button goes here: the view from Section 111, Row 10, Seat 8. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    FName MySeatTargetId = TEXT("CT_SeatFieldView");

    /** The seat's map level (lower bowl, reached from the lower concourse). Free explore opens on it the first time. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    ELFFMapLevel MySeatLevel = ELFFMapLevel::LowerConcourse;

    /** The My seat callout and card. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    FText MySeatLabel;

    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    FText MySeatNote;

    /** Free explore's places (row struct LFFAmenityMarker), imported from SourceData/Journey/LFF_Amenities.json. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    TSoftObjectPtr<UDataTable> AmenityTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/LFF/Data/DT_LFF_Amenities.DT_LFF_Amenities")));

    /** Packaged builds drop places with no VerificationSource; on keeps them (internal review builds only). The editor shows them. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    bool bShowUnverifiedAmenities = false;

    /**
     * One per ELFFMapLevel, in that order. Floor heights come from the model's own floor slabs (site frame, cm above the
     * turf): the main concourse deck at 7.7 m with the grade walkway under it at 1.6 m, the club concourse at 17.2 m, the
     * two suite levels at 21.8 and 26.7 m, the upper concourse at 31.2 m (upper seats to 48 m).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    TArray<FLFFLevelView> LevelViews;

    /** The picked level's highlight: a post-process tint over its height band, inside the stadium's footprint. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    TSoftObjectPtr<UMaterialInterface> LevelHighlightMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/LFF/UI/Materials/M_LFF_LevelHighlight.M_LFF_LevelHighlight")));

    /** The stadium's footprint for the highlight (half-widths, cm), so the city tiles at the same height stay untouched. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    FVector2D StadiumFootprintRadii = FVector2D(13000.0, 14000.0);

    /** KELLY #69BE28 at 35%: the level shows through its tint. Everything else in the stadium dims a little. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    FLinearColor LevelHighlightColor = FLinearColor(0.141f, 0.514f, 0.021f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "0.1", ClampMax = "0.6"))
    float LevelHighlightOpacity = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float LevelDimOutside = 0.3f;

    /** Free flight: cruise speed (cm/s), Shift's multiplier, mouse-look degrees per pixel of drag, the ease into and out of
     *  moving (seconds), and the space you can fly in (site frame: radius from the centre, lowest and highest Z). */
    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "100.0"))
    float FlySpeed = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "1.0", ClampMax = "5.0"))
    float FlyFastMultiplier = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "0.02", ClampMax = "1.0"))
    float FlyLookDegreesPerPixel = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FlyEaseSeconds = 0.3f;

    /** One mouse-wheel notch moves this far, eased. */
    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    float FlyWheelStep = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    float FlyBoundsRadius = 60000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    float FlyMinZ = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Explore")
    float FlyMaxZ = 30000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText IntroTitle;

    UPROPERTY(EditDefaultsOnly, Category = "Copy", meta = (MultiLine = "true"))
    FText IntroBody;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText PrototypeBadgeText;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerIllustrative;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerApproximate;

    /** The one small line under the badge on journey steps. */
    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerLineIllustrative;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText DisclaimerLineApproximate;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText TargetMissingText;

    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FText JourneyMissingText;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float DefaultEaseSeconds = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float MinTransitionSeconds = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.1"))
    float MaxTransitionSeconds = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (ClampMin = "0.05"))
    float ReducedMotionFadeSeconds = 0.35f;

    /** Neutral dark grey rather than black, to soften the fade. */
    UPROPERTY(EditDefaultsOnly, Category = "Timing")
    FLinearColor FadeColor = FLinearColor(0.08f, 0.08f, 0.10f, 1.0f);

    /**
     * Low plaza and crowd murmur, looped while Sound is on (no horns, no fireworks). Steps may override it.
     * Cooked via DirectoriesToAlwaysCook=/Game/LFF/Audio (DefaultGame.ini).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> AmbientBed;

    /** Short muffled sample for a step's Sound preview. Plays once, only after the visitor picks Soft. */
    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> PreviewSound;

    UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BedVolume = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PreviewVolume = 0.35f;

    /** One full bright-dim cycle of the Lights preview. 4 s is 0.25 Hz, far below any flash threshold. */
    UPROPERTY(EditDefaultsOnly, Category = "Preview", meta = (ClampMin = "3.0", ClampMax = "10.0"))
    float LightPulseSeconds = 4.0f;

    /** Peak exposure lift of the Lights preview, in stops. The pulse is a smooth sine, never a step. */
    UPROPERTY(EditDefaultsOnly, Category = "Preview", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float LightPulseStops = 0.9f;

    /** Anthem, Standard: where the camera rides beside the eagle, in the bird's own frame (cm; X ahead, Y right). */
    UPROPERTY(EditDefaultsOnly, Category = "Anthem")
    FVector AnthemFollowOffset = FVector(200.0f, 700.0f, -50.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Anthem", meta = (ClampMin = "30.0", ClampMax = "90.0"))
    float AnthemFollowFieldOfView = 55.0f;

    /** How far into the flight the camera leaves the seat to join the eagle (fraction of the flight). */
    UPROPERTY(EditDefaultsOnly, Category = "Anthem", meta = (ClampMin = "0.0", ClampMax = "0.8"))
    float AnthemFollowStart = 0.3f;

    /** The slow ease from the eagle back to the seat before the fireworks. */
    UPROPERTY(EditDefaultsOnly, Category = "Anthem", meta = (ClampMin = "1.0", ClampMax = "6.0"))
    float AnthemReturnSeconds = 3.5f;

    /** Voiceover level. Kept a little under the bed level; the bed ducks under it while it speaks. */
    UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VoiceVolume = 0.3f;

    /** How far the bed dips while the voice speaks (6-10 dB). */
    UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "-10.0", ClampMax = "-6.0"))
    float VoiceDuckDecibels = -8.0f;

    /** A short breath after the camera arrives before the voice starts, so a step never opens with a jolt. */
    UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float VoiceStartDelaySeconds = 0.5f;

    /** Beds cross-fade over this long when a step names a different one. */
    UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.2", ClampMax = "4.0"))
    float BedCrossfadeSeconds = 1.5f;

    /**
     * The splash orbit passes through this camera target (the first step's wide shot), circling LFF_SiteRoot at its
     * radius and height, so Start glides along the orbit into the first step.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Splash")
    FName SplashOrbitTargetId = TEXT("CT_StadiumWide");

    /** One slow lap. */
    UPROPERTY(EditDefaultsOnly, Category = "Splash", meta = (ClampMin = "25.0", ClampMax = "40.0"))
    float SplashOrbitSeconds = 40.0f;

    /** "Learn More" on the last step. */
    UPROPERTY(EditDefaultsOnly, Category = "Copy")
    FString LearnMoreUrl = TEXT("https://www.philadelphiaeagles.com/eaglesautismfoundation/");

    FText GetDisclaimerText(ELFFDisclaimerLevel Level) const;
    FText GetDisclaimerLine(ELFFDisclaimerLevel Level) const;
};
