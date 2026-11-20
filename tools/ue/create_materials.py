"""Create the small set of materials the code-built world uses.

Run inside the editor's Python (commandlet is fine):
    UnrealEditor-Cmd.exe SuzuneMatsuri.uproject -run=pythonscript -script=tools/ue/create_materials.py

M_MatsuriGlow: lit base colour + emissive = Color * Glow. Used for coins, shots, lanterns, foxfire
(Matsuri::Graybox::Paint with bGlow). Parameters: Color (vector), Glow (scalar, default 6).

M_MatsuriSurface: a tiling texture projected by world position on each axis (the face's normal picks the
plane), so walls, crates and floors of any size share one texel density. Base colour =
Albedo * Tint * Brightness. Parameters: Albedo (texture), Tint (vector), Brightness (scalar, 1.4),
TileSize (scalar, cm per repeat, 300). Used by Matsuri::Graybox::PaintSurface.
"""
import unreal

FOLDER = "/Game/Matsuri/Materials"
GLOW_NAME = "M_MatsuriGlow"
SURFACE_NAME = "M_MatsuriSurface"
DEFAULT_ALBEDO = "/Engine/EngineMaterials/DefaultWhiteGrid"

mel = unreal.MaterialEditingLibrary


def create_glow() -> None:
    path = f"{FOLDER}/{GLOW_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"[create_materials] {path} already exists, skipped")
        return

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(GLOW_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"could not create {path}")

    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.8, 0.3, 1.0))

    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 200)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 6.0)

    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 150)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(glow, "", emissive, "B")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)

    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {path}")
    unreal.log(f"[create_materials] created {path}")


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def create_surface() -> None:
    path = f"{FOLDER}/{SURFACE_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"[create_materials] {path} already exists, skipped")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(SURFACE_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"could not create {path}")

    albedo = expression(material, unreal.MaterialExpressionTextureObjectParameter, -1400, 0,
                        parameter_name="Albedo", texture=unreal.load_asset(DEFAULT_ALBEDO))
    tile = expression(material, unreal.MaterialExpressionScalarParameter, -1600, -300, parameter_name="TileSize", default_value=300.0)
    world = expression(material, unreal.MaterialExpressionWorldPosition, -1600, -450)
    scaled = expression(material, unreal.MaterialExpressionDivide, -1400, -400)
    mel.connect_material_expressions(world, "", scaled, "A")
    mel.connect_material_expressions(tile, "", scaled, "B")

    normal = expression(material, unreal.MaterialExpressionVertexNormalWS, -1600, 300)
    absolute = expression(material, unreal.MaterialExpressionAbs, -1400, 300)
    mel.connect_material_expressions(normal, "", absolute, "")

    # One projection per axis: (plane, mask of world position, the normal component that selects it)
    planes = [("xy", (True, True, False), (False, False, True)),
              ("xz", (True, False, True), (False, True, False)),
              ("yz", (False, True, True), (True, False, False))]
    total = None
    for i, (_, uv_mask, weight_mask) in enumerate(planes):
        y = -300 + i * 220
        uv = expression(material, unreal.MaterialExpressionComponentMask, -1150, y, r=uv_mask[0], g=uv_mask[1], b=uv_mask[2], a=False)
        mel.connect_material_expressions(scaled, "", uv, "")
        sample = expression(material, unreal.MaterialExpressionTextureSample, -900, y)
        mel.connect_material_expressions(uv, "", sample, "UVs")
        mel.connect_material_expressions(albedo, "", sample, "Tex")
        weight = expression(material, unreal.MaterialExpressionComponentMask, -900, y + 120, r=weight_mask[0], g=weight_mask[1], b=weight_mask[2], a=False)
        mel.connect_material_expressions(absolute, "", weight, "")
        weighted = expression(material, unreal.MaterialExpressionMultiply, -650, y)
        mel.connect_material_expressions(sample, "RGB", weighted, "A")
        mel.connect_material_expressions(weight, "", weighted, "B")
        if total is None:
            total = weighted
        else:
            summed = expression(material, unreal.MaterialExpressionAdd, -450, y)
            mel.connect_material_expressions(total, "", summed, "A")
            mel.connect_material_expressions(weighted, "", summed, "B")
            total = summed

    tint = expression(material, unreal.MaterialExpressionVectorParameter, -450, 300, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    brightness = expression(material, unreal.MaterialExpressionScalarParameter, -450, 450, parameter_name="Brightness", default_value=1.4)
    tinted = expression(material, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(total, "", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    lit = expression(material, unreal.MaterialExpressionMultiply, -100, 100)
    mel.connect_material_expressions(tinted, "", lit, "A")
    mel.connect_material_expressions(brightness, "", lit, "B")
    roughness = expression(material, unreal.MaterialExpressionConstant, -100, 300, r=0.82)

    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {path}")
    unreal.log(f"[create_materials] created {path}")


def ensure_glow_usage() -> None:
    """The boss's bullets are instanced meshes: without this usage flag the engine draws them with its
    default grey material instead of the glow (seen in the first boss screenshots)."""
    material = unreal.load_asset(f"{FOLDER}/{GLOW_NAME}")
    if material is None or material.get_editor_property("used_with_instanced_static_meshes"):
        return
    material.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {GLOW_NAME}")
    unreal.log(f"[create_materials] {GLOW_NAME}: usable on instanced meshes")


create_glow()
ensure_glow_usage()
create_surface()
