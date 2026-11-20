"""List what an imported asset pack contains, with sizes, so scenes can be dressed with the right pieces.

    tools/ue_py.sh catalog_pack tools/ue/catalog_pack.py      (MATSURI_PACK=/Game/Mountain_Shrine by default)

Writes Saved/pack_catalog.json: every static mesh (bounds in cm, LOD0 triangles, Nanite), Niagara system,
foliage type, material and map under the pack folder.
"""
import json
import os

import unreal

PACK = os.environ.get("MATSURI_PACK", "/Game/Mountain_Shrine")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "pack_catalog.json")


def log(msg):
    unreal.log_warning(f"[matsuri-catalog] {msg}")


def mesh_entry(path, mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    nanite = mesh.get_editor_property("nanite_settings").get_editor_property("enabled")
    try:
        triangles = mesh.get_num_triangles(0)
    except Exception:
        triangles = None
    return {"path": path, "size_cm": [round(size.x), round(size.y), round(size.z)],
            "min_z": round(box.min.z), "triangles_lod0": triangles, "nanite": bool(nanite),
            "materials": len(mesh.get_editor_property("static_materials"))}


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(PACK, recursive=True)
    catalog = {"meshes": [], "niagara": [], "foliage": [], "materials": [], "maps": [], "other": {}}
    for data in assets:
        cls = str(data.asset_class_path.asset_name)
        path = str(data.package_name)
        if cls == "StaticMesh":
            mesh = unreal.load_asset(path)
            if mesh:
                catalog["meshes"].append(mesh_entry(path, mesh))
        elif cls == "NiagaraSystem":
            catalog["niagara"].append(path)
        elif "Foliage" in cls:
            catalog["foliage"].append({"path": path, "class": cls})
        elif cls in ("Material", "MaterialInstanceConstant"):
            catalog["materials"].append(path)
        elif cls == "World":
            catalog["maps"].append(path)
        else:
            catalog["other"][cls] = catalog["other"].get(cls, 0) + 1
    catalog["meshes"].sort(key=lambda m: m["path"])
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=1)
    log(f"{len(catalog['meshes'])} meshes, {len(catalog['niagara'])} niagara, {len(catalog['foliage'])} foliage, "
        f"{len(catalog['materials'])} materials, {len(catalog['maps'])} maps -> {OUT}")


main()
