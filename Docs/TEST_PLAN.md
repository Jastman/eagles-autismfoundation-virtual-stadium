# Test plan (V1 golden path)

Evidence rule (LEDGER D5): a gate passes only with a log, screenshot or recording. MCP results must be reproducible from the CLI. Record each result in `Docs/LEDGER.md` §9 with the date, machine and commit.

## T1. Build and unit tests (Gate 4 prerequisite)
| # | Check | Pass |
|---|---|---|
| T1.1 | Editor target compiles on the Mac (`Docs/LOCAL_AGENT_SETUP.md` §3) | Exit code 0 |
| T1.2 | `Automation RunTests LFF.` headless | 5 tests, 0 failures |
| T1.3 | Same on the Windows PC | Same |

## T2. PIE checklist (Gate 4 and Gate 5), on `L_LFF_Stadium`
| # | Action | Expected |
|---|---|---|
| 1 | Play | The splash card ("Welcome to Lincoln Financial Field", **Start Your Day**) shows over the step 1 view. Only the badge (top left) and Settings (top right) are around it: no Back, no Start again, no toggles. The disclaimer sentence is under the button. The camera doesn't move. Sound is off. |
| 2 | Right arrow or click Start Your Day | The splash fades out and step 1 appears with no camera move: top bar "Step 1 of 16 · Pause · Sound · Settings", the one line "Illustration. Not directions for the real stadium." under the badge, the small lower-left caption with the Illustration chip, and Back · pips · Next along the bottom. The stadium fills most of the frame. |
| 3 | Right arrow | A fade and cut to the parking lot (step 2 is authored ReducedMotion): three stand-in cars and "Parking (illustration)" above them. Right again: an eased move of about 3 s to the line at the gate. |
| 4 | Right arrow during a move | The camera retargets smoothly, with no jump. |
| 5 | P during a move | The camera freezes and the pause overlay appears. Right arrow does nothing. P again resumes. |
| 6 | Left arrow | Goes back one step. On step 1, Back returns to the splash without moving the camera. |
| 7 | At step 16, Right arrow | The button reads "Start again". Pressing it fades back to the intro. It doesn't wrap to step 1 mid-journey. |
| 8 | R mid-journey | "Start again?" appears. Clicking No or pressing controller B cancels (Esc also works in a packaged build; in PIE, Esc ends the session). Enter or controller A confirms. |
| 9 | Press 1 (Less movement), then Right | The move is a fade: out about 0.35 s, cut, back in. No visible camera path. |
| 10 | Press 3 (Calm Mode) | "Calm Mode: On". Sound reads Off and Less movement reads On, both disabled. The log shows "Sound off: primary volume 0" and "Calm Mode on: N crowd actors hidden". The next Next or Back is a plain cut (log: "Instant to CT_…"). On steps 11–12 the "Preview this?" row is gone. Press 3 again: the visitor's own Sound and Less movement come back. |
| 11 | S (or Settings), click each toggle, then Close | Settings opens over a dimmed world with the key legend and Start again. Labels update right away. High contrast makes the panels opaque. Settings survive a Restart. Buttons show no key hints anywhere else. |
| 12 | Mouse-click Next, then press Space | Space advances once. It doesn't re-click the button or move focus. |
| 13 | Controller: D-pad, A, B, Y, Menu, View | Same behaviour as the keys. |
| 14 | Rename one `CT_` target's Target Id, then Play | That step shows "This step is not available yet", the camera stays put, and an error is logged. |
| 15 | Any step | The disclaimer chip and the "Prototype · Illustrative · Example ticket" badge are visible. The Cesium/Google credits aren't covered. |
| 16 | Watch for 60 s on any step | Nothing moves or flashes by itself. |
| 17 | Press 2 (Sound) on and off | On: the plaza murmur fades in (log "Ambient bed playing: SW_LFF_AmbientPlaza"), nothing else plays. Off: silence (log "primary volume 0"). No horns or fireworks at any point. |
| 18 | Step 11 (Loud sounds) | "Preview this? Off" shows; nothing plays on arrival. With Sound off the button is disabled and says to turn Sound on. With Sound on, V or the button plays an 8 s muffled sample once, then reads Off again. Turning Sound off mid-sample stops it, and turning Sound back on doesn't restart it. |
| 19 | Step 12 (Bright lights), V | The picture brightens and dims slowly (4 s cycle), never flickers. V again ramps back to normal in under a second. Next also stops it. |
| 20 | Steps 1, 7, 8, 9, 14 | Each stop shows its stand-in (kiosk, van, portal and so on) with its label above it. No world label sits behind the caption card or in the lower third. |
| 21 | Any Next that would pass through a wall or prop | It fades and cuts instead of easing (log "…crosses geometry; fading and cutting instead of easing"). The camera never flies through geometry. |


### T2 session 3 rows (voice, splash orbit, timeline, cues, crowd)
| # | Action | Expected |
|---|---|---|
| 22 | Play and wait on the splash | The camera circles the stadium slowly (one lap in 40 s, log "Splash: orbit of 400 m…"). With 1 or 3 on, it holds still ("Splash: still wide shot"). No voice. |
| 23 | Start Your Day | The camera glides along the orbit into the wide shot of the stadium and skyline (log "glide along the splash orbit"). Card: "Step 1 · ~9:00 AM", chip Approximate. The timeline shows six marks; none is emphasized. |
| 24 | Next to parking | One car rolls into the empty bay and stops (log "Scene cue CarArrival playing"). With 1 or 3 on: the car is already parked. |
| 25 | Next to the 10:00, 11:00 and 11:30 steps | Each gate slides open once the camera arrives (log "Scene cue Gate… playing"); the step's timeline mark is the only emphasized one. With 3 on: already open. Back to the step before: closed again. |
| 26 | Click a timeline mark | The journey jumps to that step (log "Timeline: jump to step N"). Marks do nothing while paused or in Settings. |
| 27 | Ticket step | The phone rises from the bottom (about 1.2 s), then the barcode sweep plays. With 1 or 3 on: already up, still. No standing board in the world. |
| 28 | Sound On, Voice On (2, then 5 or Settings) | The card is read once, 0.5 s after arriving (log "voice playing VO_NN…; bed ducked -8 dB"); the bed dips under it and comes back after ("Voice finished."). |
| 29 | Next while the voice is reading | The voice stops at once (log "Voice stopped." in the same frame as the step change); the next card is read only after the camera arrives. No overlap. |
| 30 | Sound Off with Voice On | Silence (primary volume 0). Voice reads Off and is disabled. Sound On again reads the current card. Calm Mode: Voice Off, captions stay. |
| 31 | Anthem step, V (Calm Mode off) | Any voice stops; the eagle flies across once (about 9 s) and three soft firework bursts open and shrink over the east stand; then the preview turns itself Off. Nothing flashes. With 3 on, the preview row is hidden. |
| 32 | 6 (Crowd) | Crowd: Off hides the 15 stand-in people; Low shows them. Calm Mode hides them. Staff stay visible. |
| 33 | Last step | Next reads "Start Over" (back to the splash, voice stopped). "Learn More" opens the Eagles Autism Foundation page in the browser. No logo (none in the project). |

## T3. Packaged build (Gate 6), on both Mac and Windows
| # | Check | Pass |
|---|---|---|
| 1 | Launch the archive from `Saved/Packaged` | The app opens straight into the intro. No editor is required. |
| 2 | Google tiles stream in | Tiles visible within 30 s on the demo network. |
| 3 | Repeat T2 rows 1–13 | Same results |
| 4 | Network off, then relaunch | The app still runs and the journey UI works. Tiles are missing; that's documented in KNOWN_LIMITATIONS. |
| 5 | 1080p and the demo TV resolution | Text is readable from about 2.5 m. The layout doesn't clip. |
| 6 | Frame time | A stable frame rate is recorded with the lead's target, once the hardware is known. Capture method is **VERIFY**, from UE 5.8 docs only. |
| 7 | The zip installs cleanly on a collaborator machine | Following the release notes, it opens past SmartScreen or Gatekeeper. |

## T4. Video recording (deliverable b)
Capture the **packaged Development build** in OBS at the TV's resolution, 60 fps. The operator follows this shot list and holds each step for about 12 s so it can be read aloud:
1. The intro, held for 10 s.
2. Steps 1 to 4 with the default motion.
3. Open Settings and show Less movement and Calm Mode. Re-run step 2 → 3 in reduced motion.
4. The end screen, then "Start again".

Keep the badge, disclaimer and credits in frame. Record the first take by **Oct 5**, and the final take after the stakeholders sign off on the wording.

## T5. Accessibility review (Gate 5 and Gate 7)
Walk through `Docs/STAKEHOLDER_REVIEW.md` with the Eagles Autism Foundation and autistic advisors. Log their changes to the wording as JSON edits.
