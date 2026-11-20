"""Instantiate the UE 5.8 C++ First Person template as a named project.

Mirrors what the editor's New Project wizard does (GameProjectUtils):
copy Config/Content/Source, rename the module, apply the template's text
replacements, mount the shared content packs listed in TemplateDefs.ini and
add ActiveGameNameRedirects so the template blueprints find the renamed module.

Usage:
    python tools/scaffold_project.py --name SuzuneMatsuri --dest SuzuneMatsuri
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

ENGINE = Path(r"C:\Program Files\Epic Games\UE_5.8")
TEMPLATE_NAME = "TP_FirstPerson"
TEXT_EXTENSIONS = {".cpp", ".h", ".ini", ".cs"}
IGNORED_CONFIG = {"TemplateDefs.ini", "config.ini"}
# (MountName, DetailLevel) from TemplateDefs.ini SharedContentPacks
SHARED_PACKS = [("Characters", "High"), ("LevelPrototyping", "High"), ("Input", "High"), ("Weapons", "Standard")]


def replace_template_name(text: str, project: str) -> str:
    text = text.replace(TEMPLATE_NAME.upper(), project.upper())
    text = text.replace(TEMPLATE_NAME.lower(), project.lower())
    return re.sub(re.escape(TEMPLATE_NAME), project, text, flags=re.IGNORECASE)


def copy_source(template: Path, dest: Path, project: str) -> None:
    for src in (template / "Source").rglob("*"):
        if src.is_dir():
            continue
        rel = src.relative_to(template)
        target = dest / replace_template_name(str(rel), project)
        target.parent.mkdir(parents=True, exist_ok=True)
        if src.suffix.lower() in TEXT_EXTENSIONS:
            text = src.read_text(encoding="utf-8-sig")
            target.write_text(replace_template_name(text, project), encoding="utf-8")
        else:
            shutil.copy2(src, target)


def copy_config(template: Path, dest: Path, project: str) -> None:
    (dest / "Config").mkdir(parents=True, exist_ok=True)
    for src in (template / "Config").iterdir():
        if src.name in IGNORED_CONFIG:
            continue
        text = src.read_text(encoding="utf-8-sig")
        (dest / "Config" / src.name).write_text(replace_template_name(text, project), encoding="utf-8")
    engine_ini = dest / "Config" / "DefaultEngine.ini"
    redirects = (
        "\n[/Script/Engine.Engine]\n"
        f'+ActiveGameNameRedirects=(OldGameName="{TEMPLATE_NAME}",NewGameName="/Script/{project}")\n'
        f'+ActiveGameNameRedirects=(OldGameName="/Script/{TEMPLATE_NAME}",NewGameName="/Script/{project}")\n'
    )
    engine_ini.write_text(engine_ini.read_text(encoding="utf-8") + redirects, encoding="utf-8")


def copy_content(template: Path, dest: Path) -> None:
    shutil.copytree(template / "Content", dest / "Content", dirs_exist_ok=True)
    for mount, level in SHARED_PACKS:
        pack = ENGINE / "Templates" / "TemplateResources" / level / mount / "Content"
        if not pack.is_dir():
            raise FileNotFoundError(f"shared content pack missing: {pack}")
        shutil.copytree(pack, dest / "Content" / mount, dirs_exist_ok=True)


def write_uproject(template: Path, dest: Path, project: str) -> None:
    data = json.loads((template / f"{TEMPLATE_NAME}.uproject").read_text(encoding="utf-8-sig"))
    data["EngineAssociation"] = "5.8"
    data["Description"] = "Suzune's Night Festival - coin-collecting roguelite hero shooter"
    for module in data["Modules"]:
        module["Name"] = replace_template_name(module["Name"], project)
    (dest / f"{project}.uproject").write_text(json.dumps(data, indent="\t"), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", required=True)
    parser.add_argument("--dest", required=True, type=Path)
    args = parser.parse_args()

    template = ENGINE / "Templates" / TEMPLATE_NAME
    if args.dest.exists() and any(args.dest.iterdir()):
        print(f"refusing to overwrite non-empty {args.dest}", file=sys.stderr)
        return 1
    args.dest.mkdir(parents=True, exist_ok=True)
    copy_source(template, args.dest, args.name)
    copy_config(template, args.dest, args.name)
    copy_content(template, args.dest)
    write_uproject(template, args.dest, args.name)
    print(f"created {args.dest / (args.name + '.uproject')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
