# Journey schema and implementation plan (V1 golden path)

Status: Proposed, 2026-09-28. Rationale: `Docs/ADR/ADR-0002-journey-runtime-architecture.md`. Nothing here has been compiled yet. **VERIFY** marks an API or behaviour that needs checking on a UE 5.8 install.

Naming:
- C++ types use `LFF` with no underscore (`ULFFJourneySubsystem`, files `LFFJourneySubsystem.h`).
- Assets use `Type_LFF_Name`.
- The content root is `/Game/LFF/`.
- Module: `VirtualStadium`. `Build.cs` adds `EnhancedInput` and `DeveloperSettings`.

## 1. Existing code: keep, rename, delete

No `.uasset` exists yet (LEDGER F1), so renames need no CoreRedirects.

| Existing | Action | Result |
|---|---|---|
| `Data/StadiumScheduleStop.h` `FStadiumScheduleStop` | Rename and reshape. Remove `WorldLocation`, `CameraRotation`, `FlyToDuration` and the placeholder fields. Keep `PreviewImage`. | `Data/LFFJourneyTypes.h` `FLFFJourneyStep` |
| `Data/StadiumScheduleDataAsset.*` | **Delete** (replaced by a Data Table, ADR D2) | — |
| `Tour/StadiumTourCamera.*` | Rename and strip down to camera motion only. Keep the lerp, slerp and retarget-from-current-pose code (moved to `FLFFTransitionMath`). Remove step state, input, widget and markers. | `Camera/LFFCameraRig.*` `ALFFCameraRig` |
| `Tour/StadiumStopMarker.*` | Rename. Placement moves from step data to the level. `ConfigureFromStop` becomes authored properties plus `SetLabelVisible`. | `World/LFFLabelMarker.*` `ALFFLabelMarker` |
| `UI/StadiumTourWidget.*` | Rename. Replace the per-frame `BlueprintPure` getters with snapshot push. Buttons are kept as `BindWidgetOptional`. | `UI/LFFHUDWidget.*` `ULFFHUDWidget` |
| `VirtualStadium.h/.cpp`, `*.Target.cs` | Keep | — |
| `Config/DefaultInput.ini` legacy `ActionMappings` | **Delete**. Set Enhanced Input classes (§6). | — |
| `Content/Data/ScheduleStops.template.json` (8 stops) | `git mv` to `SourceData/Journey/LFF_GoldenPath.json` and rewrite as 4 rows (§4). Keep the other 4 stops in `SourceData/Journey/LFF_Backlog.json` (not imported). | — |

## 2. C++ classes

Paths are relative to `Source/VirtualStadium/`. Each `Public/X/Y.h` has a matching `Private/X/Y.cpp`, except the header-only files marked *(h)*.

| File | Type | Responsibility | Key API |
|---|---|---|---|
| `Public/Data/LFFJourneyTypes.h` *(h)* | UENUMs, USTRUCTs | The schema (§3) | — |
| `Public/Data/LFFJourneyValidation.h` | `struct FLFFJourneyValidation` (static) | Validates a step array; returns errors and warnings | `static bool Validate(const TArray<FLFFJourneyStep>&, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)`; `static bool LoadStepsFromTable(const UDataTable*, TArray<FLFFJourneyStep>& Out)` |
| `Public/Journey/LFFJourneyStateMachine.h` | `struct FLFFJourneyStateMachine` (plain C++, no world or time) plus `namespace LFFJourneyRules` | Step index, phase, pause, transition serial. No wrap. | `Load(TArray<FLFFJourneyStep>)`, `Next()`, `Back()`, `Restart()`, `Pause()`, `Resume()`, `GoTo(int32)` → `ELFFCommandResult`; `NotifyTransitionFinished(uint32 Id)` → bool (a stale Id is ignored); `CanNext()`, `CanBack()`, `IsAtEnd()`, `GetIndex()`, `Num()`, `GetPhase()`, `IsPaused()`, `GetTransitionId()`, `GetCurrentStep()`. Rules: `ResolveTransitionProfile(ELFFTransitionProfile, bool bReducedMotion)`, `StricterDisclaimer(A,B)`, `ShouldShowDisclaimer(Level)`, `ClampDuration(float, const ULFFKioskSettings&)` |
| `Public/Journey/LFFInactivityTracker.h` | `struct FLFFInactivityTracker` (plain C++, time injected) | Idle → Warning → Reset decision | `Configure(Warn, Countdown, PausedWarn)`, `NotifyActivity(double Now)`, `Evaluate(double Now, bool bPaused, bool bPristine)` → `ELFFInactivityState {Active, Warning, ResetDue}`, `GetSecondsUntilReset(Now)` |
| `Public/Journey/LFFJourneySubsystem.h` | `ULFFJourneySubsystem : UWorldSubsystem` | Owns the state machine, tracker and audio director. Registries for rig, targets and labels. Runs transitions, the 1 Hz inactivity timer and the kiosk reset. Builds the snapshot. | `UFUNCTION(BlueprintCallable)`: `Next`, `Back`, `TogglePause`, `Pause`, `Resume`, `Restart`, `SetSettingsOpen(bool)`, `NotifyUserActivity`, `DismissInactivityWarning`, `PlayNoisePreview`, `StopNoisePreview`. `UFUNCTION(BlueprintPure)`: `GetSnapshot`, `GetCurrentStep(FLFFJourneyStep&)` → bool. `UPROPERTY(BlueprintAssignable) FLFFOnJourneyChanged OnJourneyChanged` (DYNAMIC_MULTICAST, `const FLFFJourneySnapshot&`). Native: `RegisterCameraRig`/`Unregister…`, `RegisterCameraTarget`/`Unregister…`, `RegisterLabelMarker`/`Unregister…`, `DebugGoToStep(FName)`. Overrides: `DoesSupportWorldType` (Game and PIE only, **VERIFY**), `Initialize`, `Deinitialize`. The journey starts when the rig registers; targets resolve lazily at each transition, so `BeginPlay` order doesn't matter. |
| `Public/Journey/LFFAudioDirector.h` | `ULFFAudioDirector : UObject` (owned by the subsystem) | One 2D bed with crossfade and an opt-in noise preview. Does nothing while sound is off. | `SetBed(TSoftObjectPtr<USoundBase>)`, `SetSoundEnabled(bool)`, `PlayPreview(TSoftObjectPtr<USoundBase>)`, `StopAll(float FadeSeconds)`. Uses `UGameplayStatics::SpawnSound2D` and `UAudioComponent::FadeIn`/`FadeOut`. |
| `Public/Settings/LFFKioskSettings.h` | `ULFFKioskSettings : UDeveloperSettings` (`Config=Game, defaultconfig`) | Operator defaults (§3.4) | `static const ULFFKioskSettings* Get()` → `GetDefault<>` |
| `Public/Settings/LFFSettingsSubsystem.h` | `ULFFSettingsSubsystem : UGameInstanceSubsystem` | The current visitor's settings, held in memory only (no SaveGame) | `GetSettings()`, `SetReducedMotion(bool)`, `SetSoundEnabled(bool)`, `SetTextScale(ELFFTextScale)`, `SetCrowdPreview(bool)`, `ResetToKioskDefaults()`, `FLFFOnSettingsChanged OnSettingsChanged` (dynamic multicast) |
| `Public/Camera/LFFTransitionMath.h` *(h)* | `struct FLFFTransitionMath` (static) | Pure pose interpolation with roll forced to 0 | `static FTransform Evaluate(const FTransform& From, const FTransform& To, float Alpha, float EaseExp = 2.f)` using `FMath::InterpEaseInOut` and `FQuat::Slerp` (from the existing code) |
| `Public/Camera/LFFCameraRig.h` | `ALFFCameraRig : ACameraActor` | Applies motion only. Ticks only during `Ease` (`bStartWithTickEnabled=false`, `SetActorTickEnabled`). `ReducedMotion` uses `APlayerCameraManager::StartCameraFade` plus an FTimer cut. | `BeginTransition(const ALFFCameraTarget*, ELFFTransitionProfile, float Seconds, uint32 Id)`, `SetTransitionPaused(bool)`, `FinishNow()`, `SnapTo(const ALFFCameraTarget*)`, `FadeHold(bool bOut, float Seconds)`. Native delegate `OnTransitionFinished(uint32 Id)`. Registers in `BeginPlay`. |
| `Public/Camera/LFFCameraTarget.h` | `ALFFCameraTarget : AActor` | A named camera pose, attached under the Cesium-owned stadium root. Editor arrow and camera icon. Never ticks. | `UPROPERTY(EditInstanceOnly) FName TargetId`; `float FieldOfViewDeg = 70`. Registers and unregisters in `BeginPlay`/`EndPlay`. |
| `Public/World/LFFLabelMarker.h` | `ALFFLabelMarker : AActor` (from the stop marker) | A placed placeholder label, sign or mesh. Hidden unless the current step lists it. | `FName LabelId`, `FText LabelText`, sprite and mesh properties; `SetLabelVisible(bool)` |
| `Public/Core/LFFGameMode.h` | `ALFFGameMode : AGameModeBase` | `DefaultPawnClass = nullptr` (**VERIFY** that no spawn warning appears), `PlayerControllerClass` comes from the BP | — |
| `Public/Core/LFFPlayerController.h` | `ALFFPlayerController : APlayerController` | Adds IMCs, binds IAs and forwards them to the subsystem. Creates the HUD, sets view target and input mode. Swaps the IMC when a modal opens. | `EditDefaultsOnly`: `TObjectPtr<UInputMappingContext> JourneyContext, DebugContext`; `TObjectPtr<UInputAction> NextAction, BackAction, PauseAction, RestartAction, SettingsAction, DebugAction`; `TSubclassOf<ULFFHUDWidget> HUDClass`. Overrides `BeginPlay`, `SetupInputComponent`. |
| `Public/UI/LFFHUDWidget.h` | `ULFFHUDWidget : UUserWidget` (Abstract) | Binds `OnJourneyChanged`/`OnSettingsChanged`, pushes data to children, handles button clicks and shows or hides modals from the snapshot | `BindWidget`: `StepCard`, `Disclaimer`. `BindWidgetOptional` (`UButton`): `BackButton`, `NextButton`, `StartAgainButton`, `PauseButton`, `SettingsButton`, `SoundButton`, `NoisePreviewButton`. `BindWidgetOptional`: `SettingsPanel`, `PauseOverlay`, `StillTherePanel`, `DebugOverlay`. `BlueprintImplementableEvent OnSnapshotChanged(const FLFFJourneySnapshot&)` for visual-only reactions. |
| `Public/UI/LFFStepCardWidget.h` | `ULFFStepCardWidget : UUserWidget` | Title, body, "Step 2 of 4", preview image | `SetStep(const FLFFJourneyStep&, int32 Index, int32 Count)`; `BindWidget UTextBlock TitleText, BodyText, ProgressText`; `BlueprintImplementableEvent OnStepSet` |
| `Public/UI/LFFDisclaimerWidget.h` | `ULFFDisclaimerWidget : UUserWidget` | Banner. **It can't be hidden** while the level isn't `Verified`; C++ forces `Visible`. | `SetLevel(ELFFDisclaimerLevel)`; `BindWidget UTextBlock DisclaimerText` (a required binding, so a WBP without it fails to compile) |
| `Public/UI/LFFSettingsWidget.h` | `ULFFSettingsWidget : UUserWidget` | Large two-state toggles that apply immediately | `BindWidget UButton ReducedMotionButton, SoundButton, TextSizeButton, DoneButton`; `BindWidgetOptional CrowdPreviewButton`. Labels read state ("Sound: Off"). |
| `Private/Debug/LFFDebugCommands.cpp` | cvars and console commands (non-Shipping only) | Debug hooks | `LFF.Debug 0/1`, `LFF.GoToStep <StepId>`, `LFF.TriggerInactivity`, `LFF.ValidateJourney` (checks row↔target resolution and the stadium-root attachment), `LFF.InactivitySeconds <n>`. Uses `FAutoConsoleCommandWithWorldAndArgs` and `TAutoConsoleVariable`. |
| `Private/Tests/LFFJourneyStateMachine.spec.cpp` | Automation Spec `LFF.Journey.*` | Rules and state tests (§7) | `BEGIN_DEFINE_SPEC(..., EAutomationTestFlags::ApplicationContextMask \| EAutomationTestFlags::ProductFilter)` (**VERIFY** the 5.8 flag spelling) |
| `Private/Tests/LFFJourneyData.spec.cpp` | Automation Spec `LFF.Data.*` | Checks the JSON source and table for drift | Editor context |

`WBP_LFF_PauseOverlay`, `WBP_LFF_StillThere` and `WBP_LFF_DebugOverlay` are plain `UUserWidget` Blueprints. Their buttons call `ULFFJourneySubsystem` commands. They hold no state.

## 3. USTRUCT / UENUM fields (`LFFJourneyTypes.h`)

### 3.1 Enums (all `UENUM(BlueprintType)`, `uint8`)
- `ELFFDisclaimerLevel`: `Verified`, `Approximate`, `Illustrative`
- `ELFFTransitionProfile`: `Instant` (cut), `Ease` (fly), `ReducedMotion` (fade, cut, fade)
- `ELFFJourneyPhase`: `Empty`, `Transitioning`, `AtStep`
- `ELFFCommandResult`: `Accepted`, `RejectedAtStart`, `RejectedAtEnd`, `RejectedDisabledByStep`, `RejectedPaused`, `RejectedEmpty`, `RejectedInvalidIndex`, `RejectedDebounced`
- `ELFFRestartReason`: `User`, `Inactivity`, `Debug`
- `ELFFTextScale`: `Standard`, `Large` (×1.25)

### 3.2 `FLFFJourneyStep : FTableRowBase`
The row name is `StepId`, via `ImportKeyField="StepId"` (**VERIFY**).

| Field | Type | Default | Notes / validation |
|---|---|---|---|
| `StepId` | `FName` | None | Unique, kebab-case, equals the row name |
| `TitlePlainLanguage` | `FText` | — | Required; warning if over 60 chars |
| `BodyPlainLanguage` | `FText` (MultiLine) | — | Required; warning if over 280 chars or over 3 sentences |
| `DisclaimerLevel` | `ELFFDisclaimerLevel` | `Illustrative` | The safest level is the default |
| `VerificationSource` | `FString` | "" | **Error** if empty when `Verified` (who and when verified it) |
| `CameraTargetId` | `FName` | None | Required; must resolve to exactly one `ALFFCameraTarget` (checked by the runtime and `LFF.ValidateJourney`) |
| `TransitionProfile` | `ELFFTransitionProfile` | `Ease` | Reduced motion turns `Ease` into `ReducedMotion` |
| `TransitionSeconds` | `float` | 0 | 0 means the kiosk default; clamped to `[MinTransitionSeconds, MaxTransitionSeconds]` |
| `AudioBed` | `TSoftObjectPtr<USoundBase>` | null | Null means silence. Plays only if sound is on. |
| `bNoisePreviewAllowed` | `bool` | false | Shows an opt-in "Hear the crowd" button; never autoplays |
| `NoisePreviewSound` | `TSoftObjectPtr<USoundBase>` | null | Required if the preview is allowed; volume capped by `PreviewMaxVolume` |
| `bCrowdDensityAllowed` | `bool` | false | Crowd placeholder shown only if this **and** the setting are on. There's no crowd system in V1; a snapshot flag is reserved for it. |
| `LabelsToShow` | `TArray<FName>` | [] | Each entry must match an `ALFFLabelMarker.LabelId` (warning if not) |
| `bNextEnabled` | `bool` | true | At the last step Next becomes "Start again" regardless |
| `bBackEnabled` | `bool` | true | Forced off at index 0 |
| `PreviewImage` | `TSoftObjectPtr<UTexture2D>` | null | Kept from the scaffold |

### 3.3 `FLFFVisitorSettings` and `FLFFJourneySnapshot` (BlueprintType)
- **Settings:** `bool bReducedMotion=false`, `bool bSoundEnabled=false`, `bool bCrowdPreview=false`, `ELFFTextScale TextScale=Standard`.
- **Snapshot:** `int32 StepIndex`, `int32 StepCount`, `FName StepId`, `ELFFJourneyPhase Phase`, `bool bPaused`, `bool bAtEnd`, `bool bCanNext`, `bool bCanBack`, `ELFFDisclaimerLevel EffectiveDisclaimer` (the stricter of from/to while moving), `ELFFTransitionProfile ResolvedProfile`, `bool bSettingsOpen`, `bool bInactivityWarning`, `float InactivityCountdownSeconds`, `bool bNoisePreviewAvailable`, `bool bCrowdVisible`, `bool bTargetMissing`.

### 3.4 `ULFFKioskSettings` (`[/Script/VirtualStadium.LFFKioskSettings]` in `DefaultGame.ini`)
- `TSoftObjectPtr<UDataTable> JourneyTable` = `/Game/LFF/Data/DT_LFF_GoldenPath`
- `FLFFVisitorSettings DefaultVisitorSettings`
- `FText DisclaimerApproximate` = "Approximate view. This is not a navigation instruction for the real stadium."
- `FText DisclaimerIllustrative` = "Illustration only. This is not a navigation instruction for the real stadium."
- Timing (seconds):
  - `DefaultEaseSeconds=3.0`
  - `MinTransitionSeconds=0.5`
  - `MaxTransitionSeconds=6.0`
  - `ReducedMotionFadeSeconds=0.35`
  - `ResetFadeSeconds=1.0`
  - `CommandDebounceSeconds=0.25`
- Inactivity (seconds): `InactivityWarningSeconds=120`, `InactivityCountdownSeconds=30`, `PausedInactivityWarningSeconds=300` (lead decision D12)
- Audio: `PreviewMaxVolume=0.5`, `BedVolume=0.3`
- `FLinearColor FadeColor=(0.08,0.08,0.10)` (neutral, not black)
- `bool bShowMouseCursor=true`
- `bool bDebugOverlayAtStartup=false` (ignored in Shipping)

## 4. Golden-path rows (`SourceData/Journey/LFF_GoldenPath.json`)

| # | StepId | CameraTargetId | Disclaimer | Profile | Labels | Back |
|---|---|---|---|---|---|---|
| 1 | `exterior-arrival` | `CT_ExteriorArrival` | Approximate | Ease | `LBL_Plaza` | off |
| 2 | `gate-entry` | `CT_GateEntry` | Illustrative | Ease | `LBL_Gate` | on |
| 3 | `interior-concourse` | `CT_InteriorConcourse` | Illustrative | Ease | `LBL_Concourse` | on |
| 4 | `seat-field-view` | `CT_SeatFieldView` | Approximate | Ease | `LBL_Section` | on |

Row format: an array of objects, with keys equal to the UPROPERTY names. Enums are string names, soft pointers are object-path strings, and FText values are plain strings. **Canonical format:** create one row in the editor, run *Export as JSON*, and copy its exact key spelling. For example, whether bool keys keep the `b` prefix is **VERIFY**.

## 5. Assets (`/Game/LFF/...`, created on the engine workstation)

| Path | Type | Notes |
|---|---|---|
| `Maps/L_LFF_Stadium` | Level | Shared with the Cesium agent. No Level Blueprint logic. |
| `Data/DT_LFF_GoldenPath` | DataTable (`FLFFJourneyStep`) | Imported from the SourceData JSON. Reimport, never hand-edit. |
| `Input/IA_LFF_Next`, `IA_LFF_Back`, `IA_LFF_Pause`, `IA_LFF_Restart`, `IA_LFF_Settings`, `IA_LFF_DebugToggle` | InputAction (Digital bool) | — |
| `Input/IMC_LFF_Journey`, `Input/IMC_LFF_Debug` | InputMappingContext | Debug is added only in non-Shipping builds |
| `Core/BP_LFF_GameMode`, `Core/BP_LFF_PlayerController` | Data-only BPs | Asset references only, no graph logic |
| `World/BP_LFF_LabelMarker` | BP of `ALFFLabelMarker` | Neutral sprite and mesh defaults |
| `UI/WBP_LFF_HUD`, `WBP_LFF_StepCard`, `WBP_LFF_Disclaimer`, `WBP_LFF_Settings`, `WBP_LFF_PauseOverlay`, `WBP_LFF_StillThere`, `WBP_LFF_DebugOverlay` | Widget BPs | Hierarchy in §6.2 |
| `UI/Fonts/F_LFF_Body` | Font | Accessible sans; the choice and licence are an open question |
| `Audio/SC_LFF_Bed`, `SC_LFF_Preview`; `Audio/Placeholders/SW_LFF_Bed_*` | SoundClass / SoundWave | Placeholder, licence-clean |
| `Placeholders/T_LFF_Preview_*` | Texture2D | Neutral art (A5) |

Placed in the level (not assets): 4 `ALFFCameraTarget` (`CT_*`), about 4 `BP_LFF_LabelMarker` (`LBL_*`) and 1 `ALFFCameraRig`. All targets and labels are attached under the stadium root (tag `LFF_SiteRoot`, owned by the Cesium agent).

## 6. Input and UI

### 6.1 Input mapping (`IMC_LFF_Journey`)
Touch and mouse go through UMG buttons only. There are no gestures in V1, to avoid accidental swipes.

| IA | Keyboard | Gamepad | Trigger | Subsystem call |
|---|---|---|---|---|
| `IA_LFF_Next` | Right, Space, PageDown (presenter clickers) | D-pad Right, RB | Pressed | `Next()` (or `Restart()` if `bAtEnd`) |
| `IA_LFF_Back` | Left, PageUp, Backspace | D-pad Left, LB | Pressed | `Back()` |
| `IA_LFF_Pause` | P, Esc | Menu/Start | Pressed | `TogglePause()` |
| `IA_LFF_Settings` | S | View/Select | Pressed | `SetSettingsOpen(true)` |
| `IA_LFF_Restart` | R | Y | **Hold 1.0 s** (`UInputTriggerHold`) | `Restart(User)` |
| `IA_LFF_DebugToggle` (`IMC_LFF_Debug`) | Ctrl+Shift+D | — | Chord | toggles `LFF.Debug` |

Rules:
- While a modal is open, the PC removes `IMC_LFF_Journey`, sets UI-only input mode and focuses the first modal button. A/Enter activates, and B/Esc closes the modal (`NativeOnKeyDown`).
- With no modal open, HUD buttons are `IsFocusable=false`, so Space can't both click a button and trigger Next (**VERIFY**).
- `DefaultInput.ini`:
  - `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`
  - `DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent`
  - `DefaultTouchInterface=None` (no virtual joystick)
  - **VERIFY** whether these are already the 5.8 defaults.

### 6.2 Widget hierarchy and hit targets
Sizes are in Slate units at the 1920×1080 DPI reference. Scale via the DPI curve on 4K.

```
WBP_LFF_HUD (ULFFHUDWidget)
├─ TopBar: PauseButton, SoundButton ("Sound: Off"), SettingsButton   ≥120×120 each, gap ≥32, icon + text
├─ WBP_LFF_Disclaimer (always visible for Approx/Illus; top-centre; ≥28 pt; neutral colour, not red; no animation)
├─ WBP_LFF_StepCard: ProgressText "Step 2 of 4" | Title ≥48 pt | Body ≥32 pt | PreviewImage
├─ NavBar: BackButton ≥240×120 (bottom-left) … NextButton / StartAgainButton ≥320×120 (bottom-right); ≥400 apart
├─ NoisePreviewButton ≥240×120 (shown only if bNoisePreviewAvailable; label says it plays sound)
└─ Modal layer (only one at a time, dimmed backdrop): WBP_LFF_Settings (toggle rows ≥480×120, Done ≥320×120)
   | WBP_LFF_PauseOverlay (Resume, Start again, Settings ≥320×120) | WBP_LFF_StillThere ("I'm still here" ≥480×140,
   calm bar, no flashing digits) | WBP_LFF_DebugOverlay (non-Shipping only, non-interactive)
```

Global rules:
- Every interactive element is at least 96×96, with a gap of at least 24.
- Buttons never change position between steps.
- Text contrast is at least 7:1.
- Large text scale is ×1.25.
- Buttons use `TouchMethod=PreciseTap` (**VERIFY** the name).
- Controls stay in the lower-reach band if the display mounting requires it (open question).

## 7. Tests (Automation Spec, headless)

**`LFF.Journey.StateMachine`:**
- Load 4 steps and land at index 0, `AtStep`.
- `Next` at the end returns `RejectedAtEnd` with the index unchanged (**no wrap**).
- `Back` at 0 returns `RejectedAtStart`.
- A step with `bNextEnabled=false` returns `RejectedDisabledByStep`.
- While paused, Next and Back return `RejectedPaused`. `Restart` is accepted and unpauses.
- Next during `Transitioning` is accepted and increments the transition Id.
- A stale `NotifyTransitionFinished` is ignored.
- An empty load returns `RejectedEmpty` for every command.

**`LFF.Journey.Rules`:**
- The 3×2 `ResolveTransitionProfile` table.
- `StricterDisclaimer`.
- `ShouldShowDisclaimer` is true for Approximate and Illustrative.
- `ClampDuration`.

**`LFF.Journey.Inactivity`:**
- Warning at 60 s, reset due at 75 s.
- Activity resets the timer.
- Pause uses 180 s.
- A pristine state never warns.

**`LFF.Camera.TransitionMath`:**
- Alpha 0 and 1 return the exact endpoints.
- The output is monotonic.
- Roll is always 0.

**`LFF.Data`:**
- The repo JSON parses via `UDataTable::CreateTableFromJSONString` into a transient table with zero problems and passes `Validate`.
- **Drift:** its rows equal `DT_LFF_GoldenPath` field by field.
- Every non-Verified step has a disclaimer. There are no duplicate IDs. A `Verified` step without a source fails.

Run command:

```
UnrealEditor-Cmd VirtualStadium.uproject -ExecCmds="Automation RunTests LFF.;Quit" -unattended -nullrhi -nopause -log
```

**VERIFY** these flags on 5.8.

## 8. Authoring workflow
1. Edit the copy in `SourceData/Journey/LFF_GoldenPath.json` and open a PR, where reviewers read the diff.
2. On the workstation, reimport `DT_LFF_GoldenPath` and run `LFF.Data` tests.
3. To add or move a camera pose:
   - Place or select a `CT_*` target under the stadium root.
   - Pilot it and frame the shot.
   - Set `TargetId`.
4. Run `LFF.ValidateJourney` in PIE and fix any missing targets or labels.
5. Commit the `.umap` and `.uasset` changes (LFS per LEDGER F12) together with the JSON.
6. Never put coordinates in the JSON. Never reorder rows in the editor alone.

## 9. File-level implementation order (golden path)
1. `VirtualStadium.Build.cs`: add `EnhancedInput`, `DeveloperSettings`.
2. `Public/Data/LFFJourneyTypes.h`. Delete `StadiumScheduleStop.h` and `StadiumScheduleDataAsset.*`.
3. `Journey/LFFJourneyStateMachine.*`, `Journey/LFFInactivityTracker.*`, `Data/LFFJourneyValidation.*`, `Camera/LFFTransitionMath.h`.
4. `Private/Tests/LFFJourneyStateMachine.spec.cpp`. **Gate:** compile plus the headless tests pass.
5. `Settings/LFFKioskSettings.*`, `Settings/LFFSettingsSubsystem.*`, plus `DefaultGame.ini` section.
6. `Camera/LFFCameraTarget.*`, then `Camera/LFFCameraRig.*` (made by `git mv` of `StadiumTourCamera.*`, then stripped).
7. `Journey/LFFAudioDirector.*`, then `Journey/LFFJourneySubsystem.*`.
8. `World/LFFLabelMarker.*` (via `git mv` of `StadiumStopMarker.*`).
9. `Core/LFFGameMode.*`, `Core/LFFPlayerController.*`, plus `DefaultInput.ini` (legacy removed) and `DefaultEngine.ini` (`GameDefaultMap`, `GlobalDefaultGameMode=/Game/LFF/Core/BP_LFF_GameMode.BP_LFF_GameMode_C`).
10. `UI/LFFHUDWidget.*` (via `git mv` of `StadiumTourWidget.*`), `UI/LFFStepCardWidget.*`, `UI/LFFDisclaimerWidget.*`, `UI/LFFSettingsWidget.*`.
11. `Private/Debug/LFFDebugCommands.cpp`, `Private/Tests/LFFJourneyData.spec.cpp`.
12. `SourceData/Journey/LFF_GoldenPath.json` (via `git mv` of the template) and `LFF_Backlog.json`.
13. Workstation work: import the DT, create the IA/IMC and data-only BPs, create the WBPs, and place `CT_*`, `LBL_*` and the rig in `L_LFF_Stadium`. **Gate:** the PIE checklist in ADR-0002.
14. `Docs/ManualSetup.md` §3–7 rewrite, a LEDGER update and a CHANGELOG entry.
