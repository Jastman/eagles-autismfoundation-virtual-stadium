#!/usr/bin/env python3
"""Extract actor placement data from an editor-saved (uncooked) UE 5.x .umap for the CesiumJS web port.

Usage (repo root, Python 3.10+, no third-party packages):
    python3 web/tools/extract_umap.py [path/to/Level.umap] [--out web/data] [--dump-names]

Defaults: Content/LFF/Levels/L_LFF_Stadium.umap -> web/data/. Writes
    level_extract.json   every actor in the persistent level: class, label, tags, root component transform,
                         attach parent, class-specific properties, world transform, site-frame transform
    camera_targets.json  { TargetId: { site, world, fov } }
    camera_paths.json    LFFCameraPath actors (waypoints in world and site frames)
    cues.json            LFFCueActor (and subclass) actors with their cue properties
    labels.json          actors tagged LFF_Label (LBL_* tags, text, world/site position)
    places.json          actors tagged LFF_Place_*
    georeference.json    CesiumGeoreference origin, LFF_SiteRoot transform + globe anchor, tileset ids
    props.json           every actor with ALL its components (mesh, override materials, visibility, text, box
                         extents, spline points, instanced-static-mesh instances; relative/world/site transforms)
    materials.json       material instances in /Game/LFF/Materials and every material the level references that
                         exists in Content/ (vector/scalar/texture parameters, parent chain). Assets stored as Git
                         LFS pointers are reported with an error (run `git lfs pull` first to include them).

Rerun after saving the level in Unreal to re-export. The parser reads the FPackageFileSummary, name map,
import map, export map and the tagged-property stream of each export (UE5 object version >= 1012, "complete
type name" property tags). It does NOT handle unversioned (cooked) packages; it stops with an error on those.

Conventions in the output:
  * Units: cm and degrees, exactly as Unreal stores them. Unreal world frame at the CesiumGeoreference origin is
    X = East, Y = South, Z = Up (left-handed). Rotators are {pitch, yaw, roll} in UE's convention.
  * Properties that are absent were left at their class / archetype default when saved (delta serialization);
    the script fills known C++ defaults for the LFF classes and marks them with "defaulted": [...].
  * "site" transforms are relative to the actor tagged LFF_SiteRoot (inverse(SiteRoot world) * actor world).
"""
import argparse
import json
import math
import os
import struct
import sys

PACKAGE_TAG = 0x9E2A83C1

# ----------------------------------------------------------------------------------------------- binary reader


class Reader:
    def __init__(self, data, pos=0):
        self.d = data
        self.p = pos

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def i8(self):
        v = struct.unpack_from("<b", self.d, self.p)[0]
        self.p += 1
        return v

    def u16(self):
        v = struct.unpack_from("<H", self.d, self.p)[0]
        self.p += 2
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.d, self.p)[0]
        self.p += 4
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def i64(self):
        v = struct.unpack_from("<q", self.d, self.p)[0]
        self.p += 8
        return v

    def u64(self):
        v = struct.unpack_from("<Q", self.d, self.p)[0]
        self.p += 8
        return v

    def f32(self):
        v = struct.unpack_from("<f", self.d, self.p)[0]
        self.p += 4
        return v

    def f64(self):
        v = struct.unpack_from("<d", self.d, self.p)[0]
        self.p += 8
        return v

    def raw(self, n):
        v = self.d[self.p:self.p + n]
        self.p += n
        return bytes(v)

    def guid(self):
        a, b, c, d = struct.unpack_from("<IIII", self.d, self.p)
        self.p += 16
        return f"{a:08X}{b:08X}{c:08X}{d:08X}"

    def utf8string(self):
        """FUtf8String (UE 5.5+ FSoftObjectPath sub path): int32 byte count, no terminator."""
        n = self.i32()
        if n < 0 or n > 1 << 24:
            raise ValueError(f"bad utf8 string length {n} at {self.p - 4}")
        s = self.d[self.p:self.p + n].decode("utf-8").rstrip("\0")
        self.p += n
        return s

    def fstring(self):
        n = self.i32()
        if n == 0:
            return ""
        if n > 0:
            if n > 1 << 24:
                raise ValueError(f"bad string length {n} at {self.p - 4}")
            s = self.d[self.p:self.p + n - 1].decode("latin-1")
            self.p += n
            return s
        n = -n
        if n > 1 << 24:
            raise ValueError(f"bad string length {-n} at {self.p - 4}")
        s = self.d[self.p:self.p + 2 * (n - 1)].decode("utf-16-le")
        self.p += 2 * n
        return s


# ----------------------------------------------------------------------------------------------- package


class Package:
    def __init__(self, data):
        self.d = data
        r = Reader(data)
        s = self.summary = {}
        if r.u32() != PACKAGE_TAG:
            raise ValueError("not a UE package (bad tag)")
        s["LegacyFileVersion"] = legacy = r.i32()
        if legacy != -4:
            s["LegacyUE3Version"] = r.i32()
        s["FileVersionUE4"] = r.i32()
        s["FileVersionUE5"] = r.i32() if legacy <= -8 else 0
        s["FileVersionLicenseeUE"] = r.i32()
        ue5 = s["FileVersionUE5"]
        if ue5 == 0 and s["FileVersionUE4"] == 0:
            raise ValueError("unversioned package (cooked); this parser needs a versioned editor package")
        if ue5 < 1012:
            raise ValueError(f"UE5 object version {ue5} < 1012 (pre complete-type-name tags) is not supported")
        if ue5 >= 1016:  # PACKAGE_SAVED_HASH
            s["SavedHash"] = r.raw(20).hex()
            s["TotalHeaderSize"] = r.i32()
        n = r.i32()
        cv = []
        for _ in range(n):
            g = r.guid()
            cv.append({"guid": g, "version": r.i32()})
        s["CustomVersions"] = cv
        if ue5 < 1016:
            s["TotalHeaderSize"] = r.i32()
        s["PackageName"] = r.fstring()
        s["PackageFlags"] = r.u32()
        filter_editor_only = bool(s["PackageFlags"] & 0x80000000)
        s["NameCount"], s["NameOffset"] = r.i32(), r.i32()
        if ue5 >= 1008:  # ADD_SOFTOBJECTPATH_LIST
            s["SoftObjectPathsCount"], s["SoftObjectPathsOffset"] = r.i32(), r.i32()
        if not filter_editor_only:
            s["LocalizationId"] = r.fstring()
        s["GatherableTextDataCount"], s["GatherableTextDataOffset"] = r.i32(), r.i32()
        s["ExportCount"], s["ExportOffset"] = r.i32(), r.i32()
        s["ImportCount"], s["ImportOffset"] = r.i32(), r.i32()
        if ue5 >= 1015:  # VERSE_CELLS
            s["CellExportCount"], s["CellExportOffset"] = r.i32(), r.i32()
            s["CellImportCount"], s["CellImportOffset"] = r.i32(), r.i32()
        if ue5 >= 1014:  # METADATA_SERIALIZATION_OFFSET
            s["MetaDataOffset"] = r.i32()
        s["DependsOffset"] = r.i32()
        s["SoftPackageReferencesCount"], s["SoftPackageReferencesOffset"] = r.i32(), r.i32()
        s["SearchableNamesOffset"] = r.i32()
        s["ThumbnailTableOffset"] = r.i32()
        if ue5 >= 1018:  # IMPORT_TYPE_HIERARCHIES
            s["ImportTypeHierarchiesCount"], s["ImportTypeHierarchiesOffset"] = r.i32(), r.i32()
        if ue5 < 1016:
            s["Guid"] = r.guid()
        if not filter_editor_only:
            s["PersistentGuid"] = r.guid()
        gens = []
        for _ in range(r.i32()):
            gens.append((r.i32(), r.i32()))
        s["Generations"] = gens

        def engine_version():
            maj, mnr, pat = r.u16(), r.u16(), r.u16()
            cl = r.u32()
            return f"{maj}.{mnr}.{pat}-{cl & 0x7FFFFFFF}+{r.fstring()}"

        s["SavedByEngineVersion"] = engine_version()
        s["CompatibleWithEngineVersion"] = engine_version()
        s["CompressionFlags"] = r.u32()
        if r.i32() != 0:
            raise ValueError("compressed package chunks are not supported")
        s["PackageSource"] = r.u32()
        for _ in range(r.i32()):
            r.fstring()
        if legacy > -7:
            r.i32()
        s["AssetRegistryDataOffset"] = r.i32()
        s["BulkDataStartOffset"] = r.i64()
        s["WorldTileInfoDataOffset"] = r.i32()
        for _ in range(r.i32()):
            r.i32()
        s["PreloadDependencyCount"], s["PreloadDependencyOffset"] = r.i32(), r.i32()
        if ue5 >= 1001:
            s["NamesReferencedFromExportDataCount"] = r.i32()
        if ue5 >= 1002:
            s["PayloadTocOffset"] = r.i64()
        if ue5 >= 1009:
            s["DataResourceOffset"] = r.i32()
        self.ue4, self.ue5 = s["FileVersionUE4"], ue5

        # names
        r.p = s["NameOffset"]
        self.names = []
        for _ in range(s["NameCount"]):
            self.names.append(r.fstring())
            r.u32()  # hashes
        # soft object paths list
        self.soft_paths = []
        if s.get("SoftObjectPathsCount"):
            r.p = s["SoftObjectPathsOffset"]
            for _ in range(s["SoftObjectPathsCount"]):
                self.soft_paths.append(self.read_soft_path_inline(r))
        # imports
        r.p = s["ImportOffset"]
        self.imports = []
        for _ in range(s["ImportCount"]):
            imp = {"ClassPackage": self.fname(r), "ClassName": self.fname(r), "Outer": r.i32(),
                   "ObjectName": self.fname(r)}
            if not filter_editor_only:
                imp["PackageName"] = self.fname(r)
            if ue5 >= 1003:
                imp["bImportOptional"] = r.i32()
            self.imports.append(imp)
        # exports
        r.p = s["ExportOffset"]
        self.exports = []
        for i in range(s["ExportCount"]):
            e = {"index": i + 1, "Class": r.i32(), "Super": r.i32(), "Template": r.i32(), "Outer": r.i32(),
                 "ObjectName": self.fname(r), "ObjectFlags": r.u32(), "SerialSize": r.i64(),
                 "SerialOffset": r.i64(), "bForcedExport": r.i32(), "bNotForClient": r.i32(),
                 "bNotForServer": r.i32()}
            if ue5 < 1005:
                r.guid()
            if ue5 >= 1006:
                e["bIsInheritedInstance"] = r.i32()
            e["PackageFlags"] = r.u32()
            e["bNotAlwaysLoadedForEditorGame"] = r.i32()
            e["bIsAsset"] = r.i32()
            if ue5 >= 1003:
                e["bGeneratePublicHash"] = r.i32()
            e["FirstExportDependency"] = r.i32()
            e["Deps"] = [r.i32() for _ in range(4)]
            if ue5 >= 1010:
                e["ScriptStart"], e["ScriptEnd"] = r.i64(), r.i64()
            self.exports.append(e)
        if r.p != s["ExportOffset"] + 0 and s["ExportCount"]:
            self.export_entry_size = (r.p - s["ExportOffset"]) // s["ExportCount"]

    # names ------------------------------------------------------------------------------------------------
    def fname(self, r):
        i, n = r.i32(), r.i32()
        if i < 0 or i >= len(self.names):
            raise ValueError(f"bad name index {i} at {r.p - 8}")
        s = self.names[i]
        return f"{s}_{n - 1}" if n else s

    def read_soft_path_inline(self, r):
        pkg, asset = self.fname(r), self.fname(r)
        sub = r.utf8string() if self.ue5 >= 1007 else r.fstring()
        path = "" if pkg == "None" else (pkg + ("." + asset if asset != "None" else ""))
        return path + (":" + sub if sub else "")

    # object references ------------------------------------------------------------------------------------
    def obj_name(self, idx):
        if idx == 0:
            return None
        if idx < 0:
            return self.imports[-idx - 1]["ObjectName"]
        return self.exports[idx - 1]["ObjectName"]

    def obj_path(self, idx):
        """Full path like /Script/Engine.StaticMeshActor or PersistentLevel.Foo.Bar."""
        if idx == 0:
            return None
        parts = []
        cur = idx
        while cur != 0:
            if cur < 0:
                imp = self.imports[-cur - 1]
                parts.append(imp["ObjectName"])
                cur = imp["Outer"]
            else:
                ex = self.exports[cur - 1]
                parts.append(ex["ObjectName"])
                cur = ex["Outer"]
        parts.reverse()
        if idx < 0 and parts[0].startswith("/"):
            if len(parts) == 1:
                return parts[0]
            return parts[0] + "." + parts[1] + "".join(":" + p for p in parts[2:3]) + "".join("." + p for p in parts[3:])
        return ".".join(parts)

    def class_name(self, export):
        return self.obj_name(export["Class"])


# ----------------------------------------------------------------------------------------------- properties

# structs whose payload is native/binary when tag flag HasBinaryOrNativeSerialize is set (or always immutable)
NATIVE_STRUCTS = {
    "Vector": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64()},
    "Vector3d": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64()},
    "Vector3f": lambda r: {"x": r.f32(), "y": r.f32(), "z": r.f32()},
    "Vector2D": lambda r: {"x": r.f64(), "y": r.f64()},
    "Vector2f": lambda r: {"x": r.f32(), "y": r.f32()},
    "Vector4": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64(), "w": r.f64()},
    "Vector4d": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64(), "w": r.f64()},
    "Vector4f": lambda r: {"x": r.f32(), "y": r.f32(), "z": r.f32(), "w": r.f32()},
    "Rotator": lambda r: {"pitch": r.f64(), "yaw": r.f64(), "roll": r.f64()},
    "Rotator3d": lambda r: {"pitch": r.f64(), "yaw": r.f64(), "roll": r.f64()},
    "Rotator3f": lambda r: {"pitch": r.f32(), "yaw": r.f32(), "roll": r.f32()},
    "Quat": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64(), "w": r.f64()},
    "Quat4d": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64(), "w": r.f64()},
    "Quat4f": lambda r: {"x": r.f32(), "y": r.f32(), "z": r.f32(), "w": r.f32()},
    "LinearColor": lambda r: {"r": r.f32(), "g": r.f32(), "b": r.f32(), "a": r.f32()},
    "Color": lambda r: (lambda b, g, rr, a: {"r": rr, "g": g, "b": b, "a": a})(r.u8(), r.u8(), r.u8(), r.u8()),
    "Guid": lambda r: r.guid(),
    "IntPoint": lambda r: {"x": r.i32(), "y": r.i32()},
    "IntVector": lambda r: {"x": r.i32(), "y": r.i32(), "z": r.i32()},
    "Box": lambda r: {"min": {"x": r.f64(), "y": r.f64(), "z": r.f64()},
                      "max": {"x": r.f64(), "y": r.f64(), "z": r.f64()}, "valid": r.u8()},
    "Box2D": lambda r: {"min": {"x": r.f64(), "y": r.f64()}, "max": {"x": r.f64(), "y": r.f64()}, "valid": r.u8()},
    "Plane": lambda r: {"x": r.f64(), "y": r.f64(), "z": r.f64(), "w": r.f64()},
    "Matrix": lambda r: [[r.f64() for _ in range(4)] for _ in range(4)],
    "Matrix44d": lambda r: [[r.f64() for _ in range(4)] for _ in range(4)],
    "DateTime": lambda r: r.i64(),
    "Timespan": lambda r: r.i64(),
    "FrameNumber": lambda r: r.i32(),
}


class TypeNode:
    __slots__ = ("name", "params")

    def __init__(self, name, params):
        self.name = name
        self.params = params

    def __repr__(self):
        return self.name + (f"({', '.join(map(repr, self.params))})" if self.params else "")


class Undecodable(Exception):
    pass


class PropReader:
    def __init__(self, pkg):
        self.pkg = pkg
        self.unknown = set()

    def type_name(self, r):
        name = self.pkg.fname(r)
        n = r.i32()
        return TypeNode(name, [self.type_name(r) for _ in range(n)])

    def read_props(self, r, end):
        """Tagged property stream until None. Returns dict name -> value (array index > 0 stored as name[i])."""
        props = {}
        while r.p < end:
            name = self.pkg.fname(r)
            if name == "None":
                break
            t = self.type_name(r)
            size = r.i32()
            flags = r.u8()
            idx = 0
            if flags & 0x01:
                idx = r.i32()
            if flags & 0x02:
                r.guid()
            if flags & 0x04:
                ext = r.u8()
                if ext & 0x02:
                    r.u8()  # OverridableOperation
                    r.u8()  # ExperimentalOverridableLogic
            start = r.p
            try:
                if t.name == "BoolProperty":
                    val = bool(flags & 0x10)
                else:
                    val = self.value(r, t, start + size, native=bool(flags & 0x08), top=True)
                if r.p != start + size and t.name != "TextProperty":
                    # FText Base history in UE 5.5+ carries 4 extra trailing bytes (always 0 here); ignore those.
                    val = {"__partial__": val, "__size__": size}
            except (Undecodable, ValueError, struct.error, IndexError, UnicodeDecodeError) as ex:
                self.unknown.add(repr(t))
                val = {"__undecoded__": repr(t), "__size__": size, "__error__": str(ex)[:120]}
            r.p = start + size
            props[name if idx == 0 else f"{name}[{idx}]"] = val
        return props

    def value(self, r, t, end, native=False, top=False):
        n = t.name
        if n == "IntProperty":
            return r.i32()
        if n == "UInt32Property":
            return r.u32()
        if n == "Int64Property":
            return r.i64()
        if n == "UInt64Property":
            return r.u64()
        if n == "Int16Property":
            return struct.unpack("<h", r.raw(2))[0]
        if n == "UInt16Property":
            return r.u16()
        if n == "Int8Property":
            return r.i8()
        if n == "FloatProperty":
            return r.f32()
        if n == "DoubleProperty":
            return r.f64()
        if n == "BoolProperty":  # inside containers: one byte
            return bool(r.u8())
        if n == "NameProperty":
            return self.pkg.fname(r)
        if n == "StrProperty":
            return r.fstring()
        if n == "ByteProperty":
            if t.params and t.params[0].name != "None" or (top and end - r.p == 8):
                return self.pkg.fname(r) if end - r.p >= 8 or not top else r.u8()
            return r.u8()
        if n == "EnumProperty":
            return self.pkg.fname(r)
        if n in ("ObjectProperty", "ClassProperty", "WeakObjectProperty", "LazyObjectProperty", "InterfaceProperty"):
            if n == "LazyObjectProperty":
                return r.guid()
            idx = r.i32()
            return {"ref": idx, "path": self.pkg.obj_path(idx)} if idx else None
        if n in ("SoftObjectProperty", "SoftClassProperty"):
            return self.soft_path(r)
        if n == "TextProperty":
            return self.text(r)
        if n == "StructProperty":
            sname = t.params[0].name if t.params else "?"
            return self.struct(r, sname, end, native)
        if n == "ArrayProperty":
            inner = t.params[0]
            count = r.i32()
            return [self.elem(r, inner, end) for _ in range(count)]
        if n == "SetProperty":
            inner = t.params[0]
            rem = r.i32()
            for _ in range(rem):
                self.elem(r, inner, end)
            count = r.i32()
            return [self.elem(r, inner, end) for _ in range(count)]
        if n == "MapProperty":
            kt, vt = t.params[0], t.params[1]
            rem = r.i32()
            for _ in range(rem):
                self.elem(r, kt, end)
            count = r.i32()
            out = []
            for _ in range(count):
                k = self.elem(r, kt, end)
                v = self.elem(r, vt, end)
                out.append([k, v])
            return out
        if n == "OptionalProperty":
            has = r.i32()
            return self.elem(r, t.params[0], end) if has else None
        raise Undecodable(n)

    def elem(self, r, t, end):
        if t.name == "StructProperty":
            sname = t.params[0].name if t.params else "?"
            return self.struct(r, sname, end, native=sname in NATIVE_STRUCTS)
        return self.value(r, t, end)

    def struct(self, r, sname, end, native):
        if sname in NATIVE_STRUCTS and (native or True):
            # Immutable math structs are always binary in tagged data (UE writes them natively).
            if native or end - r.p == self.native_size(sname):
                return NATIVE_STRUCTS[sname](r)
        if sname in ("SoftObjectPath", "SoftClassPath"):
            return self.soft_path(r)
        # Structs flagged native (e.g. PostProcessSettings) that still write a tagged stream: try it, and let the
        # caller mark the value partial/undecoded if it doesn't consume the payload exactly.
        save = r.p
        try:
            return self.read_props(r, end)
        except (ValueError, IndexError, struct.error, Undecodable):
            if native:
                r.p = save
                raise Undecodable(f"native struct {sname}")
            raise

    _sizes = {}

    def native_size(self, sname):
        if sname not in self._sizes:
            rr = Reader(bytes(256))
            NATIVE_STRUCTS[sname](rr)
            self._sizes[sname] = rr.p
        return self._sizes[sname]

    def soft_path(self, r):
        if self.pkg.soft_paths:
            i = r.i32()
            return self.pkg.soft_paths[i] if 0 <= i < len(self.pkg.soft_paths) else f"<softpath {i}>"
        return self.pkg.read_soft_path_inline(r)

    def text(self, r):
        flags = r.u32()
        hist = r.i8()
        if hist == -1:
            has = r.i32()
            return {"text": r.fstring() if has else "", "flags": flags}
        if hist == 0:
            ns, key, src = r.fstring(), r.fstring(), r.fstring()
            return {"text": src, "namespace": ns, "key": key, "flags": flags}
        if hist == 11:  # StringTableEntry
            return {"stringTable": self.pkg.fname(r), "key": r.fstring(), "flags": flags}
        raise Undecodable(f"FText history {hist}")


def export_props(pkg, pr, e):
    """Tagged properties of an export (dict). Returns {} for exports with no script data."""
    if e["SerialSize"] <= 0 or e.get("ScriptEnd", 0) <= e.get("ScriptStart", 0):
        return {}
    base = e["SerialOffset"]
    r = Reader(pkg.d, base + e["ScriptStart"])
    ctl = r.u8()  # EClassSerializationControlExtension
    if ctl & 0x02:
        r.u8()  # overridden operation
    return pr.read_props(r, base + e["ScriptEnd"])


# ----------------------------------------------------------------------------------------------- math (UE conventions)

D2R, R2D = math.pi / 180.0, 180.0 / math.pi


def quat_from_rotator(rot):
    p, y, r = (rot.get("pitch", 0.0), rot.get("yaw", 0.0), rot.get("roll", 0.0))
    sp, cp = math.sin(p * D2R / 2), math.cos(p * D2R / 2)
    sy, cy = math.sin(y * D2R / 2), math.cos(y * D2R / 2)
    sr, cr = math.sin(r * D2R / 2), math.cos(r * D2R / 2)
    return (cr * sp * sy - sr * cp * cy, -cr * sp * cy - sr * cp * sy, cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy)


def _norm_axis(a):
    a = math.fmod(a, 360.0)
    if a > 180.0:
        a -= 360.0
    elif a <= -180.0:
        a += 360.0
    return a


def rotator_from_quat(q):
    """FQuat::Rotator()."""
    x, y, z, w = q
    sing = z * x - w * y
    yaw_y, yaw_x = 2 * (w * z + x * y), 1 - 2 * (y * y + z * z)
    thr = 0.4999995
    yaw = math.atan2(yaw_y, yaw_x) * R2D
    if sing < -thr:
        pitch = -90.0
        roll = _norm_axis(-yaw - 2 * math.atan2(x, w) * R2D)
    elif sing > thr:
        pitch = 90.0
        roll = _norm_axis(yaw - 2 * math.atan2(x, w) * R2D)
    else:
        pitch = math.asin(max(-1.0, min(1.0, 2 * sing))) * R2D
        roll = math.atan2(-2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * R2D
    return {"pitch": pitch, "yaw": yaw, "roll": roll}


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    x, y, z, w = q
    tx, ty, tz = 2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx))


def quat_from_axes(xa, ya, za):
    """Quaternion of a rotation matrix given as unit axes (columns)."""
    m00, m11, m22 = xa[0], ya[1], za[2]
    tr = m00 + m11 + m22
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        q = ((ya[2] - za[1]) / s, (za[0] - xa[2]) / s, (xa[1] - ya[0]) / s, 0.25 * s)
    elif m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2
        q = (0.25 * s, (ya[0] + xa[1]) / s, (za[0] + xa[2]) / s, (ya[2] - za[1]) / s)
    elif m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2
        q = ((ya[0] + xa[1]) / s, 0.25 * s, (za[1] + ya[2]) / s, (za[0] - xa[2]) / s)
    else:
        s = math.sqrt(1.0 + m22 - m00 - m11) * 2
        q = ((za[0] + xa[2]) / s, (za[1] + ya[2]) / s, 0.25 * s, (xa[1] - ya[0]) / s)
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


class Xf:
    """FTransform (translation, rotation quat, scale). compose(child, parent) follows FTransform::Multiply."""
    __slots__ = ("t", "q", "s")

    def __init__(self, t=(0.0, 0.0, 0.0), q=(0.0, 0.0, 0.0, 1.0), s=(1.0, 1.0, 1.0)):
        self.t, self.q, self.s = tuple(t), tuple(q), tuple(s)

    def then(self, parent):
        """self relative to parent -> world."""
        st = tuple(parent.s[i] * self.t[i] for i in range(3))
        rt = qrot(parent.q, st)
        return Xf(tuple(rt[i] + parent.t[i] for i in range(3)), qmul(parent.q, self.q),
                  tuple(self.s[i] * parent.s[i] for i in range(3)))

    def relative_to(self, other):
        """self (world) expressed in other's frame (other world)."""
        qi = qinv(other.q)
        d = qrot(qi, tuple(self.t[i] - other.t[i] for i in range(3)))
        return Xf(tuple(d[i] / (other.s[i] or 1.0) for i in range(3)), qmul(qi, self.q),
                  tuple(self.s[i] / (other.s[i] or 1.0) for i in range(3)))

    def point(self, p):
        rt = qrot(self.q, tuple(self.s[i] * p[i] for i in range(3)))
        return tuple(rt[i] + self.t[i] for i in range(3))

    def to_json(self):
        return {"location": vec_json(self.t), "rotation": rot_json(rotator_from_quat(self.q)),
                "quat": dict(zip("xyzw", (rnd(c, 9) for c in self.q))), "scale": vec_json(self.s)}


def rnd(v, n=6):
    if isinstance(v, float):
        v = round(v, n)
        return 0.0 if v == 0 else v
    return v


def vec_json(v):
    return {"x": rnd(v[0]), "y": rnd(v[1]), "z": rnd(v[2])}


def rot_json(r):
    return {"pitch": rnd(r["pitch"]), "yaw": rnd(r["yaw"]), "roll": rnd(r["roll"])}


def vtuple(v, default=(0.0, 0.0, 0.0)):
    if not isinstance(v, dict):
        return default
    return (v.get("x", 0.0), v.get("y", 0.0), v.get("z", 0.0))


def xf_from_matrix(m):
    """FMatrix rows (scaled X, Y, Z axes; translation in row 3) -> Xf."""
    axes = [tuple(m[i][:3]) for i in range(3)]
    sc = [math.sqrt(sum(c * c for c in a)) for a in axes]
    det = (axes[0][0] * (axes[1][1] * axes[2][2] - axes[1][2] * axes[2][1])
           - axes[0][1] * (axes[1][0] * axes[2][2] - axes[1][2] * axes[2][0])
           + axes[0][2] * (axes[1][0] * axes[2][1] - axes[1][1] * axes[2][0]))
    if det < 0:
        sc[0] = -sc[0]
    unit = [tuple(c / (sc[i] or 1.0) for c in axes[i]) for i in range(3)]
    return Xf(tuple(m[3][:3]), quat_from_axes(*unit), tuple(sc))


# ----------------------------------------------------------------------------------------------- geodesy (WGS84)

WGS84_A, WGS84_F = 6378137.0, 1 / 298.257223563
WGS84_E2 = WGS84_F * (2 - WGS84_F)


def geodetic_to_ecef(lat, lon, h):
    la, lo = lat * D2R, lon * D2R
    n = WGS84_A / math.sqrt(1 - WGS84_E2 * math.sin(la) ** 2)
    return ((n + h) * math.cos(la) * math.cos(lo), (n + h) * math.cos(la) * math.sin(lo),
            (n * (1 - WGS84_E2) + h) * math.sin(la))


def ecef_to_geodetic(p):
    x, y, z = p
    lon = math.atan2(y, x)
    r = math.hypot(x, y)
    lat = math.atan2(z, r * (1 - WGS84_E2))
    h = 0.0
    for _ in range(10):
        n = WGS84_A / math.sqrt(1 - WGS84_E2 * math.sin(lat) ** 2)
        h = r / math.cos(lat) - n
        lat = math.atan2(z, r * (1 - WGS84_E2 * n / (n + h)))
    return lat * R2D, lon * R2D, h


def enu_axes(lat, lon):
    la, lo = lat * D2R, lon * D2R
    e = (-math.sin(lo), math.cos(lo), 0.0)
    n = (-math.sin(la) * math.cos(lo), -math.sin(la) * math.sin(lo), math.cos(la))
    u = (math.cos(la) * math.cos(lo), math.cos(la) * math.sin(lo), math.sin(la))
    return e, n, u


class Georef:
    """Cesium for Unreal CartographicOrigin georeference: Unreal (cm) X=East, Y=South, Z=Up at the origin."""

    def __init__(self, lat, lon, h):
        self.lat, self.lon, self.h = lat, lon, h
        self.o = geodetic_to_ecef(lat, lon, h)
        self.e, self.n, self.u = enu_axes(lat, lon)

    def dir_to_ecef(self, v):
        x, y, z = v[0] / 100.0, -v[1] / 100.0, v[2] / 100.0
        return tuple(x * self.e[i] + y * self.n[i] + z * self.u[i] for i in range(3))

    def to_ecef(self, p):
        d = self.dir_to_ecef(p)
        return tuple(self.o[i] + d[i] for i in range(3))

    def cesium_pose(self, xf):
        """lon/lat/height + Cesium heading/pitch/roll (deg) of an Unreal world pose (camera looks along +X)."""
        p = self.to_ecef(xf.t)
        lat, lon, h = ecef_to_geodetic(p)
        e, n, u = enu_axes(lat, lon)
        f = self.dir_to_ecef(qrot(xf.q, (100.0, 0.0, 0.0)))
        up = self.dir_to_ecef(qrot(xf.q, (0.0, 0.0, 100.0)))
        fe, fn, fu = (sum(f[i] * a[i] for i in range(3)) for a in (e, n, u))
        heading = math.atan2(fe, fn) * R2D % 360.0
        pitch = math.asin(max(-1.0, min(1.0, fu))) * R2D
        # roll: aviation convention (right side down positive), up vector vs. the level (zero-roll) up vector
        right0 = (fn, -fe, 0.0)  # forward x up(0,0,1) in ENU, for a right-handed ENU frame: right = f x u
        rn = math.hypot(right0[0], right0[1]) or 1.0
        right0 = (right0[0] / rn, right0[1] / rn, 0.0)
        up0 = (right0[1] * fu - 0.0 * fn, 0.0 * fe - right0[0] * fu, right0[0] * fn - right0[1] * fe)
        ue, un, uu = (sum(up[i] * a[i] for i in range(3)) for a in (e, n, u))
        roll = math.atan2(ue * right0[0] + un * right0[1], ue * up0[0] + un * up0[1] + uu * up0[2]) * R2D
        return {"longitude": rnd(lon, 9), "latitude": rnd(lat, 9), "height": rnd(h, 4),
                "ecef": [rnd(c, 4) for c in p], "heading": rnd(heading), "pitch": rnd(pitch), "roll": rnd(roll)}


# ----------------------------------------------------------------------------------------------- level extraction

CUE_DEFAULTS = {"CueId": "None", "Trigger": "ELFFCueTrigger::Arrival", "StartOffset": {"x": 0.0, "y": 0.0, "z": 0.0},
                "StartRotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}, "PlaySeconds": 3.0, "PlayDelaySeconds": 0.0,
                "bLinear": False, "bHiddenBefore": False, "bHiddenAfter": False, "MovingAnimation": None,
                "StillAnimation": None, "bSpectacle": False}
CLASS_DEFAULTS = {  # C++ defaults (Source/VirtualStadium) for properties omitted by delta serialization
    "LFFCameraTarget": {"TargetId": "None", "FieldOfView": 70.0},
    "LFFCameraPath": {"FromTargetId": "None", "ToTargetId": "None", "Waypoints": [], "TravelSeconds": 8.0,
                      "bCutAtEnd": False},
    "LFFCueActor": CUE_DEFAULTS,
    # ALFFFireworksActor() overrides Trigger/bHidden*/bLinear and the burst arrays (LFFFireworksActor.cpp)
    "LFFFireworksActor": {**CUE_DEFAULTS, "Trigger": "ELFFCueTrigger::Preview", "bHiddenBefore": True,
                          "bHiddenAfter": True, "bLinear": True,
                          "BurstOffsets": [{"x": 0.0, "y": -2600.0, "z": 0.0}, {"x": 0.0, "y": 0.0, "z": 700.0},
                                           {"x": 0.0, "y": 2600.0, "z": 100.0}],
                          "BurstColors": [{"r": 1.0, "g": 0.78, "b": 0.45, "a": 1.0},
                                          {"r": 0.25, "g": 0.85, "b": 0.55, "a": 1.0},
                                          {"r": 0.8, "g": 0.85, "b": 0.95, "a": 1.0}], "SparksPerBurst": 60,
                          "BurstRadius": 1500.0, "BurstSeconds": 3.5, "BurstInterval": 1.6, "SparkSize": 130.0,
                          "SparkMaterial": None, "SparkGlow": 4000.0},
    "LFFPersonActor": CUE_DEFAULTS,
    "LFFCrowdFanActor": {**CUE_DEFAULTS, "VariationSeed": 1018, "WalkRoute": [], "WalkSpeed": 85.0, "RoutePhase": 0.0,
                         "AnimationDistance": 6500.0},
}
NON_ACTOR_LEVEL_CHILDREN = {"Model", "Polys", "BodySetup", "NavigationSystemModuleConfig", "WorldThumbnailInfo",
                            "LevelScriptBlueprint", "MetaData", "BookMark", "LevelActorContainer", "Level",
                            "WorldPartitionActorDescContainer", "NavigationSystemV1"}
# C++ constructor values of default subobjects (Source/VirtualStadium/Private), applied when the level omits them
_PERSON_COMPONENTS = {"Pose": {"RelativeRotation": {"pitch": 0.0, "yaw": -90.0, "roll": 0.0}, "bVisible": False},
                      "Body": {"RelativeRotation": {"pitch": 0.0, "yaw": -90.0, "roll": 0.0}}}
COMPONENT_DEFAULTS = {"LFFPersonActor": _PERSON_COMPONENTS, "LFFCrowdFanActor": _PERSON_COMPONENTS,
                      "LFFRoomComfortZone": {"RoomBounds": {"BoxExtent": {"x": 1.0, "y": 1.0, "z": 1.0}}},
                      "LFFCameraTarget": {"DirectionArrow": {"ArrowSize": 2.0}}}
CUE_CLASSES = {"LFFCueActor", "LFFFireworksActor", "LFFPersonActor", "LFFCrowdFanActor"}
# components whose bHiddenInGame defaults to true in the engine
HIDDEN_IN_GAME_BY_DEFAULT = {"ArrowComponent", "BillboardComponent", "BoxComponent", "SphereComponent",
                             "CapsuleComponent", "SplineComponent", "DrawFrustumComponent", "BrushComponent"}
ACTOR_BOOKKEEPING = {"RootComponent", "ActorGuid", "ActorInstanceGuid", "ActorLabel", "FolderPath", "Tags",
                     "InstanceComponents", "BlueprintCreatedComponents", "bIsSpatiallyLoaded", "ContentBundleGuid",
                     "ExternalDataLayerAsset", "DataLayerAssets", "HLODLayer", "SpriteScale"}
COMPONENT_SKIP = {"AttachParent", "AttachSocketName", "RelativeLocation", "RelativeRotation", "RelativeScale3D",
                  "BodyInstance", "CachedBounds", "CachedBounds[1]", "CachedBounds[2]", "CreationMethod",
                  "StaticMeshImportVersion", "AssetUserData", "VisibilityId", "UCSSerializationIndex",
                  "bNetAddressable", "SplineCurves", "Spline", "LightGuid", "MapBuildDataId", "InstancingRandomSeed",
                  "StaticMesh", "OverrideMaterials", "SkeletalMeshAsset", "SkinnedAsset", "SkeletalMesh",
                  "bHiddenInGame", "bVisible", "Text", "WorldSize", "TextRenderColor", "HorizontalAlignment",
                  "VerticalAlignment", "BoxExtent", "ReparamTable"}


def clean(v, own=None):
    """JSON-friendly copy of a decoded property value: object refs -> path strings, floats rounded."""
    if isinstance(v, dict):
        if "ref" in v and "path" in v and len(v) == 2:
            return v["path"]
        if "__undecoded__" in v:
            return {"undecoded": v["__undecoded__"]}
        return {k: clean(x) for k, x in v.items()}
    if isinstance(v, list):
        return [clean(x) for x in v]
    if isinstance(v, float):
        return rnd(v, 9)
    return v


def enum_short(v):
    return v.split("::")[-1] if isinstance(v, str) else v


class Level:
    def __init__(self, path):
        self.path = path
        self.pkg = Package(open(path, "rb").read())
        self.pr = PropReader(self.pkg)
        pkg = self.pkg
        self.props = {}
        self.defaulted = {}
        self.cls = {}
        for e in pkg.exports:
            self.cls[e["index"]] = pkg.class_name(e)
        self.level_idx = next(e["index"] for e in pkg.exports if self.cls[e["index"]] == "Level")
        # actors = direct children of PersistentLevel, minus the level's own non-actor subobjects
        self.actor_idx = [e["index"] for e in pkg.exports if e["Outer"] == self.level_idx
                          and self.cls[e["index"]] not in NON_ACTOR_LEVEL_CHILDREN]
        self.children = {}
        for e in pkg.exports:
            self.children.setdefault(e["Outer"], []).append(e["index"])
        self._world = {}

    def get(self, idx):
        if idx not in self.props:
            p = export_props(self.pkg, self.pr, self.pkg.exports[idx - 1])
            outer = self.pkg.exports[idx - 1]["Outer"]
            if outer > 0:
                dflt = COMPONENT_DEFAULTS.get(self.cls.get(outer), {}).get(self.name(idx), {})
                filled = [k for k in dflt if k not in p]
                for k in filled:
                    p[k] = dflt[k]
                if filled:
                    self.defaulted[idx] = filled
            self.props[idx] = p
        return self.props[idx]

    def name(self, idx):
        return self.pkg.exports[idx - 1]["ObjectName"]

    def ref(self, v):
        return v["ref"] if isinstance(v, dict) and "ref" in v else 0

    def owner_actor(self, comp_idx):
        cur = comp_idx
        while cur > 0:
            outer = self.pkg.exports[cur - 1]["Outer"]
            if outer == self.level_idx:
                return cur
            cur = outer
        return None

    def is_scene_component(self, idx, actor_idx):
        p = self.get(idx)
        if self.ref(self.get(actor_idx).get("RootComponent")) == idx:
            return True
        return any(k in p for k in ("AttachParent", "RelativeLocation", "RelativeRotation", "RelativeScale3D"))

    def components(self, actor_idx):
        ap = self.get(actor_idx)
        listed = {self.ref(v) for k in ("InstanceComponents", "BlueprintCreatedComponents")
                  for v in (ap.get(k) or [])}
        out = []
        for c in self.children.get(actor_idx, []):
            cn = self.cls[c]
            if cn.endswith("Component") or c in listed or self.is_scene_component(c, actor_idx):
                out.append(c)
        return out

    def relative(self, idx):
        p = self.get(idx)
        return Xf(vtuple(p.get("RelativeLocation")), quat_from_rotator(p.get("RelativeRotation") or {}),
                  vtuple(p.get("RelativeScale3D"), (1.0, 1.0, 1.0)))

    def world(self, idx, depth=0):
        if idx in self._world:
            return self._world[idx]
        p = self.get(idx)
        rel = self.relative(idx)
        parent = self.ref(p.get("AttachParent"))
        if parent > 0 and depth < 64:
            pw = self.world(parent, depth + 1)
            w = rel.then(pw)
            # absolute flags keep that part of the relative transform in world space
            if p.get("bAbsoluteLocation"):
                w.t = rel.t
            if p.get("bAbsoluteRotation"):
                w.q = rel.q
            if p.get("bAbsoluteScale"):
                w.s = rel.s
        else:
            w = rel
        self._world[idx] = w
        return w

    def attach_info(self, comp_idx):
        p = self.get(comp_idx)
        par = self.ref(p.get("AttachParent"))
        if par == 0:
            return None
        if par < 0:
            return {"import": self.pkg.obj_path(par)}
        owner = self.owner_actor(par)
        info = {"actor": self.name(owner) if owner else None,
                "actorLabel": self.get(owner).get("ActorLabel") if owner else None,
                "component": self.name(par)}
        if p.get("AttachSocketName") not in (None, "None"):
            info["socket"] = p["AttachSocketName"]
        return info

    # ---- instance data of InstancedStaticMeshComponent (native, after the tagged properties)
    def ism_instances(self, idx):
        e = self.pkg.exports[idx - 1]
        d = self.pkg.d
        start, end = e["SerialOffset"] + e["ScriptEnd"], e["SerialOffset"] + e["SerialSize"]
        # PerInstanceSMData.BulkSerialize: int32 element size (128 = FMatrix of doubles), int32 count, data.
        for off in range(start, min(end - 8, start + 64), 4):
            esz, cnt = struct.unpack_from("<ii", d, off)
            if esz == 128 and 0 <= cnt and off + 8 + cnt * 128 <= end:
                out = []
                for i in range(cnt):
                    vals = struct.unpack_from("<16d", d, off + 8 + i * 128)
                    out.append(xf_from_matrix([vals[0:4], vals[4:8], vals[8:12], vals[12:16]]))
                return out
        return None


def transform_block(w, site_root_world):
    out = {"world": w.to_json()}
    if site_root_world is not None:
        out["site"] = w.relative_to(site_root_world).to_json()
    return out


def flat_pose(xf_json):
    loc, rot = xf_json["location"], xf_json["rotation"]
    return {"x": loc["x"], "y": loc["y"], "z": loc["z"], "pitch": rot["pitch"], "yaw": rot["yaw"], "roll": rot["roll"]}


def extract(level_path):
    L = Level(level_path)
    pkg = L.pkg
    # site root
    site_idx = next((a for a in L.actor_idx if "LFF_SiteRoot" in (L.get(a).get("Tags") or [])), None)
    site_root_world = None
    if site_idx:
        rc = L.ref(L.get(site_idx).get("RootComponent"))
        site_root_world = L.world(rc) if rc else None
    georef = None
    for a in L.actor_idx:
        if L.cls[a] == "CesiumGeoreference":
            gp = L.get(a)
            georef = Georef(gp.get("OriginLatitude", 39.736401), gp.get("OriginLongitude", -105.25737),
                            gp.get("OriginHeight", 2250.0))

    actors = []
    for a in L.actor_idx:
        ap = L.get(a)
        cls = L.cls[a]
        e = pkg.exports[a - 1]
        rc = L.ref(ap.get("RootComponent"))
        entry = {"export": e["ObjectName"], "exportIndex": a, "class": cls,
                 "classPath": pkg.obj_path(e["Class"]), "label": ap.get("ActorLabel"),
                 "tags": ap.get("Tags") or [], "folder": ap.get("FolderPath")}
        if "bHidden" in ap:
            entry["bHidden"] = ap["bHidden"]
        elif cls == "LFFCameraPath":
            entry["bHidden"] = True  # SetActorHiddenInGame(true) in the constructor
        # class-specific actor properties (+ C++ defaults for LFF classes)
        own = set(L.children.get(a, []))
        props, defaulted = {}, []
        for k, v in ap.items():
            if k in ACTOR_BOOKKEEPING or k == "bHidden":
                continue
            if isinstance(v, dict) and v.get("ref") in own:
                continue
            if isinstance(v, dict) and "__undecoded__" in v and "Delegate" in v["__undecoded__"]:
                continue
            props[k] = clean(v)
        for k, dv in CLASS_DEFAULTS.get(cls, {}).items():
            if k not in props:
                props[k] = dv
                defaulted.append(k)
        if props:
            entry["props"] = props
        if defaulted:
            entry["defaulted"] = defaulted
        # root component
        if rc > 0:
            rp = L.get(rc)
            entry["rootComponent"] = {
                "name": L.name(rc), "class": L.cls[rc],
                "relative": {"location": vec_json(vtuple(rp.get("RelativeLocation"))),
                             "rotation": rot_json(rp.get("RelativeRotation") or {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}),
                             "scale": vec_json(vtuple(rp.get("RelativeScale3D"), (1.0, 1.0, 1.0)))},
                "relativeExplicit": [k for k in ("RelativeLocation", "RelativeRotation", "RelativeScale3D") if k in rp],
                "attachParent": L.attach_info(rc)}
            for k in ("bAbsoluteLocation", "bAbsoluteRotation", "bAbsoluteScale"):
                if rp.get(k):
                    entry["rootComponent"][k] = True
            entry.update(transform_block(L.world(rc), site_root_world))
            if georef:
                entry["cesium"] = georef.cesium_pose(L.world(rc))
        # components (all scene components + a few non-scene ones with data)
        comps = []
        comp_idx = L.components(a)
        for c in comp_idx:
            cp = L.get(c)
            ccls = L.cls[c]
            ce = {"name": L.name(c), "class": ccls, "isRoot": c == rc}
            if c in L.defaulted:
                ce["defaulted"] = L.defaulted[c]
            scene = L.is_scene_component(c, a)
            if scene:
                ce["attachParent"] = L.attach_info(c)
                ce["relative"] = L.relative(c).to_json()
                ce.update(transform_block(L.world(c), site_root_world))
                hidden = cp.get("bHiddenInGame", ccls in HIDDEN_IN_GAME_BY_DEFAULT)
                ce["bHiddenInGame"] = hidden
                ce["bVisible"] = cp.get("bVisible", True)
            for key in ("StaticMesh", "SkeletalMeshAsset", "SkinnedAsset", "SkeletalMesh"):
                if cp.get(key):
                    ce["mesh"] = clean(cp[key])
                    break
            if "OverrideMaterials" in cp:
                ce["materials"] = clean(cp["OverrideMaterials"])
            if ccls == "TextRenderComponent" or "Text" in cp:
                t = cp.get("Text")
                ce["text"] = (t.get("text") if isinstance(t, dict) else t) if t is not None else "Text"
                ce["worldSize"] = cp.get("WorldSize", 26.0)
                ce["textRenderColor"] = cp.get("TextRenderColor", {"r": 255, "g": 255, "b": 255, "a": 255})
                ce["horizontalAlignment"] = enum_short(cp.get("HorizontalAlignment", "EHTA_Left"))
                ce["verticalAlignment"] = enum_short(cp.get("VerticalAlignment", "EVRTA_TextBottom"))
            if "BoxExtent" in cp or ccls == "BoxComponent":
                ce["boxExtent"] = vec_json(vtuple(cp.get("BoxExtent"), (32.0, 32.0, 32.0)))
            if "SplineCurves" in cp:
                pts = [q.get("OutVal") for q in ((cp["SplineCurves"].get("Position") or {}).get("Points") or [])]
                ce["splinePointsLocal"] = [vec_json(vtuple(q)) for q in pts]
                if scene:
                    w = L.world(c)
                    ce["splinePointsWorld"] = [vec_json(w.point(vtuple(q))) for q in pts]
                    if site_root_world is not None:
                        ce["splinePointsSite"] = [vec_json(Xf(w.point(vtuple(q))).relative_to(site_root_world).t)
                                                  for q in pts]
                    if georef:
                        ce["splinePointsLonLatHeight"] = [
                            (lambda g: [rnd(g[1], 9), rnd(g[0], 9), rnd(g[2], 4)])(
                                ecef_to_geodetic(georef.to_ecef(w.point(vtuple(q))))) for q in pts]
            if ccls in ("InstancedStaticMeshComponent", "HierarchicalInstancedStaticMeshComponent"):
                inst = L.ism_instances(c)
                if inst is not None:
                    w = L.world(c)
                    ce["instances"] = []
                    for ix in inst:
                        iw = ix.then(w)
                        item = {"local": ix.to_json(), "world": iw.to_json()}
                        if site_root_world is not None:
                            item["site"] = iw.relative_to(site_root_world).to_json()
                        ce["instances"].append(item)
                else:
                    ce["instances"] = None
                    ce["instancesNote"] = "PerInstanceSMData not found"
            extra = {k: clean(v) for k, v in cp.items() if k not in COMPONENT_SKIP
                     and not (isinstance(v, dict) and "__undecoded__" in v and "Delegate" in v["__undecoded__"])}
            if extra:
                ce["props"] = extra
            comps.append(ce)
        # non-component subobjects with data (e.g. CubeBuilder of a volume)
        subs = {}
        for c in L.children.get(a, []):
            if c in comp_idx or L.cls[c].endswith("Component") or L.cls[c] in ("Model", "Polys", "BodySetup"):
                continue
            sp = L.get(c)
            if sp and not L.is_scene_component(c, a):
                subs[L.name(c)] = {"class": L.cls[c], "props": clean(sp)}
        entry["components"] = comps
        if subs:
            entry["subobjects"] = subs
        actors.append(entry)
    return L, actors, site_idx, site_root_world, georef


# ----------------------------------------------------------------------------------------------- materials


def asset_file(content_dir, obj_path):
    """/Game/LFF/Materials/MI_X.MI_X -> <content>/LFF/Materials/MI_X.uasset (None for /Engine, /Script, plugins)."""
    if not obj_path or not obj_path.startswith("/Game/"):
        return None
    pkg_path = obj_path.split(".")[0]
    f = os.path.join(content_dir, pkg_path[len("/Game/"):] + ".uasset")
    return f if os.path.isfile(f) else None


_PKG_CACHE = {}


class LfsPointer(Exception):
    pass


def _load_pkg(path):
    if path not in _PKG_CACHE:
        data = open(path, "rb").read()
        if data.startswith(b"version https://git-lfs"):
            raise LfsPointer("Git LFS pointer (content not fetched; run git lfs pull to include it)")
        pkg = Package(data)
        _PKG_CACHE[path] = (pkg, PropReader(pkg))
    return _PKG_CACHE[path]


def read_material_asset(path, obj_name=None):
    """Material / material instance export of a package: the top-level asset, or the export named obj_name
    (e.g. a MaterialInstanceConstant stored as a subobject of a skeletal mesh package)."""
    pkg, pr = _load_pkg(path)
    for e in pkg.exports:
        cn = pkg.class_name(e)
        if obj_name is not None and e["ObjectName"] != obj_name:
            continue
        if (e["Outer"] == 0 or obj_name is not None) and cn in ("MaterialInstanceConstant", "Material",
                                                                  "MaterialInstanceDynamic"):
            p = export_props(pkg, pr, e)
            out = {"class": cn, "package": pkg.summary["PackageName"], "object": pkg.obj_path(e["index"]),
                   "parent": clean(p.get("Parent")), "vectorParameters": {}, "scalarParameters": {},
                   "textureParameters": {}}
            for v in p.get("VectorParameterValues") or []:
                nm = (v.get("ParameterInfo") or {}).get("Name")
                out["vectorParameters"][nm] = {k: rnd(x, 9) for k, x in (v.get("ParameterValue") or {}).items()}
            for v in p.get("ScalarParameterValues") or []:
                nm = (v.get("ParameterInfo") or {}).get("Name")
                out["scalarParameters"][nm] = rnd(v.get("ParameterValue", 0.0), 9)
            for v in p.get("TextureParameterValues") or []:
                nm = (v.get("ParameterInfo") or {}).get("Name")
                out["textureParameters"][nm] = clean(v.get("ParameterValue"))
            if "BasePropertyOverrides" in p:
                out["basePropertyOverrides"] = clean(p["BasePropertyOverrides"])
            if cn == "Material":  # base material: just a few useful flags
                out["props"] = {k: clean(v) for k, v in p.items() if k in ("BlendMode", "ShadingModel", "TwoSided",
                                                                           "bUsedWithInstancedStaticMeshes")}
            return out
    return None


def read_static_mesh_asset(path):
    pkg, pr = _load_pkg(path)
    for e in pkg.exports:
        if e["Outer"] == 0 and pkg.class_name(e) == "StaticMesh":
            p = export_props(pkg, pr, e)
            mats = []
            for m in p.get("StaticMaterials") or []:
                mats.append({"slot": m.get("MaterialSlotName"), "material": clean(m.get("MaterialInterface"))})
            out = {"package": pkg.summary["PackageName"], "staticMaterials": mats}
            if "ExtendedBounds" in p:
                out["extendedBounds"] = clean(p["ExtendedBounds"])
            return out
    return None


def collect_materials(content_dir, actors):
    refs = set()
    meshes = set()
    for a in actors:
        for c in a["components"]:
            for m in c.get("materials") or []:
                if isinstance(m, str):
                    refs.add(m)
            if isinstance(c.get("mesh"), str):
                meshes.add(c["mesh"])
        sm = (a.get("props") or {}).get("SparkMaterial")
        if isinstance(sm, str):
            refs.add(sm)
    # work list of (file, object name or None for the package's top-level asset, reference path or None)
    work = []
    mat_dir = os.path.join(content_dir, "LFF", "Materials")
    for root, _, names in os.walk(mat_dir):
        for n in sorted(names):
            if n.endswith(".uasset"):
                work.append((os.path.join(root, n), None))

    def add_ref(r):
        f = asset_file(content_dir, r)
        if f:
            work.append((f, r.rsplit(".", 1)[-1].split(":")[-1]))

    for r in sorted(refs):
        add_ref(r)
    mesh_info = {}
    for m in sorted(meshes):
        f = asset_file(content_dir, m)
        if not f:
            mesh_info[m] = {"note": "asset not in Content/ (engine or plugin content)"}
            continue
        try:
            info = read_static_mesh_asset(f)
        except Exception as ex:  # noqa: BLE001
            info = {"error": str(ex)[:200]}
        if info:
            mesh_info[m] = info
            for sm in info.get("staticMaterials", []):
                if isinstance(sm.get("material"), str):
                    add_ref(sm["material"])
    mats = {}
    seen = set()
    while work:
        f, obj = work.pop()
        if (f, obj) in seen:
            continue
        seen.add((f, obj))
        try:
            info = read_material_asset(f, obj)
        except Exception as ex:  # noqa: BLE001
            info = None
            mats[os.path.relpath(f, content_dir)] = {"error": str(ex)[:200]}
        if not info:
            continue
        name = info["package"]
        key = f"{name}.{info['object'].split('.', 1)[-1]}" if info.get("object") else name
        mats[key] = info
        if isinstance(info.get("parent"), str):
            add_ref(info["parent"])
    # effective parameters (own values over parent chain values)
    for key, info in mats.items():
        vec, sca, tex = {}, {}, {}
        chain, cur = [], info
        while cur and len(chain) < 16:
            chain.append(cur)
            par = cur.get("parent")
            cur = mats.get(par) if par else None
            if par and not cur:
                info["rootParentOutsideContent"] = par
        for c in reversed(chain):
            vec.update(c.get("vectorParameters", {}))
            sca.update(c.get("scalarParameters", {}))
            tex.update(c.get("textureParameters", {}))
        info["effectiveVectorParameters"], info["effectiveScalarParameters"] = vec, sca
        info["effectiveTextureParameters"] = tex
    unresolved = sorted(r for r in refs if r not in mats)
    return mats, mesh_info, unresolved


# ----------------------------------------------------------------------------------------------- outputs


def write_json(path, obj, compact=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        if compact:
            json.dump(obj, f, separators=(",", ":"), ensure_ascii=False)
        else:
            json.dump(obj, f, indent=1, ensure_ascii=False)
        f.write("\n")


def text_of(actor):
    for c in actor["components"]:
        if "text" in c:
            return c
    return None


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("umap", nargs="?", default=os.path.join(repo, "Content", "LFF", "Levels", "L_LFF_Stadium.umap"))
    ap.add_argument("--out", default=os.path.join(repo, "web", "data"))
    ap.add_argument("--content", default=os.path.join(repo, "Content"), help="Content dir for /Game/ assets")
    args = ap.parse_args(argv)

    L, actors, site_idx, site_root_world, georef = extract(args.umap)
    s = L.pkg.summary
    by_label = {a["label"]: a for a in actors}
    site_actor = next((a for a in actors if a["exportIndex"] == site_idx), None)
    source = {"file": os.path.relpath(args.umap, repo), "packageName": s["PackageName"],
              "legacyFileVersion": s["LegacyFileVersion"], "fileVersionUE4": s["FileVersionUE4"],
              "fileVersionUE5": s["FileVersionUE5"], "licenseeVersion": s["FileVersionLicenseeUE"],
              "savedByEngineVersion": s["SavedByEngineVersion"],
              "compatibleWithEngineVersion": s["CompatibleWithEngineVersion"], "packageFlags": hex(s["PackageFlags"]),
              "savedHash": s.get("SavedHash"), "nameCount": s["NameCount"], "importCount": s["ImportCount"],
              "exportCount": s["ExportCount"], "customVersionCount": len(s["CustomVersions"])}
    conventions = {
        "units": "cm, degrees (Unreal)",
        "worldFrame": "Unreal world at the CesiumGeoreference origin: X=East, Y=South, Z=Up (left-handed)",
        "rotator": "UE FRotator {pitch, yaw, roll}; quat = FRotator::Quaternion(); yaw+ turns X toward +Y (south)",
        "site": "transform relative to the LFF_SiteRoot actor's root component (inverse(siteRoot) * world)",
        "cesium": "lon/lat (deg), height (m, WGS84 ellipsoid), heading (deg clockwise from north at that point), "
                  "pitch (deg, + up), roll (deg) for the actor root pose looking along its +X axis",
        "defaulted": "properties absent from the file (equal to the C++ default when saved); default value filled in",
    }
    counts = {}
    for a in actors:
        counts[a["class"]] = counts.get(a["class"], 0) + 1
    level_actors = []
    for a in actors:
        la = {k: v for k, v in a.items() if k != "components"}
        la["componentCount"] = len(a["components"])
        level_actors.append(la)
    write_json(os.path.join(args.out, "level_extract.json"), {
        "source": source, "conventions": conventions, "actorClassCounts": dict(sorted(counts.items())),
        "siteRoot": site_actor["export"] if site_actor else None,
        "undecodedPropertyTypes": sorted(L.pr.unknown), "actors": level_actors})

    # props.json: every actor with all its components
    write_json(os.path.join(args.out, "props.json"), {
        "source": source, "conventions": conventions,
        "actors": [{k: a.get(k) for k in ("label", "export", "class", "tags", "folder", "bHidden", "props",
                                          "defaulted", "world", "site", "components") if k in a} for a in actors]},
               compact=True)

    # camera targets
    cams = {}
    for a in actors:
        if a["class"] == "LFFCameraTarget":
            tid = a["props"]["TargetId"]
            cams[tid] = {"label": a["label"], "export": a["export"], "fov": a["props"]["FieldOfView"],
                         "fovDefaulted": "FieldOfView" in a.get("defaulted", []),
                         "site": flat_pose(a["site"]) if "site" in a else None,
                         "world": flat_pose(a["world"]), "cesium": a.get("cesium"), "tags": a["tags"]}
    write_json(os.path.join(args.out, "camera_targets.json"), dict(sorted(cams.items())))

    # camera paths
    paths = []
    for a in actors:
        if a["class"] == "LFFCameraPath":
            p = a["props"]
            wps = [vtuple(w) for w in p["Waypoints"]]
            item = {"label": a["label"], "export": a["export"], "from": p["FromTargetId"], "to": p["ToTargetId"],
                    "travelSeconds": p["TravelSeconds"], "bCutAtEnd": p["bCutAtEnd"],
                    "defaulted": a.get("defaulted", []),
                    "waypointsWorld": [vec_json(w) for w in wps]}
            if site_root_world is not None:
                item["waypointsSite"] = [vec_json(Xf(w).relative_to(site_root_world).t) for w in wps]
            if georef:
                item["waypointsLonLatHeight"] = [
                    (lambda g: [rnd(g[1], 9), rnd(g[0], 9), rnd(g[2], 4)])(ecef_to_geodetic(georef.to_ecef(w)))
                    for w in wps]
            paths.append(item)
    write_json(os.path.join(args.out, "camera_paths.json"), paths)

    # cues
    cues = []
    for a in actors:
        if a["class"] in CUE_CLASSES and a["props"].get("CueId") not in (None, "None"):
            p = dict(a["props"])
            item = {"label": a["label"], "export": a["export"], "class": a["class"], "cueId": p.pop("CueId"),
                    "trigger": enum_short(p.pop("Trigger")), "tags": a["tags"], "defaulted": a.get("defaulted", []),
                    "props": p, "world": a.get("world"), "site": a.get("site"),
                    "attachParent": (a.get("rootComponent") or {}).get("attachParent")}
            if a["class"] == "LFFFireworksActor":
                # OnPlayStarted() recomputes PlaySeconds from the bursts (LFFFireworksActor.cpp)
                item["effectivePlaySeconds"] = rnd(max(1.0, (len(p["BurstOffsets"]) - 1) * p["BurstInterval"]
                                                       + p["BurstSeconds"]), 6)
            # start pose (StartOffset/StartRotation are in the actor's own frame, relative to the placed end pose)
            if "world" in a:
                end = Xf(vtuple(a["world"]["location"]), tuple(a["world"]["quat"][k] for k in "xyzw"),
                         vtuple(a["world"]["scale"], (1, 1, 1)))
                start = Xf(vtuple(p["StartOffset"]), quat_from_rotator(p["StartRotation"])).then(end)
                item["startWorld"] = start.to_json()
                if site_root_world is not None:
                    item["startSite"] = start.relative_to(site_root_world).to_json()
            cues.append(item)
    cues.sort(key=lambda c: (c["cueId"], c["label"] or ""))
    write_json(os.path.join(args.out, "cues.json"), cues)

    # labels
    labels = []
    for a in actors:
        if "LFF_Label" in a["tags"]:
            tc = text_of(a)
            item = {"label": a["label"], "export": a["export"], "class": a["class"],
                    "lblTags": [t for t in a["tags"] if t.startswith("LBL_")], "tags": a["tags"],
                    "text": tc["text"] if tc else None, "worldSize": tc["worldSize"] if tc else None,
                    "textRenderColor": tc["textRenderColor"] if tc else None,
                    "horizontalAlignment": tc["horizontalAlignment"] if tc else None,
                    "verticalAlignment": tc["verticalAlignment"] if tc else None,
                    "world": (tc or a).get("world"), "site": (tc or a).get("site"),
                    "plates": [{"name": c["name"], "mesh": c.get("mesh"), "materials": c.get("materials"),
                                "relative": c.get("relative"), "world": c.get("world"), "site": c.get("site")}
                               for c in a["components"] if c["class"] == "StaticMeshComponent"]}
            if tc and georef:
                item["cesium"] = georef.cesium_pose(Xf(vtuple(tc["world"]["location"]),
                                                       tuple(tc["world"]["quat"][k] for k in "xyzw")))
            labels.append(item)
    labels.sort(key=lambda x: x["label"] or "")
    write_json(os.path.join(args.out, "labels.json"), labels)

    # places
    places = []
    for a in actors:
        pt = [t for t in a["tags"] if t.startswith("LFF_Place_")]
        if pt:
            places.append({"label": a["label"], "export": a["export"], "class": a["class"], "placeTags": pt,
                           "tags": a["tags"], "world": a.get("world"), "site": a.get("site"),
                           "cesium": a.get("cesium"),
                           "boxes": [{"name": c["name"], "boxExtent": c["boxExtent"], "world": c.get("world"),
                                      "site": c.get("site")} for c in a["components"] if "boxExtent" in c]})
    places.sort(key=lambda x: x["placeTags"][0])
    write_json(os.path.join(args.out, "places.json"), places)

    # georeference
    geo = {}
    for a in actors:
        if a["class"] == "CesiumGeoreference":
            geo["georeference"] = {"label": a["label"], "tags": a["tags"],
                                   **{k: v for k, v in a["props"].items() if k.startswith("Origin") or k in (
                                       "Ellipsoid", "Scale", "ShowLoadRadii", "SubLevelCamera")}}
            if georef:
                geo["georeference"]["originEcef"] = [rnd(c, 4) for c in georef.o]
        elif a["class"] == "Cesium3DTileset":
            geo.setdefault("tilesets", []).append({
                "label": a["label"], "export": a["export"], "IonAssetID": a["props"].get("IonAssetID"),
                "Url": a["props"].get("Url"), "TilesetSource": a["props"].get("TilesetSource", "(default) FromCesiumIon"),
                "MaximumScreenSpaceError": a["props"].get("MaximumScreenSpaceError", "(default) 16"),
                "props": a["props"], "overlays": [{"name": c["name"], "class": c["class"], "props": c.get("props")}
                                                  for c in a["components"]]})
        elif a["class"] == "CesiumCartographicPolygon":
            sp = next((c for c in a["components"] if "splinePointsWorld" in c), None)
            geo.setdefault("cartographicPolygons", []).append({
                "label": a["label"], "pointsWorld": sp and sp["splinePointsWorld"],
                "pointsSite": sp and sp.get("splinePointsSite"),
                "pointsLonLatHeight": sp and sp.get("splinePointsLonLatHeight"),
                "globeAnchor": next((c.get("props") for c in a["components"]
                                     if c["class"] == "CesiumGlobeAnchorComponent"), None)})
        elif a["class"] == "CesiumSunSky":
            geo["sunSky"] = {"label": a["label"], "props": a.get("props")}
    if site_actor:
        anchor = next((c for c in site_actor["components"] if c["class"] == "CesiumGlobeAnchorComponent"), None)
        sr = {"label": site_actor["label"], "export": site_actor["export"], "world": site_actor.get("world"),
              "rootComponent": site_actor.get("rootComponent"), "cesium": site_actor.get("cesium"),
              "globeAnchor": anchor and anchor.get("props")}
        m = (anchor or {}).get("props", {}).get("ActorToEarthCenteredEarthFixedMatrix")
        if m and georef:
            anchor_t = m[3][:3]
            computed = georef.to_ecef(site_root_world.t)
            sr["check"] = {"anchorEcefTranslation": anchor_t, "ecefFromGeoreferenceAndWorldLocation":
                           [rnd(c, 4) for c in computed],
                           "differenceMeters": rnd(math.dist(anchor_t, computed), 4)}
            g = ecef_to_geodetic(anchor_t)
            sr["anchorLonLatHeight"] = [rnd(g[1], 9), rnd(g[0], 9), rnd(g[2], 4)]
        geo["siteRoot"] = sr
    write_json(os.path.join(args.out, "georeference.json"), geo)

    # materials
    mats, meshes, unresolved = collect_materials(args.content, actors)
    write_json(os.path.join(args.out, "materials.json"), {
        "note": "Material instances under /Game/LFF/Materials plus every material referenced by the level's "
                "components (and their parents) that exists in Content/. effective* = own values over the parent "
                "chain within Content/; parameters inherited from engine/plugin parents are not available.",
        "materials": dict(sorted(mats.items())), "staticMeshes": meshes, "referencedButNotInContent": unresolved})

    print(f"{s['PackageName']}: UE4 {s['FileVersionUE4']} UE5 {s['FileVersionUE5']} ({s['SavedByEngineVersion']}); "
          f"{len(actors)} actors, {len(cams)} camera targets, {len(paths)} paths, {len(cues)} cue actors, "
          f"{len(labels)} labels, {len(places)} places, {len(mats)} materials -> {args.out}")
    if L.pr.unknown:
        print("undecoded property types:", ", ".join(sorted(L.pr.unknown)))


if __name__ == "__main__":
    sys.exit(main())
