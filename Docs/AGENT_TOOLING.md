# Agent tooling: Unreal skills, MCP servers, UI skills

Status: Draft, 2026-09-28. The user asked for the tools below. **None of them could be installed or read from the cloud session**, for these reasons:
- mcpmarket.com, mcpservers.org and medium.com are blocked by the session's network egress proxy.
- The session's GitHub access is limited to this repo, so `EpicGames/*` can't be read.
- The org plugin and skill catalogs don't contain them.

All install steps below are for **Claude Code on the UE 5.8 workstation**. See `Docs/LOCAL_AGENT_SETUP.md`.

## Inventory and recommendation

| Tool | Kind | Use for | Recommendation | Status |
|---|---|---|---|---|
| [EpicGames/unreal-engine-skills-for-claude-code-plugin](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin) | Claude Code plugin (Epic, first-party) | UE 5.8 conventions, editor/MCP workflows | **Primary.** Install first. | Not inspected (outside session repo scope) |
| Unreal MCP (ships with UE 5.8, experimental) | Editor-embedded MCP server, localhost | Driving the editor: actors, assets, tests | **Primary editor MCP.** CLI logs remain the gate evidence (LEDGER D5). | Not connected |
| [VibeUE](https://mcpservers.org/servers/kevinpbuckley/vibeue) | Third-party UE editor plugin plus MCP | Editor automation (Blueprint/UMG editing, if it offers that) | **Optional, only if Unreal MCP lacks a needed capability** (for example UMG/Blueprint graph authoring). Don't run two editor-control MCPs at once until you've checked they don't conflict. Check 5.8 support. | Not inspected |
| [unreal-engine-expert](https://mcpmarket.com/tools/skills/unreal-engine-expert) | Third-party skill | General UE guidance | Optional. Probably overlaps with Epic's plugin, so prefer Epic's if they conflict. | Not inspected |
| [unreal-development](https://mcpmarket.com/tools/skills/unreal-development) | Third-party skill | General UE guidance | Optional. Same overlap caveat. | Not inspected |
| [ui-ux-interface-designer](https://mcpmarket.com/tools/skills/ui-ux-interface-designer) | Third-party skill | UI layout and visual design | Optional. Most likely web/React-oriented, so only its principles carry over to UMG. `Docs/ACCESSIBILITY.md` wins on any conflict. | Not inspected |
| [ui-interface-guardrails](https://mcpmarket.com/tools/skills/ui-interface-guardrails) | Third-party skill | UI review checklist | Useful as a review pass on WBP_LFF_* designs. `Docs/ACCESSIBILITY.md` wins on any conflict. | Not inspected |
| [Medium: "5 Claude skills that fix Claude's ugly UI"](https://divadsanders.medium.com/5-claude-skills-that-fix-claudes-ugly-ui-ai-web-design-ec9cd27bf5fe) | Article listing skills | Web UI design | Read it and pick at most one skill. It's web-focused, and the project's calm, low-stimulation UI rules take priority over visual flair. | Not inspected |

## Install on the workstation (Claude Code)

1. **Epic plugin.** In Claude Code, run `/plugin marketplace add EpicGames/unreal-engine-skills-for-claude-code-plugin`, then `/plugin` to browse and install it. If the repo isn't a marketplace, follow its README instead. Once the names are confirmed, record the exact plugin name and version here and add them to `.claude/settings.json` (`extraKnownMarketplaces` and `enabledPlugins`) so the whole team gets the same setup.
2. **Unreal MCP.** Follow `Docs/LOCAL_AGENT_SETUP.md` §2.
3. **VibeUE (optional).** Install per its README into `Plugins/` or the engine, then add its MCP endpoint to the local MCP config. Record its version and 5.8 compatibility here.
4. **Third-party skills.** Download them, **read each `SKILL.md` in full**, then commit the approved ones under `.claude/skills/<name>/`. That way the cloud session and every teammate use the same reviewed version.

## Trust rules for third-party skills and MCPs

- A skill is instructions the agent will follow, and an MCP server is code with editor access. Treat both as third-party code.
- Review each one before committing it:
  - no instructions to exfiltrate data, disable safety checks or auto-approve tools;
  - no hidden network calls;
  - no scripts you haven't read.
- Pin versions (commit SHA or release tag) and record them in this file.
- Order of authority if guidance conflicts:
  1. Project docs (`ACCESSIBILITY.md`, the ADRs, `LEDGER.md`)
  2. Epic first-party plugin and official docs
  3. Third-party skills
- Don't give an MCP server Cesium ion tokens or other credentials through tool arguments.

## Getting these tools into the cloud session too (optional)

Pick one of these:
- Commit the reviewed skill folders to `.claude/skills/` in this repo, and the cloud session will load them on its next start.
- Or widen the cloud environment's network policy and GitHub repo scope (see <https://code.claude.com/docs/en/claude-code-on-the-web>).

Editor-embedded MCP servers (Unreal MCP, VibeUE) stay workstation-only either way.
