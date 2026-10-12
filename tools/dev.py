#!/usr/bin/env python3
"""Portable project commands. Run: python tools/dev.py test|build|package."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import zipfile
from pathlib import Path

import ue

ROOT = Path(__file__).resolve().parents[1]
# Nome do pacote só com [A-Za-z0-9._-]: "anchor-ue5-plugin ✓" -> "anchor-ue5-plugin".
PACKAGE_NAME = re.sub(r"[^A-Za-z0-9._-]+", "-", ROOT.name).strip("-") or "project"
EXCLUDE = {
    ".git", ".godot", "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".venv", ".vs", ".idea",
    "target", "node_modules", "dist", "build", "obj", "Binaries", "Intermediate", "DerivedDataCache", "Saved",
}


def execute(args: list[str]) -> None:
    subprocess.run(args, cwd=ROOT, check=True)


def package() -> Path:
    destination = ROOT / "dist" / f"{PACKAGE_NAME}.zip"
    destination.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as out:
        for path in sorted(ROOT.rglob("*")):
            parts = path.relative_to(ROOT).parts
            if not path.is_file() or path.is_symlink() or path.suffix in {".pyc", ".pyo"}:
                continue
            if any(p in EXCLUDE or p.endswith(".egg-info") for p in parts):
                continue
            out.write(path, Path(PACKAGE_NAME) / path.relative_to(ROOT))
    return destination


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["test", "build", "package"])
    args = parser.parse_args(argv)
    if args.command == "package":
        print(package().relative_to(ROOT).as_posix())
        return 0
    if args.command == "test":
        # Gate host-side: manifesto/árvore/estilo C++ + pytest; compilar exige Unreal Engine 5.8+ e UBT.
        execute([sys.executable, "tools/validate_plugin.py"])
        execute([sys.executable, "-m", "pytest", "-q", "tests"])
        return 0
    if ue.find_engine() is None:
        parser.error("build requires Unreal Engine 5.8+ (UBT); see docs/INSTALACAO.md.")
    return ue.main(["build-plugin"])


if __name__ == "__main__":
    raise SystemExit(main())
