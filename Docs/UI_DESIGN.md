# HUD design: "Chapters" (2026-10-02)

The fourth UI pass, and a structural redesign. It replaces both the paper-light theme (`UI_AUDIT.md`) and the first "night field" pass, which changed colours and type but kept the old cards. Widgets are built by `Tools/Editor/build_wbp_lff_ui.py`; the C++ contract is in `JOURNEY_SCHEMA.md`. If anything here conflicts with `ACCESSIBILITY.md`, `ACCESSIBILITY.md` wins.

## Brief
- Eagles colours. Desktop Mac and Windows now, desktop and mobile web later.
- **Accessibility first**, for autistic and neurodiverse visitors: predictable, calm, easy to read.
- Clean and minimal. A real reimagining, modelled on the references:
  - three Three.js scene pieces posted on X ("Aegean", "Stillwater", an ocean scene);
  - sael.net/plane-of-focus.
- No "illustration" / "simulation" / disclaimer copy anywhere in the UI (the user's call, 2026-10-02).

## The structure we took from the references
| Reference pattern | Here |
|---|---|
| No boxes: titles sit on the scene | The step is a **caption on the scene**, lower left, on a soft dark field with no hard edge (`T_LFF_Fog`, a 9-slice with a 132 px feathered rim). |
| Eyebrow over a big title ("01 / Under the lemon tree") | `02 / Gates open · 10:00 AM` in kelly over a 44 px title and 26 px body. |
| One chapter bar at the bottom ("01 Lemon courtyard · 02 Little harbor …") | The **chapter bar**: Back, six chapter chips, Next. The current chip is raised and light and says "2 of 3"; done chapters say "Done"; later ones say "4 steps". Click a chip to go to its first step. |
| Wordmark top left, small labelled controls top right | EAF logo, a hairline, "Game day guide / Lincoln Financial Field". Voice and Settings as a pill cluster on the right. |
| Keycap hints along the bottom (Stillwater) | `← Back  → Next  Tab Move  S Settings  P Pause` in the credits band on the right. Settings rows show their own number key. |
| Title screen with the scene beside it | The splash is a headline on a soft field at the left with Standard / Calm and **Start your day [Enter]**; the stadium orbits on the right. |
| Panels over the scene, not instead of it | Settings is a **right-hand sheet** over a light dim; the scene and your place stay visible on the left. Paused and Start again? are words in the middle of a dimmed scene. |

**Not taken** (they fail the project rules): 10–12 px type, letterspaced all caps, thin weights, icon-only buttons, see-through text backgrounds, serif display titles.

## Chapters
`ULFFJourneyPanel::Chapters` (editable class default), against the 23 steps in `SourceData/Journey/LFF_GoldenPath.json`:

| # | Chapter | Steps (1-based) |
|---|---|---|
| 01 | Getting there | 1–4 |
| 02 | Gates open | 5–7 |
| 03 | The plaza | 8–10 |
| 04 | To my seat | 11–14 |
| 05 | The game | 15–18 |
| 06 | Breaks and help | 19–23 (the last step's eyebrow reads "Enjoy the game") |

Smaller chunks ("2 of 3") are easier to hold than "Step 6 of 23", and the whole day stays visible. If steps are added, update `FirstStep` there.

## Tokens (1080p)
| Token | Value | Use | Contrast |
|---|---|---|---|
| `SURFACE` | `#072A2F` | The soft fields (at 90–94%), the bar, the sheet | n/a |
| `RAISED` | `#0F3B41` | Secondary buttons, Off rows, keycaps | n/a |
| `INK` | `#F4F2EC` | Main text | 13.6:1 on SURFACE |
| `INK_MUTED` | `#B8C7C8` | Secondary text | 8.7:1 |
| `KELLY` | `#69BE28` | Next / Start, eyebrows, chapter numbers, the spoken word | 6.5:1 as text; dark text on it 6.5:1 |
| High contrast | Fields, bar and sheet go `#000000` | Settings > High contrast | INK on black 18.8:1 |

Text on a soft field starts 104–120 px in from its edge, where the field is at full strength, so contrast holds whatever the camera shows. Field text also has a soft 1 px shadow.

**Type:** Atkinson Hyperlegible (Braille Institute, SIL OFL; `SourceData/UI/Fonts`). Headline 64, caption title 44, body 26, controls 22, labels 18 (the floor). Eyebrows are bold with slight letter spacing, sentence case.

**Shape:** fields have no edge; the bar, chips and Settings rows are 16–30 px rounded rectangles; standalone buttons are pills. Hit targets are 64 px (Start 76).

## Karaoke (voice on)
The spoken word gets a soft kelly highlight behind it and a solid 4 px kelly underline. Its font and colour don't change and nothing reflows; it steps word to word with no animation. `ULFFKaraokeDecorator` (`<w>word</>`).

## Keyboard focus
Tab / Shift+Tab (D-pad Down / Up) move a two-tone ring (light ring, dark halo) through the buttons on screen, chapter chips included. Enter presses it; a modal keeps it inside. See `ACCESSIBILITY.md` §1, "As built".

## Motion
Panels fade in over 0.25 s (instant with Less movement or Calm Mode). Nothing slides, pulses or flashes.

## Assets
- `Tools/Pipeline/prepare_ui_fog.py` writes `T_LFF_Fog`, `T_LFF_FadeTop`, `T_LFF_FadeBottom` and `T_LFF_FadeSide` to `SourceData/UI`.
- `Tools/Editor/import_ui_fog.py` imports them into `/Game/LFF/UI/Icons` as UI textures (run it in the editor before `build_wbp_lff_ui.py`).
