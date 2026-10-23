# Working in this repo

This repo holds two builds of one experience: the Unreal project (UE 5.8.3 + Cesium for Unreal) and the web build in
`web/` (CesiumJS, iPad Safari first). They must stay the same experience.

- Before committing a change to either build, run `cd web && npm run check-parity`.
- A web-only change to behaviour, copy, timing or UX gets a row in `Docs/WEB_PARITY.md` with Unreal status `Pending`,
  in the same commit. Put copy and timing changes in `SourceData/Journey/*.json` when possible: both builds read it.
- When an Unreal file listed in `web/parity/sources.json` changes, port the change to its web counterparts, then
  `node web/tools/check_parity.mjs --accept <file>`.
- After saving `Content/LFF/Levels/L_LFF_Stadium.umap`: `python3 web/tools/extract_umap.py && node web/tools/build_scene_data.mjs`.
- Never commit Cesium ion tokens. They live in `web/.env.local` (git-ignored).
