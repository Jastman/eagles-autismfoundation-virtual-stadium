"""Builds the stadium fans' animations on the MetaHuman Crowd Sample skeleton (SKEL_MetaHumanCrowd), in /Game/LFF/Crowd.
The sample has a standing idle and a walk only (its two gesture takes are 53 s long, one on another skeleton).

  AS_LFF_SitIdle     seated: pelvis lowered, thighs forward, shins down, feet flat where they stood, hands on the thighs
  AS_LFF_SitClap     seated, clapping (it eases in from and back out to AS_LFF_SitIdle's pose)
  AS_LFF_StandCheer  standing, both arms up in a V, pumping (eases in from and out to the standing idle)
  AS_LFF_StandClap   standing, clapping in front of the chest (the same)

All four retain the source idle's breathing and sway. The last key matches the first, including unedited tracks.
ALFFCrowdFanActor plays occasional gestures between independently phased idle loops.

Runs inside the editor (execute_python_code with runpy, or Tools/Editor/run_headless.sh with LFF_POSES_WRITE=1).
Check mode (the default) prints where the joints land; write mode saves the assets.

Each pose is computed in component space from the idle's own bone transforms, so no bone axis is assumed: a bone is
turned so it points along a direction given in the character's frame (forward is the way the toes point, up is +Z),
and every bone below it follows.
"""
import math
import os

import unreal

SOURCE = "/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Stand_Idle_Loop"
OUT_DIR = "/Game/LFF/Crowd"
AL = unreal.AnimationLibrary
WRITE = os.environ.get("LFF_POSES_WRITE") == "1"
CHAINS = {"l": 1.0, "r": -1.0}  # side: +1 the character's left

# Directions in the character's frame (x: to its left, y: forward, z: up), for its left side (x flips for the right).
SIT_THIGH = (0.10, 1.0, -0.45)        # hips above the chair pan; knees lower, feet on the tread
SIT_CALF = (0.02, 0.10, -1.0)         # straight down, the foot a touch ahead of the knee
REST_UPPERARM = (0.05, 0.10, -1.0)    # hanging, a little forward
REST_LOWERARM = (0.0, 0.92, -0.38)    # forearm forward, the hand on the thigh
CLAP_UPPERARM = (0.15, 0.45, -0.88)   # elbows a little out and forward
CLAP_APART, CLAP_TOGETHER = (-0.45, 0.80, 0.40), (-0.80, 0.55, 0.25)  # forearms in toward the middle, hands meeting
CLAP_HZ = 2.0
CHEER_LOW, CHEER_HIGH = (0.55, 0.20, 0.80), (0.45, 0.10, 0.95)       # upper arms up and out in a V, pumping
CHEER_LOWERARM = (0.30, 0.15, 0.95)
CHEER_HZ = 1.0
RAMP = 0.5   # seconds to ease into a cheer and back out of it


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def between(a, b):
    """The shortest rotation taking direction a to direction b (both normalised)."""
    axis = a.cross(b)
    s = axis.length()
    d = max(-1.0, min(1.0, a.dot(b)))
    if s < 1e-6:
        if d > 0.0:
            return unreal.Quat(0.0, 0.0, 0.0, 1.0)
        axis = a.cross(vec(1.0, 0.0, 0.0))
        if axis.length() < 1e-6:
            axis = a.cross(vec(0.0, 1.0, 0.0))
        return unreal.MathLibrary.rotator_from_axis_and_angle(axis.normal(), 180.0).quaternion()
    return unreal.MathLibrary.rotator_from_axis_and_angle(axis.normal(), math.degrees(math.atan2(s, d))).quaternion()


def mix(a, b, w):
    return tuple(x + (y - x) * w for x, y in zip(a, b))


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


class Pose:
    """One frame: local transforms from the animation, component space on demand, edits in component space."""

    def __init__(self, anim, names, parents, frame):
        self.local = {n: t for n, t in zip(names, AL.get_bone_poses_for_frame(anim, names, frame, False))}
        self.parent = parents
        self.cs = {}
        self.up = vec(0.0, 0.0, 1.0)
        hips = self.comp("thigh_l").translation - self.comp("thigh_r").translation
        self.left = vec(hips.x, hips.y, 0.0).normal()
        self.fwd = self.up.cross(self.left).normal()

    def comp(self, n):
        if n not in self.cs:
            p = self.parent[n]
            self.cs[n] = self.local[n] if p is None else self.local[n] * self.comp(p)
        return self.cs[n]

    def d(self, v, side=1.0):
        return (self.left * (v[0] * side) + self.fwd * v[1] + self.up * v[2]).normal()

    def now(self, bone, toward):
        return (self.comp(toward).translation - self.comp(bone).translation).normal()

    def set_comp_rotation(self, n, q):
        """Gives bone n this component-space rotation; its children keep their local transforms (so they follow)."""
        target = unreal.Transform(self.comp(n).translation, q.rotator(), self.comp(n).scale3d)
        self.local[n] = target.make_relative(self.comp(self.parent[n]))
        self.cs = {}

    def point(self, bone, toward, direction, weight=1.0):
        """Turns bone (weight 0: not at all, 1: fully) so the line to its child joint `toward` runs along `direction`."""
        if weight <= 0.0:
            return
        here = self.now(bone, toward)
        goal = (here * (1.0 - weight) + direction * weight).normal()
        self.set_comp_rotation(bone, between(here, goal) * self.comp(bone).rotation)


def sit_legs(pose):
    """Lowers and moves back the pelvis so the shins point down to the feet where they stood, thighs forward."""
    feet = {s: pose.comp(f"foot_{s}") for s in CHAINS}
    thigh_len = (pose.comp("calf_l").translation - pose.comp("thigh_l").translation).length()
    calf_len = (pose.comp("foot_l").translation - pose.comp("calf_l").translation).length()
    t, c = pose.d(SIT_THIGH), pose.d(SIT_CALF)
    drop = pose.comp("thigh_l").translation.z - (feet["l"].translation.z - (t.z * thigh_len + c.z * calf_len))
    back = t.dot(pose.fwd) * thigh_len + c.dot(pose.fwd) * calf_len
    pelvis = pose.local["pelvis"]
    pose.local["pelvis"] = unreal.Transform(pelvis.translation + vec(0.0, 0.0, -drop) - pose.fwd * back,
                                            pelvis.rotation.rotator(), pelvis.scale3d)
    pose.cs = {}
    for s, side in CHAINS.items():
        pose.point(f"thigh_{s}", f"calf_{s}", pose.d(SIT_THIGH, side))
        pose.point(f"calf_{s}", f"foot_{s}", pose.d(SIT_CALF, side))
        pose.set_comp_rotation(f"foot_{s}", feet[s].rotation)  # flat, as it stood
    return drop, back


def rest_arms(pose, weight=1.0):
    for s, side in CHAINS.items():
        pose.point(f"upperarm_{s}", f"lowerarm_{s}", pose.d(REST_UPPERARM, side), weight)
        pose.point(f"lowerarm_{s}", f"hand_{s}", pose.d(REST_LOWERARM, side), weight)


def clap_arms(pose, t, weight):
    together = 0.5 - 0.5 * math.cos(2.0 * math.pi * CLAP_HZ * t)  # 0 apart, 1 hands together
    for s, side in CHAINS.items():
        pose.point(f"upperarm_{s}", f"lowerarm_{s}", pose.d(CLAP_UPPERARM, side), weight)
        pose.point(f"lowerarm_{s}", f"hand_{s}", pose.d(mix(CLAP_APART, CLAP_TOGETHER, together), side), weight)


def cheer_arms(pose, t, weight):
    pump = 0.5 - 0.5 * math.cos(2.0 * math.pi * CHEER_HZ * t)
    for s, side in CHAINS.items():
        pose.point(f"upperarm_{s}", f"lowerarm_{s}", pose.d(mix(CHEER_LOW, CHEER_HIGH, pump), side), weight)
        pose.point(f"lowerarm_{s}", f"hand_{s}", pose.d(CHEER_LOWERARM, side), weight)


def ramp(t, length):
    """0 at both ends of the clip, 1 in the middle: the cheer grows out of the idle and settles back into it."""
    return smooth(t / RAMP) * smooth((length - t) / RAMP)


def sit_idle(pose, t, length):
    sit_legs(pose)
    rest_arms(pose)


def sit_clap(pose, t, length):
    sit_legs(pose)
    rest_arms(pose)
    clap_arms(pose, t, ramp(t, length))


def stand_cheer(pose, t, length):
    cheer_arms(pose, t, ramp(t, length))


def stand_clap(pose, t, length):
    clap_arms(pose, t, ramp(t, length))


CLIPS = {"AS_LFF_SitIdle": sit_idle, "AS_LFF_SitClap": sit_clap, "AS_LFF_StandCheer": stand_cheer, "AS_LFF_StandClap": stand_clap}
EDITED = ["pelvis"] + [f"{b}_{s}" for s in CHAINS for b in ("thigh", "calf", "foot", "upperarm", "lowerarm")]
REPORT = ("pelvis", "thigh_l", "calf_l", "foot_l", "lowerarm_l", "hand_l", "hand_r")


def main():
    src = unreal.load_asset(SOURCE)
    if not isinstance(src, unreal.AnimSequence):
        raise RuntimeError(f"Source animation is missing: {SOURCE}")
    names = [str(n) for n in AL.get_animation_track_names(src)]
    frames = AL.get_num_frames(src)
    key_count = src.get_editor_property("number_of_sampled_keys")
    length = AL.get_sequence_length(src)
    required = set(EDITED) | {"ball_l", "thigh_r", "hand_l", "hand_r"}
    if required - set(names) or frames < 2 or key_count != frames + 1 or length <= 0.0:
        raise RuntimeError(f"Unsupported source tracks/sampling: missing={required - set(names)}, frames={frames}, keys={key_count}, length={length}")
    if WRITE:
        existing = [f"{OUT_DIR}/{clip}" for clip in CLIPS if unreal.EditorAssetLibrary.does_asset_exist(f"{OUT_DIR}/{clip}")]
        if existing:
            raise RuntimeError(f"Refusing to overwrite animation assets. Preserve them before an intentional rebuild: {existing}")
    parents = {}
    for n in names:
        path = [str(p) for p in AL.find_bone_path_to_root(src, n)]
        parents[n] = path[1] if len(path) > 1 else None

    for clip, build in CLIPS.items():
        keys = {n: ([], [], []) for n in names}
        checks = (0, frames // 2 + 7, frames)
        first = None
        for f in (range(key_count) if WRITE else checks):
            pose = Pose(src, names, parents, f)
            build(pose, f * length / frames, length)
            if f == 0:
                first = dict(pose.local)
            elif f == frames:
                pose.local = dict(first)
                pose.cs = {}
            if f in checks:
                where = "  ".join(f"{n} ({pose.comp(n).translation.x:.0f},{pose.comp(n).translation.y:.0f},{pose.comp(n).translation.z:.0f})"
                                  for n in REPORT)
                print(f"{clip} frame {f}: {where}")
            for n in names:
                tr = pose.local[n]
                keys[n][0].append(tr.translation)
                keys[n][1].append(tr.rotation)
                keys[n][2].append(tr.scale3d)
        if not WRITE:
            continue
        path = f"{OUT_DIR}/{clip}"
        seq = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, path)
        if not isinstance(seq, unreal.AnimSequence):
            raise RuntimeError(f"Animation duplication failed: {path}")
        ctrl = seq.controller
        ctrl.open_bracket(unreal.Text(f"LFF {clip}"), True)
        try:
            for n in names:
                if not ctrl.set_bone_track_keys(n, keys[n][0], keys[n][1], keys[n][2], True):
                    raise RuntimeError(f"Failed to key {path}: {n}")
        finally:
            ctrl.close_bracket(True)
        if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
            raise RuntimeError(f"Animation save failed: {path}")
        print(f"saved {path}: {key_count} keys, {length:g} seconds, closed loop")
    if not WRITE:
        print("check only (LFF_POSES_WRITE=1 saves)")


main()
