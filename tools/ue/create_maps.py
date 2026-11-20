"""Create the empty map shells the game modes populate at runtime.

Run inside the editor's Python (commandlet is fine):
    UnrealEditor-Cmd.exe SuzuneMatsuri.uproject -run=pythonscript -script=tools/ue/create_maps.py

Maps stay empty on purpose: AMatsuriFloorGameMode builds lighting, gray-box
geometry and collectibles from FMatsuriFloorSpec, AMatsuriBossGameMode builds the boss
courtyard (MatsuriBossArena), and AMatsuriMenuGameMode only shows UI. Game modes are routed by map-name prefix in DefaultEngine.ini.
"""
import unreal

MAP_FOLDER = "/Game/Matsuri/Maps"
MAP_NAMES = ("L_Title", "L_Floor01", "L_Floor02", "L_Floor03", "L_Boss_Honden")


def create_map(name: str) -> None:
    path = f"{MAP_FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"[create_maps] {path} already exists, skipped")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    world = tools.create_asset(name, MAP_FOLDER, unreal.World, unreal.WorldFactory())
    if world is None:
        raise RuntimeError(f"could not create {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(world, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {path}")
    unreal.log(f"[create_maps] created {path}")


for map_name in MAP_NAMES:
    create_map(map_name)
