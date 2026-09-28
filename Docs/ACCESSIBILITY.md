# Accessibility and Neuroinclusive UX Rules

## 1. Controls

### Hit-target sizes
- Minimum 48×48 dp (approximately 54×54 px at 1080p, 108×108 px at 4K) for any touchable button.
- Next/Back/Pause buttons placed at the bottom or right edge with consistent 16–24 px spacing between them and the screen edge.
- Confirm focus order (Tab, Shift+Tab) and visible focus indicator on all interactive elements (keyboard and controller).

### Button labels
- All buttons must show text + icon, never color or icon alone.
- Label text: "Next", "Back", "Pause", "Resume", "Restart", "Settings", "Home".
- Labels in 18 pt or larger at 1080p, 36 pt or larger at 4K, sans-serif.

### Input: keyboard, mouse, controller, touch
- Single-press actions (Next, Back, Pause, Restart) → no hold or double-tap required.
- Keyboard: arrow keys (Left/Right), spacebar (Next), Esc (Pause), R (Restart), Tab/Shift+Tab for navigation.
- Controller: D-pad Left/Right (Next/Back), A/Cross (Resume/Pause), Y/Triangle (Restart), Start/Menu (Settings).
- Touch: 48×48 px minimum tap targets on bottom control bar.
- **No timed interactions**: no countdown bars, no auto-advance, no age-gate time limits.

### Focus and keyboard navigation
- Tab order: intro → disclaimer → first control → viewport → next/back buttons → settings button → pause button.
- Visible focus indicator: 2–3 px contrast-minimum border or ring (WCAG AA, 3:1 at minimum).

---

## 2. Typography and Contrast

### Font size (headline / body / controls)
- **1080p**: Headline 32–40 pt, body 16 pt, controls 18 pt minimum.
- **4K**: Headline 64–80 pt, body 32 pt, controls 36 pt minimum.
- All sans-serif (Roboto, Inter, Segoe UI, or equivalent).

### WCAG Contrast Ratios
- Body text on background: 4.5:1 (WCAG AA, normal text).
- Large text (18pt+) on background: 3:1 (WCAG AA, large text).
- Button text on button background: 4.5:1.
- Focus indicators on all interactive elements: 3:1 minimum.

### Plain language
- One idea per paragraph; maximum 12–15 words per sentence in step descriptions.
- No jargon; replace "concourse" with "the main walkway inside" if introducing it.
- Short descriptive titles: 2–4 words (e.g., "Arrival", "Gate Entry", "Seating").

---

## 3. Motion

### Default behavior (standard mode)
- Camera fly-to transition: 3–4.5 seconds (from `FlyToDuration` in schedule data).
- Easing: ease-in-out cubic (current implementation via `FMath::InterpEaseInOut`, exponent 2).
- **No camera roll**: Pitch (up/down) and Yaw (left/right) only; no rotation around the forward axis.
- **No sudden FOV changes**: maintain 90° FOV throughout.

### Reduced-motion mode
- **Off by default for the demo** (matches `bReducedMotion` below; lead decision, to be revisited at stakeholder review). It's offered as a one-tap "Less movement" toggle on the intro screen and in Settings.
- The camera doesn't fly. It does a short fade-through: fade to a neutral color over about 0.3 s, change position, fade back over about 0.3 s. A hard cut can be disorienting, so a fade is preferred.
- No camera path motion. The location and rotation change while the screen is faded.
- Immediate availability of Next/Back (not blocked during transition).

### Motion restrictions (all modes)
- No autoplay or automatic advancement.
- No camera jitter, bob, or micro-animations.
- No flashing or rapid-blink graphics (WCAG 2.3.1: no more than three flashes in any one-second period, unless below the general and red flash thresholds. V1 target: no flashing content at all.).

---

## 4. Audio

### Default
- **Off by default**: app starts silent, no background music or narration.
- **Toggle in Settings**: "Sound On/Off" switch.
- When off, all audio output is suppressed (no accidental sound).

### If audio is enabled
- Captions or text transcripts for any narration or dialogue.
- Noise previews (e.g., stadium crowd, chanting) must be behind an explicit warning: "This stop has loud crowd sounds. Press [Listen] to preview."
- Volume capped at 85 dB SPL (approximately 80% of max system volume).
- **Recommendation for Golden Path v1**: defer noise previews; include captions for static text-to-speech only if added later.

---

## 5. Calm Mode

### What it changes
- Background music off (or very quiet, <50 dB SPL).
- Animations use reduced-motion settings (instant cuts, no easing).
- No bold reds or saturated colors; muted palette (pastels, grays, earth tones).
- Longer dwell time before any auto-timeout (e.g., 10 minutes instead of 5).
- No pop-up notifications or warnings; only gentle on-screen text.

### Demo default
- Recommended: **Calm Mode off** (standard mode on) for the initial kiosk demo to showcase motion and full controls.
- Stakeholder review will decide based on target audience feedback.

---

## 6. Inactivity and Reset

### Timeout values
- 5 minutes of no input (Next, Back, Pause, Settings, or touch) before gentle prompt.
- Prompt: "Are you still here? I'll return to the start in 30 seconds." (visible on screen, no sound).
- 30-second countdown, then auto-reset to intro disclaimer + first stop (not an abrupt jump).

### Gentle reset
- Fade to black, hold for 1 second, fade in to intro screen.
- Show "Ready to start again?" with a Next button.
- No audio sting or sudden motion.

---

## 7. Disclaimers

### DisclaimerLevel and visual treatment
Add a `DisclaimerLevel` enum field to `FStadiumScheduleStop`:
```
enum class EDisclaimerLevel : uint8 {
    Verified,        // real stadium data, verified by operations
    Approximate,     // reasonable estimate, not verified
    Illustrative     // placeholder only, not for navigation
};
```

### On-screen badge
Each stop displays a small, persistent badge with icon + text (not color-only):
- **Verified** (green checkmark + text): "Real location"
- **Approximate** (yellow triangle + text): "Approximate location"
- **Illustrative** (blue rectangle + text): "Illustration"

### Disclaimer text (appears on each step card)
- **Approximate/Illustrative steps must include**: "This is a preview to help you prepare. It is not directions for the real stadium."
- Placed above or below the step description.

### Golden Path v1 disclaimer levels
| Stop | Level | Reason |
|---|---|---|
| Arrival (exterior) | Approximate | parking/drop-off zone referenced from template, not verified by ops |
| Gate Entry (security) | Illustrative | interior bag-check area not scanned; placement is placeholder |
| Concourse (interior) | Illustrative | no verified interior data; conceptual walkway |
| Seating (field view) | Approximate | seat counts and layout from public data, not verified ops access |

---

## 8. Settings Model (C++ struct)

### Data and ranges
```
struct FTourSettings {
    bool bReducedMotion;           // default: false
    bool bSoundEnabled;            // default: false
    bool bCalmModeEnabled;         // default: false (stakeholder review)
    float InactivityTimeoutSecs;   // default: 300.0 (5 min), range: 120–600
    float TextSizeBias;            // default: 1.0, range: 0.8–1.5 (for scaling at different resolutions)
    float ContrastBias;            // default: 1.0, range: 1.0–1.3 (for increasing background/text contrast)
};
```

### Settings persistence
- Save to `Saved/Config/VirtualStadium_Settings.json` or engine's user settings.
- Load on startup if available; use defaults otherwise.
- Settings UI updates the live struct and persists on change (no "Apply" button needed).

### Settings menu items
| Label | Type | Default | Range | Note |
|---|---|---|---|---|
| Reduce Motion | Toggle | Off | On/Off | Camera cuts to next stop instead of flying |
| Sound | Toggle | Off | On/Off | Enables music, narration, sound preview buttons |
| Calm Mode | Toggle | Off | On/Off | Muted colors, longer timeouts, no rapid animations |
| Reset to Demo | Button | — | — | Clears user settings, returns to defaults |
| Text Size | Slider | 1.0× | 0.8–1.5× | Scales UI text only, does not affect video captions |
| Contrast | Slider | 1.0× | 1.0–1.3× | Darkens background, lightens text (post-processing or CSS) |

---

## 9. Implementation checklist for Gate 5 (accessibility controls and reduced motion)

- [ ] Add `DisclaimerLevel` enum and field to `FStadiumScheduleStop`.
- [ ] Add disclaimer badge rendering to `UStadiumTourWidget`.
- [ ] Implement `FTourSettings` struct and save/load logic.
- [ ] Add Settings menu UI with sliders and toggles.
- [ ] Implement reduced-motion camera transitions (instant vs eased).
- [ ] Add Pause/Resume button and input handling.
- [ ] Implement inactivity timeout with gentle prompt and reset.
- [ ] Verify button hit-targets, focus order, and contrast ratios at 1080p and 4K.
- [ ] Test keyboard, controller, and touch input paths.
- [ ] Review disclaimer text and badge rendering.

---

## References
- WCAG 2.1 Level AA: https://www.w3.org/WAI/WCAG21/quickref/
- WCAG 2.3.1 Three Flashes or Below Threshold: https://www.w3.org/WAI/WCAG21/Understanding/three-flashes-or-below-threshold.html
- Apple Human Interface Guidelines (Accessibility): https://developer.apple.com/design/human-interface-guidelines/accessibility/
- Interaction Design Foundation: https://www.interaction-design.org/literature/topics/accessibility
