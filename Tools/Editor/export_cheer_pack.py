"""Exports the Free Crowd Cheering Sounds pack (/Game/Crowd_Cheering/WAV, from Fab) to Saved/CheerExport/*.wav, so
Tools/Audio/make_cheer_bed.py can cut the seat bed from it. Run with the editor closed:
  Tools/Editor/run_headless.sh Tools/Editor/export_cheer_pack.py "LFF cheer"
"""
import os
import unreal
out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "CheerExport")
try:
    for path in unreal.EditorAssetLibrary.list_assets("/Game/Crowd_Cheering/WAV", recursive=False):
        wave = unreal.load_asset(path.split(".")[0])
        task = unreal.AssetExportTask()
        task.set_editor_property("object", wave)
        task.set_editor_property("filename", os.path.join(out, wave.get_name() + ".wav"))
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("prompt", False)
        ok = unreal.Exporter.run_asset_export_task(task)
        unreal.log(f"[LFF cheer] {wave.get_name()}: {ok}, {wave.get_editor_property('duration'):.1f} s, looping={wave.get_editor_property('looping')}")
    unreal.log("[LFF cheer] DONE")
except Exception as e:  # noqa: BLE001
    unreal.log_error(f"[LFF cheer] FAILED: {e}")
