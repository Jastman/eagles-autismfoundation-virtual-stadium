# Usage Ledger

Ceiling: **$145**. Reserve: **$25** (about 17%). Planned: **$120**. Budget resets Wed 2026-09-30 20:00 EDT.
Actual usage isn't directly measurable in this environment. Figures are lead estimates and serve as a stop-loss, not an accounting record.

| Workstream | Priority | Assigned Model/Agent | Estimated Budget | Actual Usage | Deliverable | Status | Stop Condition |
|---|---:|---|---:|---:|---|---|---|
| Phase 0: repo/env/version inspection | 1 | Lead | $12 | ~$8 | LEDGER.md, this file, Phase 0 report, tooling docs | Done | Blocking questions answered, or 2 cycles |
| Stadium asset inspection and ingestion SOP | 2 | Asset agent (Haiku) | $15 | ~$1 | ASSET_INGESTION.md, RISK_REGISTER.md | Draft done | SOP plus stand-in spec; no asset means blocked checklist |
| Cesium/geospatial architecture | 1 | Cesium agent (Opus) | $18 | ~$6 | ADR-0001, CESIUM_PLACEMENT.md | Draft done | One reproducible setup; unverifiable APIs marked UNKNOWN |
| Unreal systems and journey architecture (incl. UI schema) | 1 | Systems + UI agent (Opus) | $18 | ~$7 | ADR-0002, JOURNEY_SCHEMA.md | Draft done | Class map and schema an engineer can build |
| Accessibility and UX rules | 2 | A11y agent (Haiku) | $6 | ~$1 | ACCESSIBILITY.md, COPY_DECK.md, STAKEHOLDER_REVIEW.md | Draft done | Settings model plus disclaimer copy |
| Security, licensing and privacy | 3 | Folded into asset agent | $3 | $0 | RISK_REGISTER.md | Done (merged) | Ship/no-ship list |
| Architecture critique | 2 | Critic (Opus) | $5 | ~$5 | Ranked findings → ADR-0003 | Done | Runs once on the draft |
| Golden-path implementation | 1 | Lead | $33 | ~$15 | 8 classes + tests, config, data, setup/test docs | Code written, awaiting compile | Gates 1–3 resolved or waived |
| Testing, packaging and perf plan | 2 | QA agent | $10 | $0 | TEST_PLAN.md, build scripts | Not started | Evidence-based plan; can't run the engine here |
| Delivery plan | 3 | Lead (folded in) | $0 | $0 | Milestones in LEDGER | — | Folded into lead synthesis to save budget |
| **Contingency reserve** | — | — | **$25** | $0 | — | Held | Don't touch without telling the user |
| Lead synthesis (cycle 3) | 1 | Lead | (in Phase 0/arch lines) | ~$6 | LEDGER D6–D17 | Done |
| Failed launches (Sonnet unavailable) | — | — | — | ~$0.5 | — | Absorbed |
| **Total** | | | **$145** | **~$50** | | | |

Scope checks happen at $30, $60 and $90 consumed.

## 25% checkpoint (2026-09-28, about $30 spent)
Planned work is on track. Scope changed: the video deliverable (D14) and example-ticket tokens (D10) were added, both small. The macOS build is secondary (A1). No cuts needed. The next spend is the critic ($5), then golden-path implementation ($33).

## About 35% spent (2026-09-28, about $50)
Implementation came in under its line item because the critic cut the class count. The next spend is reactive: fixing compile errors from the workstation (keep it under $15 in total). Tier B work starts only after T2 passes.
