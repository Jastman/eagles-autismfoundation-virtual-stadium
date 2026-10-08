# HUD design: Scene-first stadium guide (2026-10-07)

The active presentation replaces the permanent sidebar and central chapter arc with two compact corner groups. The direction comes from the supplied clean, scene-led Apple Park reference: the stadium is the experience; controls and information stay at its edges. Eagles identity, factual journey copy and existing interaction semantics are preserved.

Build the saved widgets with `Tools/Editor/build_wbp_lff_modes.py`. The native binding contract is in `JOURNEY_SCHEMA.md`. `ACCESSIBILITY.md` takes precedence. The earlier sidebar/arc proposal remains in `UI_MODES_SPEC.md` as history, not implementation guidance.

## Composition

| Region | Contents | Behavior |
|---|---|---|
| Upper left | EAF wordmark, guide name and venue | Identity-only card; no mode switch here. |
| Below identity | Step, title/body, optional preview, ticket and finale | Guided tour only. Scrollable caption; the day timeline is disclosed by a labelled button. |
| Upper right | Guided tour, Explore, Settings; Voice, Calm Mode, Crowd | One persistent control dock. Labels and On/Off state remain explicit. |
| Below controls | Five chapter chips, then Back/Next | Guided tour only. Chips wrap into a grid on narrower views and with Large text. |
| Below controls | My seat, Resume tour, flight guidance, places | Explore only. Place categories are immediate; level controls expand on demand. |
| Stadium | Spatial place markers and My seat | Opaque name chips; avoid visible panel bounds and one another. |
| Bottom edge | Cesium and Google attribution | Clear of both corner groups and spatial controls. |

At the reference logical size, the information group is at most 480 units wide and controls 540, with 24-unit edge padding. Each is capped at 43% of the actual logical viewport width. Large text gives the caption up to 600 units while respecting that cap. Height limits and scrolling keep long content reachable rather than shrinking type.

The stadium renders across the whole viewport. There is no permanent 600-unit sidebar, separate bottom navigation rail, central arc or split scene-rendering rectangle. Legacy asset names (`SidebarHeader`, `TimelineArc`) remain so existing references stay valid.

## Welcome

Before choosing a mode, the stadium remains visible with an Eagles midnight-green (`#004C54`) fade concentrated in the lower-left corner. There is no full-screen veil, central card or patterned overlay. The EAF white logo and Five Below Foundation logo from `Assets/Logos/` sit above **GAMEDAY VISUAL SCHEDULE**.

**Guided Tour**, **Explore**, and the **Calm Mode** switch share the lower-left action row. The entire illustrated pill is the toggle, with **Calm Mode** below and explicit On/Off text inside its thumb. Off shows cheering fans; On shows empty stadium seats, with no sun/moon imagery. The dedicated `WBP_LFF_CalmToggle` mirrors the journey's boolean Calm Mode state and plays its 240 ms eased `CalmTransition` UMG animation forward or reverse. Reversing mid-transition continues from the current position. Less movement, initialization and off-screen/modal updates snap to the correct endpoint. This deliberate switch transition is the only motion retained when enabling Calm Mode; there is no idle or looping animation. High contrast hides the illustrations while preserving the outlined switch, thumb, label and state.

Hovering the toggle or selecting it with the keyboard/controller focus ring shows: “Limits sounds, animations, and movement throughout the experience.” The hint is in a canvas overlay, excluded from layout measurement, so the mode buttons never grow or move on hover. It hides while a modal is open. Existing Calm Mode behavior is unchanged: sounds and narration are silenced, camera moves become instant, and crowd/intensity animation is suppressed.

Settings stays at the bottom right. Controls wrap on narrower logical viewports, with a separate Settings row to avoid overlap. The bottom attribution band remains clear. High contrast replaces the gradient with solid black.

For a welcome-only rebuild, run `python3 Tools/Pipeline/prepare_welcome_ui.py`, then `python3 Tools/Editor/build_wbp_lff_modes.py --welcome-only` with PIE stopped and MCP running. This imports only the welcome textures and rebuilds only `WBP_LFF_Welcome`; other HUD trees, amenities and world assets are untouched.

## Explore

- Category toggles show that kind's mapped locations across all levels. Guest Services includes sections 121, 136, 206 and 226 without first choosing Upper deck.
- Enabling a category frames its visible anchors, stem tops and My seat in the unobstructed part of the viewport. Framing accounts for horizontal FOV, aspect ratio and the actual corner-panel geometry.
- Level buttons retain explicit framing/highlighting. Manual flight and authored navigation clear automatic category-framing intent.
- Selecting a marker exposes its detail card. Show me uses an authored camera when one exists; otherwise it frames the mapped location, not a fabricated interior.
- Quiet zones and restrooms have no mapped rows and remain disabled, labelled **Not mapped**.
- The saved amenities table has seven development rows. Existing packaged-build verification rules still exclude rows with no verification source; this redesign does not certify those facilities.

## Tour and accessibility

The five chapters are Arrival (steps 1-4), Gates (5-7), Plaza (8-10), To my seat (11-14), and Game (15-23). Boundaries come from `ALFFGameMode::TourStepFirstSteps`; the panel supplies their names. A chip shows its name and progress, not a redundant number line.

Tour/Explore switching preserves the exact tour bookmark. Resume tour restores it. Narration reads the unchanged title and body; the karaoke highlight and underline do not reflow text. Timeline marks, ticket frames, intensity previews, finale actions, pause/restart confirmation and Settings are retained.

All interactive targets are at least 64 logical units. Labels have an 18-unit floor and wrap when necessary. Text sits on opaque surfaces. Large text expands the caption without reducing control sizes; High contrast makes surfaces black.

Tab/Shift+Tab and controller D-pad Down/Up traverse visible, enabled controls; Enter/A activates the ringed control. Focus follows the caption, dock, chapters, navigation, places and spatial markers. Settings, Pause and restart confirmation trap this traversal. Offscreen focused controls scroll into view without animation.

## Tokens

| Token | Value | Use |
|---|---|---|
| `SURFACE` | `#072A2F` | Opaque information and control surfaces |
| `RAISED` | `#0F3B41` | Secondary controls |
| `INK` | `#F4F2EC` | Main text |
| `INK_MUTED` | `#B8C7C8` | Supporting text |
| `KELLY` | `#69BE28` | Primary action, active state and spoken-word highlight |
| High contrast | `#000000` | Surface override |

Atkinson Hyperlegible remains the project font. Caption title is 32, body 24, primary navigation 22, compact mode controls and labels 18. Light selected chips use dark text; state never depends on color alone.

Panels use the existing short fade, instant with Less movement or Calm Mode. No attention-seeking UI pulses or flashing. The prior user decision to omit on-screen disclaimer copy remains unchanged; verification metadata and packaging safeguards remain in the data model.

## Crowd presentation

Nearby seats use bounded, individually animated fans: six existing MetaHuman appearances, seated idle/clap, standing idle/cheer/clap, independent phases and rates, and occasional timer-driven gestures. The current 48-candidate cap at 70% occupancy yields 34 detailed fans, mostly seated. Middle distance uses Mass; the far bowl and Mass-cap overflow use instanced billboards.

Actor roots sit on measured stadium treads, not the chair undersides stored in `seats.json`. The sidecar covers 56,643 of 56,707 source seats; 64 unmeasured seats are reported and excluded, never assigned guessed heights. Only the visitor's immediate camera space is cleared, not a large cone of nearby rows.

Crowd Off and Calm Mode hide detailed fans, stop their gesture timers and disable skinned-component ticks. Less movement immediately freezes visible detailed fans; turning it off restores independent idle and gesture timing. Engine crowd assets are not modified.

Reproduction and safe crowd-only configuration are documented in `LOCAL_AGENT_SETUP.md`.
