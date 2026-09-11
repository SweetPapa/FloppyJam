#!/usr/bin/env python3
"""Use v4's canonical importer without modifying its generated desktop header."""
import importlib.util
import json
import pathlib
import sys
root = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("levels", root / "v4/tools/gen_levels.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
output = pathlib.Path(sys.argv[1]).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
# Adapt only instructional copy; geometry, timing and campaign keys stay canonical.
for entry in module.CAMPAIGN:
    with (root / "v4/levels" / (entry["key"] + ".json")).open() as source:
        assert len(json.load(source).get("checkpoints", [])) <= 64, "Mobile checkpoint capacity exceeded"
    hint = entry["hint"]
    for old, new in [("Press the color", "Tap the color"), (" W red, S blue, A yellow, D green.", ""),
                     ("key badges", "node outlines"), ("next key", "next color"),
                     ("Every key", "Every button"), ("color keys", "color buttons"),
                     ("V enables reduced motion.", "Reduced motion is available in Settings.")]:
        hint = hint.replace(old, new)
    entry["hint"] = hint
module.OUT = str(output)
module.emit()
