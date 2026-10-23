"""Bakes the fixed game-day sun and sky into the CGTrader stadium for the web build (CesiumJS, unlit).

UNTESTED IN BLENDER. This was written without a Blender install to run it against. Only the pure-Python parts
(`--self-test`: solar position, frame conversions, the GLB patcher) have been run. Run `--preview` end to end
before a full bake, and read the log: every Blender call that differs between versions is feature-checked and
logs what it dropped. Targets Blender 4.2 LTS to 5.x, the same as prepare_stadium.py (it needs Mesh.corner_normals,
which is 4.1+).

Why: the web build streams the licensed model from Cesium ion as 3D Tiles and can't run Lumen. The experience has
one fixed sun: 1:00 pm EDT on Sunday 18 October 2026, partly cloudy (ADR-0004, amended;
Tools/Editor/setup_cinematic_look.py). So this bakes sun + sky lighting (direct, bounce, sky occlusion, sun
shadows) into texture atlases offline in Cycles and exports GLBs whose materials are KHR_materials_unlit. The web
then draws them unlit, at almost no runtime cost.

Input: the licensed OBJ folder, the same `--src` that prepare_stadium.py takes. The default is
"Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange)". The script never writes into Assets/.

Command lines (run from the repo root; the Blender path is the macOS one, as in the other pipeline scripts):
  # Check the sun numbers. Plain Python, no Blender:
  python3 Tools/Pipeline/bake_stadium_lighting.py --self-test
  # Quick end-to-end test (low samples, small atlases). Writes to Saved/WebBake/preview/:
  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/bake_stadium_lighting.py -- \
      --src "Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange)" --preview
  # Full bake. Writes to Saved/WebBake/:
  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/bake_stadium_lighting.py -- \
      --src "Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange)"
  # Rebake one or more groups only (the whole model still loads, so shadows and bounce stay right):
  ... -- --src "..." --only field
  Useful flags: --samples 256, --texels-per-metre 12, --field-texels-per-metre 24, --max-atlas 4096,
  --cloud-occlusion 0.25, --exposure-ev 0, --device auto|gpu|cpu, --seats-mode vertex|atlas,
  --masked-mode atlas|vertex, --texture-limit 2048, --atlases-per-bake 4, --no-seats. See --help.

How it works:
  1. Geometry. The OBJ is re-imported and prepared in-process with prepare_stadium.py's own functions: import,
     classify, chunk and merge. They're read from that file with `ast`, so there's one source of truth and
     prepare_stadium.py is unchanged. The FBX chunks in Saved/StadiumImport aren't used: they're exported with
     path_mode=STRIP, so they carry no textures and nothing to bake albedo from, and they're in the registered site
     frame. Chunk names are computed in the registered frame, so they match Unreal's SM_LFF_* meshes.
  2. Frames. The output stays in the MODEL frame: same origin (midfield turf), metres and axes as the source OBJ,
     not registered. That's the frame of the ion assets already uploaded (6040628 stadium, 6040635 seats,
     6040637 field), and the web applies the ADR-0004 registration (182.25 deg, -2.25 m E, +2.5 m N) at runtime
     (web/src/scene/tiles.ts). The geometry never moves; the sun is rotated into the model frame instead:
       ENU = Rz(+6.5 deg) * map         (site yaw -6.5 deg; Unreal yaw turns East toward South)
       map = Rz(182.25 deg) * model + t (prepare_stadium.py registration, counter-clockwise from above)
     The GLB is Y-up as glTF requires, so its vertex coordinates equal the OBJ's.
  3. Sun and sky. NOAA solar position for 2026-10-18 17:00 UTC at midfield (39.9008036, -75.167479), in pure
     Python (solar_position). Unreal's CesiumSunSky gives elevation 40.3 deg, azimuth 184.6 deg; see --self-test.
     The lights:
       - A Sun lamp gives the direct light (angle 0.53 deg).
       - A Nishita sky (multiple-scattering on Blender 5 if Nishita is gone) matched to the same sun gives the sky
         light. Its sun disc is off, so the sun isn't counted twice.
     Balance: the lamp strength comes from a calibration bake on a horizontal test plane. The sky supplies
     --clear-sky-diffuse-fraction (default 0.18) of the horizontal light under a clear sky. --cloud-occlusion c
     then dims the direct sun by (1 - c) and whitens the sky by c, for the partly cloudy day.
     Exposure: a white, unshadowed horizontal surface maps to 1.0 x albedo (times 2^--exposure-ev). A soft
     highlight shoulder follows, then sRGB. The web's ACES tonemapper and exposure apply on top, exactly as for the
     Google tiles' photo textures, so match brightness at the plaza seam with --exposure-ev.
  4. Lightmap UVs and atlases. The groups are:
       stadium = STRUCTURE + GROUND, field = FIELD, seats = SEATS.
     Each chunk is split spatially into pieces that fit an atlas. Alpha-masked faces go in separate pieces and
     atlases (PNG with alpha); opaque faces become JPEG. The pieces are bin-packed into atlases of up to
     --max-atlas, sized to the texel budget:
       --texels-per-metre for structure, x --ground-scale for the paved site, --field-texels-per-metre for the field.
     A second UV layer, "LightmapUV", is Smart-UV-projected per atlas with a margin. The achieved texel density
     is measured and reported.
  5. Bake (Cycles, GPU when available: Metal on macOS, else OptiX, CUDA, HIP or oneAPI; CPU fallback). Each atlas
     gets three passes:
       - diffuse LIGHTING (direct + indirect, without colour) at --samples;
       - ALBEDO (diffuse colour);
       - when needed, an AUX emission pass that carries the alpha mask (R) and the LED/display glow mask (G).
     Then, in numpy: the lighting is denoised (OIDN through the compositor, or an island-aware blur as the
     fallback) and normalised. The result is albedo x lighting. LED/display materials (Unreal's rule
     ^led|display|lednr|scoreboard) glow statically: they never fall below --led-glow x albedo, and nothing
     animates or flashes (Docs/ACCESSIBILITY.md). Finally the colours are dilated into the margins.
     Unreal's turf tint (0.55, 0.6, 0.5) and its masked rule are mirrored:
       - the turf tint: Tools/Editor/import_stadium.py;
       - masked = PNG with alpha, cut-off 0.4.
     Seats default to --seats-mode vertex: 3.7 M triangles of small seats lightmap badly, because UV margins eat
     the atlas. So their lighting is baked to vertex colours (COLOR_0) and multiplied with the seats' own
     textures.
  6. Export. Materials become one unlit material per atlas; the atlas UV becomes TEXCOORD_0 and the original UVs
     are dropped. Each GLB is then patched: KHR_materials_unlit, double-sided, alphaMode MASK with cut-off 0.4
     where masked.

Outputs (Saved/ is git-ignored; nothing is committed):
  Saved/WebBake/stadium.glb, seats.glb, field.glb  (preview: Saved/WebBake/preview/)
  Saved/WebBake/atlases/*.jpg|png                  the baked atlases, kept for inspection
  Saved/WebBake/<group>_parts.zip                  only when a GLB exceeds --max-glb-mb: one GLB per atlas
  Saved/WebBake/bake_report.json                   sun (ENU, map and model frames), calibration, atlases and
                                                   their densities, triangle counts, file sizes, timings

Upload to Cesium ion (new assets; the three existing ones stay untouched):
  1. ion.cesium.com > My Assets > Add data > Upload. Pick stadium.glb, or <group>_parts.zip if one was written:
     ion tiles every model in a zip into one tileset.
  2. Type: "3D Model (convert to 3D Tiles)", the same as the existing assets. Leave georeferencing off: no
     location, no "use georeferenced coordinates". The web applies the ADR-0004 registration itself when ion's
     placement lands more than 2 km from midfield ("auto" placement). Texture compression (KTX2/WebP), if
     offered, is fine and saves iPad GPU memory.
  3. Repeat for seats.glb and field.glb. Note the three new asset IDs.
  4. Point the web at them in web/.env.local (git-ignored; never commit tokens), in the order stadium, seats,
     field:
       VITE_STADIUM_ASSET_IDS=<stadium>,<seats>,<field>
     Or test first without editing anything: ?stadiumAssets=<stadium>,<seats>,<field>. The scoped stadium token
     (VITE_STADIUM_ION_TOKEN) must include the new asset IDs.
  5. Check the console: "applied the ADR-0004 registration (now X m from midfield)" should report the same X as
     for the old assets.
  The web currently lights these tilesets and lets them receive shadow maps (web/src/scene/look.ts). With baked
  lighting that double-shades them, so they should become CAST_ONLY. That's a web change: it needs its
  Docs/WEB_PARITY.md row.

Cost (M-series Max, Metal, defaults): about 8 to 10 4K atlases plus the seat vertex bake. Expect roughly 1.5 to 4
hours, most of it the stadium lighting pass. --preview: about 10 to 20 minutes, mostly the 2.2 GB OBJ import and
merge. Memory: 32 GB or more is recommended (the model, 150 source textures in Cycles, and 4K float bake buffers).
On 16 GB, use --max-atlas 2048 --atlases-per-bake 1 --texture-limit 2048.
"""
import argparse, ast, collections, datetime, json, math, os, re, struct, sys, time, zipfile
from types import SimpleNamespace

try:  # Blender only. The solar math, frame conversions and GLB patcher run under plain Python (--self-test).
    import bpy
    from mathutils import Matrix, Vector
except ImportError:
    bpy = None
try:
    import numpy as np
except ImportError:
    np = None

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
T0 = time.time()

# --------------------------------------------------------------------------------------------- site constants
SITE_LAT, SITE_LON = 39.9008036, -75.1674790   # midfield (Docs/CESIUM_PLACEMENT.md, V1 amendments)
SITE_YAW_DEG = -6.5                            # LFF_SiteRoot heading; map -> ENU is +6.5 deg CCW
REGISTRATION_DEG = 182.25                      # ADR-0004: model -> site map frame, CCW from above
REGISTRATION_OFFSET_EN = (-2.25, 2.5)          # metres; only for chunk naming (geometry is never moved)
SUN_UTC = "2026-10-18T17:00:00Z"               # 1:00 pm EDT (setup_cinematic_look.py: 13.0 h, UTC-5 + DST)
UNREAL_SUN = {"elevation_deg": 40.3, "azimuth_deg": 184.6}  # setup_cinematic_look.py, verified in-editor

# Mirrors of Tools/Editor/import_stadium.py, so the bake reads like the Unreal materials.
LED_RULE = r"^led|display|lednr|scoreboard"    # PBR_RULES: static glow only
TINT_RULES = {"grass": (0.55, 0.6, 0.5)}       # keyed by norm(name)
MASK_CUTOFF = 0.4                              # M_LFF_StadiumMasked opacity_mask_clip_value

GROUPS = {"stadium": ("STRUCTURE", "GROUND"), "seats": ("SEATS",), "field": ("FIELD",)}
LM_UV = "LightmapUV"
SRC_UV = "UVMap"                               # prepare_stadium.merge() names the source UV layer this
PREP_FUNCS = ("import_obj", "tri_count", "dominant_material", "world_bounds", "bake_transforms", "chunk_key", "merge")
PREP_CONSTS = ("STADIUM", "FIELD", "SEATS", "GROUND_MATERIALS", "SECTORS", "INNER_RADIUS_M")


def log(msg):
    print(f"[bake_stadium_lighting {time.time() - T0:6.0f}s] {msg}", flush=True)


# ============================================================================ pure Python: sun and frames
def solar_position(when_utc, lat_deg, lon_deg, pressure_mbar=1013.25, temperature_c=15.0):
    """NOAA solar position (the NOAA Solar Calculator spreadsheet; about 0.01 deg here, years 1800-2100).
    Returns degrees; the azimuth is clockwise from true north. elevation_deg includes atmospheric refraction."""
    if when_utc.tzinfo is None:
        raise ValueError("when_utc must be timezone-aware")
    when_utc = when_utc.astimezone(datetime.timezone.utc)
    jd = 2440587.5 + when_utc.timestamp() / 86400.0
    jc = (jd - 2451545.0) / 36525.0
    rad, deg = math.radians, math.degrees
    l0 = (280.46646 + jc * (36000.76983 + jc * 0.0003032)) % 360.0           # geometric mean longitude
    m = 357.52911 + jc * (35999.05029 - 0.0001537 * jc)                       # geometric mean anomaly
    e = 0.016708634 - jc * (0.000042037 + 0.0000001267 * jc)                  # orbit eccentricity
    c = (math.sin(rad(m)) * (1.914602 - jc * (0.004817 + 0.000014 * jc))
         + math.sin(rad(2 * m)) * (0.019993 - 0.000101 * jc) + math.sin(rad(3 * m)) * 0.000289)
    omega = 125.04 - 1934.136 * jc
    app_long = l0 + c - 0.00569 - 0.00478 * math.sin(rad(omega))
    mean_obliq = 23.0 + (26.0 + (21.448 - jc * (46.815 + jc * (0.00059 - jc * 0.001813))) / 60.0) / 60.0
    obliq = mean_obliq + 0.00256 * math.cos(rad(omega))
    decl = deg(math.asin(math.sin(rad(obliq)) * math.sin(rad(app_long))))
    y = math.tan(rad(obliq / 2.0)) ** 2
    eot = 4.0 * deg(y * math.sin(2 * rad(l0)) - 2 * e * math.sin(rad(m)) + 4 * e * y * math.sin(rad(m)) * math.cos(2 * rad(l0))
                    - 0.5 * y * y * math.sin(4 * rad(l0)) - 1.25 * e * e * math.sin(2 * rad(m)))   # minutes
    minutes = when_utc.hour * 60.0 + when_utc.minute + when_utc.second / 60.0
    tst = (minutes + eot + 4.0 * lon_deg) % 1440.0                            # true solar time, minutes
    ha = tst / 4.0 - 180.0 if tst / 4.0 >= 0 else tst / 4.0 + 180.0           # hour angle, + = afternoon
    cos_z = (math.sin(rad(lat_deg)) * math.sin(rad(decl))
             + math.cos(rad(lat_deg)) * math.cos(rad(decl)) * math.cos(rad(ha)))
    zen = deg(math.acos(max(-1.0, min(1.0, cos_z))))
    el = 90.0 - zen
    # Azimuth from north, clockwise (NOAA's formulation).
    den = math.cos(rad(lat_deg)) * math.sin(rad(zen))
    if abs(den) < 1e-12:
        az = 180.0
    else:
        a = deg(math.acos(max(-1.0, min(1.0, (math.sin(rad(lat_deg)) * math.cos(rad(zen)) - math.sin(rad(decl))) / den))))
        az = (a + 180.0) % 360.0 if ha > 0 else (540.0 - a) % 360.0
    # Refraction (NOAA), scaled for pressure and temperature.
    if el > 85.0:
        r = 0.0
    elif el > 5.0:
        t = math.tan(rad(el))
        r = 58.1 / t - 0.07 / t ** 3 + 0.000086 / t ** 5
    elif el > -0.575:
        r = 1735.0 + el * (-518.2 + el * (103.4 + el * (-12.79 + el * 0.711)))
    else:
        r = -20.772 / math.tan(rad(el))
    r = r / 3600.0 * (pressure_mbar / 1010.0) * (283.0 / (273.0 + temperature_c))
    return {"utc": when_utc.strftime("%Y-%m-%dT%H:%M:%SZ"), "julian_day": jd, "declination_deg": decl,
            "equation_of_time_min": eot, "hour_angle_deg": ha, "azimuth_deg": az,
            "elevation_geometric_deg": el, "elevation_deg": el + r, "refraction_deg": r}


def rot_z(v, deg_ccw):
    a = math.radians(deg_ccw)
    c, s = math.cos(a), math.sin(a)
    return (c * v[0] - s * v[1], s * v[0] + c * v[1], v[2])


def sun_frames(azimuth_deg, elevation_deg, site_yaw_deg=SITE_YAW_DEG, registration_deg=REGISTRATION_DEG):
    """Unit vector TOWARD the sun in each frame:
      enu       East, North, Up at midfield;
      map       the site map frame of prepare_stadium.py (X across the field to the east stand, Y to the north end);
                ENU = Rz(-site_yaw) * map;
      model     Blender's model frame after the OBJ import (Z-up, unregistered); map = Rz(registration) * model;
      gltf      the same model frame, Y-up, as written to the GLB (= the source OBJ's axes): (x, z, -y)."""
    az, el = math.radians(azimuth_deg), math.radians(elevation_deg)
    enu = (math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el))
    map_to_enu = -site_yaw_deg
    mp = rot_z(enu, -map_to_enu)
    model = rot_z(mp, -registration_deg)
    gltf = (model[0], model[2], -model[1])
    bearing = lambda v: math.degrees(math.atan2(v[0], v[1])) % 360.0  # clockwise from the frame's +Y
    return {
        "enu": enu, "map": mp, "model_blender_zup": model, "model_gltf_yup": gltf,
        "azimuth_enu_deg": azimuth_deg, "azimuth_map_deg": bearing(mp), "azimuth_model_deg": bearing(model),
        "elevation_deg": elevation_deg, "map_to_enu_ccw_deg": map_to_enu, "registration_ccw_deg": registration_deg,
        # Cycles' Nishita sun direction is (-cos(el) sin(rot), cos(el) cos(rot), sin(el)): rotation 0 = +Y, CCW.
        "nishita_sun_rotation_rad": math.atan2(-model[0], model[1]),
    }


def compute_sun(sun_utc=SUN_UTC, lat=SITE_LAT, lon=SITE_LON, site_yaw=SITE_YAW_DEG, registration=REGISTRATION_DEG):
    when = datetime.datetime.fromisoformat(sun_utc.replace("Z", "+00:00"))
    pos = solar_position(when, lat, lon)
    frames = sun_frames(pos["azimuth_deg"], pos["elevation_deg"], site_yaw, registration)
    return {"site": {"lat": lat, "lon": lon, "site_yaw_deg": site_yaw}, "position": pos, "frames": frames,
            "local_time": (when.astimezone(datetime.timezone(datetime.timedelta(hours=-4)))).strftime("%Y-%m-%d %H:%M EDT"),
            "unreal_cesium_sunsky": UNREAL_SUN}


# ============================================================================ pure Python: GLB patching
def read_glb(data):
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        raise ValueError("not a glTF 2.0 binary")
    off, chunks = 12, []
    while off < length:
        clen, ctype = struct.unpack_from("<II", data, off)
        chunks.append((ctype, data[off + 8:off + 8 + clen]))
        off += 8 + clen
    if not chunks or chunks[0][0] != 0x4E4F534A:
        raise ValueError("first GLB chunk is not JSON")
    return json.loads(chunks[0][1].decode("utf-8")), chunks[1:]


def write_glb(doc, rest):
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    body = struct.pack("<II", len(js), 0x4E4F534A) + js
    for ctype, payload in rest:
        payload = payload + b"\0" * (-len(payload) % 4)
        body += struct.pack("<II", len(payload), ctype) + payload
    return struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body


def make_materials_unlit(doc, masked_names):
    """Every material becomes KHR_materials_unlit: base colour (texture x factor x COLOR_0) only. It's two-sided,
    like Unreal's M_LFF_Stadium masters. Materials named in masked_names get alphaMode MASK at the Unreal
    cut-off; the rest are opaque. Returns how many materials were patched."""
    n = 0
    for mat in doc.get("materials", []):
        pbr = mat.setdefault("pbrMetallicRoughness", {})
        for k in ("metallicRoughnessTexture",):
            pbr.pop(k, None)
        pbr["metallicFactor"], pbr["roughnessFactor"] = 0.0, 1.0
        for k in ("normalTexture", "occlusionTexture", "emissiveTexture", "emissiveFactor"):
            mat.pop(k, None)
        ext = mat.setdefault("extensions", {})
        for k in [k for k in ext if k != "KHR_materials_unlit"]:
            ext.pop(k)
        ext["KHR_materials_unlit"] = {}
        if mat.get("name") in masked_names:
            mat["alphaMode"], mat["alphaCutoff"] = "MASK", MASK_CUTOFF
        else:
            mat.pop("alphaMode", None); mat.pop("alphaCutoff", None)
        mat["doubleSided"] = True
        mat.pop("extras", None)
        n += 1
    still = {k for m in doc.get("materials", []) for k in m.get("extensions", {})}
    keep = lambda e: not e.startswith("KHR_materials_") or e in still
    doc["extensionsUsed"] = sorted({e for e in doc.get("extensionsUsed", []) if keep(e)} | {"KHR_materials_unlit"})
    if "extensionsRequired" in doc:
        doc["extensionsRequired"] = [e for e in doc["extensionsRequired"] if keep(e)]
    return n


def patch_glb_file(path, masked_names):
    with open(path, "rb") as f:
        doc, rest = read_glb(f.read())
    n = make_materials_unlit(doc, masked_names)
    with open(path, "wb") as f:
        f.write(write_glb(doc, rest))
    return n


# ============================================================================ self-test (plain Python)
def self_test():
    sun = compute_sun()
    pos, fr = sun["position"], sun["frames"]
    def rounded(v):
        if isinstance(v, float):
            return round(v, 4)
        if isinstance(v, (list, tuple)):
            return [rounded(x) for x in v]
        if isinstance(v, dict):
            return {k: rounded(x) for k, x in v.items()}
        return v
    print(json.dumps(rounded(sun), indent=1))
    ok = True

    def check(cond, msg):
        nonlocal ok
        print(("PASS " if cond else "FAIL ") + msg)
        ok &= bool(cond)

    check(40.0 <= pos["elevation_deg"] <= 42.0, f"elevation {pos['elevation_deg']:.2f} deg in 40-42")
    check(183.0 <= pos["azimuth_deg"] <= 195.0, f"azimuth {pos['azimuth_deg']:.2f} deg in 183-195 (from north, clockwise)")
    check(abs(pos["elevation_deg"] - UNREAL_SUN["elevation_deg"]) < 0.3 and abs(pos["azimuth_deg"] - UNREAL_SUN["azimuth_deg"]) < 0.5,
          f"matches Unreal CesiumSunSky (el {UNREAL_SUN['elevation_deg']}, az {UNREAL_SUN['azimuth_deg']})")
    # Round trip: model -> map -> ENU must give back the ENU sun.
    back = rot_z(rot_z(fr["model_blender_zup"], REGISTRATION_DEG), -SITE_YAW_DEG)
    check(max(abs(a - b) for a, b in zip(back, fr["enu"])) < 1e-12, "model -> map -> ENU round trip")
    # The field axis (map +Y, the north end) must point at bearing 353.5 deg true (CESIUM_PLACEMENT.md).
    north_end = rot_z((0.0, 1.0, 0.0), -SITE_YAW_DEG)
    check(abs(math.degrees(math.atan2(north_end[0], north_end[1])) % 360.0 - 353.5) < 1e-9, "field axis bearing 353.5 deg")
    # Cross-check the NOAA code against a published value: solar noon declination near the June solstice is ~23.44.
    june = solar_position(datetime.datetime(2026, 6, 21, 16, 0, tzinfo=datetime.timezone.utc), SITE_LAT, SITE_LON)
    check(abs(june["declination_deg"] - 23.44) < 0.05, f"June solstice declination {june['declination_deg']:.3f}")
    # Nishita convention round trip.
    r, m = fr["nishita_sun_rotation_rad"], fr["model_blender_zup"]
    h = math.hypot(m[0], m[1])
    check(abs(-h * math.sin(r) - m[0]) < 1e-12 and abs(h * math.cos(r) - m[1]) < 1e-12, "Nishita sun_rotation reproduces the model-frame sun")
    # GLB patcher on a minimal file.
    doc = {"asset": {"version": "2.0"}, "materials": [{"name": "LFF_Baked_stadium_00", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
                                                      {"name": "LFF_Baked_stadium_01m", "alphaMode": "BLEND"}]}
    bin_chunk = (0x004E4942, b"\1\2\3")
    doc2, rest = read_glb(write_glb(doc, [bin_chunk]))
    make_materials_unlit(doc2, {"LFF_Baked_stadium_01m"})
    doc3, rest3 = read_glb(write_glb(doc2, rest))
    check(doc3["materials"][0]["extensions"] == {"KHR_materials_unlit": {}} and "alphaMode" not in doc3["materials"][0]
          and doc3["materials"][1]["alphaMode"] == "MASK" and doc3["extensionsUsed"] == ["KHR_materials_unlit"]
          and rest3[0][1][:3] == b"\1\2\3", "GLB unlit patch round trip")
    print("SELF-TEST " + ("PASSED" if ok else "FAILED"))
    return ok


# ============================================================================ arguments
def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else (sys.argv[1:] if bpy is None else [])
    ap = argparse.ArgumentParser(description="Bake the game-day sun and sky into the stadium for the web (see docstring).")
    ap.add_argument("--self-test", action="store_true", help="print the computed sun and run the pure-Python checks")
    ap.add_argument("--src", default=os.path.join("Assets", "Eagles Stadium Philadelphia 3D Model", "OBJ File format (Exchange)"))
    ap.add_argument("--out", default=None, help="default Saved/WebBake (Saved/WebBake/preview with --preview)")
    ap.add_argument("--preview", action="store_true", help="low samples, small atlases: a quick end-to-end test")
    ap.add_argument("--only", nargs="+", choices=sorted(GROUPS), default=None, help="bake and export only these groups")
    ap.add_argument("--no-seats", action="store_true", help="don't load the seat parts at all (quick tests; no seat shadows)")
    ap.add_argument("--samples", type=int, default=None, help="lighting samples per texel (default 256; preview 16)")
    ap.add_argument("--texels-per-metre", type=float, default=None, help="structure texel density (default 12; preview 2)")
    ap.add_argument("--field-texels-per-metre", type=float, default=None, help="NFL field density (default 24; preview 4)")
    ap.add_argument("--ground-scale", type=float, default=0.5, help="paved site/lots density relative to structure")
    ap.add_argument("--masked-scale", type=float, default=3.0, help="alpha-masked density relative to its layer")
    ap.add_argument("--max-atlas", type=int, default=None, help="largest atlas edge in pixels (default 4096; preview 1024)")
    ap.add_argument("--min-atlas", type=int, default=256)
    ap.add_argument("--pack-efficiency", type=float, default=0.5, help="assumed share of an atlas that islands fill")
    ap.add_argument("--margin-px", type=int, default=None, help="island margin and dilation in pixels (default 4; preview 2)")
    ap.add_argument("--atlases-per-bake", type=int, default=None, help="atlases per Cycles bake call (memory vs. scene syncs)")
    ap.add_argument("--seats-mode", choices=("vertex", "atlas"), default="vertex")
    ap.add_argument("--masked-mode", choices=("atlas", "vertex"), default="atlas",
                    help="alpha-masked faces: atlas (lit atlas with the mask in alpha) or vertex (own textures x COLOR_0)")
    ap.add_argument("--device", choices=("auto", "gpu", "cpu"), default="auto")
    ap.add_argument("--denoise", choices=("oidn", "blur", "off"), default=None, help="default oidn (falls back to blur)")
    ap.add_argument("--texture-limit", default=None, help="Cycles texture size limit, e.g. 2048 (default off; preview 1024)")
    ap.add_argument("--sun-utc", default=SUN_UTC)
    ap.add_argument("--lat", type=float, default=SITE_LAT)
    ap.add_argument("--lon", type=float, default=SITE_LON)
    ap.add_argument("--site-yaw-deg", type=float, default=SITE_YAW_DEG)
    ap.add_argument("--registration-deg", type=float, default=REGISTRATION_DEG)
    ap.add_argument("--sun-angle-deg", type=float, default=0.53)
    ap.add_argument("--clear-sky-diffuse-fraction", type=float, default=0.18,
                    help="share of horizontal light from the sky under a clear sky (sets the sun lamp strength)")
    ap.add_argument("--cloud-occlusion", type=float, default=0.25, help="0 clear .. 1 overcast: dims the sun, whitens the sky")
    ap.add_argument("--exposure-ev", type=float, default=0.0)
    ap.add_argument("--led-glow", type=float, default=1.0, help="LED/display texels never fall below this x albedo")
    ap.add_argument("--jpeg-quality", type=int, default=92)
    ap.add_argument("--png", action="store_true", help="write opaque atlases as PNG instead of JPEG")
    ap.add_argument("--keep-normals", action="store_true", help="export normals (unlit rendering doesn't need them)")
    ap.add_argument("--max-glb-mb", type=float, default=1500.0, help="above this, also write per-atlas GLBs in a zip")
    a = ap.parse_args(argv)
    pv = a.preview
    for k, full, quick in (("samples", 256, 16), ("texels_per_metre", 12.0, 2.0), ("field_texels_per_metre", 24.0, 4.0),
                           ("max_atlas", 4096, 1024), ("margin_px", 4, 2), ("atlases_per_bake", 4, 8),
                           ("denoise", "oidn", "blur"), ("texture_limit", None, "1024")):
        if getattr(a, k) is None:
            setattr(a, k, quick if pv else full)
    if a.out is None:
        a.out = os.path.join(REPO, "Saved", "WebBake", "preview" if pv else "")
    a.out = os.path.abspath(a.out)
    a.only = a.only or sorted(GROUPS)
    if a.no_seats and "seats" in a.only and len(a.only) == 1:
        ap.error("--only seats needs the seats loaded (drop --no-seats)")
    if a.no_seats:
        a.only = [g for g in a.only if g != "seats"]
    src = a.src if os.path.isabs(a.src) or os.path.isdir(a.src) else os.path.join(REPO, a.src)
    a.src = os.path.abspath(src)
    return a


# ============================================================================ Blender: helpers
def op_call(op, **kw):
    """Calls a bpy operator with only the keyword arguments this Blender version knows (logs what it dropped)."""
    try:
        known = {p.identifier for p in op.get_rna_type().properties}
    except Exception:
        known = None
    if known is not None:
        dropped = sorted(k for k in kw if k not in known)
        if dropped:
            log(f"  ({op.idname_py() if hasattr(op, 'idname_py') else op}: this Blender has no {dropped})")
        kw = {k: v for k, v in kw.items() if k in known}
    return op(**kw)


def set_first(obj, attr, values):
    for v in values:
        try:
            setattr(obj, attr, v)
            return v
        except (TypeError, AttributeError, ValueError):
            continue
    return None


def set_input(node, names, value):
    for n in names:
        s = node.inputs.get(n)
        if s is not None and not s.is_linked:
            s.default_value = value
            return True
    return False


def base_name(name):
    return re.sub(r"\.\d{3}$", "", name)


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def load_prepare_stadium(src):
    """prepare_stadium.py is a top-level script (it runs and parses argv on import), so its definitions are read
    with ast and executed into a namespace: the same code, no edits to it. Fails loudly if any are missing."""
    path = os.path.join(HERE, "prepare_stadium.py")
    tree = ast.parse(open(path, encoding="utf-8").read(), path)
    keep = [n for n in tree.body if (isinstance(n, ast.FunctionDef) and n.name in PREP_FUNCS) or
            (isinstance(n, ast.Assign) and all(isinstance(t, ast.Name) and t.id in PREP_CONSTS for t in n.targets))]
    ns = {"__name__": "prepare_stadium_defs", "bpy": bpy, "np": np, "math": math, "os": os, "collections": collections,
          "json": json, "Matrix": Matrix, "Vector": Vector, "args": SimpleNamespace(src=src), "log": log, "T0": T0}
    exec(compile(ast.Module(body=keep, type_ignores=[]), path, "exec"), ns)
    missing = [n for n in PREP_FUNCS + PREP_CONSTS if n not in ns]
    if missing:
        raise RuntimeError(f"prepare_stadium.py no longer defines {missing}; update bake_stadium_lighting.py")
    return SimpleNamespace(**{n: ns[n] for n in PREP_FUNCS + PREP_CONSTS})


def select_only(objs, active=None):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or (objs[0] if objs else None)


def new_image(name, size, float_buffer=True, alpha=True):
    img = bpy.data.images.new(name, size, size, alpha=alpha, float_buffer=float_buffer)
    if float_buffer:
        set_first(img.colorspace_settings, "name", ("Non-Color", "Linear Rec.709", "Linear"))
    return img


def image_rgba(img):
    w, h = img.size
    buf = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(buf)
    return buf.reshape(h, w, 4)


def tri_area_uv(me, uv_name):
    """UV area of every polygon of `me` in its `uv_name` layer (fan triangulation, numpy)."""
    nl, npoly = len(me.loops), len(me.polygons)
    ls = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_start", ls)
    lt = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_total", lt)
    uv = np.empty(nl * 2, np.float32); me.uv_layers[uv_name].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2).astype(np.float64)
    first = np.repeat(ls, lt)
    k = np.arange(nl) - first
    ok = (k >= 1) & (k <= np.repeat(lt, lt) - 2)
    a, b, f = np.nonzero(ok)[0], np.nonzero(ok)[0] + 1, first[ok]
    d1, d2 = uv[a] - uv[f], uv[b] - uv[f]
    return 0.5 * np.abs(d1[:, 0] * d2[:, 1] - d1[:, 1] * d2[:, 0]).sum()


# ---------------------------------------------------------------------------- numpy image ops
def shift_sum(img, w):
    """3x3 binomial blur of img*w and w (edge-clamped)."""
    def blur(x):
        p = np.pad(x, [(1, 1), (1, 1)] + [(0, 0)] * (x.ndim - 2), mode="edge")
        x = p[:-2] * 0.25 + p[1:-1] * 0.5 + p[2:] * 0.25
        return x[:, :-2] * 0.25 + x[:, 1:-1] * 0.5 + x[:, 2:] * 0.25
    return blur(img * w[..., None]), blur(w)


def blur_masked(img, cov, iters=2):
    """Island-aware smoothing: a normalised convolution over covered texels only (the denoise fallback)."""
    w = cov.astype(np.float32)
    for _ in range(iters):
        num, den = shift_sum(img, w)
        img = np.where(cov[..., None], num / np.maximum(den, 1e-6)[..., None], img)
    return img


def dilate(img, cov, iters):
    """Fills uncovered texels from covered neighbours, iters pixels out (the bake margin, done in numpy)."""
    img = np.where(cov[..., None], img, 0.0).astype(np.float32)
    filled = cov.copy()
    h, w = cov.shape
    for _ in range(iters):
        if filled.all():
            break
        fp = np.pad(filled, 1)
        ip = np.pad(img, ((1, 1), (1, 1), (0, 0)))
        acc = np.zeros_like(img)
        cnt = np.zeros((h, w), np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy == 0 and dx == 0:
                    continue
                f = fp[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
                acc += ip[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] * f[..., None]
                cnt += f
        new = (~filled) & (cnt > 0)
        img[new] = acc[new] / cnt[new][:, None]
        filled |= new
    return img


def tonemap(x, knee=0.8):
    """Linear up to the knee, then a smooth shoulder toward 1.0, so sun-facing white walls don't clip hard."""
    x = np.maximum(x, 0.0)
    return np.where(x <= knee, x, knee + (1.0 - knee) * (1.0 - np.exp(-(x - knee) / (1.0 - knee))))


def srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def luminance(rgb):
    return 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


# ============================================================================ the bake
class Baker:
    def __init__(self, a):
        self.a = a
        self.timings = collections.OrderedDict()
        self.report = {"blender": bpy.app.version_string, "args": {k: v for k, v in vars(a).items()}, "warnings": []}
        self.mat_info = {}        # original material name -> {"masked", "glow", "alpha_image"}
        self.orig_mats = {}       # object name -> [original materials]
        self.atlases = []         # dicts
        self.vertex_objs = {g: [] for g in GROUPS}
        self.layer_of = {}        # object name -> layer
        self.final_masked_mats = set()

    def stage(self, name, t):
        self.timings[name] = round(time.time() - t, 1)
        log(f"{name}: {self.timings[name]} s")

    def warn(self, msg):
        log("WARNING " + msg)
        self.report["warnings"].append(msg)

    # ------------------------------------------------------------------ 1. geometry
    def load_geometry(self):
        t = time.time()
        P = load_prepare_stadium(self.a.src)
        self.P = P
        bpy.ops.wm.read_factory_settings(use_empty=True)
        objs = P.import_obj(P.STADIUM, "STADIUM") + P.import_obj(P.FIELD, "FIELD")
        if not self.a.no_seats:
            for s in P.SEATS:
                objs += P.import_obj(s, "SEATS")
        P.bake_transforms(objs)
        # Classification, exactly as prepare_stadium.py step 2 (inline there).
        for o in objs:
            if o["lff_layer"] == "STADIUM":
                lo, hi = P.world_bounds(o)
                m = P.dominant_material(o)
                o["lff_layer"] = "GROUND" if (m in P.GROUND_MATERIALS or (m == "Grass" and hi.z < 3.0)) else "STRUCTURE"
        log(f"classified: {dict(collections.Counter(o['lff_layer'] for o in objs))}")
        # Chunk keys in the registered site frame (so names match Unreal's SM_LFF_*), geometry left in model frame.
        reg = Matrix.Translation((REGISTRATION_OFFSET_EN[0], REGISTRATION_OFFSET_EN[1], 0.0)) @ \
            Matrix.Rotation(math.radians(self.a.registration_deg), 4, "Z")
        groups = collections.defaultdict(list)
        for o in objs:
            o.matrix_world = reg
            groups[P.chunk_key(o)].append(o)
            o.matrix_world = Matrix.Identity(4)
        self.chunks = {}
        for key, members in sorted(groups.items()):
            name = "SM_LFF_" + "".join(p.capitalize() for p in key.lower().split("_"))
            ob = P.merge(name, members)
            self.chunks[name] = ob
            self.layer_of[name] = members[0]["lff_layer"]
        meshes = {o.data for o in objs}
        bpy.data.batch_remove(objs)
        bpy.data.batch_remove([m for m in meshes if m.users == 0])
        log(f"{len(self.chunks)} chunks, {sum(P.tri_count(o) for o in self.chunks.values())} triangles")
        self.stage("load_geometry", t)

    # ------------------------------------------------------------------ 2. materials
    def prepare_materials(self):
        """Mirrors the Unreal material rules on the imported OBJ materials: masked = PNG base colour with real
        alpha (cut at 0.4); everything else opaque (glass too); turf tint; LED/display glow flag. Every image
        texture is pinned to the source UV so adding the lightmap UV can't change texture lookups."""
        t = time.time()
        used = {m for o in self.chunks.values() for m in o.data.materials if m}
        alpha_cache = {}
        for mat in sorted(used, key=lambda m: m.name):
            try:
                mat.use_nodes = True
            except Exception:
                pass
            nt = mat.node_tree
            bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
            name = base_name(mat.name)
            info = {"masked": False, "glow": bool(re.search(LED_RULE, name.lower())), "alpha_image": None}
            for n in list(nt.nodes):
                if n.type == "TEX_IMAGE" and not n.inputs["Vector"].is_linked:
                    uvn = nt.nodes.new("ShaderNodeUVMap"); uvn.uv_map = SRC_UV
                    nt.links.new(uvn.outputs["UV"], n.inputs["Vector"])
                elif n.type == "NORMAL_MAP":
                    n.uv_map = SRC_UV
            if bsdf is None:
                self.warn(f"material {mat.name} has no Principled BSDF; baked as-is")
                self.mat_info[mat.name] = info
                continue
            base_in = bsdf.inputs["Base Color"]
            tex = base_in.links[0].from_node if base_in.is_linked and base_in.links[0].from_node.type == "TEX_IMAGE" else None
            img = tex.image if tex else None
            if img is not None and img.filepath.lower().endswith(".png"):
                if img.name not in alpha_cache:
                    has = False
                    try:
                        if img.channels == 4:
                            px = np.empty(img.size[0] * img.size[1] * 4, np.float32)
                            img.pixels.foreach_get(px)
                            has = float(px[3::4].min()) < 0.98
                            del px
                    except Exception as e:
                        self.warn(f"couldn't read {img.name} alpha: {e}")
                    alpha_cache[img.name] = has
                info["masked"] = alpha_cache[img.name]
            # The bake is diffuse-only, so all base colour must be diffuse. Unreal keeps glass opaque, and its metallic
            # values only matter for Lumen reflections, which a static bake can't carry.
            for s_name in ("Metallic", "Transmission Weight", "Transmission"):
                s = bsdf.inputs.get(s_name)
                if s is not None:
                    for l in list(s.links):
                        nt.links.remove(l)
                    s.default_value = 0.0
            alpha_in = bsdf.inputs["Alpha"]
            for l in list(alpha_in.links):
                nt.links.remove(l)
            alpha_in.default_value = 1.0
            if info["masked"]:
                gt = nt.nodes.new("ShaderNodeMath"); gt.operation = "GREATER_THAN"
                gt.inputs[1].default_value = MASK_CUTOFF
                nt.links.new(tex.outputs["Alpha"], gt.inputs[0])
                nt.links.new(gt.outputs[0], alpha_in)
                info["alpha_image"] = img
                set_first(mat, "blend_method", ("CLIP", "HASHED"))
            tint = TINT_RULES.get(norm(name))
            if tint:
                if tex:
                    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"
                    mix.inputs["Factor"].default_value = 1.0
                    a_in = next(s for s in mix.inputs if s.name == "A" and s.type == "RGBA")
                    b_in = next(s for s in mix.inputs if s.name == "B" and s.type == "RGBA")
                    b_in.default_value = (*tint, 1.0)
                    nt.links.new(tex.outputs["Color"], a_in)
                    nt.links.new(next(s for s in mix.outputs if s.type == "RGBA"), base_in)
                else:
                    c = base_in.default_value
                    base_in.default_value = (c[0] * tint[0], c[1] * tint[1], c[2] * tint[2], 1.0)
            self.mat_info[mat.name] = info
        masked = sorted(k for k, v in self.mat_info.items() if v["masked"])
        glow = sorted(k for k, v in self.mat_info.items() if v["glow"])
        log(f"materials: {len(self.mat_info)}; masked {masked}; glow {glow}")
        self.report["materials"] = {"count": len(self.mat_info), "masked": masked, "glow": glow}
        self.stage("prepare_materials", t)

    # ------------------------------------------------------------------ 3. pieces, atlases, lightmap UVs
    def density(self, layer, masked):
        a = self.a
        d = {"STRUCTURE": a.texels_per_metre, "GROUND": a.texels_per_metre * a.ground_scale,
             "FIELD": a.field_texels_per_metre, "SEATS": a.texels_per_metre}[layer]
        return d * (a.masked_scale if masked else 1.0)

    def submesh(self, src, faces, name):
        """New object from a subset of src's polygons: positions, faces, source UVs, corner normals, slots
        (the same bulk-array approach as prepare_stadium.merge)."""
        me = src.data
        nv, nl, npoly = len(me.vertices), len(me.loops), len(me.polygons)
        co = np.empty(nv * 3, np.float32); me.vertices.foreach_get("co", co)
        lv = np.empty(nl, np.int32); me.loops.foreach_get("vertex_index", lv)
        ls = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_start", ls)
        lt = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_total", lt)
        pm = np.empty(npoly, np.int32); me.polygons.foreach_get("material_index", pm)
        uv = np.zeros(nl * 2, np.float32)
        if SRC_UV in me.uv_layers:
            me.uv_layers[SRC_UV].data.foreach_get("uv", uv)
        cn = np.empty(nl * 3, np.float32); me.corner_normals.foreach_get("vector", cn)
        tots, starts = lt[faces], ls[faces]
        new_ls = (np.cumsum(tots) - tots).astype(np.int32)
        n_l = int(tots.sum())
        loop_idx = np.repeat(starts - new_ls, tots) + np.arange(n_l)
        used, new_lv = np.unique(lv[loop_idx], return_inverse=True)
        out = bpy.data.meshes.new(name)
        out.vertices.add(len(used)); out.vertices.foreach_set("co", co.reshape(-1, 3)[used].ravel())
        out.loops.add(n_l); out.loops.foreach_set("vertex_index", new_lv.astype(np.int32))
        out.polygons.add(len(faces)); out.polygons.foreach_set("loop_start", new_ls)
        out.polygons.foreach_set("loop_total", tots.astype(np.int32))
        out.polygons.foreach_set("material_index", pm[faces])
        out.uv_layers.new(name=SRC_UV).data.foreach_set("uv", uv.reshape(-1, 2)[loop_idx].ravel())
        for m in me.materials:
            out.materials.append(m)
        out.update(calc_edges=True)
        out.normals_split_custom_set(cn.reshape(-1, 3)[loop_idx].tolist())
        ob = bpy.data.objects.new(name, out)
        bpy.context.scene.collection.objects.link(ob)
        return ob

    def build_atlases(self):
        t = time.time()
        a = self.a
        cap = float(a.max_atlas) ** 2
        bins = collections.defaultdict(list)   # (group, layer, masked) -> [(need, obj)]
        for name in sorted(self.chunks):
            ob = self.chunks[name]
            layer = self.layer_of[name]
            group = next(g for g, layers in GROUPS.items() if layer in layers)
            if group == "seats" and a.seats_mode == "vertex":
                self.vertex_objs[group].append(ob)
                self.orig_mats[ob.name] = list(ob.data.materials)
                continue
            me = ob.data
            npoly = len(me.polygons)
            area = np.empty(npoly, np.float32); me.polygons.foreach_get("area", area)
            ctr = np.empty(npoly * 3, np.float32); me.polygons.foreach_get("center", ctr); ctr = ctr.reshape(-1, 3)
            pm = np.empty(npoly, np.int32); me.polygons.foreach_get("material_index", pm)
            slot_masked = np.array([bool(m and self.mat_info.get(m.name, {}).get("masked")) for m in me.materials] or [False])
            face_masked = slot_masked[np.clip(pm, 0, len(slot_masked) - 1)]
            pieces = []
            for masked in (False, True):
                faces = np.nonzero(face_masked == masked)[0]
                if len(faces) == 0:
                    continue
                d = self.density(layer, masked)
                need = lambda f: float(area[f].sum()) * d * d / a.pack_efficiency

                def split(f):
                    if need(f) <= 0.9 * cap or len(f) < 64:
                        return [f]
                    p = ctr[f]
                    ax = int(np.argmax(p.max(axis=0) - p.min(axis=0)))
                    order = np.argsort(p[:, ax], kind="stable")
                    cum = np.cumsum(area[f][order])
                    k = int(np.clip(np.searchsorted(cum, cum[-1] / 2.0), 1, len(f) - 1))
                    return split(f[order[:k]]) + split(f[order[k:]])
                for f in split(faces):
                    pieces.append((masked, need(f), f))
            if len(pieces) == 1 and len(pieces[0][2]) == npoly:
                objs = [(pieces[0][0], pieces[0][1], ob)]
            else:
                objs = [(m, n, self.submesh(ob, np.sort(f), f"{name}_P{i:02d}{'m' if m else ''}"))
                        for i, (m, n, f) in enumerate(pieces)]
                mesh = ob.data
                bpy.data.batch_remove([ob]); bpy.data.batch_remove([mesh])
            for masked, n, o in objs:
                self.layer_of[o.name] = layer
                self.orig_mats[o.name] = list(o.data.materials)
                if masked and a.masked_mode == "vertex":
                    self.vertex_objs[group].append(o)
                else:
                    bins[(group, layer, masked)].append((n, o))
        # First-fit decreasing into atlases of at most max_atlas^2 texels.
        for (group, layer, masked), items in sorted(bins.items()):
            packs = []
            for n, o in sorted(items, key=lambda x: -x[0]):
                for p in packs:
                    if p["need"] + n <= cap:
                        p["need"] += n; p["objs"].append(o); break
                else:
                    packs.append({"need": n, "objs": [o]})
            for p in packs:
                size = int(min(a.max_atlas, max(a.min_atlas, 2 ** math.ceil(math.log2(math.sqrt(max(p["need"], 1.0)))))))
                aid = len(self.atlases)
                self.atlases.append({"id": aid, "name": f"{group}_{aid:02d}{'m' if masked else ''}", "group": group,
                                     "layer": layer, "masked": masked, "size": size, "objs": p["objs"],
                                     "target_texels_per_metre": self.density(layer, masked),
                                     "estimated_need_texels": int(p["need"])})
        log(f"{len(self.atlases)} atlases: " + ", ".join(f"{x['name']}@{x['size']}" for x in self.atlases))
        self.stage("build_atlases", t)

    def unwrap(self, atlas):
        a = self.a
        objs = atlas["objs"]
        if bpy.context.object and bpy.context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        for o in objs:
            lm = o.data.uv_layers.get(LM_UV) or o.data.uv_layers.new(name=LM_UV, do_init=False)
            o.data.uv_layers.active = lm
            o.data.uv_layers[SRC_UV].active_render = True
        select_only(objs)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        frac = a.margin_px / float(atlas["size"])
        op_call(bpy.ops.uv.smart_project, angle_limit=math.radians(66.0), margin_method="FRACTION", island_margin=frac,
                area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        uv_area = sum(tri_area_uv(o.data, LM_UV) for o in objs)
        area3d = 0.0
        for o in objs:
            ar = np.empty(len(o.data.polygons), np.float32); o.data.polygons.foreach_get("area", ar)
            area3d += float(ar.sum())
        achieved = math.sqrt(uv_area * atlas["size"] ** 2 / max(area3d, 1e-9))
        atlas.update(area_m2=round(area3d, 1), uv_fill=round(uv_area, 3), achieved_texels_per_metre=round(achieved, 2),
                     triangles=sum(self.P.tri_count(o) for o in objs))
        if achieved < 0.6 * atlas["target_texels_per_metre"]:
            self.warn(f"{atlas['name']}: {achieved:.1f} texels/m against a target of {atlas['target_texels_per_metre']:.1f} "
                      f"(many small islands; raise --max-atlas, lower --pack-efficiency to split into more atlases)")
        log(f"unwrapped {atlas['name']}: {len(objs)} objects, {atlas['triangles']} tris, {area3d:.0f} m2, "
            f"UV fill {uv_area:.2f}, {achieved:.1f} texels/m (target {atlas['target_texels_per_metre']:.1f})")

    # ------------------------------------------------------------------ 4. sun, sky, Cycles
    def setup_render(self):
        t = time.time()
        a = self.a
        sc = bpy.context.scene
        sc.render.engine = "CYCLES"
        self.device = self.setup_device(a.device)
        cy = sc.cycles
        for k, v in (("diffuse_bounces", 3), ("glossy_bounces", 1), ("transmission_bounces", 2), ("max_bounces", 6),
                     ("transparent_max_bounces", 16), ("caustics_reflective", False), ("caustics_refractive", False),
                     ("sample_clamp_indirect", 10.0), ("use_light_tree", True), ("use_denoising", False)):
            if hasattr(cy, k):
                setattr(cy, k, v)
        if a.texture_limit:
            sc.render.use_simplify = True
            set_first(cy, "texture_limit_render", (str(a.texture_limit),))
        sun = compute_sun(a.sun_utc, a.lat, a.lon, a.site_yaw_deg, a.registration_deg)
        self.sun = sun
        d = Vector(sun["frames"]["model_blender_zup"])
        ld = bpy.data.lights.new("LFF_Sun", "SUN")
        ld.angle = math.radians(a.sun_angle_deg)
        ld.color = (1.0, 0.96, 0.90)   # a little warm at 40 deg elevation; sky light supplies the blue
        ld.energy = 1.0
        self.sun_obj = bpy.data.objects.new("LFF_Sun", ld)
        sc.collection.objects.link(self.sun_obj)
        self.sun_obj.rotation_mode = "QUATERNION"
        self.sun_obj.rotation_quaternion = d.to_track_quat("Z", "Y")   # light travels along local -Z, away from the sun
        world = bpy.data.worlds.new("LFF_GameDaySky")
        sc.world = world
        try:
            world.use_nodes = True
        except Exception:
            pass
        nt = world.node_tree
        nt.nodes.clear()
        sky = nt.nodes.new("ShaderNodeTexSky")
        chosen = set_first(sky, "sky_type", ("NISHITA", "MULTIPLE_SCATTERING", "SINGLE_SCATTERING"))
        for k, v in (("sun_disc", False), ("sun_elevation", math.radians(sun["position"]["elevation_deg"])),
                     ("sun_rotation", sun["frames"]["nishita_sun_rotation_rad"]), ("sun_size", math.radians(a.sun_angle_deg)),
                     ("air_density", 1.0), ("dust_density", 1.5), ("aerosol_density", 1.5), ("ozone_density", 1.0)):
            if hasattr(sky, k):
                setattr(sky, k, v)
        bw = nt.nodes.new("ShaderNodeRGBToBW")
        mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MIX"
        mix.inputs["Factor"].default_value = max(0.0, min(1.0, a.cloud_occlusion))
        bg = nt.nodes.new("ShaderNodeBackground")
        out = nt.nodes.new("ShaderNodeOutputWorld")
        a_in = next(s for s in mix.inputs if s.name == "A" and s.type == "RGBA")
        b_in = next(s for s in mix.inputs if s.name == "B" and s.type == "RGBA")
        nt.links.new(sky.outputs["Color"], a_in)
        nt.links.new(sky.outputs["Color"], bw.inputs[0])
        nt.links.new(bw.outputs[0], b_in)       # clouds whiten the sky (same luminance, desaturated)
        nt.links.new(next(s for s in mix.outputs if s.type == "RGBA"), bg.inputs["Color"])
        nt.links.new(bg.outputs[0], out.inputs["Surface"])
        self.bg = bg
        log(f"sky {chosen}; sun az {sun['position']['azimuth_deg']:.2f} el {sun['position']['elevation_deg']:.2f} (ENU), "
            f"model-frame azimuth {sun['frames']['azimuth_model_deg']:.2f}; device {self.device}")
        if chosen is None:
            self.warn("no Nishita/multiple-scattering sky type in this Blender; the sky light uses the node default")
        self.calibrate()
        self.stage("setup_render", t)

    def setup_device(self, mode):
        sc = bpy.context.scene
        sc.cycles.device = "CPU"
        if mode == "cpu":
            return "CPU"
        try:
            prefs = bpy.context.preferences.addons["cycles"].preferences
        except Exception:
            return "CPU"
        order = ["METAL"] if sys.platform == "darwin" else ["OPTIX", "CUDA", "HIP", "ONEAPI"]
        for kind in order:
            try:
                prefs.compute_device_type = kind
            except (TypeError, ValueError):
                continue
            try:
                prefs.refresh_devices()
            except Exception:
                prefs.get_devices()
            devs = [d for d in prefs.devices if d.type == kind]
            if devs:
                for d in prefs.devices:
                    d.use = d.type == kind
                sc.cycles.device = "GPU"
                return f"GPU {kind}: " + ", ".join(d.name for d in devs)
        if mode == "gpu":
            self.warn("no GPU compute device found; baking on the CPU")
        return "CPU"

    def bake(self, kind, objs, samples, target="IMAGE_TEXTURES"):
        sc = bpy.context.scene
        sc.cycles.samples = samples
        select_only(objs)
        kw = dict(margin=0, margin_type="EXTEND", use_clear=True, use_selected_to_active=False, target=target,
                  save_mode="INTERNAL")
        if target == "IMAGE_TEXTURES":
            kw["uv_layer"] = LM_UV
        if kind == "LIGHT":
            kw.update(type="DIFFUSE", pass_filter={"DIRECT", "INDIRECT"})
        elif kind == "ALBEDO":
            kw.update(type="DIFFUSE", pass_filter={"COLOR"})
        else:
            kw.update(type="EMIT")
        t = time.time()
        res = op_call(bpy.ops.object.bake, **kw)
        if "FINISHED" not in res:
            raise RuntimeError(f"bake {kind} failed: {res}")
        return time.time() - t

    def calibrate(self):
        """Measures sun and sky light on an unoccluded white horizontal plane (everything else hidden), then sets
        the sun strength from the clear-sky diffuse fraction and the cloud factor, and the exposure reference."""
        a = self.a
        sc = bpy.context.scene
        me = bpy.data.meshes.new("LFF_CalibPlane")
        me.from_pydata([(-5, -5, 0), (5, -5, 0), (5, 5, 0), (-5, 5, 0)], [], [(0, 1, 2, 3)])
        for n in (SRC_UV, LM_UV):
            me.uv_layers.new(name=n).data.foreach_set("uv", [0, 0, 1, 0, 1, 1, 0, 1])
        plane = bpy.data.objects.new("LFF_CalibPlane", me)
        sc.collection.objects.link(plane)
        mat = bpy.data.materials.new("LFF_CalibWhite")
        try:
            mat.use_nodes = True
        except Exception:
            pass
        bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        bsdf.inputs["Base Color"].default_value = (1, 1, 1, 1)
        bsdf.inputs["Roughness"].default_value = 1.0
        img = new_image("LFF_Calib", 16)
        tn = mat.node_tree.nodes.new("ShaderNodeTexImage"); tn.image = img; mat.node_tree.nodes.active = tn
        me.materials.append(mat)
        hidden = [o for o in sc.objects if o not in (plane, self.sun_obj) and not o.hide_render]
        for o in hidden:
            o.hide_render = True
        strength_in = self.bg.inputs["Strength"]
        sky_strength = strength_in.default_value
        try:
            def measure():
                self.bake("LIGHT", [plane], 64)
                px = image_rgba(img)
                cov = px[..., 3] > 0.5
                return px[..., :3][cov].mean(axis=0) if cov.any() else px[..., :3].reshape(-1, 3).mean(axis=0)
            self.sun_obj.data.energy, strength_in.default_value = 1.0, 0.0
            sun_unit = measure()
            self.sun_obj.data.energy, strength_in.default_value = 0.0, sky_strength
            sky = measure()
        finally:
            for o in hidden:
                o.hide_render = False
        s_h, k_h = float(luminance(sun_unit)), float(luminance(sky))
        f = min(0.95, max(0.01, a.clear_sky_diffuse_fraction))
        if k_h <= 1e-6 or s_h <= 1e-6:
            self.warn(f"calibration failed (sun {s_h}, sky {k_h}); using sun strength 3, sky as-is")
            clear = 3.0
        else:
            clear = k_h * (1.0 - f) / (f * s_h)
        energy = clear * (1.0 - max(0.0, min(1.0, a.cloud_occlusion)))
        self.sun_obj.data.energy = energy
        self.e_ref = max(energy * s_h + k_h, 1e-6)
        self.report["calibration"] = {"sun_per_unit_strength_horizontal": s_h, "sky_horizontal": k_h,
                                      "clear_sky_sun_strength": clear, "sun_strength": energy,
                                      "reference_horizontal_irradiance": self.e_ref,
                                      "effective_diffuse_fraction": k_h / self.e_ref,
                                      "exposure_scale": 2.0 ** a.exposure_ev / self.e_ref}
        log(f"calibration: sun {s_h:.4g}/unit, sky {k_h:.4g} -> sun strength {energy:.4g}, "
            f"diffuse fraction {k_h / self.e_ref:.2f}")
        bpy.data.objects.remove(plane); bpy.data.meshes.remove(me); bpy.data.materials.remove(mat); bpy.data.images.remove(img)

    # ------------------------------------------------------------------ 5. bake atlases
    def atlas_materials(self, atlas, img):
        """Per-atlas copies of the original materials with an inactive-to-shading, active-for-bake image node."""
        copies = {}
        for o in atlas["objs"]:
            for i, m in enumerate(self.orig_mats[o.name]):
                if m is None:
                    continue
                if m.name not in copies:
                    c = m.copy(); c.name = f"{m.name}__bake{atlas['id']:02d}"
                    tn = c.node_tree.nodes.new("ShaderNodeTexImage"); tn.image = img
                    c.node_tree.nodes.active = tn
                    copies[m.name] = c
                o.data.materials[i] = copies[m.name]
        atlas["bake_mats"] = copies

    def aux_materials(self, atlas, img):
        """Emission-only stand-ins: R = the material's thresholded alpha (as Cycles sees it), G = LED glow mask."""
        aux = {}
        for o in atlas["objs"]:
            for i, m in enumerate(self.orig_mats[o.name]):
                if m is None:
                    continue
                if m.name not in aux:
                    info = self.mat_info.get(m.name, {})
                    x = bpy.data.materials.new(f"{m.name}__aux{atlas['id']:02d}")
                    try:
                        x.use_nodes = True
                    except Exception:
                        pass
                    nt = x.node_tree
                    nt.nodes.clear()
                    comb = nt.nodes.new("ShaderNodeCombineColor")
                    comb.inputs[1].default_value = 1.0 if info.get("glow") else 0.0
                    comb.inputs[2].default_value = 0.0
                    if info.get("alpha_image") is not None:
                        uvn = nt.nodes.new("ShaderNodeUVMap"); uvn.uv_map = SRC_UV
                        tx = nt.nodes.new("ShaderNodeTexImage"); tx.image = info["alpha_image"]
                        gt = nt.nodes.new("ShaderNodeMath"); gt.operation = "GREATER_THAN"; gt.inputs[1].default_value = MASK_CUTOFF
                        nt.links.new(uvn.outputs["UV"], tx.inputs["Vector"])
                        nt.links.new(tx.outputs["Alpha"], gt.inputs[0])
                        nt.links.new(gt.outputs[0], comb.inputs[0])
                    else:
                        comb.inputs[0].default_value = 1.0
                    em = nt.nodes.new("ShaderNodeEmission"); em.inputs["Strength"].default_value = 1.0
                    out = nt.nodes.new("ShaderNodeOutputMaterial")
                    nt.links.new(comb.outputs[0], em.inputs["Color"])
                    nt.links.new(em.outputs[0], out.inputs["Surface"])
                    tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = img; nt.nodes.active = tn
                    aux[m.name] = x
                o.data.materials[i] = aux[m.name]
        return aux

    def needs_aux(self, atlas):
        return atlas["masked"] or any(self.mat_info.get(m.name, {}).get("glow")
                                      for o in atlas["objs"] for m in self.orig_mats[o.name] if m)

    def bake_atlases(self, group):
        a = self.a
        todo = [x for x in self.atlases if x["group"] == group]
        for b in range(0, len(todo), max(1, a.atlases_per_bake)):
            batch = todo[b:b + max(1, a.atlases_per_bake)]
            t = time.time()
            imgs = {x["id"]: new_image(f"LFF_Bake_{x['name']}", x["size"]) for x in batch}
            for x in batch:
                self.atlas_materials(x, imgs[x["id"]])
            objs = [o for x in batch for o in x["objs"]]
            data = {x["id"]: {} for x in batch}
            secs = self.bake("LIGHT", objs, a.samples)
            for x in batch:
                px = image_rgba(imgs[x["id"]])
                data[x["id"]]["light"], data[x["id"]]["cov"] = px[..., :3].astype(np.float16), px[..., 3] > 0.5
            log(f"  lighting pass for {[x['name'] for x in batch]}: {secs:.0f} s")
            secs = self.bake("ALBEDO", objs, max(8, a.samples // 16))
            for x in batch:
                px = image_rgba(imgs[x["id"]])
                data[x["id"]]["albedo"] = px[..., :3].astype(np.float16)
                data[x["id"]]["cov"] |= px[..., 3] > 0.5
            log(f"  albedo pass: {secs:.0f} s")
            aux_batch = [x for x in batch if self.needs_aux(x)]
            if aux_batch:
                aux_mats = []
                for x in aux_batch:
                    aux_mats += list(self.aux_materials(x, imgs[x["id"]]).values())
                secs = self.bake("AUX", [o for x in aux_batch for o in x["objs"]], max(8, a.samples // 16))
                for x in aux_batch:
                    data[x["id"]]["aux"] = image_rgba(imgs[x["id"]])[..., :2].astype(np.float16)
                    for o in x["objs"]:   # back to the bake copies until export
                        for i, m in enumerate(self.orig_mats[o.name]):
                            if m is not None:
                                o.data.materials[i] = x["bake_mats"][m.name]
                bpy.data.batch_remove(aux_mats)
                log(f"  alpha/glow pass: {secs:.0f} s")
            for x in batch:
                bpy.data.images.remove(imgs[x["id"]])
                self.compose(x, data.pop(x["id"]))
            self.timings[f"bake_{group}_{b // max(1, a.atlases_per_bake)}"] = round(time.time() - t, 1)

    def denoise(self, rgb, cov):
        mode = self.a.denoise
        if mode == "off":
            return rgb
        if mode == "oidn":
            try:
                return self.oidn(rgb)
            except Exception as e:
                if not getattr(self, "_oidn_warned", False):
                    self.warn(f"OIDN denoise through the compositor failed ({e}); using the island-aware blur")
                    self._oidn_warned = True
                self.a.denoise = "blur"
        return blur_masked(rgb, cov, 2)

    def oidn(self, rgb):
        """Runs the compositor's Denoise node (OpenImageDenoise) on an image: a throwaway scene, rendered once
        to EXR. Handles the 4.x (scene.node_tree + Composite) and 5.x (compositing_node_group) APIs."""
        h, w = rgb.shape[:2]
        tmp = os.path.join(self.a.out, "_tmp"); os.makedirs(tmp, exist_ok=True)
        src = bpy.data.images.new("LFF_DenoiseIn", w, h, alpha=True, float_buffer=True)
        set_first(src.colorspace_settings, "name", ("Non-Color", "Linear Rec.709"))
        px = np.ones((h, w, 4), np.float32); px[..., :3] = rgb
        src.pixels.foreach_set(px.ravel()); del px
        sc = bpy.data.scenes.new("LFF_Denoise")
        cam = bpy.data.objects.new("LFF_DenoiseCam", bpy.data.cameras.new("LFF_DenoiseCam"))
        sc.collection.objects.link(cam); sc.camera = cam
        set_first(sc.render, "engine", ("BLENDER_WORKBENCH", "BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"))
        sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = w, h, 100
        sc.render.use_compositing, sc.render.use_sequencer = True, False
        sc.render.image_settings.file_format = "OPEN_EXR"
        sc.render.image_settings.color_depth = "32"
        set_first(sc.view_settings, "view_transform", ("Standard",))
        group = None
        if hasattr(sc, "compositing_node_group"):
            group = bpy.data.node_groups.new("LFF_Denoise", "CompositorNodeTree")
            group.interface.new_socket(name="Image", in_out="OUTPUT", socket_type="NodeSocketColor")
            tree, out_socket = group, group.nodes.new("NodeGroupOutput").inputs[0]
            sc.compositing_node_group = group
        else:
            sc.use_nodes = True
            tree = sc.node_tree
            tree.nodes.clear()
            out_socket = tree.nodes.new("CompositorNodeComposite").inputs["Image"]
        inp = tree.nodes.new("CompositorNodeImage"); inp.image = src
        dn = tree.nodes.new("CompositorNodeDenoise")
        for k, v in (("use_hdr", True), ("prefilter", "ACCURATE"), ("quality", "HIGH")):
            if hasattr(dn, k):
                set_first(dn, k, (v,))
        set_input(dn, ("HDR",), True)
        tree.links.new(inp.outputs["Image"], dn.inputs["Image"])
        tree.links.new(dn.outputs["Image"], out_socket)
        path = os.path.join(tmp, "denoise.exr")
        sc.render.filepath = path
        try:
            bpy.ops.render.render(write_still=True, scene=sc.name)
            res = bpy.data.images.load(path, check_existing=False)
            set_first(res.colorspace_settings, "name", ("Non-Color", "Linear Rec.709"))
            outpx = image_rgba(res)[..., :3].copy()
            bpy.data.images.remove(res)
        finally:
            bpy.data.images.remove(src)
            bpy.data.objects.remove(cam)
            bpy.data.scenes.remove(sc)
            if group is not None:
                bpy.data.node_groups.remove(group)
            if os.path.exists(path):
                os.remove(path)
        if outpx.shape != rgb.shape or not np.isfinite(outpx).all():
            raise RuntimeError("denoised image has the wrong shape or non-finite values")
        return outpx

    def compose(self, atlas, d):
        a = self.a
        cov = d["cov"]
        if cov.all() or not cov.any():
            cov = (d["albedo"].astype(np.float32).sum(axis=2) + d["light"].astype(np.float32).sum(axis=2)) > 0
        light = d["light"].astype(np.float32) * (2.0 ** a.exposure_ev / self.e_ref)
        light = dilate(light, cov, a.margin_px + 2)          # so the denoiser doesn't pull edges toward black
        light = np.maximum(self.denoise(light, cov), 0.0)
        albedo = d["albedo"].astype(np.float32)
        alpha = np.ones(cov.shape, np.float32)
        if "aux" in d:
            aux = d["aux"].astype(np.float32)
            if atlas["masked"]:
                alpha = np.clip(aux[..., 0], 0.0, 1.0)
                albedo = albedo / np.maximum(alpha, 0.05)[..., None]   # Cycles weights the albedo by coverage
            glow = np.clip(aux[..., 1], 0.0, 1.0)[..., None]
            light = light + glow * np.maximum(a.led_glow - light, 0.0)
        rgb = srgb(tonemap(np.clip(albedo, 0.0, 1.0) * light))
        rgba = np.concatenate([rgb, alpha[..., None]], axis=2).astype(np.float32)
        rgba = dilate(rgba, cov, a.margin_px)
        if not atlas["masked"]:
            rgba[..., 3] = 1.0
        ext = "png" if (atlas["masked"] or a.png) else "jpg"
        os.makedirs(os.path.join(a.out, "atlases"), exist_ok=True)
        path = os.path.join(a.out, "atlases", f"T_LFF_Baked_{atlas['name']}.{ext}")
        size = atlas["size"]
        img = bpy.data.images.new(f"T_LFF_Baked_{atlas['name']}", size, size, alpha=atlas["masked"], float_buffer=False)
        img.pixels.foreach_set(rgba.ravel())   # byte image: values are stored as given (already sRGB-encoded)
        img.filepath_raw = path
        img.file_format = "PNG" if ext == "png" else "JPEG"
        try:
            img.save(filepath=path, quality=a.jpeg_quality)
        except TypeError:
            img.save()
        bpy.data.images.remove(img)
        atlas["file"] = os.path.relpath(path, a.out)
        atlas["file_bytes"] = os.path.getsize(path)
        lum = luminance(light)[cov]
        atlas["light_percentiles"] = [round(float(v), 3) for v in np.percentile(lum, [5, 50, 95, 99])] if lum.size else []
        log(f"  composed {atlas['name']}: {atlas['file']} ({atlas['file_bytes'] / 1e6:.1f} MB), "
            f"normalised light p5/p50/p95/p99 {atlas['light_percentiles']}")

    # ------------------------------------------------------------------ 5b. vertex-colour bake (seats, optional masked)
    def bake_vertex(self, group):
        objs = self.vertex_objs[group]
        if not objs:
            return
        t = time.time()
        a = self.a
        for o in objs:
            me = o.data
            attr = me.color_attributes.get("LFF_Light") or me.color_attributes.new("LFF_Light", "FLOAT_COLOR", "POINT")
            try:
                me.color_attributes.active_color = attr
            except Exception:
                set_first(me.color_attributes, "active_color_name", (attr.name,))
            try:
                me.color_attributes.render_color_index = list(me.color_attributes).index(attr)
            except Exception:
                pass
        secs = self.bake("LIGHT", objs, a.samples, target="VERTEX_COLORS")
        scale = 2.0 ** a.exposure_ev / self.e_ref
        for o in objs:
            attr = o.data.color_attributes["LFF_Light"]
            c = np.empty(len(attr.data) * 4, np.float32); attr.data.foreach_get("color", c)
            c = c.reshape(-1, 4)
            c[:, :3] = tonemap(c[:, :3] * scale)   # linear: COLOR_0 multiplies the linear base colour
            c[:, 3] = 1.0
            attr.data.foreach_set("color", c.ravel())
        log(f"vertex-lit {group}: {len(objs)} objects, {secs:.0f} s")
        self.timings[f"bake_vertex_{group}"] = round(time.time() - t, 1)

    # ------------------------------------------------------------------ 6. export
    def finalize_atlas_objects(self, atlas):
        path = os.path.join(self.a.out, atlas["file"])
        img = bpy.data.images.load(path, check_existing=True)
        m = bpy.data.materials.new(f"LFF_Baked_{atlas['name']}")
        try:
            m.use_nodes = True
        except Exception:
            pass
        nt = m.node_tree
        bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        if atlas["masked"]:
            nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
            set_first(m, "blend_method", ("CLIP", "HASHED"))
            self.final_masked_mats.add(m.name)
        bsdf.inputs["Roughness"].default_value = 1.0
        bsdf.inputs["Metallic"].default_value = 0.0
        m.use_backface_culling = False
        for o in atlas["objs"]:
            me = o.data
            me.uv_layers.remove(me.uv_layers[SRC_UV])
            lm = me.uv_layers[LM_UV]
            me.uv_layers.active = lm; lm.active_render = True
            me.materials.clear()
            me.materials.append(m)
            me.polygons.foreach_set("material_index", np.zeros(len(me.polygons), np.int32))
            me.update()
        for c in atlas.get("bake_mats", {}).values():
            bpy.data.materials.remove(c)
        atlas["material"] = m.name

    def finalize_vertex_objects(self, group):
        copies = {}
        for o in self.vertex_objs[group]:
            for i, m in enumerate(self.orig_mats[o.name]):
                if m is None:
                    continue
                if m.name not in copies:
                    c = m.copy(); c.name = f"LFF_VertexLit_{base_name(m.name)}_{group}"
                    nt = c.node_tree
                    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
                    if bsdf is not None:
                        base_in = bsdf.inputs["Base Color"]
                        vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "LFF_Light"
                        mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"
                        mix.inputs["Factor"].default_value = 1.0
                        a_in = next(s for s in mix.inputs if s.name == "A" and s.type == "RGBA")
                        b_in = next(s for s in mix.inputs if s.name == "B" and s.type == "RGBA")
                        if base_in.is_linked:
                            nt.links.new(base_in.links[0].from_socket, a_in)
                        else:
                            a_in.default_value = base_in.default_value
                        nt.links.new(vc.outputs["Color"], b_in)
                        nt.links.new(next(s for s in mix.outputs if s.type == "RGBA"), base_in)
                    c.use_backface_culling = False
                    if self.mat_info.get(m.name, {}).get("masked"):
                        self.final_masked_mats.add(c.name)
                    copies[m.name] = c
                o.data.materials[i] = copies[m.name]
            if LM_UV in o.data.uv_layers:
                o.data.uv_layers.remove(o.data.uv_layers[LM_UV])

    def export_glb(self, objs, path, vertex_colors):
        select_only(objs)
        kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False,
                  export_texcoords=True, export_normals=self.a.keep_normals, export_tangents=False,
                  export_materials="EXPORT", export_image_format="AUTO", export_cameras=False, export_lights=False,
                  export_extras=False, export_animations=False, export_skins=False, export_morph=False,
                  export_attributes=False, export_draco_mesh_compression_enable=False, will_save_settings=False)
        colours = dict(export_vertex_color="ACTIVE" if vertex_colors else "NONE", export_colors=vertex_colors)
        try:
            op_call(bpy.ops.export_scene.gltf, **kw, **colours)
        except TypeError as e:   # an enum value this exporter version doesn't know: use its default colour handling
            self.warn(f"glTF exporter rejected the vertex-colour options ({e}); retrying with its defaults")
            op_call(bpy.ops.export_scene.gltf, **kw)
        return patch_glb_file(path, self.final_masked_mats)

    def export_group(self, group):
        t = time.time()
        a = self.a
        atl = [x for x in self.atlases if x["group"] == group]
        for x in atl:
            self.finalize_atlas_objects(x)
        self.finalize_vertex_objects(group)
        objs = [o for x in atl for o in x["objs"]] + self.vertex_objs[group]
        if not objs:
            self.warn(f"{group}: nothing to export")
            return
        path = os.path.join(a.out, f"{group}.glb")
        n = self.export_glb(objs, path, bool(self.vertex_objs[group]))
        size = os.path.getsize(path)
        info = {"file": os.path.basename(path), "bytes": size, "materials_unlit": n,
                "triangles": sum(self.P.tri_count(o) for o in objs), "objects": len(objs),
                "atlases": [x["name"] for x in atl], "vertex_lit_objects": len(self.vertex_objs[group])}
        if size > a.max_glb_mb * 1e6:
            parts = [(x["name"], x["objs"]) for x in atl] + ([("vertexlit", self.vertex_objs[group])] if self.vertex_objs[group] else [])
            zpath = os.path.join(a.out, f"{group}_parts.zip")
            with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
                for pname, pobjs in parts:
                    ppath = os.path.join(a.out, "_tmp", f"{group}_{pname}.glb")
                    os.makedirs(os.path.dirname(ppath), exist_ok=True)
                    self.export_glb(pobjs, ppath, pname == "vertexlit")
                    z.write(ppath, os.path.basename(ppath))
                    os.remove(ppath)
            info["parts_zip"] = os.path.basename(zpath)
            self.warn(f"{group}.glb is {size / 1e6:.0f} MB; also wrote {info['parts_zip']} (upload that instead)")
        self.report.setdefault("outputs", {})[group] = info
        log(f"exported {group}: {info['file']} {size / 1e6:.1f} MB, {info['triangles']} tris")
        self.stage(f"export_{group}", t)

    # ------------------------------------------------------------------ run
    def run(self):
        a = self.a
        os.makedirs(a.out, exist_ok=True)
        log(f"Blender {bpy.app.version_string}; src {a.src}; out {a.out}; groups {a.only}"
            f"{' (preview)' if a.preview else ''}")
        if not os.path.isdir(a.src):
            raise SystemExit(f"--src not found: {a.src}")
        self.load_geometry()
        self.prepare_materials()
        t = time.time()
        self.build_atlases()
        for x in self.atlases:
            if x["group"] in a.only:
                self.unwrap(x)
        self.stage("unwrap", t)
        self.setup_render()
        # Bake everything before changing any material or UV: every group shadows and bounces light onto the others.
        for g in ("field", "stadium", "seats"):
            if g in a.only:
                self.bake_atlases(g)
                self.bake_vertex(g)
        for g in ("field", "stadium", "seats"):
            if g in a.only:
                self.export_group(g)
        sun = self.sun
        self.report.update({
            "sun": {"utc": sun["position"]["utc"], "local": sun["local_time"],
                    "enu": {"azimuth_deg": sun["position"]["azimuth_deg"], "elevation_deg": sun["position"]["elevation_deg"],
                            "direction_to_sun": sun["frames"]["enu"]},
                    "site_map_frame": {"azimuth_deg": sun["frames"]["azimuth_map_deg"], "direction_to_sun": sun["frames"]["map"]},
                    "model_frame_blender_zup": {"azimuth_from_plus_y_cw_deg": sun["frames"]["azimuth_model_deg"],
                                                "direction_to_sun": sun["frames"]["model_blender_zup"]},
                    "model_frame_gltf_yup": {"direction_to_sun": sun["frames"]["model_gltf_yup"]},
                    "nishita_sun_rotation_rad": sun["frames"]["nishita_sun_rotation_rad"],
                    "declination_deg": sun["position"]["declination_deg"], "unreal_cesium_sunsky": UNREAL_SUN},
            "device": self.device,
            "frame": "model frame of the source OBJ (metres, Y-up in the GLB, origin midfield turf, NOT registered); "
                     "the web applies ADR-0004's 182.25 deg / (-2.25 E, +2.5 N) at runtime",
            "atlases": [{k: v for k, v in x.items() if k not in ("objs", "bake_mats")} for x in self.atlases if x["group"] in a.only],
            "timings_s": self.timings, "total_s": round(time.time() - T0, 1)})
        with open(os.path.join(a.out, "bake_report.json"), "w") as f:
            json.dump(self.report, f, indent=1, default=str)
        tmp = os.path.join(a.out, "_tmp")
        if os.path.isdir(tmp) and not os.listdir(tmp):
            os.rmdir(tmp)
        log(f"DONE: {', '.join(f'{g}.glb' for g in a.only)} and bake_report.json in {a.out}")


def main():
    a = parse_args()
    if a.self_test:
        sys.exit(0 if self_test() else 1)
    if bpy is None:
        raise SystemExit("Run inside Blender (see the docstring), or use --self-test under plain Python.")
    if np is None:
        raise SystemExit("numpy is missing from Blender's Python")
    try:
        Baker(a).run()
    except Exception:
        import traceback
        log("FAILED\n" + traceback.format_exc())
        sys.exit(1)


if __name__ == "__main__":
    main()
