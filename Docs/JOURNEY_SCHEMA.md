# Journey schema (V1, as written)

Status: compiled and tested on the Mac (UE 5.8.3). Session 3 (2026-09-30) added voice, the splash orbit, the timeline, scene cues and the ticket phone.
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
| `Public/World/LFFCueActor.h` / `.cpp` | `AActor` | A moving scene piece driven by a step's `SceneCues`: a gate leaf, the arriving car, the eagle's pass. Placed at its end pose; moves once from `StartOffset`, never loops. Ticks only while moving. |
| `Public/World/LFFFireworksActor.h` / `.cpp` | `ALFFCueActor` | Soft fireworks for the anthem preview: slow bursts of instanced glowing spheres that open, drift and shrink. Steady brightness, no flashes. |
| `Public/Core/LFFGameMode.h` / `.cpp` | `AGameModeBase` | No pawn. The table path, copy (intro, badge, disclaimers) and timing live here as C++ defaults. |
| `Public/Core/LFFPlayerController.h` / `.cpp` | `APlayerController` | Builds the Enhanced Input actions and mapping context in code. Loads `WBP_LFF_HUD`. |
| `Public/UI/LFFHUDWidget.h` / `.cpp` | `UUserWidget` (Abstract) | Parent of `WBP_LFF_HUD`, the one HUD. Holds the five panels, picks which are on screen (splash, step, modal) and fades them in. Logs an error if the badge or disclaimer widgets are missing. |
| `Public/UI/LFFJourneyPanel.h` / `.cpp` | `UUserWidget` (Abstract) | Parent of the five panels. Pushes the snapshot into named widgets, builds the progress pips, applies Large text and High contrast, routes clicks and enforces the badge and disclaimer. |
| `Private/Tests/LFFJourneyStateMachineTest.cpp` | Automation tests | `LFF.Journey.StateMachine.*` (Navigation, PauseRestart, EdgeCases, Jump), `LFF.Journey.Rules` |

## Enums
- `ELFFDisclaimerLevel`: `Verified`, `Approximate`, `Illustrative`. Timed steps of the example game (session 3) are `Approximate`; every other step is `Illustrative`. Nothing is `Verified`.
- `ELFFTransitionProfile`: `Instant`, `Ease`, `ReducedMotion`. `LFFJourneyRules::ResolveMove` picks the move actually used: Calm Mode → `Instant` (cut); Less movement, or a straight path that crosses geometry → `ReducedMotion` (fade, cut, fade); otherwise the authored profile.
- `ELFFIntensityPreview`: `None`, `Sound`, `Lights`, `Anthem` (one pass of the eagle with soft fireworks; visual only).
- `ELFFCueState`: `Before`, `Play`, `After` (`LFFJourneyRules::ResolveCueState`). `ELFFCueTrigger` (on `ALFFCueActor`): `Arrival`, `Preview`.
- `ELFFJourneyPhase`: `Empty`, `Intro`, `Transitioning`, `AtStep`.
- `ELFFCommandResult`: `Accepted`, `RejectedEmpty`, `RejectedNotStarted`, `RejectedAlreadyStarted`, `RejectedAtStart`, `RejectedAtEnd`, `RejectedDisabledByStep`, `RejectedPaused`.

## `FLFFJourneyStep` (row of `/Game/LFF/Data/DT_LFF_GoldenPath`)
The source of truth is `SourceData/Journey/LFF_GoldenPath.json`. Reimport it; never hand-edit the table. The row `Name` becomes `StepId`.

| Field | Type | Default | Notes |
|---|---|---|---|
| `Title` | FText | — | Short: 2–4 words, 30 characters or fewer |
| `Body` | FText | — | The visual-schedule sentence, first person. At most 2 short sentences (120 characters) |
| `DisclaimerLevel` | enum | Illustrative | A step marked `Verified` with no `VerificationSource` is downgraded at load |
| `ClockText` | FText | "" | Example-game clock ("~9:30 AM"), shown as "Step 3 · ~9:30 AM". Timed steps are `Approximate` |
| `TimelineLabel` | FText | "" | Non-empty puts the step on the timeline above Back/Next ("10:00 AM\nPlaza gates") |
| `VerificationSource` | FString | "" | Who verified the step, and when |
| `CameraTargetId` | FName | None | Must match exactly one `ALFFCameraTarget` |
| `TransitionProfile` | enum | Ease | |
| `TransitionSeconds` | float | 0 | 0 means `DefaultEaseSeconds` (3). Clamped to 0.5–6 |
| `LabelsToShow` | FName[] | [] | Actors tagged `LFF_Label` plus one of these names are visible during the step, unless the label would land in the lower third of the step's framing |
| `AudioBed` | soft `USoundBase` | none | Overrides the game mode's `AmbientBed` for this step. Plays only when Sound is on |
| `IntensityPreview` | enum | None | `None`, `Sound` or `Lights`. Offers "Preview this? Off / Soft" on the step; Off on every arrival |
| `PreviewSound` | soft `USoundBase` | none | Overrides the game mode's `PreviewSound` for a `Sound` preview |
| `PreviewImage` | soft `UTexture2D` | none | Optional. The Blueprint may show it |
| `VoiceWave` | soft `USoundBase` | none | The card read aloud (`/Game/LFF/Audio/VO/VO_NN_Name`). Missing = no voice, card stays |
| `SceneCues` | FName[] | [] | `ALFFCueActor` ids: arrival cues play when the camera arrives and stay done on later steps; preview cues play only on Soft |
| `bShowTicketPhone` | bool | false | The phone with the mobile ticket rises from the bottom of the screen |
| `bNextEnabled` / `bBackEnabled` | bool | true | Omitted from the JSON, so the defaults apply |

## Snapshot (`FLFFJourneySnapshot`), pushed on every change
- Position: `Phase`, `StepIndex`, `StepCount`, `StepId`.
- Content: `Title`, `Body`, `ProgressText` ("Step 2 of 23"), `ClockText`, `PreviewImage`, `bShowTicketPhone`.
- Timeline and voice: `TimelineMarks` (step index + label per timed step), `bVoicePlaying`.
- Disclaimer: `DisclaimerLevel`, `DisclaimerText` (splash sentence), `DisclaimerLine` (the one line under the badge on steps), `bShowDisclaimer`, `PrototypeBadgeText`.
- Preview: `IntensityPreview`, `bShowPreviewControls`, `bPreviewOn`, `bPreviewAvailable`.
- State: `bStarted`, `bPaused`, `bAtEnd`, `bCanNext`, `bCanBack`.
- Overlays and settings: `bSettingsOpen`, `bRestartConfirmOpen`, `bTargetMissing`, `Settings`.

## Behaviour
- **Splash (session 3).** The camera slowly orbits the stadium (one lap in `SplashOrbitSeconds`, 40 s) on a circle through `CT_StadiumWide`, around `LFF_SiteRoot`. Less movement or Calm Mode holds it still. **Start** glides along the orbit, the short way round, into step 1's wide shot (1.5–6 s by angle). Back on step 1 resumes the orbit from there; Restart fades to the wide shot, then orbits. No voice on the splash.
- **Next and Back.** They never wrap. At the last step, Next becomes **Start Over**, which returns to the splash with a fade and stops the voice.
- **Timeline marks** jump straight to their step (`JumpToStep`). Only steps with a `TimelineLabel` have a mark, so a mark can't point at a missing step. Only the current step's mark is emphasized.
- **Voice.** Off by default. With Voice and Sound on, the step's `VoiceWave` plays once, 0.5 s after the camera arrives, at `VoiceVolume` (0.3, a little under the bed), and the bed ducks by `VoiceDuckDecibels` (−8 dB). A new step, Back, a jump, Start Over, Pause, Sound off, Calm Mode or starting a preview stops it at once. Resume reads the card again. Sound off and Calm Mode also make the Voice toggle read Off and disable it. The card is the caption: it's always on screen, and the voice reads exactly its Title and Body.
- **Scene cues.** An arrival cue (`GatePlaza`, `GateClub`, `GateInner`, `CarArrival`) is `Before` until its step, `Play` on it (it moves once the camera arrives), and `After` on every later step. Less movement and Calm Mode show `After` straight away (the gate is already open, the car already parked). Preview cues (`Anthem`: the eagle and fireworks) play only while the anthem preview is Soft, then hide and turn the preview back to Off.
- **Crowd.** Low (default) shows the stand-in people tagged `LFF_Crowd`; Off or Calm Mode hides them.
- **Ticket phone.** On the ticket step it rises from below the screen edge over 1.2 s (ease-out), then the ticket frames play at 10 fps. Less movement and Calm Mode show it already up, on a still frame.
- **Learn More** (last step only) opens `LearnMoreUrl` in the browser.
- **Pause.** Next and Back are blocked, and an Ease move freezes in place.
- **Restart (R, controller Y, or the button)** asks **"Start again?"** first. Confirm (Enter or controller A) means yes; Cancel (Esc or controller B) means no.
- **Settings are kept across restart** and reset only when the app launches. Nothing is saved.
- **Missing target.** The step still shows, with "This step is not available yet". The camera stays put and an error is logged.
- **Sound.** Off (the default) sets the audio device's primary volume to 0, so nothing plays: beds, previews and voice. On plays one low bed, fading in over 2 s: the plaza murmur (`SW_LFF_AmbientPlaza`) by default, the soft cheer (`SW_LFF_CheerBed`) on the seat, kickoff and finale steps. Beds cross-fade over 1.5 s. No horns or fireworks in either.
- **Calm Mode** means Less movement on, Sound and Voice off, crowd actors (tag `LFF_Crowd`) hidden, gates already open, previews locked off and hidden, and camera cuts only. While it's on, the Sound and Less movement toggles read Off / On and are disabled; turning Calm Mode off restores the visitor's own choices.
- **Intensity previews** (flares, loud sounds, bright lights, anthem) never autoplay and never start under the voice (picking Soft stops it). Anthem: one 9 s pass of the eagle across the field with three soft firework bursts; it turns back to Off when done. "Preview this? Off / Soft" (button or V). Sound: an 8 s muffled sample (`SW_LFF_PreviewCrowdMuffled`) with the bed ducked; it needs Sound on (the button says so and is disabled otherwise), and it turns itself back to Off when it ends. Lights: exposure rises and falls as a smooth 4 s sine, up to +0.9 stops, and ramps back over 0.8 s when stopped. Nothing flickers. Pause, a step change, Sound off (for the Sound preview) and Calm Mode all turn a preview off.
- **No wall clipping.** Before an Ease, a 40 cm sphere is swept both ways along the straight path; if it hits anything, the move fades and cuts instead. It only sees geometry with collision (the concourse walls have none, so moves into the concourse are authored `ReducedMotion`).
- **World labels** stay above their prop, and any label that would fall in the lower third of the step's framing (where the caption card is) is hidden at runtime (`LFFJourneyRules::ShouldHideLabelAt`).
- **Back on step 1** returns to the splash. The splash sits on step 1's view, so the camera doesn't move.
- **Start again from Settings** closes Settings and asks "Start again?".
- **UI motion.** Panels fade in over 0.25 s with an ease-out; so does the caption on a new step. Reduced motion or Calm Mode makes every UI change an instant cut. Nothing slides or flies in.
- **High contrast** (Settings only, no key) makes the glass panels opaque black and darkens the scrim. **Large text** scales the splash and caption cards by 1.25.

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
| Voice / Crowd | 5 / 6 | — |
| Preview this? (intensity steps) | V | — |

Esc also ends PIE in the editor. That's expected there; test Cancel in a packaged build or with the controller.

## HUD widgets (`/Game/LFF/UI`, built by `Tools/Editor/build_wbp_lff_ui.py`)

`WBP_LFF_HUD` (parent `LFFHUDWidget`) is the one HUD. It must contain five panel instances with these names, or it won't compile:

| Instance | Widget | On screen | Contents |
|---|---|---|---|
| `TopChrome` | `WBP_LFF_TopChrome` | Always | Prototype badge (top left) with `DisclaimerLineText` under it on journey steps. Top bar (top right): `StepControls` (Step X of Y, Pause, Sound), shown after Start, then Settings. |
| `Splash` | `WBP_LFF_Splash` | Before Start | Centre-low card: "Welcome to", title, body, **Start Your Day**, the disclaimer sentence. |
| `StepCaption` | `WBP_LFF_StepCaption` | After Start | Small lower-left card (about 450 px wide at 1080p), above the timeline: step number and clock, disclaimer chip (Verified / Approximate / Illustration), title, body, and `PreviewRow` on intensity steps. `TicketPhone` (right of centre) on the ticket step. |
| `BottomRail` | `WBP_LFF_BottomRail` | After Start | `TimelinePanel` (the thin timeline above the rail), then Back, progress pips, Next (reads "Start Over" on the last step) and `LearnMorePanel` on the last step. |
| `Settings` | `WBP_LFF_Settings` | Settings, Paused or "Start again?" open | Scrim over the world, then one card: Settings (toggles, key legend, Start again), Paused, or the restart question. |

The five panels share the parent `LFFJourneyPanel`. Every name below is optional; a panel fills in the ones it contains.
- **Session 3 names:** `VoiceButton`/`VoiceLabel`, `CrowdButton`/`CrowdLabel` (Settings); `TimelinePanel`, `TimelineMarks` (a `HorizontalBox`; C++ adds one clickable mark per timed step), `TimelineNoteText`, `LearnMorePanel`, `LearnMoreButton` (BottomRail); `TicketPhone`, `TicketImage` (an `Image`; C++ plays `/Game/LFF/UI/Ticket/T_LFF_Ticket_NN`) (StepCaption). Added to existing widgets by `build_wbp_lff_ui.py --extend`.
- **Text:** `TitleText`, `BodyText`, `ProgressText` ("Step 2 of 16"), `StepNumberText` ("Step 2"), `DisclaimerText` (full sentence), `DisclaimerLineText` (the short line), `PreviewLabel`, `PreviewNoteText`, `DisclaimerChipText` (one word), `PrototypeBadgeText`, `TargetMissingText`, `NextLabel`, `PauseLabel`, `SoundLabel`, `ReducedMotionLabel`, `CalmModeLabel`, `LargeTextLabel`, `HighContrastLabel`.
- **Groups:** `StepControls`, `DisclaimerChip`, `SettingsPanel`, `PausePanel`, `RestartConfirmPanel` (only one modal card shows at a time), `PreviewRow`, `Scrim` (a `Border`; it blocks clicks), `ProgressPips` (a `HorizontalBox`; C++ adds one pip per step), `TextScale` (a `ScaleBox` set to User Specified).
- **Buttons:** `StartButton`, `NextButton`, `BackButton`, `PauseButton`, `ResumeButton`, `SoundButton`, `SettingsButton`, `SettingsCloseButton`, `RestartButton`, `RestartYesButton`, `RestartNoButton`, `ReducedMotionButton`, `CalmModeButton`, `LargeTextButton`, `HighContrastButton`, `PreviewButton`.
- **Naming conventions C++ reads:** a `Border` whose name starts with `Surface` turns opaque in High contrast.
- **Disclaimer rule.** C++ always shows `PrototypeBadgeText`, shows `DisclaimerChip` on every step, shows `DisclaimerText` whenever the level isn't Verified, and shows `DisclaimerLineText` ("Illustration. Not directions for the real stadium.") on every non-Verified step. `LFFHUDWidget` logs an error if `TopChrome` has no `PrototypeBadgeText` or `DisclaimerLineText`, `StepCaption` has no `DisclaimerChipText` or `Splash` has no `DisclaimerText`.

Layout rules (from `ACCESSIBILITY.md` and LEDGER D26):
- One primary (midnight green) action per screen: Start Your Day, Next, Resume, or "Yes, start again". Everything else is a dark secondary button.
- **The stadium is the product.** The chrome stays small: the caption card is about 450 × 180–240 px at 1080p (under a quarter of the lower-left), the top bar and badge are about 64 px tall.
- Hit targets at 1080p: 64 px everywhere (the project minimum), Start 72 px. Buttons never move between steps; they're disabled rather than hidden.
- Buttons show words only. The keyboard legend lives in Settings.
- Bone-white text on dark frosted glass (a background blur under a tint at 86% opacity). Contrast is above 7:1. Sizes follow ACCESSIBILITY.md at 1080p: caption title 30, body 22, controls 20–24, badge and disclaimer line 16.
- Leave the bottom 72 px clear for the Cesium and Google credits.
