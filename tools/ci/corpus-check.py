#!/usr/bin/env python3
"""Corpus integrity check for the C++23 modernization.

Static gate, intentionally distinct from the rAthena runtime YAML/NPC loaders:
  * every active npc: path in npc/scripts_*.conf resolves to a file;
  * every tracked YAML database/config/NPC file is syntactically parseable;
  * Footer.Imports paths resolve, including the documented import-tmpl fallback.

No rewriting, no dependence on production credentials, no fake gameplay claims.
"""
from __future__ import annotations

from pathlib import Path
import sys

import yaml


def check(root: Path) -> int:
    issues: list[str] = []
    references = 0
    yaml_count = 0
    yaml_empty = 0
    import_count = 0

    manifests = sorted((root / "npc").glob("scripts_*.conf"))
    if len(manifests) < 7:
        issues.append(f"Expected the core NPC manifests, got {len(manifests)}")

    for manifest in manifests:
        for number, line in enumerate(manifest.read_text(encoding="utf-8-sig").splitlines(), 1):
            line = line.strip()
            if not line.startswith("npc:"):
                continue
            target = line.partition(":")[2].strip()
            references += 1
            if not target.startswith("npc/") or not (root / target).is_file():
                issues.append(f"{manifest.relative_to(root)}:{number}: missing/invalid NPC script {target!r}")

    # A count floor protects against accidentally commenting out whole manifest
    # groups. The baseline contains 376 active references (2026-10-29).
    if references < 350:
        issues.append(f"Only {references} active NPC references; expected >=350")

    yaml_files = sorted(
        path
        for directory in ("db", "conf", "npc")
        for path in (root / directory).rglob("*.yml")
        if path.is_file()
    )
    if len(yaml_files) < 100:
        issues.append(f"Only {len(yaml_files)} YAML files found; expected >=100")

    for path in yaml_files:
        rel = path.relative_to(root)
        try:
            # PyYAML is a secondary syntax gate, not a replacement for
            # rapidyaml/yaml-cpp or rAthena's schema and script semantics.
            data = yaml.safe_load(path.read_text(encoding="utf-8-sig"))
        except (yaml.YAMLError, UnicodeError, OSError) as exc:
            issues.append(f"{rel}: cannot parse YAML: {exc}")
            continue
        yaml_count += 1
        if data is None:  # empty import template / comment-only file
            yaml_empty += 1
            continue
        if not isinstance(data, dict):
            issues.append(f"{rel}: root is {type(data).__name__}, expected mapping")
            continue

        header = data.get("Header")
        if header is not None and (
            not isinstance(header, dict)
            or not isinstance(header.get("Type"), str)
            or not isinstance(header.get("Version"), int)
        ):
            issues.append(f"{rel}: invalid Header.Type/Version")

        footer = data.get("Footer")
        if not isinstance(footer, dict):
            continue
        imports = footer.get("Imports", [])
        if not isinstance(imports, list):
            issues.append(f"{rel}: Footer.Imports must be a list")
            continue
        for entry in imports:
            if not isinstance(entry, dict) or not isinstance(entry.get("Path"), str):
                issues.append(f"{rel}: malformed Footer.Imports entry {entry!r}")
                continue
            import_count += 1
            target = entry["Path"]
            if target.startswith("/") or ".." in Path(target).parts:
                issues.append(f"{rel}: import leaves checkout: {target!r}")
                continue
            if (root / target).is_file():
                continue
            # The server creates import/ directories from these templates.
            # A checkout need not track/generated user-overridable imports.
            if target.startswith("db/import/"):
                template = root / target.replace("db/import/", "db/import-tmpl/", 1)
                if template.is_file():
                    continue
            if target.startswith("conf/import/"):
                template = root / target.replace("conf/import/", "conf/import-tmpl/", 1)
                if template.is_file():
                    continue
            issues.append(f"{rel}: missing YAML import {target!r}")

    print(f"NPC manifests={len(manifests)} active references={references}")
    print(f"YAML scanned={yaml_count} empty/comment-only={yaml_empty} Footer imports={import_count}")
    for issue in issues:
        print(f"FAIL: {issue}", file=sys.stderr)
    if issues:
        print(f"Corpus check failed: {len(issues)} finding(s)", file=sys.stderr)
        return 1
    print("CORPUS CHECK PASSED (syntax and references, not gameplay)")
    return 0


if __name__ == "__main__":
    sys.exit(check(Path(__file__).resolve().parents[2]))
