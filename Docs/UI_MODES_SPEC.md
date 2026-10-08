# Historical HUD spec: Guided tour and Explore (2026-10-05)

**Superseded by the scene-first corner HUD on 2026-10-07.** Use `UI_DESIGN.md` for current presentation and `JOURNEY_SCHEMA.md` for current bindings. The active builder remains `Tools/Editor/build_wbp_lff_modes.py`, but it no longer builds the sidebar, central arc or separate bottom navigation described below. Tour/Explore, bookmarks, verification safeguards, flight and labelled controls are preserved; category filters now span levels and frame into the measured unobstructed viewport.

Historical status: **built** (2026-10-06), with these changes from the proposal below:
- **Guided tour:** no sidebar. The header (wordmark and mode switch) is its own opaque card top left; under it, one opaque panel holds the schedule column (one mark per timed step) beside the caption card. The scene shows around them.
- **Free explore:** the map is the stadium itself (callouts on the model, the picked level tinted), not a 2D plate in the sidebar; free flight replaces D4's turn-only view. The sidebar keeps the level buttons, place toggles and the picked place's card.
- Calm Mode sits beside Settings top right; there is no sidebar footer. D3 (render only right of the sidebar) wasn't built.
- `LFFHUDWidget.h` and `Docs/JOURNEY_SCHEMA.md` have the tree as built. The rest of this doc is the proposal as written.

Original status: **proposed**, not built. This is the fifth UI pass. It replaces the "Chapters" layout in `UI_DESIGN.md`: the caption on the scene, the chapter bar and the splash orbit. It keeps that doc's tokens, type, focus ring, karaoke and motion rules. If anything here conflicts with `ACCESSIBILITY.md`, `ACCESSIBILITY.md` wins. Items marked **VERIFY** need a UE 5.8.3 compile or a PIE run to confirm.

## Brief
- The bottom of the screen is cluttered: text, steps and buttons overlap one another and the scene. Separate them.
- **No text on the 3D scene.** Every line of text sits on an opaque dark surface.
- A new **Explore** mode beside the **Guided tour**, switchable at any time without losing your place in the tour.
- A welcome screen that asks which mode, with as few words as possible.
- Eagles midnight green, deep slates and off-whites. Nothing flashes or pulses.

## Decisions at a glance
These are the calls this spec makes. Each one can be reversed.

| # | Decision | Why |
|---|---|---|
| D1 | **Five arc nodes** (Arrival, Gates, Plaza, To my seat, Game). The old sixth chapter, "Breaks and help" (steps 19–23), folds into Game. | It matches the brief. Breaks and help is also what Explore now covers. Keeping six nodes is a one-line change to `ULFFJourneyPanel::Chapters`. |
| D2 | The **Tour / Explore switch lives at the top of the sidebar**, under the wordmark, and isn't in a header band across the screen. | The switch changes the sidebar, so it sits right above what it changes. It's on every screen after the welcome. It also leaves the top of the stage free for the arc. |
| D3 | **The 3D view renders only to the right of the sidebar** (a viewport rectangle). The UI doesn't cover it. | No text can ever sit over the scene. Rendering 31% fewer pixels helps the Mac. With Maintain Y FOV, the authored shots stay centred. |
| D4 | **Explore lets you turn, not walk.** "Show me" moves the camera to a named spot; you look around from there (±45° yaw, ±15° pitch). | Predictable, so nobody gets lost or motion-sick, and no new collision work is needed. Free flight is out of scope. |
| D5 | **The welcome screen is opaque and still.** The orbit is retired. The camera waits behind it at `CT_StadiumWide`. | This is what the brief asks for: choose before the stadium appears. It also hides Cesium tiles while they stream in. |
| D6 | **Unverified map markers don't ship.** A point of interest with no `VerificationSource` is dropped at load in packaged builds. | A wrong sensory-room location could send a distressed child the wrong way. The location is still an open question (`STAKEHOLDER_REVIEW.md`). |
| D7 | **Kelly also means "my seat".** It already means go/primary and the spoken word. | The seat beacon has to be the strongest thing on the map, and kelly is the strongest token we have. |

---

## 1. Wireframes

### Welcome
Minimal copy: two lines, two cards, one toggle.

```
WELCOME (full screen, opaque SURFACE; the camera is parked behind it and doesn't move)
+----------------------------------------------------------------------------------------------------------+
| (EAF) Game day guide                                                                                     |
|       Lincoln Financial Field                                                                            |
|                                                                                                          |
|                                                                                                          |
|                                                                                                          |
|                                            Welcome to the Linc.                                          |
|                                      How do you want to look around?                                     |
|                                                                                                          |
|                                                                                                          |
|                +--------------------------------+      +--------------------------------+                |
|                |                                |      |                                |                |
|                |  >  Guided tour                |      |  #  Explore                    |                |
|                |                                |      |                                |                |
|                |     Step by step, in order.    |      |     At your own pace.          |                |
|                |                                |      |                                |                |
|                |                         [ T ]  |      |                         [ E ]  |                |
|                +--------------------------------+      +--------------------------------+                |
|                                                                                                          |
|                                                                                                          |
|                                          Calm Mode  [ Off ]  [3]                                         |
|                                                                                                          |
|                                                                                                          |
|                                                                                           [ Settings ]   |
+----------------------------------------------------------------------------------------------------------+
```
- The two cards are equal: both are `RAISED`, 520 × 220, with an icon and a word. Neither one is kelly, so the screen doesn't push either choice. Keyboard focus starts on Guided tour, so Enter takes the tour.
- One click starts the chosen mode. There's no "select, then confirm" step.

### Guided tour and Free explore, side by side
`@` is the current node or My seat, `#…#` is the selected chip, `*` is "You are here", and Q / R / G are amenity markers.

```
GUIDED TOUR                                                          FREE EXPLORE
+----------------------+-----------------------------------------+   +----------------------+-----------------------------------------+
| (EAF) Game day guide |                          [ Settings ]   |   | (EAF) Game day guide |                          [ Settings ]   |
|   Lincoln Fin. Field | ``-._                         _.-''     |   |   Lincoln Fin. Field | ``-._                         _.-''     |
| [# Tour #][ Explore ]|      `o-----o-----@-----o-----o'        |   | [ Tour ][# Explore #]|      `o-----o-----@-----o-----o'        |
| =====================|   Arrival Gates Plaza  To my  Game      |   | =====================|   Arrival Gates Plaza  To my  Game      |
|                      |                    2 of 3  seat         |   | Stadium map          |                my place    seat         |
| 03 / Plaza           |                                         |   | Viewing: Lower conc. |                                         |
| 10:00 AM             |                                         |   | +-----+ +----------+ |                                         |
|                      |                                         |   | |Upper| | .------. | |                                         |
| I can visit the      |          3D STADIUM VIEW                |   | |Suite| |/  Q   R \| |      3D VIEW AT THE CHOSEN SPOT         |
| Popcorn for the      |                                         |   | |Club | ||  @  *  || |                                         |
| People stand on      |   rendered only in this column;         |   | |#Low#| |\  G     /| |   drag, or <- ->, to look around        |
| Pepsi Plaza.         |   nothing is drawn under the            |   | |Field| | '------' | |   (turn only, no walking)               |
|                      |   sidebar                               |   | +-----+ +----------+ |                                         |
| (body: 26 px, INK on |                                         |   | @ My seat  * You     |                                         |
|  opaque SURFACE)     |                                         |   | Show on map          |                                         |
|                      |                                         |   | [x]Sensory [x]Quiet  |                                         |
| [ Preview? Off|Soft ]|                                         |   | [ ]Guest   [ ]Restrm |                                         |
|   (intensity steps)  |                                         |   | [ ]Food    [Hide all]|                                         |
|                      |                                         |   |                      |                                         |
|                      |                                         |   | ---------------------|                                         |
|                      |                                         |   | Q Quiet zone         |                                         |
|                      | +-----------+         +-----------+     |   | Sec 112 [ Show me ]  | +-----------+   +-----------------+     |
|                      | |  < Back   |         |  Next  >  |     |   |                      | |  My seat  |   | Resume tour  >  |     |
| =====================| +-----------+         +-----------+     |   | =====================| +-----------+   +-----------------+     |
| [Voice On] [Calm Off]|                     Cesium | Google     |   | [Voice On] [Calm Off]|                     Cesium | Google     |
+----------------------+-----------------------------------------+   +----------------------+-----------------------------------------+
```

What stays put between the two modes: the sidebar width, the wordmark, the mode switch, the footer toggles, the arc, Settings, and the bottom button slots. Only the **sidebar content** and the **labels in the two bottom slots** change. This is deliberate: a mode switch should feel like turning a page, not moving to a new room.

### Pixel grid (1920 × 1080 reference; DPI scaling handles the rest)

| Region | Rect (x, y, w, h) | Surface | Contents |
|---|---|---|---|
| Sidebar | 0, 0, 600, 1080 | `SURFACE` at 100% | Everything with prose |
| ↳ Header | 32, 24, 536, 152 | — | Wordmark (64 tall), then the mode switch (2 × 260 × 64) |
| ↳ Content | 32, 192, 536, 776 | — | Widget Switcher: Narrative or Map. A ScrollBox inside handles Large text overflow. The scroll bar shows only when needed, and nothing scrolls on its own. |
| ↳ Footer | 32, 984, 536, 64 | — | Voice and Calm Mode toggles, 260 × 64 each |
| Stage | 600, 0, 1320, 1080 | The 3D view | No text except what's on the arc band and on buttons |
| ↳ Arc | 850, 0, 820, 168 | Band `SURFACE` at 94%, edges fade | Five nodes and labels (§4) |
| ↳ Settings | 1696, 24, 192, 64 | `RAISED` pill | Icon and word |
| ↳ Bottom-left slot | 632, 904, 280, 88 | `RAISED` | Back (tour) / My seat (explore) |
| ↳ Bottom-right slot | right-aligned to 1888, 904, 280–360, 88 | Kelly (primary) | Next (tour) / Resume tour (explore) |
| ↳ Credits | 600, 1008, 1320, 72 | — | Kept clear for Cesium and Google |

The stage centre is x = 1260. The arc and the "3D view" are centred on the **stage**, not on the window.

### Tokens
The `UI_DESIGN.md` tokens are unchanged (`SURFACE #072A2F`, `RAISED #0F3B41`, `INK #F4F2EC`, `INK_MUTED #B8C7C8`, `KELLY #69BE28`). One is added:

| Token | Value | Use | Contrast |
|---|---|---|---|
| `MIDNIGHT` | `#004C54` (Eagles midnight green) | Hover fill on sidebar buttons, base tone of the map plates | INK 8.7:1, INK_MUTED 5.6:1. **Kelly on it is only 4.2:1**, so use kelly there for shapes, never for text. |

How a selection looks is the same everywhere, using the existing current-chapter-chip language: the **current or selected thing is a light chip** (`INK` fill, `SURFACE` text, 13.6:1). That covers the mode switch, the level tab, the selected marker and the current arc node. It also gets a bold weight, so colour isn't the only signal.

---

## 2. State transitions

### Model
The tour's state machine (`FLFFJourneyStateMachine`) **doesn't change and never runs in Explore**. Explore is a second, independent state beside it. "Not losing your place" is therefore structural: nothing in Explore can write `StepIndex`.

```
                 +-----------+
   app launch -> |  Welcome  |  (Phase = Intro)
                 +-----------+
                  T |     | E
                    v     v
   +------------------+  E / Explore tab   +------------------+
   |  Tour            | -----------------> |  Explore         |
   |  StepIndex = n   | <----------------- |  StepIndex = n   |  (frozen: the bookmark)
   |  Next/Back/Jump  |  T / Tour tab /    |  level, filters, |
   +------------------+  Resume tour /     |  selection, view |
         |               arc node          +------------------+
         | Start Over (last step), or "Start again?" Yes, from either mode
         v
      Welcome
```

New snapshot fields (`FLFFJourneySnapshot`):

| Field | Type | Notes |
|---|---|---|
| `Mode` | `ELFFExperienceMode` { `Tour`, `Explore` } | Ignored while `Phase == Intro` |
| `ResumeText` | FText | "Resume tour · Plaza, 2 of 3", or "Start the tour" if the tour hasn't started |
| `MapLevel` | `ELFFMapLevel` { `Field`, `LowerConcourse`, `Club`, `Suites`, `UpperDeck` } | |
| `AmenityMask` | uint8 (bit per `ELFFAmenity`) | Default: `SensoryRoom` and `QuietZone` on |
| `SelectedPoiId` / `ViewPoiId` | FName | What's selected on the map, and where the camera is |
| `SeatLevel` | `ELFFMapLevel` | From the actor tagged `LFF_MySeat` |
| `bLookOffset` | bool | The view has been turned away from the authored pose |

New commands on `ULFFJourneySubsystem`: `ChooseTour()`, `ChooseExplore()`, `SetMapLevel(Level)`, `ToggleAmenity(Category)`, `HideAllAmenities()`, `SelectPoi(Id)`, `ShowPoi(Id)`, `GoToMySeat()`, `Look(DeltaYaw, DeltaPitch)`. Next, Back and Jump return the new `ELFFCommandResult::RejectedInExplore` while in Explore. While a modal is open (Settings, Paused, Start again?), mode switches are rejected the same way other commands are.

### Transition rules

| From | Event | Camera | Sidebar | Bottom slots | Voice |
|---|---|---|---|---|---|
| Welcome | **T** / Guided tour card | Stays (it's already at step 1's `CT_StadiumWide`) | Welcome fades out over 0.25 s and shows Narrative | Back / Next | Reads step 1 after 0.5 s if Voice is on |
| Welcome | **E** / Explore card | Stays | Welcome fades out and shows the Map, **on the seat's level** so the beacon is the first thing seen | My seat / "Start the tour" | Silent |
| Tour (step n) | **E** / Explore tab | **Doesn't move.** Explore starts from where you're standing. | Cross-fades to the Map (0.25 s; instant in Calm Mode or Less movement), on the level of the current view (`ALFFCameraTarget::MapLevel`) | My seat / "Resume tour · *chapter, k of m*" | Stops at once |
| Explore | Select a marker | — | The marker becomes a light chip; its card fills the bottom of the sidebar | — | — |
| Explore | **Show me** | Moves to the POI's `CameraTargetId` using the existing `ResolveMove` rules (Ease, fade-cut, or a cut in Calm Mode). Look offset resets. | "You are here" moves to the POI | — | Plays the POI's `VoiceWave` if one exists, otherwise stays silent |
| Explore | Drag, or ← → | Turns within ±45° / ±15° of the authored pose. ← → step 15° (eased 0.4 s; instant in Less movement). No inertia. | — | — | — |
| Explore | **T** / Tour tab / **Resume tour** | **Always a fade-cut** (0.3 s + 0.3 s; a cut in Calm Mode) back to step n's target. A long sweep from an arbitrary spot would be a surprise; a fade marks "back to the story". | Cross-fades to Narrative at step n | Back / Next | Reads step n again after arrival (same rule as Resume from Pause) |
| Explore | Arc node *k* | The same fade-cut, to chapter *k*. On the bookmarked chapter it goes to the **bookmarked step**, not the chapter's first step. | Narrative | Back / Next | As above |
| Either | "Start again?" → Yes | Fade to `CT_StadiumWide` | Welcome | — | Stops |

What's kept, and for how long:
- **Tour position:** kept while you're in Explore, and on any number of round trips.
- **Explore state** (filters, selection, last view): kept across mode switches. It resets on Start again, because the next visitor shouldn't inherit someone else's map.
- **Settings** (Calm Mode, Voice, Large text…): kept across restart, as today. Nothing is saved to disk.

### Walkthrough: from a tour step to the sensory room on the map and back
1. **Launch.** The welcome screen is up, opaque and still. Cesium tiles stream in behind it. Focus is on **Guided tour**.
2. **The visitor presses Enter** (or clicks Guided tour, or presses T). The welcome fades out over 0.25 s. The stadium wide shot is already behind it, so nothing moves. The sidebar shows "01 / Arrival" and step 1's text. The arc shows Arrival as the light current node, "1 of 4".
3. **Next ×8.** The camera eases from shot to shot. The sidebar text changes in place, at the same position every time. The arc's current node moves to **Plaza**, "2 of 3" (step 9, Popcorn for the People).
4. **The visitor clicks Explore** at the top of the sidebar (or presses E). The camera **stays at the popcorn stand**. The sidebar cross-fades to the map:
   - The **Lower concourse** tab is current, because the plaza's outdoor targets belong to that plate (the plate includes the ground-level ring around the building).
   - "You are here" sits on the plaza.
   - The **My seat** beacon is on this level, at Section 111.
   - Sensory and Quiet are on by default, so their markers show. Guest, Restrooms and Food are off.
   - The arc keeps Plaza as the current node, with "my place" under it.
   - The bottom-right slot now says **Resume tour · Plaza, 2 of 3**. Back and Next are gone from the screen, not just disabled, because they don't exist in this mode. The slots themselves stay put.
5. **The visitor looks for the sensory room.** If the sensory room is on another level, that level's tab shows a small dot, meaning "something you turned on is here". They click the tab (or press ↑ / ↓). The plate cross-fades and the tab becomes the light chip. If the seat isn't on this level, an edge chip on the map reads "My seat is on Lower concourse". Clicking it goes back to that level.
6. **They click the Sensory room marker.** It becomes a light chip with a label. The card at the bottom of the sidebar reads "Sensory room · *location* · [Show me]". They press **Show me**: the camera moves there (a fade-cut, because concourse moves are authored `ReducedMotion`). They drag to look around the room.
7. **They click "Resume tour · Plaza, 2 of 3".** The screen fades, cuts back to the popcorn stand and fades in. The sidebar is the narrative card for step 9 again, and the arc label says "2 of 3" again. If Voice is on, it reads the card again. **Next** goes to step 10, as if they had never left.
8. **Later, E again:** the map opens with their filters and selection as they left them, on the level of their current view.

---

## 3. UMG panel architecture

### Principles (unchanged from the current HUD)
- **One HUD, built by script.** Every widget below is emitted by `Tools/Editor/build_wbp_lff_ui.py`. Nothing is hand-built in the Designer, so the layout stays reviewable in git.
- **C++ pushes, Blueprints don't bind.** Panels get `OnJourneyChanged(Snapshot)` and fill named widgets. There are no property bindings, because they tick every frame.
- **Named widgets are the contract.** Existing names (`TitleText`, `BodyText`, `StepNumberText`, `BackButton`, `NextButton`, `NextLabel`, `VoiceButton`/`VoiceLabel`/`VoiceState`, `CalmModeButton`/…, `PreviewRow`, `SettingsButton`, `Surface*` borders) keep their meaning. Only their parents move.

### Hierarchy
```
WBP_LFF_HUD  (ULFFHUDWidget)
└─ RootOverlay                               Overlay, SelfHitTestInvisible
   ├─ Shell                                  HorizontalBox, Fill/Fill
   │  ├─ SidebarSize                         SizeBox  WidthOverride 600
   │  │  └─ SurfaceSidebar                   Border  SURFACE 100%  Padding 32,24,32,32   (Visible: eats clicks)
   │  │     └─ SidebarStack                  VerticalBox
   │  │        ├─ Wordmark                   HorizontalBox (EAF logo, hairline, two text lines)
   │  │        ├─ ModeSwitch                 WBP_LFF_ModeSwitch
   │  │        │  └─ HorizontalBox  →  TourTabButton [TourTabLabel, keycap T] · ExploreTabButton [ExploreTabLabel, keycap E]
   │  │        ├─ Divider                    Image 2 px RAISED
   │  │        ├─ SidebarSwitcher            WidgetSwitcher, slot Fill      ← index from Snapshot.Mode
   │  │        │  ├─ [0] Narrative           WBP_LFF_Narrative  (ULFFJourneyPanel)
   │  │        │  │  └─ ScrollBox → TextScale(ScaleBox) → VerticalBox
   │  │        │  │       StepNumberText · ClockText · TitleText · BodyText · PreviewRow · TicketPhone · TargetMissingText · FinaleActions
   │  │        │  └─ [1] ExploreMap          WBP_LFF_ExploreMap (ULFFExploreMapPanel)   ← see below
   │  │        ├─ Divider
   │  │        └─ SidebarFooter              UniformGridPanel 1×2, SlotPadding 8
   │  │           ├─ VoiceButton  [VoiceLabel "Voice", VoiceState "On"]
   │  │           └─ CalmModeButton [CalmModeLabel "Calm", CalmModeState "Off"]
   │  └─ Stage                               Overlay, Fill, SelfHitTestInvisible   (the 3D view shows through)
   │     ├─ TimelineArc                      WBP_LFF_TimelineArc   Top/Center, SizeBox 820×168      (§4)
   │     ├─ SettingsButton                   Top/Right, Padding 0,24,32,0
   │     ├─ NavSwitcher                      WidgetSwitcher, Bottom/Fill, Padding 32,0,32,88  ← index from Snapshot.Mode
   │     │  ├─ [0] TourNav    HorizontalBox:  BackButton(280×88) · Spacer(Fill) · NextButton(280×88)[NextLabel]
   │     │  └─ [1] ExploreNav HorizontalBox:  MySeatButton(280×88) · Spacer(Fill) · ResumeTourButton(≥360×88)[ResumeTourLabel]
   │     └─ Settings                         WBP_LFF_Settings (unchanged: right-hand sheet, Paused, Start again?)
   ├─ Welcome                                WBP_LFF_Welcome, full screen, SurfaceWelcome 100%   (collapsed after a choice)
   └─ FocusRing                              runtime, last child (unchanged)
```

**Retired:** `WBP_LFF_Splash` (replaced by `WBP_LFF_Welcome`), `WBP_LFF_TopChrome` (the wordmark moves into the sidebar, Settings into the stage), `WBP_LFF_StepCaption` (becomes `WBP_LFF_Narrative`), and the chapter chips in `WBP_LFF_BottomRail` (they become the arc). The runtime caption `VoiceIconButton` goes too; Voice lives in the sidebar footer.

### Explore map panel
```
WBP_LFF_ExploreMap  (ULFFExploreMapPanel : ULFFJourneyPanel)
└─ VerticalBox
   ├─ MapTitle "Stadium map" · ViewingText "Viewing: Lower concourse"
   ├─ MapRow                                 HorizontalBox, height 320
   │  ├─ LevelTabs                           VerticalBox, 168 wide, 5 × WBP_LFF_LevelTab (64 tall, 8 gap)
   │  │     Top to bottom: Upper deck · Suites · Club · Lower concourse · Field   (the order they stack in the building)
   │  └─ MapFrame                            SizeBox 352×320 → SurfaceMap Border (MIDNIGHT) → MapStack Overlay
   │        ├─ MapPlate                      Image, MID of M_LFF_MapPlate (PlateA, PlateB, Blend)
   │        ├─ MarkerLayer                   CanvasPanel   ← C++ adds WBP_LFF_MapMarker per visible POI
   │        ├─ HereLayer                     CanvasPanel   ← "You are here" dot and view wedge
   │        ├─ SeatLayer                     CanvasPanel   ← SeatBeacon (always the top layer)
   │        └─ SeatEdgeChip                  Button, Bottom/Center, collapsed when the seat is on this level
   ├─ AmenityTitle "Show on map"
   ├─ AmenityGrid                            UniformGridPanel, 2 columns, SlotPadding 8, MinDesiredSlotHeight 64
   │     (0,0) SensoryToggle   (0,1) QuietToggle
   │     (1,0) GuestToggle     (1,1) RestroomToggle
   │     (2,0) FoodToggle      (2,1) HideAllButton
   └─ PoiCard                                Border SurfacePoi (RAISED), collapsed when nothing is selected
         PoiTitle · PoiNote · ShowMeButton
```

### Widget-by-widget notes
**Widget Switchers (`SidebarSwitcher`, `NavSwitcher`)**
- C++ calls `SetActiveWidgetIndex((int32)Snapshot.Mode)` in `ApplySnapshot`.
- Both children stay constructed, so map filters and scroll position survive a switch for free. That's most of "without losing position" on the UI side.
- A Widget Switcher can't fade. To cross-fade, set the incoming child's render opacity from 0 to 1 over 0.25 s with the existing panel fade helper. Make it an instant cut when Calm Mode or Less movement is on.
- **Focus ring bug to avoid.** `ULFFHUDWidget` checks `Widget->IsVisible()` (`LFFHUDWidget.cpp:67`). An inactive switcher child still reports Visible, so Tab would land on invisible buttons. Fix this two ways:
  1. When switching, C++ sets the inactive child to `Collapsed`. The switcher doesn't mind.
  2. The ring's candidate filter also walks up the parents and skips anything under a `UWidgetSwitcher` whose `GetActiveWidget()` isn't that branch.

**Uniform Grid Panels (`AmenityGrid`, `SidebarFooter`)**
- Every cell is the same size, so the toggles line up and nothing jumps when a label changes from Off to On. That's why these use a Uniform Grid and not a Wrap Box.
- Set `MinDesiredSlotHeight = 64` (the project hit target), `SlotPadding = 8`, and child alignment Fill/Fill.
- Cells are **Buttons, not `UCheckBox`**. They use the existing toggle-row style and the `*Label` / `*State` naming convention, so C++ applies the chosen / not-chosen colours, the focus ring treats them like every other button, and the state is spelled out ("On" / "Off") rather than shown only as a tick.
- Each cell: category icon (24 px) · name · state.

**Level tabs**
- The current tab is the light chip with a bold label. The others are `RAISED`.
- A tab whose level holds any turned-on amenity shows an 8 px INK dot. The Lower concourse tab (the seat's level) also shows a small kelly seat glyph.
- Keys: ↑ / ↓ move between levels, and so do LB / RB on a controller.

**Markers (`WBP_LFF_MapMarker`)**
- 64 × 64 hit area around a 40 px `RAISED` disc, with an `INK` category icon and a 2 px `INK_MUTED` rim.
- **Shape tells the category apart, never colour alone.** Sensory = soft wave, Quiet = headphones, Guest Services = "i", Restrooms = the standard pictogram, Food = cup. The icons come from `Tools/Pipeline/prepare_icons.py`.
- Labels show only on the selected marker and on My seat, so the map never fills with words.
- Placement: canvas slot anchors (0, 0), `Alignment (0.5, 0.5)`, `Position = PlateUV * MapFrameSize`, `bAutoSize`.

**My seat (`WBP_LFF_SeatBeacon`)**
- A 56 px kelly pin with a 4 px `INK` outline and a 2 px `SURFACE` halo, plus a static soft kelly disc behind it (25% alpha). It never pulses.
- It's always labelled "My seat · Sec 111, Row 10". It isn't in the amenity grid, so it can't be turned off.
- It's the last child of `MapStack`, so it draws over every marker.
- On another level, the `SeatEdgeChip` stands in for it.
- The **My seat** button (bottom-left in Explore, or **H**) runs `GoToMySeat()`: the map switches to the seat's level and the camera goes to `CT_SeatFieldView`.

**Map plates (2.5D)**
- These are pre-rendered textures, one per level. They're **not** a live SceneCapture: that would be expensive, and noisy (crowds, sky, specular). A baked plate is calm and costs nothing per frame.
- `Tools/Editor/bake_map_plates.py` (MCP, rerunnable) renders each level:
  - An orthographic `SceneCapture2D` looking down at 55° (that's the "2.5D").
  - Geometry above the level is removed with the capture's clip plane (`bEnableClipPlane`, which needs `r.AllowGlobalClipPlane`; **VERIFY** with Nanite). The fallback is `HiddenActors` by tag.
  - Output: 2048² PNGs in `SourceData/UI/Map/`.
- `Tools/Pipeline/prepare_map_plates.py` posterises each plate into the palette: a `MIDNIGHT` base, `RAISED` structure, `INK_MUTED` edges, no textures.
- The bake also writes each level's capture transform and ortho size to `SourceData/Journey/LFF_MapLevels.json`, which becomes `DT_LFF_MapLevels`.
- `LFFMapRules::ProjectToPlate(WorldPos, Level)` turns any world position into plate UV with that same transform. Markers are therefore placed from their world anchors at runtime and can't drift from the art. This is a pure function, so it gets a unit test.

**Points of interest (`DT_LFF_MapPOIs`, from `SourceData/Journey/LFF_MapPOIs.json`)**

| Field | Type | Notes |
|---|---|---|
| `Label` | FText | "Sensory room", 2–4 words |
| `Note` | FText | One line, location in plain words ("Near Section 112") |
| `Category` | `ELFFAmenity` | `SensoryRoom`, `QuietZone`, `GuestServices`, `Restroom`, `Food` |
| `Level` | `ELFFMapLevel` | |
| `AnchorTag` | FName | The actor tagged `LFF_Poi` plus this name; its location is projected onto the plate |
| `CameraTargetId` | FName | Where Show me goes; must match exactly one `ALFFCameraTarget` |
| `VoiceWave` | soft `USoundBase` | Optional |
| `VerificationSource` | FString | Who confirmed the location, and when. **If it's empty, the row is dropped at load in packaged builds** (D6). `ALFFGameMode::bAllowUnverifiedPois` keeps it for internal review builds. |

`ALFFCameraTarget` gets a `MapLevel` field (default `LowerConcourse`; outdoor targets use it too) so "You are here" and the Explore entry level are known for every tour step.

**Viewport rectangle (D3)**
- Add `ULFFGameViewportClient : UGameViewportClient` and set it as the game viewport client in `DefaultEngine.ini`. Override `LayoutPlayers()`: call Super, then set player 0's `Origin = (SidebarPx / ViewportPx, 0)` and `Size = (1 − that, 1)`. **VERIFY** that 5.8 still lays players out there.
- `SidebarPx` is 600 × the DPI scale. The HUD reports it whenever the viewport is resized.
- Keep the rectangle the same on the welcome screen, so its fade-out doesn't cause a FOV jump.
- Set `AspectRatioAxisConstraint = AspectRatio_MaintainYFOV`. The authored shots then keep their vertical framing; their sides are cropped symmetrically.
- The HUD must stay on `AddToViewport()` (as it is today), not `AddToPlayerScreen()`. Otherwise the sidebar would be squeezed into the player's rectangle.
- `LFFJourneyRules::ShouldHideLabelAt` can drop its lower-third rule, since the caption has gone. It now hides only labels behind the bottom button slots.

**Classes.** This adds `ULFFExploreMapPanel`, `ULFFTimelineArc` and `ULFFGameViewportClient`, plus `LFFMapRules` (a header next to `LFFJourneyRules`). That goes over ADR-0003's class budget, so record it as **ADR-0005: two modes and the fixed sidebar**, amending ADR-0003.
- `ULFFHUDWidget` stops requiring five `BindWidget` panels. On construct, it finds every `ULFFJourneyPanel` in its tree and pushes each snapshot to all of them. Recurse into child user widgets' own `WidgetTree`s, because `ForEachWidgetAndDescendants` doesn't cross user-widget boundaries.

### Input additions (`ALFFPlayerController::BuildInputObjects`)

| Action | Keyboard | Controller | Notes |
|---|---|---|---|
| Guided tour / Resume tour | **T** | — | Does nothing if you're already in the tour |
| Explore | **E** | — | Does nothing if you're already in Explore |
| Map level up / down | ↑ / ↓ | LB / RB in Explore | In the tour, LB / RB stay Back / Next |
| Turn the view | ← / → (15° steps), or mouse drag | Right stick | In the tour, ← / → stay Back / Next. The Settings key legend lists both meanings. |
| My seat | **H** | — | Explore only |

T and E are separate keys, not one toggle key. Each always means one thing, so pressing it twice is harmless. That's more predictable than a toggle, where you have to remember which mode you're in. None of these keys are bound today.

---

## 4. Timeline arc material

### What it draws
- A gentle **smile-shaped curve**, lowest in the middle, rising toward both ends. It echoes the bowl's rim.
- A soft `SURFACE` band (94%) under the curve. The node labels sit inside it.
- Both ends **fade to nothing** over the outer 12% of the width, so the arc melts into the scene with no hard edge.
- The line is `INK` up to and including the current node, and `INK_MUTED` at 55% after it.
- No texture and no `Time` node: the material is still unless a parameter changes.

### Material setup: `M_LFF_TimelineArc` (`/Game/LFF/UI/Materials`)
| Setting | Value |
|---|---|
| Material Domain | **User Interface** |
| Blend Mode | **Translucent** (soft edges need alpha; Masked would alias) |
| Shading | Unlit (implied by the UI domain) |
| Inputs used | `TexCoord[0]`, which runs 0–1 across the Image's geometry in UMG |
| Outputs | **Final Color**, **Opacity** |

Parameters (defaults are for the 820 × 168 widget):

| Parameter | Type | Default | Meaning |
|---|---|---|---|
| `Aspect` | Scalar | 4.881 (820 / 168) | Width ÷ height, so distances are measured in height units (no oval stretching) |
| `ArcLowV` | Scalar | 0.42 | V of the arc's lowest point (centre) |
| `RadiusV` | Scalar | 9.0 | Circle radius in height units. Bigger means flatter. The ends rise about 0.34. |
| `LineHalfWidth` | Scalar | 0.012 | About 2 px each side, so a 4 px line |
| `BandAbove` / `BandBelow` | Scalar | 0.10 / 0.50 | Band extent above and below the line (in V units); labels sit 0.10–0.42 below it |
| `BandSoftness` | Scalar | 0.08 | Feather on the band's lower edge |
| `FadeU` | Scalar | 0.12 | Width of the fade at each end |
| `ProgressU` | Scalar | 0.50 | U of the current node; the line is "done" up to here |
| `BandColor` | Vector | `SURFACE` (linear 0.0021, 0.0232, 0.0284) | Black in High contrast |
| `BandOpacity` | Scalar | 0.94 | 1.0 in High contrast |
| `DoneColor` / `AheadColor` | Vector | `INK` / `INK_MUTED` | |
| `AheadOpacity` | Scalar | 0.55 | 0.8 in High contrast |

### Node graph
Build it as nodes, or as a single **Custom** node that holds the HLSL below. The Custom node is easier to review in a diff; both give the same result.

1. **Aspect-correct position.** `TexCoord[0]` → `P = float2(U * Aspect, V)`.
2. **Circle centre.** `C = float2(Aspect * 0.5, ArcLowV − RadiusV)`. The centre sits far above the widget, so the bottom of the circle is the smile.
3. **Signed distance to the arc.** `S = length(P − C) − RadiusV`. Negative means above the line, positive means below it.
4. **Anti-alias width.** `AA = abs(DDX(S)) + abs(DDY(S))`, about one screen pixel. This keeps the line crisp at any DPI.
5. **Line mask.** `Line = 1 − smoothstep(LineHalfWidth − AA, LineHalfWidth + AA, abs(S))`.
6. **Band mask.** `Band = smoothstep(−BandAbove − AA, −BandAbove + AA, S) * (1 − smoothstep(BandBelow − BandSoftness, BandBelow, S))`.
7. **End fade.** `Edge = smoothstep(0, FadeU, U) * smoothstep(0, FadeU, 1 − U)`.
8. **Progress colour.** `T = smoothstep(ProgressU − AAu, ProgressU + AAu, U)`, where `AAu = abs(DDX(U))`. Then `LineRGB = lerp(DoneColor, AheadColor, T)` and `LineA = lerp(1, AheadOpacity, T)`.
9. **Combine.** `Final Color = lerp(BandColor, LineRGB, Line)`. `Opacity = saturate(max(Band * BandOpacity, Line * LineA)) * Edge`.

```hlsl
// Custom node "ArcHUD". Output type: CMOT Float 4 (rgb = colour, a = opacity).
// Inputs: UV, Aspect, ArcLowV, RadiusV, LineHalfWidth, BandAbove, BandBelow, BandSoftness,
//         FadeU, ProgressU, BandColor, BandOpacity, DoneColor, AheadColor, AheadOpacity
float2 P  = float2(UV.x * Aspect, UV.y);
float2 C  = float2(Aspect * 0.5, ArcLowV - RadiusV);
float  S  = length(P - C) - RadiusV;
float  AA = abs(ddx(S)) + abs(ddy(S));
float  Line = 1 - smoothstep(LineHalfWidth - AA, LineHalfWidth + AA, abs(S));
float  Band = smoothstep(-BandAbove - AA, -BandAbove + AA, S) * (1 - smoothstep(BandBelow - BandSoftness, BandBelow, S));
float  Edge = smoothstep(0, FadeU, UV.x) * smoothstep(0, FadeU, 1 - UV.x);
float  AAu  = abs(ddx(UV.x));
float  T    = smoothstep(ProgressU - AAu, ProgressU + AAu, UV.x);
float3 LineRGB = lerp(DoneColor, AheadColor, T);
float  LineA   = lerp(1.0, AheadOpacity, T);
return float4(lerp(BandColor, LineRGB, Line), saturate(max(Band * BandOpacity, Line * LineA)) * Edge);
```
Wire `.rgb` to Final Color and `.a` to Opacity.

### Widget: `WBP_LFF_TimelineArc` (`ULFFTimelineArc`)
```
SizeBox 820×168
└─ Overlay
   ├─ ArcImage     Image, brush = MID of M_LFF_TimelineArc, Visibility HitTestInvisible
   └─ NodeLayer    CanvasPanel   ← C++ adds 5 × WBP_LFF_ArcNode
```
- **Curve shape in one place.** `struct FLFFArcShape { float Aspect, ArcLowV, RadiusV, FadeU; FVector2D PointAtU(float U) const; }` lives in C++. It sets the material parameters and also places the nodes. `PointAtU` gives `V = ArcLowV − RadiusV + sqrt(RadiusV² − ((U − 0.5) · Aspect)²)`. The dots therefore sit exactly on the drawn line, and changing the curve means changing one struct.
- **Nodes** sit at U = 0.22, 0.36, 0.50, 0.64, 0.78. Every label (±0.07 U) falls inside `[FadeU, 1 − FadeU]`, so all text is on the full-strength band.
- Each `WBP_LFF_ArcNode` is a Button (64 × 64 hit area) with:
  - a dot: done = 14 px INK; current = a 24 px light chip in a 2 px INK ring; ahead = a 14 px hollow INK_MUTED ring;
  - a label 20 px below the dot (18 px floor, bold when current);
  - a second line under the current node: "2 of 3" in the tour, "my place" in Explore.
  - Clicking a node does the same as the old chapter chip; in Explore, see §2.
- **When the snapshot changes**, C++ sets `ProgressU` to the current node's U. There's no tween: it's a step change, consistent with "nothing slides". High contrast swaps `BandColor`, `BandOpacity` and `AheadOpacity` on the MID, because the arc isn't a `Surface*` Border.
- **Clicks pass through.** `ArcImage` is HitTestInvisible, so the band's faded tails never swallow a drag meant for the 3D view in Explore. Only the node buttons take hits.
- **Legibility check.** Labels sit inside the band where its opacity is at least 0.94 and `Edge = 1`. INK on 94% `SURFACE` over a white sky is at least 7:1, whether blending happens in linear or sRGB space. Keep the existing 1 px soft text shadow as insurance.

---

## Open questions
1. **Sensory room and quiet zone locations** (`STAKEHOLDER_REVIEW.md` Q3). Until EAF or stadium operations confirm them, D6 hides those markers in packaged builds. Explore will still have the seat, Guest Services and the Sensory Activation Vehicle on the plaza (once those are verified).
2. **Level names.** "Lower concourse / Club / Suites / Upper deck" should match the Linc's own signage, so visitors see the same words on game day.
3. **Arc nodes: five or six** (D1).

## Acceptance checks (add to `TEST_PLAN.md` once built)
- [ ] At no point, in any mode, step or setting, is a line of text drawn directly on the 3D scene.
- [ ] Tour → step 9 → Explore → three levels, two Show me's, a drag → Resume tour lands on step 9, and Next goes to step 10.
- [ ] Welcome → Explore → Resume tour (which reads "Start the tour") lands on step 1.
- [ ] In Explore, Tab never lands on Back, Next or the narrative Preview button, and in the tour it never lands on map widgets.
- [ ] In Calm Mode, every mode switch, level change, Show me and resume is an instant cut. Nothing moves on its own.
- [ ] The My seat beacon is visible or edge-chipped on all five levels, and stays drawn over every marker.
- [ ] Automation: add `LFF.Journey.StateMachine.Modes` (round trips, rejected commands, restart resets Explore state) and `LFF.Map.ProjectToPlate`.
- [ ] Large text (1.25×) in the sidebar scrolls rather than clipping. The footer and mode switch never move.
