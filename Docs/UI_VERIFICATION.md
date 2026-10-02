# UI Verification Pass (2026-10-02)

Scope: the HUD after the paper-light overhaul (`Docs/UI_AUDIT.md`). Checked in the editor with PIE screenshots, offscreen widget renders at 1920×1080 and 3840×2160, computed contrast ratios, and the real journey text.

## Method and limits
- **Contrast** is computed from the design tokens with the WCAG formula, not sampled from pixels.
- **Resolution.** The project has no custom DPI rule, so UMG uses its default shortest-side scaling (1.0 at 1080, 2.0 at 2160). Offscreen widget renders don't apply DPI scaling, so they give true 1080p layout measurements but the "4K" render is only a larger canvas. The in-game 4K look is therefore the 1080p layout drawn at 2×. A real 4K display or standalone build has not been tested.
- **Screenshots** were taken in a 1039×529 editor viewport, which UMG scales to about 0.49. That is wider than 16:9, so narrower aspect ratios (16:10, 4:3, mobile) are still untested.
- **Not tested:** the ticket phone step, Restart-confirm card, voice-over playback, a real controller, touch.

## Passed
| Check | Result |
|---|---|
| Body text (ink on paper) | 15.7:1 (needs 7:1 for AAA) |
| Secondary text (muted on paper) | 7.5:1 |
| Primary button text (white on brand) | 9.7:1 |
| Secondary button outline on white | 3.35:1 (needs 3:1) |
| Selected toggle outline (kelly on brand) | 4.2:1 |
| Progress fill on track | 6.6:1 |
| High Contrast surface (ink on white) | 17.4:1 |
| Hit targets at 1080p | All buttons 64 px tall or more (Start 72) |
| Minimum text size at 1080p | 18 px labels, 22 to 24 px controls (after the timeline fix below) |
| Worst-case step card height at 1080p | About 500 px of 688 px available. About 625 px with Large text on. No step overflows. |
| Header, dock, card, drawer overlap | None at 1080p. Drawer gap to card widened to 28 px (see below). |
| Timeline drawer opens, label flips to "Hide day timeline" | Yes |
| High Contrast turns card surfaces to white with dark text | Yes |
| Calm Mode locks Sound, Voice, Crowd and Less movement and shows them as such | Yes |

## Bugs found and fixed in this pass
1. **Chosen toggles unreadable on hover.** Only the normal state had the brand fill. Hovering "High contrast: On" or "Calm Mode: On" showed white text on a pale fill. Fixed: hover, pressed and disabled states of a chosen toggle keep a brand-colored fill (`ApplyModeButton`).
2. **Locked-but-On toggles unreadable.** "Less movement: On" under Calm Mode was white on the pale disabled fill. Fixed by the same change.
3. **Timeline labels were 16 px**, under the 18 px minimum in `Docs/ACCESSIBILITY.md`. Raised to 18 px and the marks widened from 118 to 128 px.
4. **Step card sat about 4 px above the timeline drawer** after the label change. Card offset raised from 272 to 296 px.

## Open findings (not fixed)
1. **No keyboard focus indicator and no Tab navigation.** Every HUD button is deliberately non-focusable so the game viewport keeps the arrow keys and Space. Pressing Tab does nothing visible. `ACCESSIBILITY.md` §1 requires a visible focus ring and a defined tab order. Fixing it means a custom focus model (Tab cycles buttons, Enter activates, a focus ring on the focused button) that does not fight Space = Next. This needs a decision.
2. **Large text only scales the splash and step cards.** The dock, header pills and Settings card keep their size. The unscaled controls are still at or above the 18 px floor, but visitors who turn on Large text may expect all of the UI to grow.
3. **Karaoke highlight relies on color alone.** The spoken word is green (#2F7D12) against ink (#0B1D1F): 3.4:1 between them. An underline or weight change would make it independent of hue.
4. **Disabled controls fall below 3:1** (for example "Sound: Off" under Calm Mode, about 2.8:1). WCAG exempts disabled controls, and the label still says what the state is.
5. **Window and aspect ratio.** Layout was checked at 16:9 by measurement and at about 1.96:1 by screenshot only. Narrower windows are untested.
6. **Editor shutdown crash.** Cesium's `UCesiumGaussianSplatSubsystem::Deinitialize()` asserts on every editor quit and leaves a `CrashReportClient` holding port 8000. Kill that process before relaunching if the MCP server will not start. Unrelated to the UI.

## Tooling notes
- PIE ignores a first mouse click until the pointer has moved over the target, so a hover is needed before each click when driving it with a desktop-click tool.
- Esc ends the PIE session. Use S to close Settings.
