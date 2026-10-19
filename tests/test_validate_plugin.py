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
        "SupportedTargetPlatforms": ["Win64", "Mac", "Linux"],
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


def _cpp(root: Path, rel: str, body: str) -> str:
    """Escreve um .h/.cpp (com copyright e #pragma once) no módulo CatalogAnchor; devolve o caminho relativo à raiz."""
    full = Path("Plugins/CatalogAnchor/Source/CatalogAnchor") / rel
    pragma = "#pragma once\n" if rel.endswith(".h") else ""
    (root / full).parent.mkdir(parents=True, exist_ok=True)
    (root / full).write_text(f"{vp.COPYRIGHT_LINE}\n{pragma}{body}", encoding="utf-8")
    return full.as_posix()


@pytest.mark.parametrize(
    ("name", "ok"),
    [
        ("CatalogAnchor.Save.Migrate", True),
        ("CatalogAnchor.Save.World.Migrate", True),
        ("Save.Migrate", False),
        ("CatalogAnchor.Migrate", False),
        ("CatalogAnchor..Migrate", False),
        ("CatalogAnchorX.Save.Migrate", False),
    ],
)
def test_24_automation_name_needs_prefix_area_and_case(clean_root: Path, name: str, ok: bool):
    macro = f'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveTest, "{name}", Flags)\n'
    rel = _cpp(clean_root, "Private/Tests/SaveTests.cpp", macro)
    result = vp.run_validation(clean_root)
    assert result.ok() is ok, result.errors
    assert ok or any(e.startswith(f"{rel}:2:") and "CatalogAnchor.<Área>.<Caso>" in e for e in result.errors)


def test_25_automation_names_must_be_unique_and_custom_macro_is_read(clean_root: Path):
    a = _cpp(clean_root, "Private/Tests/A.cpp", 'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA, "CatalogAnchor.Save.Same", F)\n')
    b = _cpp(
        clean_root,
        "Private/Tests/B.cpp",
        'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FB,\n\t"CatalogAnchor.Save.Same", F)\n'
        'IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FC, FBase, "Wrong.Name", F)\n'
        '// IMPLEMENT_SIMPLE_AUTOMATION_TEST(FD, "Commented.Out", F)\n',
    )
    errors = vp.run_validation(clean_root).errors
    assert any(e.startswith(f"{b}:2:") and "duplicado" in e and f"{a}:2" in e for e in errors), errors
    assert any(e.startswith(f"{b}:4:") and "'Wrong.Name'" in e for e in errors), errors
    assert not any("Commented.Out" in e for e in errors), errors


def test_26_doc_counts_are_warnings_unless_strict(clean_root: Path):
    _cpp(clean_root, "Private/Tests/A.cpp", 'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA, "CatalogAnchor.Save.A", F)\n')
    tests_dir = clean_root / "tests"
    tests_dir.mkdir()
    (tests_dir / "test_x.py").write_text(
        "import pytest\n\n\ndef test_a():\n    pass\n\n\n"
        "@pytest.mark.parametrize('x', [1, 2, 3])\ndef test_b(x):\n    pass\n",
        encoding="utf-8",
    )
    readme = clean_root / "README.md"
    readme.write_text("| pytest (5 casos) | CI |\n| Automation (2 testes C++) | UE |\n", encoding="utf-8")
    docs = clean_root / "docs" / "INSTALACAO.md"
    docs.write_text("# Instalacao\nUE 5.8\nroda os 3 testes de automação e 9 casos de pytest\n", encoding="utf-8")

    result = vp.run_validation(clean_root)
    assert result.ok(), result.errors
    places = sorted(w.split(": ", 1)[0] for w in result.warnings)
    assert places == ["README.md:1", "README.md:2", "docs/INSTALACAO.md:3", "docs/INSTALACAO.md:3"]
    assert any("cita 5 casos de pytest, mas tests/ tem 4" in w for w in result.warnings), result.warnings
    assert any("cita 2 testes de automação, mas o código tem 1" in w for w in result.warnings), result.warnings

    strict = vp.run_validation(clean_root, strict_docs=True)
    assert sorted(strict.errors) == sorted(result.warnings) and not strict.warnings
    assert vp.main(["--root", str(clean_root)]) == 0
    assert vp.main(["--root", str(clean_root), "--strict-docs"]) == 1

    readme.write_text("| pytest (4 casos) | CI |\n| Automation (1 testes C++) | UE |\n", encoding="utf-8")
    docs.write_text("# Instalacao\nUE 5.8\nroda o 1 testes verdes e 4 casos de pytest\n", encoding="utf-8")
    clean = vp.run_validation(clean_root, strict_docs=True)
    assert clean.ok() and not clean.warnings, (clean.errors, clean.warnings)


@pytest.mark.parametrize(
    ("specs", "cpp", "expected"),
    [
        ("Server, Reliable", "", "sem WithValidation"),
        ("Server, Reliable, WithValidation", "", "ServerFoo_Validate() não encontrada"),
        ("Server, Reliable, WithValidation", "// bool UCatalogRpc::ServerFoo_Validate(int32 A)\n", "_Validate() não"),
        ("Server, Reliable, WithValidation", "bool UCatalogRpc::ServerFoo_Validate(int32 A) { return true; }\n", None),
        ('Reliable, Server, Category = "A|B, C", meta = (Tip = "x,y"), WithValidation', "ServerFoo_Validate(", None),
        ('Server, Category = "A|B, C", meta = (Tip = "x)y")', "", "sem WithValidation"),
        ("Client, Reliable", "", None),
        ("BlueprintCallable", "", None),
    ],
)
def test_27_server_rpc_needs_validation(clean_root: Path, specs: str, cpp: str, expected: str | None):
    header = f"class UCatalogRpc\n{{\n\tUFUNCTION({specs})\n\tvoid ServerFoo(int32 A);\n}};\n"
    _cpp(clean_root, "Public/CatalogRpc.h", header)
    _cpp(clean_root, "Private/CatalogRpc.cpp", cpp)
    result = vp.run_validation(clean_root, strict_rpc=True)
    assert result.ok() is (expected is None), result.errors
    assert expected is None or any(
        e.startswith("Plugins/CatalogAnchor/Source/CatalogAnchor/Public/CatalogRpc.h:5:") and expected in e
        for e in result.errors
    ), result.errors


def test_28_server_rpc_violations_are_errors_unless_relaxed(clean_root: Path):
    header = "class UCatalogRpc\n{\n\tUFUNCTION(Server, Reliable)\n\tvoid ServerFoo();\n};\n"
    _cpp(clean_root, "Public/CatalogRpc.h", header)
    assert not vp.run_validation(clean_root).ok()
    result = vp.run_validation(clean_root, strict_rpc=False)
    assert result.ok() and any("sem WithValidation" in w for w in result.warnings), (result.errors, result.warnings)
    assert vp.main(["--root", str(clean_root)]) == 1
    assert vp.main(["--root", str(clean_root), "--no-strict-rpc"]) == 0


def test_29_counts_on_real_repo():
    tests = vp.automation_tests(vp.cpp_texts(ROOT))
    assert len(tests) > 0 and all(vp.AUTOMATION_NAME_RE.match(name) for _, _, name in tests)
    assert (vp.pytest_case_count(ROOT / "tests") or 0) >= len(list((ROOT / "tests").glob("test_*.py")))


def test_30_non_utf8_build_cs_and_docs_do_not_crash(clean_root: Path):
    build = clean_root / "Plugins" / "CatalogAnchor" / "Source" / "CatalogAnchor" / "CatalogAnchor.Build.cs"
    build.write_bytes(build.read_bytes() + b"\n// coment\xe1rio cp1252\n")
    doc = clean_root / "docs" / "TECNOLOGIAS.md"
    doc.write_bytes(doc.read_bytes() + b"\nacentua\xe7\xe3o\n")
    assert vp.run_validation(clean_root).ok()



@pytest.mark.parametrize(
    ("platforms", "ok"),
    [(["Win64", "Mac", "Linux"], True), (["Win64", "Linux", "Android"], False), ([], False), (None, False)],
)
def test_26_uplugin_must_declare_all_desktop_platforms(clean_root: Path, platforms, ok: bool):
    def mutate(d):
        d.pop("SupportedTargetPlatforms")
        if platforms is not None:
            d["SupportedTargetPlatforms"] = platforms

    _edit_json(_uplugin(clean_root), mutate)
    result = vp.run_validation(clean_root)
    assert result.ok() is ok, result.errors
    assert ok or any("SupportedTargetPlatforms" in e for e in result.errors)


def test_27_include_case_mismatch_is_error_but_exact_and_engine_are_fine(clean_root: Path):
    _cpp(clean_root, "Public/CatalogItem.h", "struct FItem {};\n")
    _cpp(clean_root, "Private/Ok.cpp", '#include "CatalogItem.h"\n#include "CoreMinimal.h"\n#include "Misc/Guid.h"\n')
    assert vp.run_validation(clean_root).ok()
    bad = _cpp(clean_root, "Private/Bad.cpp", '#include "catalogitem.h"\n')
    result = vp.run_validation(clean_root)
    assert any(e.startswith(f"{bad}:2:") and "caixa" in e for e in result.errors), result.errors
    assert not any("Ok.cpp" in e for e in result.errors)


def test_28_include_case_checks_subfolder_and_parent_paths(clean_root: Path):
    _cpp(clean_root, "Public/Sub/Thing.h", "struct FThing {};\n")
    bad = _cpp(clean_root, "Private/Bad.cpp", '#include "sub/Thing.h"\n#include "../Public/Sub/thing.h"\n')
    result = vp.run_validation(clean_root)
    assert sum(e.startswith(bad) for e in result.errors) == 2, result.errors


def test_29_windows_only_code_inside_platform_guard_is_fine(clean_root: Path):
    body = (
        "#if PLATFORM_WINDOWS\n#include <windows.h>\nint A = _WIN32;\n#else\nint B = 0;\n#endif\n"
        "#if defined(_WIN32)\n__declspec(dllexport) int C;\n#endif\n"
    )
    _cpp(clean_root, "Private/Guarded.cpp", body)
    assert vp.run_validation(clean_root).ok()


@pytest.mark.parametrize(
    "line",
    [
        '#include "Windows/WindowsHWrapper.h"',
        "#include <Windows.h>",
        "#if !PLATFORM_WINDOWS && _WIN32",
        "__declspec(dllexport) int X;",
        "#if _MSC_VER > 1900",
        'FString P = TEXT("Saved' + chr(92) * 2 + 'Slots");',
    ],
)
def test_30_windows_only_outside_guard_is_error(clean_root: Path, line: str):
    rel = _cpp(clean_root, "Private/Win.cpp", line + "\n")
    result = vp.run_validation(clean_root)
    assert any(e.startswith(f"{rel}:2:") and "Windows" in e for e in result.errors), result.errors


def test_31_else_branch_of_windows_guard_is_not_windows(clean_root: Path):
    rel = _cpp(clean_root, "Private/Else.cpp", "#if PLATFORM_WINDOWS\nint A;\n#else\nint B = _MSC_VER;\n#endif\n")
    result = vp.run_validation(clean_root)
    assert [e.split(":")[1] for e in result.errors if e.startswith(rel)] == ["5"], result.errors
