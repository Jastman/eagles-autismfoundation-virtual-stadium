"""Sets up the cinematic lighting and post processing in L_LFF_Stadium. Idempotent: re-run to retune.

Run headless with the editor closed (see Docs/LOCAL_AGENT_SETUP.md §5):
  UnrealEditor VirtualStadium.uproject -ExecutePythonScript="<abs path to this file>" -ModelContextProtocolPort=18000 \
      -unattended -nullrhi -nopause -nosplash -stdout

"Cinematic" within ACCESSIBILITY.md: physically based sun and sky, Lumen GI and reflections at high quality, soft
aerial perspective and a filmic tone curve - but no motion blur, lens flares, film grain, chromatic aberration or
depth of field, and slow, gentle exposure adaptation. Nothing here animates or flashes.
Project-wide renderer settings (Lumen, virtual shadow maps, distance fields) live in Config/DefaultEngine.ini.
"""
import unreal

LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
# Oct 18, 2026 (a Sunday): the Eagles home game closest to the Oct 9 demo (user direction). 1:00 pm local clock time,
# which is EDT that day. CesiumSunSky takes the STANDARD offset (EST, -5) and applies daylight saving itself; the
# earlier -4 double-counted DST and put the sun an hour late. Verified: elevation 40.3 deg, azimuth 184.6 deg.
SUN_DATE = (2026, 10, 18)
SUN_TIME_HOURS = 13.0
SUN_TIME_ZONE_STANDARD = -5.0
# Partly cloudy: static cumulus layer, ~40% of the sky. The engine's cloud material has wind drift and a storm mode
# with lightning; both are zeroed so nothing in the sky moves or flashes (ACCESSIBILITY.md, TEST_PLAN T2 row 16).
CLOUD_MATERIAL = "/Game/LFF/Sky/MI_LFF_Clouds_PartlyCloudy"
CLOUD_COVERAGE = -0.08         # engine scale: -0.2 scattered, 0.0 mostly cloudy, +0.15 near overcast
CLOUD_LAYER_KM = (1.5, 3.5)    # bottom altitude, thickness
EXPOSURE_EV100 = (7.0, 15.0)   # shaded concourse ... full sun; auto exposure stays inside this range
POST = {
    # Exposure: histogram auto exposure, slow and gentle so light changes between steps never feel sudden.
    "auto_exposure_method": unreal.AutoExposureMethod.AEM_HISTOGRAM,
    "auto_exposure_min_brightness": EXPOSURE_EV100[0],
    "auto_exposure_max_brightness": EXPOSURE_EV100[1],
    "auto_exposure_speed_up": 1.0,
    "auto_exposure_speed_down": 0.6,
    "auto_exposure_bias": 0.3,
    # Lumen at cinematic quality (the M4 Max handles this; lower via scalability on weaker machines).
    "lumen_scene_lighting_quality": 2.0,
    "lumen_scene_detail": 2.0,
    "lumen_final_gather_quality": 2.0,
    "lumen_reflection_quality": 2.0,
    "lumen_max_trace_distance": 50000.0,
    # Filmic look, restrained.
    "bloom_intensity": 0.35,
    "vignette_intensity": 0.2,
    # Comfort: all off.
    "motion_blur_amount": 0.0,
    "lens_flare_intensity": 0.0,
    "film_grain_intensity": 0.0,
    "scene_fringe_intensity": 0.0,
    "depth_of_field_focal_distance": 0.0,
}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log(f"[LFF look] {msg}")


def find_or_spawn(cls, label, folder="Lighting"):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label and isinstance(a, cls):
            return a
    a = eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def run():
    if not les.load_level(LEVEL):
        raise RuntimeError("load_level failed")

    ppv = find_or_spawn(unreal.PostProcessVolume, "PPV_LFF_Cinematic")
    ppv.set_editor_property("unbound", True)
    ppv.set_editor_property("priority", 1.0)
    s = ppv.get_editor_property("settings")
    for k, v in POST.items():
        try:
            s.set_editor_property(f"override_{k}", True)
            s.set_editor_property(k, v)
        except Exception as e:
            unreal.log_warning(f"[LFF look] post setting {k}: {e}")
    ppv.set_editor_property("settings", s)

    fog = find_or_spawn(unreal.ExponentialHeightFog, "Fog_LFF_Aerial")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for k, v in (("fog_density", 0.0035), ("fog_height_falloff", 0.15), ("start_distance", 30000.0),
                 ("enable_volumetric_fog", False), ("directional_inscattering_exponent", 8.0),
                 ("directional_inscattering_start_distance", 20000.0)):
        try:
            fc.set_editor_property(k, v)
        except Exception as e:
            unreal.log_warning(f"[LFF look] fog {k}: {e}")

    # Clouds.
    if unreal.EditorAssetLibrary.does_asset_exist(CLOUD_MATERIAL):
        cmi = unreal.load_asset(CLOUD_MATERIAL)
    else:
        folder, name = CLOUD_MATERIAL.rsplit("/", 1)
        cmi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        cmi.set_editor_property("parent", unreal.load_asset("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"))
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_scalar_parameter_value(cmi, "Cloud_GlobalCoverage", CLOUD_COVERAGE)
    mel.set_material_instance_scalar_parameter_value(cmi, "StormClouds", 0.0)
    mel.set_material_instance_vector_parameter_value(cmi, "Layout_WindControls", unreal.LinearColor(1.0, 1.0, 0.0, 0.0))
    unreal.EditorAssetLibrary.save_loaded_asset(cmi)
    clouds = find_or_spawn(unreal.VolumetricCloud, "Clouds_LFF_PartlyCloudy")
    vc = clouds.get_component_by_class(unreal.VolumetricCloudComponent)
    vc.set_editor_property("layer_bottom_altitude", CLOUD_LAYER_KM[0])
    vc.set_editor_property("layer_height", CLOUD_LAYER_KM[1])
    vc.set_editor_property("material", cmi)
    # Half the raymarch samples: the layer is static and distant and temporal reconstruction hides the difference;
    # saves ~1 ms so camera moves hold 60 fps (Docs/PERF_BASELINE.md).
    vc.set_editor_property("view_sample_count_scale", 0.5)
    vc.set_editor_property("reflection_view_sample_count_scale_value", 0.5)

    sun = next((a for a in eas.get_all_level_actors() if isinstance(a, unreal.CesiumSunSky)), None)
    if sun:
        for k, v in (("time_zone", SUN_TIME_ZONE_STANDARD), ("use_daylight_saving_time", True), ("solar_time", SUN_TIME_HOURS),
                     ("year", SUN_DATE[0]), ("month", SUN_DATE[1]), ("day", SUN_DATE[2])):
            try:
                sun.set_editor_property(k, v)
            except Exception as e:
                unreal.log_warning(f"[LFF look] sun {k}: {e}")
        try:
            sun.update_sun()
        except Exception:
            pass
        # No cloud shadows on the ground: on the M4 Max they cost ~2.4 ms (cloud shadow map) plus ~1.5 ms of
        # virtual shadow map invalidation, pushing the intro view below 60 fps (Docs/PERF_BASELINE.md).
        for dl in sun.get_components_by_class(unreal.DirectionalLightComponent):
            try:
                dl.set_editor_property("cast_cloud_shadows", False)
            except Exception as e:
                unreal.log_warning(f"[LFF look] cloud shadows: {e}")
        # Sky light recaptures in real time so Lumen's sky lighting follows the sun.
        for sl in sun.get_components_by_class(unreal.SkyLightComponent):
            try:
                sl.set_editor_property("real_time_capture", True)
            except Exception as e:
                unreal.log_warning(f"[LFF look] skylight: {e}")
    if not les.save_current_level():
        raise RuntimeError("save_current_level failed")
    log(f"DONE: PPV_LFF_Cinematic, Fog_LFF_Aerial, Clouds_LFF_PartlyCloudy (coverage {CLOUD_COVERAGE}), "
        f"sun {SUN_DATE} {SUN_TIME_HOURS}h (UTC{SUN_TIME_ZONE_STANDARD:+.0f} + DST)")


try:
    run()
except Exception as e:
    import traceback
    unreal.log_error(f"[LFF look] FAILED: {e}\n{traceback.format_exc()}")
finally:
    unreal.SystemLibrary.quit_editor()
