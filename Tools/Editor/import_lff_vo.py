"""Imports the voiceover lines (SourceData/Audio/VO/VO_*.wav) into /Game/LFF/Audio/VO/. Safe to rerun; it replaces them.

Each journey step's VoiceWave (SourceData/Journey/LFF_GoldenPath.json) names its line, e.g.
/Game/LFF/Audio/VO/VO_02_Intro. Lines play once (never loop) at ALFFGameMode::VoiceVolume, a little under the bed,
which ducks under the voice. A step whose file is missing is reported here and simply has no voice at runtime.

Drop the recorded files (VO_MANIFEST.md) into SourceData/Audio/VO/ and run, with the editor closed:
  Tools/Editor/run_headless.sh Tools/Editor/import_lff_vo.py "LFF vo"
Tools/Editor/import_lff_content.py runs the same import as part of the full content import.
"""
import glob
import json
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
VO_SRC = os.path.join(PROJECT_DIR, "SourceData", "Audio", "VO")
VO_DIR = "/Game/LFF/Audio/VO"
JOURNEY_JSON = os.path.join(PROJECT_DIR, "SourceData", "Journey", "LFF_GoldenPath.json")


def import_vo(log):
    files = sorted(glob.glob(os.path.join(VO_SRC, "VO_*.wav")))
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        for prop, value in (("filename", path), ("destination_path", VO_DIR),
                            ("destination_name", os.path.splitext(os.path.basename(path))[0]),
                            ("automated", True), ("save", False), ("replace_existing", True)):
            task.set_editor_property(prop, value)
        tasks.append(task)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported = set()
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        wave = unreal.load_asset(f"{VO_DIR}/{name}")
        if not isinstance(wave, unreal.SoundWave):
            log(f"WARNING {name} did not import as a SoundWave")
            continue
        wave.set_editor_property("looping", False)
        unreal.EditorAssetLibrary.save_loaded_asset(wave)
        imported.add(name)

    with open(JOURNEY_JSON, encoding="utf-8") as f:
        rows = json.load(f)
    wanted = [(r["Name"], r.get("VoiceWave", "").rsplit("/", 1)[-1].split(".", 1)[0]) for r in rows]
    missing = [f"{step} ({name})" for step, name in wanted if name and name not in imported]
    log(f"voice lines imported: {len(imported)} of {len([w for _, w in wanted if w])} steps"
        + (f"; missing, so those steps have no voice: {', '.join(missing)}" if missing else "; none missing"))
    return imported, missing


if __name__ == "__main__":
    try:
        import_vo(lambda msg: unreal.log(f"[LFF vo] {msg}"))
        unreal.log("[LFF vo] DONE")
    except Exception as e:  # noqa: BLE001 - report and let run_headless.sh stop the editor
        unreal.log_error(f"[LFF vo] FAILED: {e}")
