"""Dump a map's actors (meshes with transforms, lights, fog, sky, post process) so its look can be studied
and rebuilt in code.

    MATSURI_MAP=/Game/Mountain_Shrine/Maps/Final_Assembly tools/ue_py.sh dump_map tools/ue/dump_pack_map.py

Writes Saved/map_dump_<MapName>.json.
"""
import json
import os

import unreal

MAP = os.environ.get("MATSURI_MAP", "/Game/Mountain_Shrine/Maps/Final_Assembly")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   f"map_dump_{MAP.rsplit('/', 1)[-1]}.json")

LIGHT_PROPS = ["intensity", "light_color", "temperature", "use_temperature", "source_angle", "indirect_lighting_intensity",
               "volumetric_scattering_intensity", "atmosphere_sun_light", "attenuation_radius", "cast_shadows",
               "source_type", "cubemap", "sky_distance_threshold", "real_time_capture", "lower_hemisphere_is_black"]
FOG_PROPS = ["fog_density", "fog_height_falloff", "fog_inscattering_luminance", "fog_max_opacity", "start_distance",
             "volumetric_fog", "volumetric_fog_scattering_distribution", "volumetric_fog_albedo",
             "volumetric_fog_extinction_scale", "volumetric_fog_distance", "directional_inscattering_luminance",
             "directional_inscattering_exponent", "sky_atmosphere_ambient_contribution_color_scale", "second_fog_data"]


def log(msg):
    unreal.log_warning(f"[matsuri-dump] {msg}")


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def rot(r):
    return [round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1)]


def value(v):
    if isinstance(v, (bool, int, float, str)) or v is None:
        return v
    if isinstance(v, unreal.LinearColor):
        return [round(v.r, 3), round(v.g, 3), round(v.b, 3), round(v.a, 3)]
    if isinstance(v, unreal.Color):
        return [v.r, v.g, v.b, v.a]
    if isinstance(v, unreal.Object):
        return v.get_path_name()
    return str(v)


def props(obj, names):
    out = {}
    for name in names:
        try:
            out[name] = value(obj.get_editor_property(name))
        except Exception:
            pass
    return out


def post_process(settings):
    """Only the overridden post-process fields (those with their override_ flag set)."""
    out = {}
    for attr in dir(settings):
        if not attr.startswith("override_"):
            continue
        try:
            if settings.get_editor_property(attr):
                field = attr[len("override_"):]
                out[field] = value(settings.get_editor_property(field))
        except Exception:
            pass
    return out


def main():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.load_level(MAP):
        log(f"could not load {MAP}")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:  # World Partition maps: load every actor, not only the editor's loading region.
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
        log(f"world partition: loaded {len(descs)} actor descs")
    except Exception as error:
        log(f"world partition load skipped: {error}")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    dump = {"map": MAP, "meshes": [], "lights": [], "fog": [], "post": [], "foliage": {}, "other": {}}
    for actor in actors:
        cls = actor.get_class().get_name()
        t = actor.get_actor_transform()
        where = {"label": actor.get_actor_label(), "loc": vec(t.translation), "rot": rot(t.rotation.rotator()),
                 "scale": vec(t.scale3d)}
        if isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.static_mesh
            where["mesh"] = mesh.get_path_name() if mesh else None
            overrides = actor.static_mesh_component.get_editor_property("override_materials")
            if overrides:
                where["materials"] = [value(m) for m in overrides]
            dump["meshes"].append(where)
        elif isinstance(actor, unreal.InstancedFoliageActor):
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                mesh = comp.static_mesh.get_path_name() if comp.static_mesh else "?"
                dump["foliage"][mesh] = dump["foliage"].get(mesh, 0) + comp.get_instance_count()
        elif isinstance(actor, unreal.Light) or isinstance(actor, unreal.SkyLight):
            comp = actor.get_component_by_class(unreal.LightComponentBase)
            where["class"] = cls
            where.update(props(comp, LIGHT_PROPS))
            dump["lights"].append(where)
        elif isinstance(actor, unreal.ExponentialHeightFog):
            where.update(props(actor.get_component_by_class(unreal.ExponentialHeightFogComponent), FOG_PROPS))
            dump["fog"].append(where)
        elif isinstance(actor, unreal.PostProcessVolume):
            where["unbound"] = actor.get_editor_property("unbound")
            where["settings"] = post_process(actor.get_editor_property("settings"))
            dump["post"].append(where)
        else:
            entry = dump["other"].setdefault(cls, [])
            if len(entry) < 40:
                comps = [c.get_class().get_name() for c in actor.get_components_by_class(unreal.ActorComponent)]
                where["components"] = comps
                for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                    if comp.static_mesh:
                        where.setdefault("meshes", []).append(comp.static_mesh.get_path_name())
                entry.append(where)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(dump, f, indent=1)
    log(f"{len(actors)} actors, {len(dump['meshes'])} mesh actors -> {OUT}")


main()
