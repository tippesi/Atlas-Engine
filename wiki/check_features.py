#!/usr/bin/env python3
# Checks that wiki/Features.md lists every component, Lua type, capture tool event, edit operation, query and check,
# every engine folder, helper header and editor tool, and that the files it names exist. Exits with 1 otherwise.
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
engine = root / "src" / "engine"
capture = root / "src" / "capture"
editor = root / "src" / "editor"
index = root / "wiki" / "Features.md"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def stems(directory, pattern="*.h"):
    return {path.stem for path in directory.glob(pattern)}


def strings(text):
    return set(re.findall(r'"(\w+)"', text))


def keys(table):
    return set(re.findall(r'\{\s*"(\w+)"', table))


def block(path, start):
    # The brace initializer after start, e.g. the entries of a static table
    text = read(path)
    begin = text.find(start)
    if begin < 0:
        sys.exit(f"{path.relative_to(root)}: '{start}' not found, update check_features.py")
    begin = text.index("{", begin)
    depth = 0
    for i in range(begin, len(text)):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        if depth == 0:
            return text[begin:i + 1]


def lua_names():
    names = set()
    for path in (engine / "scripting").rglob("*.cpp"):
        text = read(path)
        names |= set(re.findall(r'new_(?:usertype|enum)\s*(?:<[^()]*>)?\s*\(\s*"(\w+)"', text))
        names |= set(re.findall(r'create_named\s*\(\s*"(\w+)"', text))
    return names


def required():
    return {
        "components": stems(engine / "scene" / "components", "*Component.h"),
        "Lua types, enums and tables": lua_names(),
        "capture timeline events": strings(block(capture / "Job.cpp", "std::vector<std::string> types =")),
        "capture camera modes": strings(block(capture / "Job.cpp", "std::set<std::string> modes =")),
        "capture checks": strings(block(capture / "Job.cpp", "CheckKeys(checks,")) - {"checks", "interval"},
        "capture edit operations": keys(block(capture / "SceneEditor.cpp", "editOperations = {")),
        "capture queries": keys(block(capture / "SceneTools.cpp", "queryTypes = {")),
        "helper headers": stems(engine / "common") | stems(engine / "volume") | stems(engine / "tools") |
                          stems(engine / "renderer" / "helper"),
        "editor tools and windows": stems(editor / "tools") | stems(editor / "ui" / "windows"),
        "ImGui extension panels": stems(root / "libs" / "ImguiExtension" / "panels"),
    }


def main():
    text = read(index)
    spans = re.findall(r"`([^`\n]+)`", text)
    documented = {word for span in spans for word in re.findall(r"[A-Za-z_]\w*", span)}

    missing = {}
    for category, names in required().items():
        absent = sorted(names - documented)
        if absent:
            missing[category] = absent

    folders = sorted(path.name for path in engine.iterdir() if path.is_dir())
    absent = [folder for folder in folders if not any(f"{folder}/" in span for span in spans)]
    if absent:
        missing["engine folders (as `folder/`)"] = absent

    bases = [root, root / "src", engine, root / "data" / "shader", root / "libs", root / "wiki"]
    files = {span for span in spans if re.fullmatch(r"[\w./-]+\.(h|cpp|hsh|csh|fsh|vsh|md|lua|py)", span)}
    absent = sorted(span for span in files if not any((base / span).exists() for base in bases))
    if absent:
        missing["files that don't exist"] = absent

    if missing:
        print(f"{index.relative_to(root)} is out of date:")
        for category, names in missing.items():
            print(f"  {category}: {', '.join(names)}")
        print("Add a line for each new feature (with the words someone would search for) or fix the renamed ones.")
        return 1

    print(f"{index.relative_to(root)} lists all {sum(len(names) for names in required().values())} registered names "
          f"and {len(folders)} engine folders")
    return 0


if __name__ == "__main__":
    sys.exit(main())
