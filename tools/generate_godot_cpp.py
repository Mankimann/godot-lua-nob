#!/usr/bin/env python3
"""Generate godot-cpp bindings for the nob.h build.

This wrapper keeps nob.c simple and avoids relying on SCons. It is intentionally
small: the canonical generator remains thirdparty/godot-cpp/binding_generator.py.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GODOT_CPP = ROOT / "thirdparty" / "godot-cpp"
GENERATOR = GODOT_CPP / "binding_generator.py"
API = GODOT_CPP / "gdextension" / "extension_api.json"

spec = importlib.util.spec_from_file_location("godot_cpp_binding_generator", GENERATOR)
if spec is None or spec.loader is None:
    raise RuntimeError(f"Unable to load {GENERATOR}")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.generate_bindings(str(API), False, bits="64", precision="single", output_dir=str(GODOT_CPP))
