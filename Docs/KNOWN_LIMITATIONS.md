# Known limitations (V1)

- **Illustrative only.** No location, route, gate, restroom, concession or seat view is verified. The seat is an example ticket (Section 111, Row 10, Seat 8), not real ticket data.
- **Online only.** Google Photorealistic 3D Tiles stream live and can't be cached or used offline. Without a network, the journey UI still works but the stadium surroundings are missing. The recorded video is the offline fallback.
- **Stand-in stadium.** The exterior is the Google tiles' own stadium mesh. Interiors are simple neutral proxies until the CGTrader model is licensed and ingested (`Docs/ASSET_INGESTION.md`).
- **Keyboard focus navigation (Tab) isn't supported.** Keyboard users use the shortcuts shown on each button. On a controller, the settings toggles aren't reachable; use keys 1–4 or the mouse.
- **No inactivity reset** (it's Tier B).
- **No noise preview, crowds or video boards** (they're Tier B).
- **Unsigned builds.** Windows SmartScreen and macOS Gatekeeper warn on first launch. See the release notes.
- **Not compiled by the author.** The C++ was written without a compiler. The first workstation build may need small fixes; see LEDGER §9.
- **English only.** The text uses `LOCTEXT` or Data Table FText, so it's ready to localize later.
