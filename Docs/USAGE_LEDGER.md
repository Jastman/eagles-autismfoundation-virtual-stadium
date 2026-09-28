# Usage Ledger

Ceiling: **$145**. Reserve: **$25** (about 17%). Planned: **$120**. Budget resets Wed 2026-09-30 20:00 EDT.
Actual usage isn't directly measurable in this environment. Figures are lead estimates and serve as a stop-loss, not an accounting record.

| Workstream | Priority | Assigned Model/Agent | Estimated Budget | Actual Usage | Deliverable | Status | Stop Condition |
|---|---:|---|---:|---:|---|---|---|
| Phase 0: repo/env/version inspection | 1 | Lead | $12 | ~$3 | LEDGER.md, this file, Phase 0 report | In progress | Blocking questions answered, or 2 cycles |
| Stadium asset inspection and ingestion SOP | 2 | Asset agent | $15 | $0 | ASSET_INGESTION.md, stand-in plan | Not started | SOP plus stand-in spec; no asset means blocked checklist |
| Cesium/geospatial architecture | 1 | Cesium agent | $18 | $0 | ADR-0001, CESIUM_PLACEMENT.md | Not started | One reproducible setup; unverifiable APIs marked UNKNOWN |
| Unreal systems and journey architecture (incl. UI schema) | 1 | Systems + UI agents | $18 | $0 | ADR-0002, JOURNEY_SCHEMA.md, module map | Not started | Class map and schema an engineer can build |
| Accessibility and UX rules | 2 | A11y agent | $6 | $0 | ACCESSIBILITY.md, copy deck | Not started | Settings model plus disclaimer copy |
| Security, licensing and privacy | 3 | Security agent | $3 | $0 | Risk register rows | Not started | Ship/no-ship list |
| Architecture critique | 2 | Critic | $5 | $0 | Ranked findings | Not started | Runs once on the draft |
| Golden-path implementation | 1 | Lead + impl agent | $33 | $0 | C++/config/docs | Not started | Gates 1–3 resolved or waived |
| Testing, packaging and perf plan | 2 | QA agent | $10 | $0 | TEST_PLAN.md, build scripts | Not started | Evidence-based plan; can't run the engine here |
| Delivery plan | 3 | Lead (folded in) | $0 | $0 | Milestones in LEDGER | — | Folded into lead synthesis to save budget |
| **Contingency reserve** | — | — | **$25** | $0 | — | Held | Don't touch without telling the user |
| **Total** | | | **$145** | ~$3 | | | |

Scope checks happen at $30, $60 and $90 consumed.
