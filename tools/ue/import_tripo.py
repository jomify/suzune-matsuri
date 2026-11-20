"""Import the normalised Tripo assets into SuzuneMatsuri (UE 5.8, Interchange glTF).

    tools/ue_py.sh import_tripo tools/ue/import_tripo.py

  Static props   SourceArt/Tripo/<name>.glb                 -> /Game/Matsuri/Tripo/Meshes/<name>
  Heroines       tools/tripo/out/heroes/<hero>/<hero>_<anim>.glb
                                                            -> /Game/Matsuri/Tripo/Heroes/<hero>_<Anim>  (skeletal mesh)
                                                               /Game/Matsuri/Tripo/Heroes/A_<hero>_<Anim> (animation)

Each import lands in its own folder under /Game/Matsuri/Tripo/Parts (materials, textures, skeleton,
physics asset - keep them, the moved assets reference them); the wanted asset is then moved to its
fixed name so game code can load it by path. Meshes are visual only (gameplay keeps its own collision blocks).
Assets already in place are skipped, so the script can be re-run as new art arrives; set
MATSURI_REIMPORT=1 to replace them.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PROPS_DIR = os.path.join(PROJECT, "SourceArt", "Tripo")
HEROES_DIR = os.path.normpath(os.path.join(PROJECT, "..", "tools", "tripo", "out", "heroes"))
ROOT = "/Game/Matsuri/Tripo"
EAL = unreal.EditorAssetLibrary
REIMPORT = os.environ.get("MATSURI_REIMPORT") == "1"
MEL = unreal.MaterialEditingLibrary

# Paper lanterns glow warm through their own texture (emissive = base colour x a warm factor). The glTF
# material adds emissive in absolute terms and the night floors raise exposure, so the factors are small
# and coloured: a grey factor turned the paper white. Pickups get none (their texture reads as gold/jade).
GLOW = {
    "SM_Prop_Chochin": (0.30, 0.13, 0.04),
    "SM_Yokai_Chochin": (0.20, 0.08, 0.03),
    "SM_Pickup_Koban": (0.0, 0.0, 0.0),
    "SM_Pickup_Ginka": (0.0, 0.0, 0.0),
    "SM_Pickup_Magatama": (0.0, 0.03, 0.015),
    "SM_Pickup_Ofuda": (0.0, 0.0, 0.0),
}


def log(msg):
    unreal.log_warning(f"[matsuri-tripo] {msg}")


def wanted(target):
    """False when the asset is already imported (and no re-import was asked for)."""
    if EAL.does_asset_exist(target) and not REIMPORT:
        log(f"skip {target.rsplit('/', 1)[-1]} (already imported)")
        return False
    return True


def import_file(path, staging):
    if EAL.does_directory_exist(staging):
        EAL.delete_directory(staging)  # a re-import starts clean
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", staging)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return [unreal.load_asset(p) for p in EAL.list_assets(staging, recursive=True)]


def move(asset, target):
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    if not EAL.rename_asset(asset.get_path_name().split(".")[0], target):
        raise RuntimeError(f"could not move {asset.get_name()} -> {target}")
    return unreal.load_asset(target)


def import_prop(name):
    if not wanted(f"{ROOT}/Meshes/{name}"):
        return
    assets = import_file(os.path.join(PROPS_DIR, f"{name}.glb"), f"{ROOT}/Parts/{name}")
    meshes = [a for a in assets if isinstance(a, unreal.StaticMesh)]
    if not meshes:
        raise RuntimeError(f"{name}: no static mesh")
    mesh = move(meshes[0], f"{ROOT}/Meshes/{name}")
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", False)  # low-poly props; the glTF materials lack the Nanite usage flag
        mesh.set_editor_property("nanite_settings", nanite)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    box = mesh.get_bounding_box()
    log(f"{name}: tris={mesh.get_num_triangles(0)} size_cm={box.max - box.min}")
    EAL.save_loaded_asset(mesh)


def apply_glow(name, factor):
    mesh = unreal.load_asset(f"{ROOT}/Meshes/{name}")
    if not mesh:
        return
    for slot in mesh.get_editor_property("static_materials"):
        material = slot.get_editor_property("material_interface")
        if not isinstance(material, unreal.MaterialInstanceConstant):
            continue
        base = MEL.get_material_instance_texture_parameter_value(material, "BaseColorTexture")
        if not base:
            continue
        MEL.set_material_instance_texture_parameter_value(material, "EmissiveTexture", base)
        MEL.set_material_instance_vector_parameter_value(material, "EmissiveFactor", unreal.LinearColor(*factor, 1.0))
        MEL.update_material_instance(material)
        EAL.save_loaded_asset(material)
    log(f"glow {name} {factor}")


def disable_nanite_everywhere():
    """Meshes imported before Nanite was switched off in import_prop (idempotent)."""
    for path in EAL.list_assets(f"{ROOT}/Meshes", recursive=False):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        nanite = mesh.get_editor_property("nanite_settings")
        if nanite.get_editor_property("enabled"):
            nanite.set_editor_property("enabled", False)
            mesh.set_editor_property("nanite_settings", nanite)
            EAL.save_loaded_asset(mesh)
            log(f"nanite off {mesh.get_name()}")


def import_hero_clip(hero, anim, path):
    tag = anim.capitalize()
    if not wanted(f"{ROOT}/Heroes/{hero}_{tag}"):
        return
    assets = import_file(path, f"{ROOT}/Parts/{hero}_{tag}")
    skeletal = [a for a in assets if isinstance(a, unreal.SkeletalMesh)]
    clips = [a for a in assets if isinstance(a, unreal.AnimSequence)]
    if not skeletal:
        raise RuntimeError(f"{hero} {anim}: no skeletal mesh")
    mesh = move(skeletal[0], f"{ROOT}/Heroes/{hero}_{tag}")
    if clips:
        move(clips[0], f"{ROOT}/Heroes/A_{hero}_{tag}")
    box = mesh.get_bounds().box_extent * 2
    log(f"{hero} {anim}: skeletal size_cm={box} clips={len(clips)}")


def import_textures():
    """Tiling surface textures from tools/tripo/make_tileable.py -> /Game/Matsuri/Tripo/Textures/<name>."""
    folder = os.path.join(PROPS_DIR, "Textures")
    if not os.path.isdir(folder):
        return
    for file in sorted(f for f in os.listdir(folder) if f.lower().endswith(".png")):
        name = file[:-4]
        if not wanted(f"{ROOT}/Textures/{name}"):
            continue
        assets = import_file(os.path.join(folder, file), f"{ROOT}/Parts/{name}")
        textures = [a for a in assets if isinstance(a, unreal.Texture2D)]
        if textures:
            texture = move(textures[0], f"{ROOT}/Textures/{name}")
            texture.set_editor_property("srgb", True)
            EAL.save_loaded_asset(texture)
            log(f"texture {name}")


def import_portraits():
    folder = os.path.join(PROPS_DIR, "Portraits")
    if not os.path.isdir(folder):
        return
    for file in sorted(f for f in os.listdir(folder) if f.lower().endswith(".png")):
        name = file[:-4]
        if not wanted(f"{ROOT}/Heroes/{name}"):
            continue
        assets = import_file(os.path.join(folder, file), f"{ROOT}/Parts/{name}")
        textures = [a for a in assets if isinstance(a, unreal.Texture2D)]
        if textures:
            texture = move(textures[0], f"{ROOT}/Heroes/{name}")
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("never_stream", True)
            EAL.save_loaded_asset(texture)
            log(f"portrait {name}")


def main():
    import_portraits()
    import_textures()
    if os.path.isdir(PROPS_DIR):
        for name in sorted(f[:-4] for f in os.listdir(PROPS_DIR) if f.lower().endswith(".glb")):
            try:
                import_prop(name)
            except Exception as exc:  # keep going: one bad model must not block the rest
                log(f"FAILED {name}: {exc}")
    for name, factor in GLOW.items():
        apply_glow(name, factor)
    disable_nanite_everywhere()
    if os.path.isdir(HEROES_DIR):
        for hero in sorted(os.listdir(HEROES_DIR)):
            folder = os.path.join(HEROES_DIR, hero)
            for anim in ("idle", "run", "jump"):
                path = os.path.join(folder, f"{hero}_{anim}.glb")
                if os.path.exists(path):
                    try:
                        import_hero_clip(hero, anim, path)
                    except Exception as exc:
                        log(f"FAILED {hero} {anim}: {exc}")
    log("done")


main()
