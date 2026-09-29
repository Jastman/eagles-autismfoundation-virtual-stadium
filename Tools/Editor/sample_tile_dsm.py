"""Samples a height grid of the Google tiles around the stadium, for model registration (ADR-0004).

Run from the repo root with the editor open (tiles visible, Unreal MCP on 127.0.0.1:8000) and BEFORE the tiles are
clipped over the site:
  python3 Tools/Editor/sample_tile_dsm.py [--half 30] [--step-m 5] [--out Saved/StadiumImport/tile_dsm.json]

Grid points are in the LFF_SiteRoot frame (+X across the field, +Y toward the south end); each is a vertical line
trace onto the tiles (Cesium builds tile collision). The editor camera is parked 450 m overhead first so tiles load
at even detail. Takes ~12 minutes for the default 61 x 61 grid; runs in batches to stay under the HTTP timeout.
"""
import argparse, json, os, sys, time

sys.path.insert(0, os.path.dirname(__file__))
from unreal_mcp import call  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--half", type=int, default=30)
ap.add_argument("--step-m", type=float, default=5.0)
ap.add_argument("--heading-deg", type=float, default=-6.5, help="LFF_SiteRoot yaw")
ap.add_argument("--out", default="Saved/StadiumImport/tile_dsm.json")
a = ap.parse_args()

TEMPLATE = r'''
import json, math
C, Sn = math.cos(math.radians(%(h)f)), math.sin(math.radians(%(h)f))
def run():
    out = []
    for vi in range(%(a)d, %(b)d):
        row = []
        for ui in range(-%(half)d, %(half)d + 1):
            u, v = ui * %(step)f, vi * %(step)f
            x, y = u*C - v*Sn, u*Sn + v*C
            d = execute_tool("editor_toolset.toolsets.scene.SceneTools.trace_world",
                json.dumps({"start": {"x": x, "y": y, "z": 20000}, "end": {"x": x, "y": y, "z": -3000}}))["returnValue"]
            row.append(None if d is None or d < 0 else round((20000 - d) / 100.0, 2))
        out.append(row)
    return {"rows": out}
'''
call("EditorToolset.EditorAppToolset", "SetCameraTransform", {"transform": {
    "location": {"x": 0, "y": 0, "z": 45000}, "rotation": {"pitch": -89.9, "yaw": 0, "roll": 0}, "scale": {"x": 1, "y": 1, "z": 1}}})
time.sleep(25)
rows, t = [], time.time()
for lo in range(-a.half, a.half + 1, 8):
    hi = min(lo + 8, a.half + 1)
    r = call("editor_toolset.toolsets.programmatic.ProgrammaticToolset", "execute_tool_script", {
        "script": TEMPLATE % {"h": a.heading_deg, "a": lo, "b": hi, "half": a.half, "step": a.step_m * 100}})
    rows += r["rows"]
    print(f"rows {lo}..{hi - 1} ({time.time() - t:.0f}s)", flush=True)
os.makedirs(os.path.dirname(a.out), exist_ok=True)
json.dump({"step_m": a.step_m, "half": a.half, "heading_deg": a.heading_deg, "grid_v_rows_u_cols": rows}, open(a.out, "w"))
valid = [z for row in rows for z in row if z is not None]
print(f"wrote {a.out}: {len(valid)}/{len(rows) * len(rows[0])} samples, z {min(valid)}..{max(valid)} m")
