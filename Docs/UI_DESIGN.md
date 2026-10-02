# HUD design: "Night field" (2026-10-02)

The third UI pass, and a full redesign. It replaces the paper-light theme from `UI_AUDIT.md`. The widgets are still built by `Tools/Editor/build_wbp_lff_ui.py`, and the C++ contract is in `JOURNEY_SCHEMA.md`. If anything here conflicts with `ACCESSIBILITY.md`, `ACCESSIBILITY.md` wins.

## Brief
- Eagles colours. Desktop Mac and Windows now, desktop and mobile web later.
- **Accessibility comes first**, for autistic and neurodiverse visitors: predictable, calm, easy to read.
- Clean and minimal.
- References:
  - Three Three.js scene pieces posted on X ("Aegean", "Stillwater" and an ocean scene).
  - sael.net/plane-of-focus.

## What we took from the references and what we didn't
The references share one layout. The scene fills the screen. A small wordmark sits top left and icon buttons top right. A short eyebrow line sits over a large title in the lower left. One chapter bar sits bottom centre. Panels are dark or cream, and nothing is in the middle.

**Kept:**
- The scene stays first.
- UI only in the corners and one dock.
- The eyebrow-over-title hierarchy.
- One bar at the bottom.
- Left-aligned text.

**Not kept:**
- 10–12 px type, letterspaced all-caps labels, thin weights and icon-only buttons. They look refined but fail our 18 px floor and "words, never icons alone" rule.
- Translucent panels: contrast can't depend on what the camera sees.
- Serif display titles: `ACCESSIBILITY.md` asks for sans-serif.

## Tokens (1080p)
| Token | Value | Use | Contrast |
|---|---|---|---|
| `SURFACE` | `#072A2F` | Every card, opaque | n/a |
| `RAISED` | `#0F3B41` | Secondary buttons, Off toggles | n/a |
| `INK` | `#F4F2EC` | Main text | 13.6:1 on SURFACE |
| `INK_MUTED` | `#B8C7C8` | Secondary text | 8.7:1 |
| `KELLY` | `#69BE28` | The primary action, eyebrows, the current pip and mark, the spoken word | 6.5:1 as text; dark text on it 6.5:1 |
| `OUTLINE` | INK at 55% | Secondary button outline | 5.5:1 against the card |
| `PIP_UPCOMING` | `#557A7E` | Steps still to come | 3.2:1 |
| `NOTICE` | `#F2B65A` | "This step is not available yet." | 8.4:1 |
| High contrast | Surfaces go `#000000` | Settings > High contrast | INK on black 18.8:1 |

**Why dark:**
- Opaque midnight-green cards over a bright daytime aerial read as clear, separate objects.
- They cut glare for light-sensitive visitors.
- They are the Eagles' own colour.

Kelly green is used sparingly, so it keeps a meaning: "do this", "you are here" and "being read now".

**Type:**
- Atkinson Hyperlegible (Braille Institute, SIL OFL; `SourceData/UI/Fonts`, `/Game/LFF/UI/Fonts/F_LFF_Atkinson`). It has distinct I / l / 1 and 0 / O shapes.
- Sizes: headline 52, caption title 38, body 24, controls 22, labels 18 (the floor).
- Eyebrows are 18 px bold with slight letter spacing, in sentence case (all caps reads slower).
- Body line height is about 1.5× the size (WCAG 1.4.12).

**Shape and space:**
- Cards have 28–32 px corners. Buttons are pills. Settings rows and the Standard / Calm cards are 16–20 px rounded rectangles.
- Spacing is on an 8 px grid.
- Hit targets are 64 px (Start 76).

## Screens
- **Splash:**
  - One centred card, everything left-aligned: eyebrow "Your game day guide", "Lincoln Financial Field", body.
  - Then two side-by-side **mode cards** (Standard / Calm), each with its own one-line description, so the choice and what it means are in one place.
  - Then **Start your day** and the disclaimer.
  - The chosen card is light with a kelly outline and dark text.
- **Journey step:**
  - Top left: the logo chip and the disclaimer pill. Top right: Settings.
  - Lower left: the caption card, with an eyebrow (step and clock), the Verified / Approximate / Illustration chip, Voice: On/Off, then the title, body and Preview row.
  - Bottom centre: one dock with Back, "Step N of 23" over **one pip per step**, the timeline button and Next.
  - The pips make "how much is left" countable at a glance. The current pip is a longer kelly pill, so position never rests on colour alone.
- **Day timeline:** a drawer above the dock, closed by default. The current mark is a kelly pill with dark text.
- **Settings:**
  - Toggle **rows**: the name on the left, the On / Off word on the right.
  - On = light row, kelly outline, bold "On". Off = dark row, regular "Off".
  - Calm Mode locks Sound, Voice, Crowd and Less movement and shows them as such.
  - The keyboard legend is on the right, including Tab and Enter.
- **Paused / Start again?:** the same card style. Left-aligned, with one primary action.
- **Finale:** the caption card moves to the centre with the EAF wordmark, Start Over and Learn More. A step with no body leaves no empty line.

## Karaoke (voice on)
While the voice reads, the spoken word gets:
- a soft kelly **highlight** behind it (32% over the card; the text on it is still 7.4:1);
- a solid 4 px kelly **underline** across its foot.

It never relies on colour alone (this closes `UI_VERIFICATION.md` open finding 3).

The word keeps the card's exact font and colour, and the highlight is drawn outside the word's box, so **no line reflows** as the marker moves. There's no animation: it steps word to word.

Implementation:
- `ULFFKaraokeDecorator` (`Source/VirtualStadium/*/UI/LFFKaraokeDecorator.*`) is a rich-text decorator for `<w>word</>`.
- `ULFFJourneyPanel::KaraokeMarkup` marks the current word.

## Keyboard focus
See `ACCESSIBILITY.md` §1, "As built". This closes `UI_VERIFICATION.md` open finding 1.
- Tab / Shift+Tab (D-pad Up / Down) move a two-tone ring through the buttons on screen. Enter presses the ringed button.
- A modal keeps the ring inside itself.

## Motion
Unchanged:
- Panels fade in over 0.25 s with an ease-out. With Less movement or Calm Mode they appear instantly.
- Nothing pulses, slides or flashes. The karaoke marker and focus ring move instantly.
