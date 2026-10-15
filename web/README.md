# Web build (CesiumJS, iPad)

The Game Day Visual Schedule running in a browser: the same 23-step guided tour, Explore, Places, Settings, Calm Mode,
narration with karaoke, scene cues and audio as the Unreal build, drawn with CesiumJS. iPad Safari is the target;
desktop Chrome, Edge, Firefox and Safari also work.

It reads the Unreal project's own files (`SourceData/`, `Content/LFF/Crowd/`, the level via `web/data/scene.json`), so
most content changes reach both builds. Every difference is logged in [`Docs/WEB_PARITY.md`](../Docs/WEB_PARITY.md).

## Run

Needs Node 20+. Optional: `ffmpeg` for compressed audio (without it the WAVs are served as they are), and Python 3
with Pillow for the crowd's fan art, drawn by the same code as Unreal's (without it the crowd uses simpler cards).

```sh
cd web
npm install
cp .env.example .env.local   # then fill in the tokens
npm run dev                  # http://localhost:5173 ; on an iPad on the same network: npm run dev -- --host
npm run build                # static site in web/dist/, host it anywhere (HTTPS for iPad audio + full screen)
```

`web/.env.local` (git-ignored):

| Variable | What |
|---|---|
| `VITE_CESIUM_ION_TOKEN` | ion token with access to Google Photorealistic 3D Tiles (asset 2275207) |
| `VITE_STADIUM_ION_TOKEN` | ion token scoped to the stadium assets (may be the same token) |
| `VITE_STADIUM_ASSET_IDS` | stadium, seats, field: `6040628,6040635,6040637` |

URL parameters for testing: `?ionToken=` (remembered on the device), `?stadiumToken=`, `?stadiumAssets=`,
`?stadiumPlacement=auto|registration|ion`, `?stadiumHeading=<deg>`, `?stadiumOffset=e,n,u` (metres), `?noLoadingScreen`,
`?noCrowd`.

## Check

```sh
npm test               # state machine, rules and layout tests ported from the Unreal automation tests
npm run typecheck
npm run check-parity   # Unreal <-> web drift, both directions
```

## Layout

| Path | Ports |
|---|---|
| `src/journey/` | `FLFFJourneyStateMachine`, `ULFFJourneySubsystem`, `LFFJourneyRules`, journey types |
| `src/scene/` | `ALFFCameraRig`, cues, fireworks, crowd, tile streaming, level highlight, the level's stand-ins and signs |
| `src/ui/` | `ULFFHUDWidget` and its panels, callouts, `LFFUILayout`, the loading screen |
| `src/input/` | `ALFFPlayerController` bindings plus touch |
| `src/config.ts` | `ALFFGameMode` defaults |
| `tools/` | asset preparation, level extraction, parity check |
| `parity/sources.json` | Unreal files ported by hand and the hash each had when the web last matched |
