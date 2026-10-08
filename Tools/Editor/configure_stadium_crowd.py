"""Configure only the existing bowl spawner, after compiling native crowd code and generating the four clips.

Run inside the editor:
  import runpy; runpy.run_path(<project>/Tools/Editor/configure_stadium_crowd.py, run_name="__main__")

Does not rebuild stand-ins or touch sensory-room/trailer actors. Saves the current stadium map deliberately.
"""
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).parent))
from build_lff_standins import (  # noqa: E402
    MH, MH_PEOPLE, MH_IDLE, MH_TOP, MH_JEANS, MH_SHOES, PEOPLE_DIR,
    TOP_INFLATE, JEANS_INFLATE, SHOES_INFLATE,
)


def asset(path):
    result = unreal.load_asset(path)
    if result is None:
        raise RuntimeError(f"Required crowd asset is missing: {path}")
    return result


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if editor.get_game_world() is not None or world is None or not world.get_path_name().startswith("/Game/LFF/Levels/L_LFF_Stadium."):
        raise RuntimeError("Open L_LFF_Stadium and stop PIE before configuring the crowd")
    spawners = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                if isinstance(a, unreal.LFFStadiumCrowdSpawner)]
    if len(spawners) != 1:
        raise RuntimeError(f"Expected one bowl crowd spawner, found {len(spawners)}")
    clips = {name: asset("/Game/LFF/Crowd/AS_LFF_" + name) for name in ("SitIdle", "SitClap", "StandCheer", "StandClap")}
    idle = asset(MH_IDLE)
    looks = []
    for index, (name, sex) in enumerate(MH_PEOPLE):
        base = f"{MH}/Characters/Assembled/{name}"
        look = unreal.LFFCrowdAppearance()
        look.set_editor_property("body_mesh", asset(f"{base}/Body/SKM_MHC_{name}_BodyMesh"))
        look.set_editor_property("face_mesh", asset(f"{base}/Face/SKM_MHC_{name}_FaceMesh"))
        pieces = []
        for path, inflate, material in (
            (MH_TOP.format(sex), TOP_INFLATE, asset(f"{PEOPLE_DIR}/MI_LFF_Tee_{('Green', 'White', 'Black')[index % 3]}")),
            (MH_JEANS.format(sex), JEANS_INFLATE, None),
            (MH_SHOES.format(sex), SHOES_INFLATE, None),
        ):
            piece = unreal.LFFOutfitPiece()
            piece.set_editor_property("cloth", asset(path))
            piece.set_editor_property("inflate", inflate)
            piece.set_editor_property("material", material)
            pieces.append(piece)
        look.set_editor_property("outfit", pieces)
        looks.append(look)
    spawner = spawners[0]
    generator = spawner.get_editor_property("seat_generator")
    if not isinstance(generator, unreal.LFFSeatSpawnDataGenerator):
        raise RuntimeError("Bowl spawner has no project seat generator")
    generators = spawner.get_editor_property("spawn_data_generators")
    if len(generators) != 1 or generators[0].get_editor_property("generator_instance") not in (None, generator):
        raise RuntimeError("Unexpected Mass generator configuration; not replacing it")
    spawner.modify()
    generator.modify()
    for prop, value in (
        ("front_offset", 70.0), ("clear_radius", 50.0), ("clear_ahead", 0.0),
        ("detailed_radius", 700.0), ("max_detailed_fans", 48),
        ("treads_file", "LFF/Crowd/seat_treads.json"),
    ):
        generator.set_editor_property(prop, value)
    for prop, value in (
        ("detailed_looks", looks), ("seated_idle", clips["SitIdle"]), ("seated_clap", clips["SitClap"]),
        ("standing_idle", idle), ("standing_cheer", clips["StandCheer"]), ("standing_clap", clips["StandClap"]),
    ):
        spawner.set_editor_property(prop, value)
    generators[0].set_editor_property("generator_instance", generator)
    spawner.set_editor_property("spawn_data_generators", generators)
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save crowd configuration in the stadium map")
    print(f"Saved {spawner.get_actor_label()}: {len(looks)} looks, measured floors, 48-fan cap, 50 cm visitor clearance, no forward gap")


if __name__ == "__main__":
    main()
