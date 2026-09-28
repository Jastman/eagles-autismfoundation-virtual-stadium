# Journey schema (V1, as written)

Status: **Written, not yet compiled** (2026-09-28).
- Scope and rationale: `Docs/ADR/ADR-0003-v1-minimal-scope.md`. It amends ADR-0002.
- Items marked **VERIFY** need a UE 5.8.3 compile or a PIE run to confirm.
- C++ types use `LFF` with no underscore. Assets use `Type_LFF_Name` under `/Game/LFF/`.

## Code map (`Source/VirtualStadium/`)

| File | Type | Responsibility |
|---|---|---|
| `Public/Journey/LFFJourneyTypes.h` | UENUMs, USTRUCTs | `FLFFJourneyStep` (Data Table row), `FLFFVisitorSettings`, `FLFFJourneySnapshot`, and the enums below |
| `Public/Journey/LFFJourneyStateMachine.h` / `Private/...cpp` | Plain C++ | Intro → steps. No wrap. Pause. Stale transition ids are ignored. `LFFJourneyRules` covers profile resolution, duration clamping and when to show the disclaimer. |
| `Public/Journey/LFFJourneySubsystem.h` / `.cpp` | `UWorldSubsystem` | Active only under `ALFFGameMode`. Loads `DT_LFF_GoldenPath`, finds targets, spawns the rig if the level has none, owns settings, modals, labels and audio, and broadcasts `OnJourneyChanged(Snapshot)`. |
| `Public/Camera/LFFCameraRig.h` / `.cpp` | `ACameraActor` | Ease (tick only while moving), ReducedMotion (fade, cut, fade via `StartCameraFade`), or Instant. Roll is always 0. |
| `Public/Camera/LFFCameraTarget.h` / `.cpp` | `AActor` | `TargetId` and `FieldOfView`. Place it under `LFF_SiteRoot`. |
| `Public/Core/LFFGameMode.h` / `.cpp` | `AGameModeBase` | No pawn. The table path, copy (intro, badge, disclaimers) and timing live here as C++ defaults. |
| `Public/Core/LFFPlayerController.h` / `.cpp` | `APlayerController` | Builds the Enhanced Input actions and mapping context in code. Loads `WBP_LFF_HUD`. |
| `Public/UI/LFFHUDWidget.h` / `.cpp` | `UUserWidget` (Abstract) | Pushes the snapshot into named widgets, routes clicks and enforces the disclaimer. |
| `Private/Tests/LFFJourneyStateMachineTest.cpp` | Automation tests | `LFF.Journey.StateMachine.*`, `LFF.Journey.Rules` |

## Enums
- `ELFFDisclaimerLevel`: `Verified`, `Approximate`, `Illustrative`. All of V1 is `Illustrative`.
- `ELFFTransitionProfile`: `Instant`, `Ease`, `ReducedMotion`. Reduced motion or Calm Mode turns `Ease` into `ReducedMotion`.
- `ELFFJourneyPhase`: `Empty`, `Intro`, `Transitioning`, `AtStep`.
- `ELFFCommandResult`: `Accepted`, `RejectedEmpty`, `RejectedNotStarted`, `RejectedAlreadyStarted`, `RejectedAtStart`, `RejectedAtEnd`, `RejectedDisabledByStep`, `RejectedPaused`.

## `FLFFJourneyStep` (row of `/Game/LFF/Data/DT_LFF_GoldenPath`)
The source of truth is `SourceData/Journey/LFF_GoldenPath.json`. Reimport it; never hand-edit the table. The row `Name` becomes `StepId`.

| Field | Type | Default | Notes |
|---|---|---|---|
| `Title` | FText | — | Keep to 60 characters or fewer |
| `Body` | FText | — | Keep to 280 characters or fewer, at most 3 short sentences |
| `DisclaimerLevel` | enum | Illustrative | A step marked `Verified` with no `VerificationSource` is downgraded at load |
| `VerificationSource` | FString | "" | Who verified the step, and when |
| `CameraTargetId` | FName | None | Must match exactly one `ALFFCameraTarget` |
| `TransitionProfile` | enum | Ease | |
| `TransitionSeconds` | float | 0 | 0 means `DefaultEaseSeconds` (3). Clamped to 0.5–6 |
| `LabelsToShow` | FName[] | [] | Actors tagged `LFF_Label` plus one of these names are visible during the step |
| `AudioBed` | soft `USoundBase` | none | Plays only when Sound is on |
| `PreviewImage` | soft `UTexture2D` | none | Optional. The Blueprint may show it |
| `bNextEnabled` / `bBackEnabled` | bool | true | Omitted from the JSON, so the defaults apply |

## Snapshot (`FLFFJourneySnapshot`), pushed on every change
- Position: `Phase`, `StepIndex`, `StepCount`, `StepId`.
- Content: `Title`, `Body`, `ProgressText` ("Step 2 of 4"), `PreviewImage`.
- Disclaimer: `DisclaimerLevel`, `DisclaimerText`, `bShowDisclaimer`, `PrototypeBadgeText`.
- State: `bStarted`, `bPaused`, `bAtEnd`, `bCanNext`, `bCanBack`.
- Overlays and settings: `bSettingsOpen`, `bRestartConfirmOpen`, `bTargetMissing`, `Settings`.

## Behaviour
- **Intro.** It shows the intro copy over the first step's view. Next or Start begins the journey with no camera move.
- **Next and Back.** They never wrap. At the last step, Next becomes **Start again**, which returns to the intro with a fade.
- **Pause.** Next and Back are blocked, and an Ease move freezes in place.
- **Restart (R, controller Y, or the button)** asks **"Start again?"** first. Confirm (Enter or controller A) means yes; Cancel (Esc or controller B) means no.
- **Settings are kept across restart** and reset only when the app launches. Nothing is saved.
- **Missing target.** The step still shows, with "This step is not available yet". The camera stays put and an error is logged.
- **Calm Mode** turns on reduced motion and halves the sound volume. The Blueprint applies a muted theme.

## Input (built in `ALFFPlayerController::BuildInputObjects`)

| Action | Keyboard | Controller |
|---|---|---|
| Next / Start | Right, Space, PageDown | D-pad Right, RB |
| Back | Left, Backspace, PageUp | D-pad Left, LB |
| Confirm (yes, else Next) | Enter | A |
| Cancel (no, close, resume) | Esc | B |
| Pause / Resume | P | Menu (Start) |
| Settings | S | View (Back) |
| Start again? | R | Y |
| Less movement / Sound / Calm Mode / Large text | 1 / 2 / 3 / 4 | — (mouse or keyboard) |

Esc also ends PIE in the editor. That's expected there; test Cancel in a packaged build or with the controller.

## `WBP_LFF_HUD` widget names (parent class `LFFHUDWidget`)
- **Required.** The Blueprint won't compile without these: `DisclaimerText`, `PrototypeBadgeText`, `TitleText`, `BodyText` (all `TextBlock`).
- **Optional text:** `ProgressText`, `TargetMissingText`, `NextLabel`, `PauseLabel`, `ReducedMotionLabel`, `SoundLabel`, `CalmModeLabel`, `LargeTextLabel`.
- **Optional panels** (any widget type): `DisclaimerPanel`, `IntroPanel`, `PausePanel`, `SettingsPanel`, `RestartConfirmPanel`.
- **Optional buttons:** `NextButton`, `BackButton`, `PauseButton`, `ResumeButton`, `SettingsButton`, `SettingsCloseButton`, `RestartButton`, `RestartYesButton`, `RestartNoButton`, `ReducedMotionButton`, `SoundButton`, `CalmModeButton`, `LargeTextButton`.

Layout rules (from `ACCESSIBILITY.md`):
- Buttons are at least 120×120 at 1080p, and never move between steps. They're disabled rather than hidden.
- Every button has text plus an optional icon and shows its key hint, for example "Next (→)".
- Contrast is at least 7:1, body text at least 32 pt and titles at least 48 pt.
- Leave the bottom strip clear for the Cesium and Google credits.
