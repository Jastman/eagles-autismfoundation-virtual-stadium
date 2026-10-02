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

#### As built (2026-10-02, `ULFFHUDWidget`)
- HUD buttons still never take Slate focus (the viewport keeps the arrow keys and Space), so the HUD runs its own focus ring.
- **Tab / Shift+Tab** (D-pad Down / Up on a controller) move the ring through the buttons on screen; it wraps at the ends. **Enter** (A) presses the ringed button; with no ring, Enter keeps its old meaning (Start, Yes). **Esc** drops the ring.
- Order: the card in front (splash or caption card), then the dock (Back, timeline, Next, the timeline marks when open), then Settings in the header. A modal card (Settings, Paused, Start again?) keeps the ring inside itself.
- The ring is a 4 px warm off-white line inside a 2 px dark halo, 4 px clear of the button, following its shape. It reads on the dark cards and on a bright sky (3:1 or better against both). It isn't kelly green, which already means "On" and "spoken".
- Disabled buttons and buttons that are hidden or collapsed are skipped. If the ringed button goes away (new step, card closed), the ring goes too.

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

### Voiceover and captions (as built, session 3, 2026-09-30)
- **The card is the script.** Each step's voice line is exactly its Title then its Body (`SourceData/Audio/VO/VO_MANIFEST.md`, generated from the journey JSON). No extra words, no hype.
- **Captions = the step card.** It's always on screen on journey steps, whether or not the voice is on, so the captions are never a second, different narration.
- **Voice is Off by default** and needs Sound on. Sound Off mutes beds, previews and voice. Voice Off + Sound On = beds only. Calm Mode forces Voice off.
- **One voice at a time.** A new step, Back, a timeline jump, Start Over or Pause stops the voice at once. It starts 0.5 s after the camera arrives, never during a move, never on the splash.
- **Previews never start under the voice:** picking Soft stops the voice first.
- The bed ducks 8 dB under the voice. One adult voice later: warm, unhurried, no announcer, no crowd baked in.
- A missing recording is skipped silently; the card stays.

---

## 5. Calm Mode

### What it changes (as built, 2026-09-30)
- **Sound off:** the whole audio device is muted, whatever the Sound toggle says.
- **Less movement on,** and every camera move is a plain cut (no easing, no fade).
- **Voice off,** captions (the card) stay.
- **Crowd hidden** (actors tagged `LFF_Crowd`).
- **Gates already open, the car already parked, the ticket phone already up and still,** and the splash holds a still wide shot instead of orbiting.
- **Intensity previews locked off** and their buttons hidden.
- The Sound and Less movement toggles show Off / On and are disabled until Calm Mode is turned off, which restores the visitor's own choices.

### Still recommended (not built)
- No bold reds or saturated colors; muted palette (pastels, grays, earth tones).
- Longer dwell time before any auto-timeout (e.g., 10 minutes instead of 5).
- No pop-up notifications or warnings; only gentle on-screen text.

### Demo default
- Recommended: **Calm Mode off** (standard mode on) for the initial kiosk demo to showcase motion and full controls.
- Stakeholder review will decide based on target audience feedback.

---

## 6. Inactivity and Reset

### Timeout values
- 120 seconds of no input (300 s while paused) before a gentle prompt. Lead decision D12, values in `ULFFKioskSettings` (ini).
- Prompt: "Are you still here? I'll return to the start in 30 seconds." It's shown on screen with no sound.
- A 30-second countdown, then a calm reset to the intro screen: fade out, then fade in, never an abrupt jump.

### Gentle reset
- Fade to black, hold for 1 second, fade in to intro screen.
- Show "Ready to start again?" with a Next button.
- No audio sting or sudden motion.

---

## 7. Disclaimers

> **Superseded on screen, 2026-10-02 (the user's decision).** The HUD no longer shows any disclaimer copy: no "Illustration" / "Approximate" / "Verified" chip, no "Not directions for the real stadium" line, no splash sentence, no prototype badge. The `DisclaimerLevel` data stays in the journey JSON and snapshot, so it can come back without a schema change. The rules below describe the earlier design.

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
struct FLFFVisitorSettings {
    bool bReducedMotion;           // default: false
    bool bSoundEnabled;            // default: false
    bool bCalmModeEnabled;         // default: false (stakeholder review)
    // Inactivity timing lives in ULFFKioskSettings (ini), not per-visitor settings.
    float TextSizeBias;            // default: 1.0, range: 0.8–1.5 (for scaling at different resolutions)
    float ContrastBias;            // default: 1.0, range: 1.0–1.3 (for increasing background/text contrast)
};
```

### Settings persistence (lead decision D11)
- **Settings are never saved between visitors.** Defaults come from `ULFFKioskSettings` (ini). Restart and inactivity reset both restore them. There's no personal data and no save file.
- The Settings UI updates the live `FLFFVisitorSettings` immediately. There's no "Apply" button.

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
- [ ] Implement `FLFFVisitorSettings` struct and save/load logic.
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
