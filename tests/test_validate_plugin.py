"""Testes host-side do validador: manifestos, árvore e coerência módulos <-> pastas."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import validate_plugin as vp  # noqa: E402

requires_git = pytest.mark.skipif(shutil.which("git") is None, reason="git não instalado")


@pytest.fixture
def clean_root(tmp_path: Path) -> Path:
    """Árvore mínima válida espelhando o scaffold (sem EngineVersion no .uplugin, como o real)."""
    uplugin = {
        "FileVersion": 3,
        "Version": 1,
        "VersionName": "0.1.0",
        "Modules": [
            {"Name": "CatalogAnchor", "Type": "Runtime", "LoadingPhase": "Default"},
            {"Name": "CatalogAnchorEditor", "Type": "Editor", "LoadingPhase": "Default"},
        ],
        "Plugins": [
            {"Name": "DataValidation", "Enabled": True},
            {"Name": "GameplayAbilities", "Enabled": False, "Optional": True},
        ],
    }
    uproject = {
        "FileVersion": 3,
        "EngineAssociation": "5.8",
        "Plugins": [{"Name": "CatalogAnchor", "Enabled": True}],
        "AdditionalPluginDirectories": ["../Plugins"],
    }

    plug = tmp_path / "Plugins" / "CatalogAnchor"
    (plug / "Source" / "CatalogAnchor" / "Public").mkdir(parents=True)
    (plug / "Source" / "CatalogAnchorEditor").mkdir(parents=True)
    (tmp_path / "SampleProject").mkdir()
    (tmp_path / "docs").mkdir()

    (plug / "CatalogAnchor.uplugin").write_text(json.dumps(uplugin), encoding="utf-8")
    (tmp_path / "SampleProject" / "SampleCatalog.uproject").write_text(
        json.dumps(uproject), encoding="utf-8"
    )

    (plug / "Source" / "CatalogAnchor" / "CatalogAnchor.Build.cs").write_text(
        "public class CatalogAnchor : ModuleRules {\n"
        "  public CatalogAnchor(ReadOnlyTargetRules Target) : base(Target) {\n"
        "    PublicDependencyModuleNames.AddRange(new string[]{\"Core\"});\n"
        "    // PublicDependencyModuleNames.AddRange(new string[] { \"GameplayAbilities\" });\n"
        "  }\n}\n",
        encoding="utf-8",
    )
    (plug / "Source" / "CatalogAnchorEditor" / "CatalogAnchorEditor.Build.cs").write_text(
        "public class CatalogAnchorEditor : ModuleRules {\n"
        "  public CatalogAnchorEditor(ReadOnlyTargetRules Target) : base(Target) {\n"
        "    PrivateDependencyModuleNames.AddRange(new string[]{\"UnrealEd\", \"DataValidation\"});\n"
        "  }\n}\n",
        encoding="utf-8",
    )
    (plug / "Source" / "CatalogAnchor" / "Public" / "CatalogInteractionComponent.h").write_text(
        f"{vp.COPYRIGHT_LINE}\n#pragma once\nclass UCatalogInteractionComponent {{}};\n",
        encoding="utf-8",
    )
    (plug / "Source" / "CatalogAnchor" / "Public" / "CatalogGameplaySubsystem.h").write_text(
        f"{vp.COPYRIGHT_LINE}\n#pragma once\nclass UCatalogGameplaySubsystem {{}};\n",
        encoding="utf-8",
    )
    (tmp_path / "docs" / "TECNOLOGIAS.md").write_text(
        "# Tecnologias\nUnreal Engine 5.8\n", encoding="utf-8"
    )
    (tmp_path / "docs" / "INSTALACAO.md").write_text(
        "# Instalacao\nDrop em projeto UE 5.8\n", encoding="utf-8"
    )
    return tmp_path


def _edit_json(path: Path, mutate) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    mutate(data)
    path.write_text(json.dumps(data), encoding="utf-8")


def _uplugin(root: Path) -> Path:
    return root / "Plugins" / "CatalogAnchor" / "CatalogAnchor.uplugin"


def _uproject(root: Path) -> Path:
    return root / "SampleProject" / "SampleCatalog.uproject"


def _git(root: Path, *args: str) -> None:
    subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True)


def test_01_real_repo_passes():
    result = vp.run_validation(ROOT)
    assert result.ok(), result.errors


def test_02_uplugin_semver_and_modules(clean_root: Path):
    result = vp.run_validation(clean_root)
    assert result.ok(), result.errors


def test_03_bad_semver_fails(clean_root: Path):
    _edit_json(_uplugin(clean_root), lambda d: d.update(VersionName="v1"))
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("semver" in e.lower() or "VersionName" in e for e in result.errors)


def test_04_missing_editor_module_fails(clean_root: Path):
    _edit_json(_uplugin(clean_root), lambda d: d.update(Modules=[d["Modules"][0]]))
    result = vp.run_validation(clean_root)
    assert not result.ok()


@pytest.mark.parametrize(
    ("assoc", "ok"),
    [("5.8", True), ("5.9", True), ("5.10", True), ("6.0", True), ("5.4", False), ("5.7", False), ("", False)],
)
def test_05_engine_association_must_be_at_least_5_8(clean_root: Path, assoc: str, ok: bool):
    _edit_json(_uproject(clean_root), lambda d: d.update(EngineAssociation=assoc))
    result = vp.run_validation(clean_root)
    assert result.ok() is ok, result.errors
    assert ok or any("EngineAssociation" in e and "5.8" in e for e in result.errors)


def test_06_forbidden_binaries_dir_fails(clean_root: Path):
    (clean_root / "Plugins" / "CatalogAnchor" / "Binaries").mkdir()
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("Binaries" in e for e in result.errors)


def test_07_missing_interaction_header_fails(clean_root: Path):
    path = (
        clean_root
        / "Plugins"
        / "CatalogAnchor"
        / "Source"
        / "CatalogAnchor"
        / "Public"
        / "CatalogInteractionComponent.h"
    )
    path.unlink()
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("interaction" in e.lower() or "CatalogInteraction" in e for e in result.errors)


def test_08_invalid_json_uplugin_fails(clean_root: Path):
    _uplugin(clean_root).write_text("{not-json", encoding="utf-8")
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("JSON" in e for e in result.errors)


def test_09_runtime_type_must_be_runtime(clean_root: Path):
    _edit_json(_uplugin(clean_root), lambda d: d["Modules"][0].update(Type="Editor"))
    result = vp.run_validation(clean_root)
    assert not result.ok()


def test_10_cli_main_ok_on_real_repo():
    assert vp.main(["--root", str(ROOT)]) == 0


def test_11_sample_needs_additional_plugin_directories(clean_root: Path):
    _edit_json(_uproject(clean_root), lambda d: d.pop("AdditionalPluginDirectories"))
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("AdditionalPluginDirectories" in e for e in result.errors)


def test_12_plugin_copy_inside_sample_is_duplicate(clean_root: Path):
    (clean_root / "SampleProject" / "Plugins" / "CatalogAnchor").mkdir(parents=True)
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("duplica" in e for e in result.errors)


def test_13_engine_plugin_module_requires_uplugin_dependency(clean_root: Path):
    _edit_json(
        _uplugin(clean_root),
        lambda d: d.update(Plugins=[p for p in d["Plugins"] if p["Name"] != "DataValidation"]),
    )
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("DataValidation" in e for e in result.errors)


def test_14_uncommented_gas_dependency_requires_enabled_plugin(clean_root: Path):
    build = clean_root / "Plugins" / "CatalogAnchor" / "Source" / "CatalogAnchor" / "CatalogAnchor.Build.cs"
    build.write_text(build.read_text(encoding="utf-8").replace("// Public", "Public"), encoding="utf-8")
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("GameplayAbilities" in e for e in result.errors)


def test_15_build_cs_class_must_match_module_exactly(clean_root: Path):
    build = clean_root / "Plugins" / "CatalogAnchor" / "Source" / "CatalogAnchor" / "CatalogAnchor.Build.cs"
    build.write_text(
        build.read_text(encoding="utf-8").replace("class CatalogAnchor ", "class CatalogAnchorRules "),
        encoding="utf-8",
    )
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("CatalogAnchor.Build.cs" in e for e in result.errors)


@requires_git
def test_16_gitignored_build_dirs_are_allowed(clean_root: Path):
    _git(clean_root, "init", "-q")
    (clean_root / ".gitignore").write_text("Binaries/\nSaved/\n", encoding="utf-8")
    (clean_root / "SampleProject" / "Binaries").mkdir()
    (clean_root / "SampleProject" / "Binaries" / "game.dll").write_bytes(b"\0")
    (clean_root / "SampleProject" / "Saved").mkdir()
    (clean_root / "SampleProject" / "Saved" / "log.txt").write_text("x", encoding="utf-8")
    result = vp.run_validation(clean_root)
    assert result.ok(), result.errors


@requires_git
def test_17_tracked_build_dir_fails(clean_root: Path):
    _git(clean_root, "init", "-q")
    (clean_root / ".gitignore").write_text("Binaries/\n", encoding="utf-8")
    binaries = clean_root / "Plugins" / "CatalogAnchor" / "Binaries"
    binaries.mkdir()
    (binaries / "plugin.dll").write_bytes(b"\0")
    _git(clean_root, "add", "-f", "Plugins/CatalogAnchor/Binaries/plugin.dll")
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("Plugins/CatalogAnchor/Binaries" in e for e in result.errors)


def test_18_uplugin_module_without_build_cs_fails(clean_root: Path):
    _edit_json(_uplugin(clean_root), lambda d: d["Modules"].append({"Name": "CatalogAnchorExtra", "Type": "Runtime"}))
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("CatalogAnchorExtra" in e and "Build.cs" in e for e in result.errors), result.errors


def test_19_source_folder_missing_from_uplugin_fails(clean_root: Path):
    (clean_root / "Plugins" / "CatalogAnchor" / "Source" / "Orphan").mkdir()
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any("Source/Orphan" in e for e in result.errors), result.errors


def test_20_cpp_lint_runs_inside_validation(clean_root: Path):
    rel = "Plugins/CatalogAnchor/Source/CatalogAnchor/Public/CatalogGameplaySubsystem.h"
    (clean_root / rel).write_text("#pragma once\nclass UCatalogGameplaySubsystem {};\n", encoding="utf-8")
    result = vp.run_validation(clean_root)
    assert not result.ok()
    assert any(e.startswith(f"{rel}:1:") for e in result.errors), result.errors


@pytest.mark.parametrize(("engine_version", "ok"), [("5.8.0", True), ("6.0.0", True), ("5.4.0", False), ("5", False)])
def test_21_optional_engine_version_must_be_at_least_5_8(clean_root: Path, engine_version: str, ok: bool):
    _edit_json(_uplugin(clean_root), lambda d: d.update(EngineVersion=engine_version))
    result = vp.run_validation(clean_root)
    assert result.ok() is ok, result.errors
    assert ok or any("EngineVersion" in e for e in result.errors)


def test_22_docs_must_cite_target_engine(clean_root: Path):
    (clean_root / "docs" / "INSTALACAO.md").write_text("# Instalacao\nUnreal Engine 5.4\n", encoding="utf-8")
    result = vp.run_validation(clean_root)
    assert any("INSTALACAO.md" in e for e in result.errors), result.errors


@pytest.mark.parametrize(("encoding", "ok"), [("utf-8-sig", True), ("utf-16", True), ("cp1252", False)])
def test_23_descriptor_encodings(clean_root: Path, encoding: str, ok: bool):
    """BOM UTF-8 e UTF-16 (o que a UE lê) passam; bytes que não são Unicode viram erro, não traceback."""
    path = _uproject(clean_root)
    _edit_json(path, lambda d: d.update(Description="Sample jogável"))
    path.write_bytes(path.read_text(encoding="utf-8").replace("\\u00e1", "á").encode(encoding))
    result = vp.run_validation(clean_root)
    assert result.ok() is ok, result.errors
    assert ok or any(".uproject JSON inválido" in e for e in result.errors)
