"""Import the sound effects built by tools/audio/build_sfx.py, and the music, into SuzuneMatsuri.

    tools/ue_py.sh import_audio tools/ue/import_audio.py

  SourceArt/Audio/Sfx/SFX_<Name>.wav -> /Game/Matsuri/Audio/SFX_<Name>          (SoundWave)
  SourceArt/Audio/BGM_<Name>.mp3     -> /Game/Matsuri/Audio/Music/BGM_<Name>    (SoundWave, looping)
  SourceArt/Audio/STING_<Name>.mp3   -> /Game/Matsuri/Audio/Music/STING_<Name>  (SoundWave, once)

Re-running replaces them (the WAVs are rebuilt from source, so the assets should follow).
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AUDIO_DIR = os.path.join(PROJECT, "SourceArt", "Audio")
SFX_DIR = os.path.join(AUDIO_DIR, "Sfx")
ROOT = "/Game/Matsuri/Audio"
MUSIC_ROOT = ROOT + "/Music"
MUSIC_PREFIXES = ("BGM_", "STING_")


def log(msg):
    unreal.log_warning(f"[matsuri-audio] {msg}")


def import_task(folder, file, destination):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(folder, file))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", os.path.splitext(file)[0])
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    return task


def import_effects():
    files = sorted(f for f in os.listdir(SFX_DIR) if f.lower().endswith(".wav"))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([import_task(SFX_DIR, f, ROOT) for f in files])
    for file in files:
        sound = unreal.load_asset(f"{ROOT}/{file[:-4]}")
        log(f"{file[:-4]}: {'ok' if isinstance(sound, unreal.SoundWave) else 'MISSING'}")


def import_music():
    """The tracks loop (a floor can outlast its track); the stingers play once."""
    files = sorted(f for f in os.listdir(AUDIO_DIR) if f.lower().endswith(".mp3") and f.startswith(MUSIC_PREFIXES))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([import_task(AUDIO_DIR, f, MUSIC_ROOT) for f in files])
    for file in files:
        name = os.path.splitext(file)[0]
        sound = unreal.load_asset(f"{MUSIC_ROOT}/{name}")
        if not isinstance(sound, unreal.SoundWave):
            log(f"{name}: MISSING")
            continue
        sound.set_editor_property("looping", name.startswith("BGM_"))
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
        log(f"{name}: ok, {sound.get_editor_property('duration'):.1f} s, looping {sound.get_editor_property('looping')}")


def main():
    import_effects()
    import_music()
    log("done")


main()
