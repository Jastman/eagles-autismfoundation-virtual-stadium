# Test plan (V1 golden path)

Evidence rule (LEDGER D5): a gate passes only with a log, screenshot or recording. MCP results must be reproducible from the CLI. Record each result in `Docs/LEDGER.md` §9 with the date, machine and commit.

## T1. Build and unit tests (Gate 4 prerequisite)
| # | Check | Pass |
|---|---|---|
| T1.1 | Editor target compiles on the Mac (`Docs/LOCAL_AGENT_SETUP.md` §3) | Exit code 0 |
| T1.2 | `Automation RunTests LFF.` headless | 4 tests, 0 failures |
| T1.3 | Same on the Windows PC | Same |

## T2. PIE checklist (Gate 4 and Gate 5), on `L_LFF_Stadium`
| # | Action | Expected |
|---|---|---|
| 1 | Play | The intro shows over the step 1 view. The badge and disclaimer are visible. The camera doesn't move. Sound is off. |
| 2 | Right arrow | Step 1 of 4 appears with no camera move. |
| 3 | Right arrow | An eased move of about 3 s to the gate. The card shows "Step 2 of 4". The gate label appears and the plaza label hides. |
| 4 | Right arrow during a move | The camera retargets smoothly, with no jump. |
| 5 | P during a move | The camera freezes and the pause overlay appears. Right arrow does nothing. P again resumes. |
| 6 | Left arrow | Goes back one step. On step 1, Back is disabled. |
| 7 | At step 4, Right arrow | The button reads "Start again". Pressing it fades back to the intro. It doesn't wrap to step 1 mid-journey. |
| 8 | R mid-journey | "Start again?" appears. Clicking No or pressing controller B cancels (Esc also works in a packaged build; in PIE, Esc ends the session). Enter or controller A confirms. |
| 9 | Press 1 (Less movement), then Right | The move is a fade: out about 0.35 s, cut, back in. No visible camera path. |
| 10 | Press 3 (Calm Mode) | Reduced motion is on, the muted theme shows, and the label reads "Calm Mode: On". |
| 11 | S, click each toggle, then Close | Labels update right away. Settings survive a Restart. |
| 12 | Mouse-click Next, then press Space | Space advances once. It doesn't re-click the button or move focus. |
| 13 | Controller: D-pad, A, B, Y, Menu, View | Same behaviour as the keys. |
| 14 | Rename one `CT_` target's Target Id, then Play | That step shows "This step is not available yet", the camera stays put, and an error is logged. |
| 15 | Any step | The disclaimer and the "Prototype · Illustrative · Example ticket" badge are visible. The Cesium/Google credits aren't covered. |
| 16 | Watch for 60 s on any step | Nothing moves or flashes by itself. |

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
