# UI/UX Audit and Design Direction (2026-10-02)

Scope: the HUD (`WBP_LFF_HUD` and its five pieces: Splash, TopChrome, StepCaption, BottomRail, Settings), driven by `ULFFJourneyPanel` / `ULFFHUDWidget`.
Method: PIE captures of the splash and journey steps 1 and 2 (1039×745 viewport), widget hierarchies read through VibeUE, plus `Docs/ACCESSIBILITY.md`.
Not covered: the Settings, Pause and Restart-confirm panels (the off-screen preview rendered blank because they start hidden), the finale step, the ticket phone and Calm Mode. Contrast ratios below are by eye. They still need measuring.

## What already works (keep)
- One action per screen. Next is the single filled button, and Back is clearly secondary.
- Plain-language copy, short titles, no timers, no auto-advance.
- Hit targets are large, with text labels on Back/Next.
- Settings model already covers Calm, Reduce motion, Large text, High contrast, Voice and Crowd.
- Surfaces are named `Surface*` and `TextScale`, so the C++ theming hooks already exist.

## Findings

### P0: accessibility failures against the project's own rules
1. **Logo and disclaimer line sit directly on the 3D scene.** On step 2 the "Eagles Autism Foundation" logo and the "Approximate. Not directions…" line overlap building geometry and become hard to read. The line is thin and small over bright sky. Fix: put both on a solid surface or move them into a fixed header band.
2. **Translucent blur panels everywhere.** Text contrast depends on whatever the camera is looking at, so it changes at every step. For this audience, predictable contrast matters more than glass. Fix: opaque surfaces by default. Keep blur only as an optional style.
3. **Icon-only controls.** The settings gear and the card's sound button have no text label, which breaks "never icon alone" (ACCESSIBILITY.md §1).
4. **Mode switch is unclear.** The unselected "Calm" pill looks disabled (grey text on grey). Selected and unselected differ only by outline and weight. Selection must not rely on subtle contrast.

### P1: hierarchy and clutter
5. **Three progress indicators at once:** "Step 1 of 23" (top), 23 pips (bottom), and the timeline. They compete and say the same thing.
6. **Three stacked bars at the bottom:** timeline, pips, then Back/Next. Plus the Cesium/Google attribution (required), which crowds the bottom-left.
7. **Timeline panel is dense.** Six two-line marks of uneven width, plus an "Approximate times / Example 1:00 PM game" note competing at its left edge.
8. **Top bar mixes unrelated things:** progress, mode switch, settings. The mode switch also appears on the splash and in Settings, so it exists in three places.
9. **Caption card is narrow and heavy.** About 285 px wide, so a title like "Today I am going to the Eagles game at Lincoln Financial Field." wraps to four lines. Title and body are close in size. The "Approximate" chip is tiny. The card covers the stadium on step 1.
10. **Splash has too many text styles:** five sizes and weights in one card, plus two small grey notes (about 12 px) explaining Standard and Calm.

### P2: polish
11. Type scale and spacing are ad hoc. There are no shared tokens, so every widget picks its own values.
12. Brand colour is thin. Midnight green appears only on Next and a few tints. Silver, charcoal and kelly green are absent. Everything else is generic dark glass.
13. No visible keyboard focus state could be confirmed in captures. It needs a deliberate, high-contrast ring.
14. Settings is a flat list of seven toggles with an On/Off label and no grouping (inferred from the hierarchy).

## Design direction
Brief: Eagles palette, desktop Mac/Windows now and desktop plus mobile web later, accessibility first (autistic and neurodiverse visitors), clean, minimal, easy to read.
References: sael.net/plane-of-focus (calm neutral surfaces, generous whitespace, one accent, controls grouped in a single console). The three X posts are video demos of 3D scenes. Their text content gave no UI detail, so I'm treating them as "polished, quiet, scene-first" mood only.

### Principles
1. **Scene first, UI quiet.** Three layers at most on screen: header, one step card, one control bar.
2. **Predictable.** Every element is in the same place on every step. Nothing moves except the camera, and that follows the Reduce Motion setting.
3. **One progress indicator.** Replace "Step X of 23", the pips and the timeline's progress role with a single, labelled one.
4. **Opaque, high-contrast surfaces.** At least 7:1 for body text (AAA), at least 4.5:1 for everything else.
5. **Text plus icon on every control.** Selected state is shown by fill, check mark and label, never by colour or outline alone.
6. **Few type sizes, generous spacing.** A 4-step type scale on an 8 px grid.

### Proposed tokens (to confirm)
| Token | Value | Use |
|---|---|---|
| Surface | Near-black green `#0B1D1F` (opaque) | Cards, bars |
| Surface raised | `#12292C` | Buttons at rest |
| Text primary | `#F4F1EA` (warm off-white) | Body, titles |
| Text secondary | Silver `#A5ACAF` | Labels (≥4.5:1 on Surface) |
| Brand | Midnight green `#004C54` | Primary button fill |
| Accent | Kelly green `#69BE28` | Focus ring, selected check, used sparingly |
| Calm Mode | Same layout, accent muted to silver, no green | Matches existing Calm behaviour |
| Type | 14 / 18 / 24 / 36 px at 1080p, Roboto or Inter | Label / body / title / headline |
| Space | 8, 16, 24, 40 px | All gaps and padding |
| Radius | 12 px cards, 999 px pills | Consistent |

Eagles hex values are from memory of the team's published palette. Please confirm against the Foundation's brand guide.

### Layout sketch (1080p desktop)
- **Header band (top):** logo left, "Illustration, not directions" note beside it, Settings button (icon + "Settings") right. Solid surface.
- **Step card (left, 420 px wide):** step number and approximate-location chip, title (24), body (18), then one "Listen" button with label.
- **Control bar (bottom, centred):** Back, a single progress readout "Step 2 of 23" with a slim bar, Next. Timeline moves into a collapsible "Day timeline" drawer, closed by default.
- **Mode (Standard/Calm):** lives only on the splash and in Settings, not in the top bar.

## Proposed order of work
1. Tokens and shared styles (colours, type, spacing) as one reusable set, so every widget draws from it.
2. Splash, then Settings (the two screens that set the tone).
3. Header, step card and control bar (the main journey).
4. Timeline drawer, Pause, Restart-confirm, finale, ticket phone.
5. Verification pass at 1080p and 4K: contrast measurement, keyboard focus order, Reduce Motion and Calm Mode, captured through PIE.

## Risks
- Several of these edits are C++ (`LFFJourneyPanel.cpp` drives labels, pips, timeline and theming). Removing pips or moving the mode switch changes both Blueprint and C++, and needs an editor rebuild.
- The `.uasset` files are binary and already modified in the working tree. Commit or branch first.
- Layout changes should be checked at 4K as well as 1080p. All pixel values above assume 1080p.
