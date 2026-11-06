"""Validate the repository's pinned, deliberately small JSON Schema vocabulary.

Not a general-purpose JSON Schema implementation. Unknown keywords fail closed.
Cross-file, provenance and geometric checks live in their owning modules.
"""
from __future__ import annotations
import re
from .common import finite, read, require


def validate(kind, value):
    from pathlib import Path
    document = read(Path(__file__).with_name("contracts.schema.json"))
    def check(spec, item, path):
        supported = {"$schema", "$id", "$defs", "$ref", "title", "description", "type", "required", "properties",
                     "items", "enum", "const", "minimum", "maximum", "minItems", "minLength", "pattern",
                     "additionalProperties"}
        require(set(spec) <= supported, "Unsupported contract schema keyword")
        if "$ref" in spec:
            ref = spec["$ref"]
            require(ref.startswith("#/$defs/"), "External schema refs are not allowed")
            check(document["$defs"][ref[len("#/$defs/"):]], item, path)
        if "type" in spec:
            tests = {"object": lambda: isinstance(item, dict), "array": lambda: isinstance(item, list),
                     "string": lambda: isinstance(item, str), "integer": lambda: type(item) is int,
                     "number": lambda: finite(item), "boolean": lambda: type(item) is bool, "null": lambda: item is None}
            types = spec["type"] if isinstance(spec["type"], list) else [spec["type"]]
            require(any(tests[t]() for t in types), "Schema type: " + path)
        if "const" in spec:
            require(item == spec["const"] and type(item) is type(spec["const"]), "Schema const: " + path)
        if "enum" in spec:
            require(item in spec["enum"], "Schema enum: " + path)
        if isinstance(item, dict):
            require(set(spec.get("required", [])) <= set(item), "Schema required: " + path)
            if spec.get("additionalProperties") is False:
                require(set(item) <= set(spec.get("properties", {})), "Schema unknown property: " + path)
            for key, sub in spec.get("properties", {}).items():
                if key in item:
                    check(sub, item[key], path + "." + key)
        if isinstance(item, list):
            require(len(item) >= spec.get("minItems", 0), "Schema array size: " + path)
            for i, child in enumerate(item):
                if "items" in spec:
                    check(spec["items"], child, f"{path}[{i}]")
        if isinstance(item, str):
            require(len(item) >= spec.get("minLength", 0) and
                    ("pattern" not in spec or re.search(spec["pattern"], item)), "Schema string: " + path)
        if type(item) in (int, float):
            require(finite(item) and item >= spec.get("minimum", float("-inf")) and
                    item <= spec.get("maximum", float("inf")), "Schema range: " + path)
    require(kind in document["$defs"], "Unknown entity schema: " + kind)
    check(document["$defs"][kind], value, kind)
    return value
