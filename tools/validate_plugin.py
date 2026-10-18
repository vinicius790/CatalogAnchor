#!/usr/bin/env python3
"""Host-side validator for the CatalogAnchor UE5 plugin manifest tree + lint de estilo C++.

Does NOT invoke UBT or the Unreal Editor. Safe for CI without an engine install.
Exit 0 on success; print violations and exit 1 on failure.
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path, PurePosixPath
from typing import Any

SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")
ENGINE_VERSION_RE = re.compile(r"^(\d+)\.(\d+)(?:\.\d+)?$")
MIN_ENGINE = (5, 8)  # alvo: UE 5.8; 5.9/5.10/6.x continuam aceitos
FORBIDDEN_DIR_NAMES = {
    "Binaries",
    "Intermediate",
    "DerivedDataCache",
    "Saved",
}
SCAN_SKIP_DIR_NAMES = {".git", "__pycache__", ".venv", ".pytest_cache", "dist"}
SAMPLE_PLUGIN_DIR = "../Plugins"
# Módulos UE que vivem em plugins da engine: usá-los num Build.cs exige declarar o plugin no .uplugin.
MODULE_TO_ENGINE_PLUGIN = {
    "DataValidation": "DataValidation",
    "GameplayAbilities": "GameplayAbilities",
}
CS_COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)  # mesma sintaxe de comentário em C++

# Lint de C++ (regras 1–5 do contrato de estilo).
COPYRIGHT_LINE = "// Copyright 2026 Geraliza Castlevania. MIT License."
CPP_SOURCE_GLOBS = ("Plugins/**/Source/**/*", "SampleProject/Source/**/*")
CPP_SUFFIXES = {".h", ".cpp"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
UHT_MACRO_RE = re.compile(r"^\s*(UCLASS|USTRUCT|UINTERFACE|UENUM)\s*\(")
TYPE_DECL_RE = re.compile(r"^\s*(class|struct)\s")
LOGTEMP_RE = re.compile(r"\bLogTemp\b")
# Testes de automação (nome único com prefixo), contagens citadas nos docs e RPCs de servidor.
# CUSTOM_* leva a classe-base antes do nome; o nome é sempre o primeiro literal da chamada.
AUTOMATION_TEST_RE = re.compile(
    r'^[ \t]*IMPLEMENT_\w*AUTOMATION_TEST\s*\(\s*\w+\s*,\s*(?:\w+\s*,\s*)?"([^"\n]*)"', re.MULTILINE
)
AUTOMATION_NAME_RE = re.compile(r"^CatalogAnchor(\.[A-Za-z0-9_]+){2,}$")
DOC_AUTOMATION_COUNT_RE = re.compile(r"(\d+)\s+testes\s+(?:C\+\+|de automação|verdes)")
DOC_PYTEST_COUNT_RE = re.compile(r"(\d+)\s+casos\s+de\s+pytest|pytest\s*\(\s*(\d+)\s+casos\s*\)")
UFUNCTION_RE = re.compile(r"\bUFUNCTION\s*\(")
FUNCTION_NAME_RE = re.compile(r"(\w+)\s*\(")
UTF8_BOM = b"\xef\xbb\xbf"
# Sem BOM o MSVC pode ler o fonte na codepage do Windows e corromper acentos em TEXT()/LOCTEXT.
NON_ASCII_WITHOUT_BOM = "arquivo com caractere não-ASCII precisa começar com BOM UTF-8"


def repo_root_from(start: Path | None = None) -> Path:
    if start is None:
        start = Path(__file__).resolve().parent.parent
    return start.resolve()


def load_json(path: Path) -> Any:
    # bytes: o json detecta UTF-8 (com ou sem BOM) e UTF-16/32, como o leitor de descritores da UE.
    return json.loads(path.read_bytes())


def versioned_files(root: Path) -> list[Path] | None:
    """Arquivos que um commit levaria (tracked + untracked não ignorados), relativos a root.

    None quando root não está num repositório git (ou git não está instalado).
    """
    try:
        proc = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            capture_output=True,
            check=False,
        )
    except OSError:
        return None
    if proc.returncode != 0:
        return None
    return [Path(p) for p in proc.stdout.decode("utf-8", "surrogateescape").split("\0") if p]


def find_forbidden_dirs(root: Path) -> tuple[list[str], bool]:
    """(pastas proibidas, usou_git). Sem git, varre o disco inteiro."""
    files = versioned_files(root)
    hits: set[str] = set()
    if files is not None:
        # Em git, pastas de build locais já ignoradas pelo .gitignore não contam.
        for rel in files:
            for depth, part in enumerate(rel.parts[:-1]):
                if part in FORBIDDEN_DIR_NAMES:
                    hits.add(Path(*rel.parts[: depth + 1]).as_posix())
                    break
        return sorted(hits), True

    for p in root.rglob("*"):
        if not p.is_dir():
            continue
        rel = p.relative_to(root)
        if SCAN_SKIP_DIR_NAMES.intersection(rel.parts):
            continue
        if p.name in FORBIDDEN_DIR_NAMES:
            hits.add(rel.as_posix())
    return sorted(hits), False


def engine_at_least_min(value: Any) -> bool:
    """True para "5.8", "5.10", "6.0", "5.8.1"...; False para versões antigas ou valores que não são versão."""
    match = ENGINE_VERSION_RE.match(value) if isinstance(value, str) else None
    return match is not None and (int(match[1]), int(match[2])) >= MIN_ENGINE


class ValidationResult:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def ok(self) -> bool:
        return not self.errors

    def fail(self, msg: str) -> None:
        self.errors.append(msg)

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)


def validate_uplugin(data: dict[str, Any], result: ValidationResult) -> None:
    if data.get("FileVersion") != 3:
        result.fail(f"FileVersion deve ser 3, veio {data.get('FileVersion')!r}")

    version_name = data.get("VersionName")
    if not isinstance(version_name, str) or not SEMVER_RE.match(version_name):
        result.fail(f"VersionName semver inválido: {version_name!r}")

    # Opcional: presente, a engine avisa "feito para outra versão" em toda versão nova.
    if "EngineVersion" in data and not engine_at_least_min(data["EngineVersion"]):
        result.fail(f"EngineVersion {data['EngineVersion']!r} inválida: remova a chave (recomendado) ou use ≥ 5.8")

    modules = data.get("Modules")
    if not isinstance(modules, list) or len(modules) < 2:
        result.fail("Modules: espere Runtime + Editor (≥2)")
        return

    names = {m.get("Name") for m in modules if isinstance(m, dict)}
    types = {m.get("Name"): m.get("Type") for m in modules if isinstance(m, dict)}
    if "CatalogAnchor" not in names:
        result.fail("Módulo Runtime 'CatalogAnchor' ausente")
    elif types.get("CatalogAnchor") != "Runtime":
        result.fail("CatalogAnchor deve ter Type=Runtime")
    if "CatalogAnchorEditor" not in names:
        result.fail("Módulo Editor 'CatalogAnchorEditor' ausente")
    elif types.get("CatalogAnchorEditor") != "Editor":
        result.fail("CatalogAnchorEditor deve ter Type=Editor")


def build_cs_dependencies(text: str) -> set[str]:
    """Nomes de módulo citados como string num Build.cs, ignorando comentários."""
    code = CS_COMMENT_RE.sub("", text)
    return set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"', code))


def validate_plugin_dependencies(
    uplugin: dict[str, Any], build_texts: list[str], result: ValidationResult
) -> None:
    enabled = {
        p.get("Name")
        for p in uplugin.get("Plugins") or []
        if isinstance(p, dict) and p.get("Enabled") is True
    }
    used: set[str] = set()
    for text in build_texts:
        used |= build_cs_dependencies(text)
    for module, plugin in sorted(MODULE_TO_ENGINE_PLUGIN.items()):
        if module in used and plugin not in enabled:
            result.fail(
                f"Build.cs depende do módulo {module!r}: o .uplugin precisa de "
                f'{{"Name": "{plugin}", "Enabled": true}} em Plugins'
            )


def validate_uproject(data: dict[str, Any], result: ValidationResult) -> None:
    assoc = data.get("EngineAssociation")
    if not engine_at_least_min(assoc):
        result.fail(f"Sample EngineAssociation deve ser versão ≥ 5.8 (ex.: '5.8', '5.9', '6.0'), veio {assoc!r}")
    plugins = data.get("Plugins") or []
    enabled = {
        p.get("Name")
        for p in plugins
        if isinstance(p, dict) and p.get("Enabled") is True
    }
    if "CatalogAnchor" not in enabled:
        result.fail("SampleProject deve habilitar plugin CatalogAnchor")
    extra_dirs = data.get("AdditionalPluginDirectories") or []
    normalized = {str(d).replace("\\", "/").rstrip("/") for d in extra_dirs}
    if SAMPLE_PLUGIN_DIR not in normalized:
        result.fail(
            f"SampleProject deve ter AdditionalPluginDirectories com {SAMPLE_PLUGIN_DIR!r} "
            "(acha o plugin da raiz sem symlink/cópia)"
        )


def validate_tree(root: Path, result: ValidationResult) -> dict[str, Path]:
    plugin = root / "Plugins" / "CatalogAnchor"
    paths = {
        "uplugin": plugin / "CatalogAnchor.uplugin",
        "runtime_build": plugin / "Source" / "CatalogAnchor" / "CatalogAnchor.Build.cs",
        "editor_build": plugin / "Source" / "CatalogAnchorEditor" / "CatalogAnchorEditor.Build.cs",
        "interaction_h": plugin / "Source" / "CatalogAnchor" / "Public" / "CatalogInteractionComponent.h",
        "subsystem_h": plugin / "Source" / "CatalogAnchor" / "Public" / "CatalogGameplaySubsystem.h",
        "uproject": root / "SampleProject" / "SampleCatalog.uproject",
        "tecnologias": root / "docs" / "TECNOLOGIAS.md",
        "instalacao": root / "docs" / "INSTALACAO.md",
    }
    for key, path in paths.items():
        if not path.is_file():
            result.fail(f"arquivo obrigatório ausente [{key}]: {path.relative_to(root)}")

    duplicate = root / "SampleProject" / "Plugins" / "CatalogAnchor"
    if duplicate.exists():
        result.fail(
            "SampleProject/Plugins/CatalogAnchor duplica o plugin já achado via "
            "AdditionalPluginDirectories — remova a cópia/symlink"
        )
    return paths


def validate_build_cs(path: Path, module_name: str, result: ValidationResult) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")  # Build.cs em cp1252: erro de regra, não traceback
    code = CS_COMMENT_RE.sub("", text)
    if not re.search(rf"\bclass\s+{re.escape(module_name)}\s*:\s*ModuleRules\b", code):
        result.fail(f"{path.name}: classe {module_name} : ModuleRules não encontrada")
    if "PublicDependencyModuleNames" not in code and "PrivateDependencyModuleNames" not in code:
        result.fail(f"{path.name}: sem DependencyModuleNames")


def validate_module_coherence(plugin_dir: Path, uplugin: dict[str, Any], result: ValidationResult) -> None:
    """Cada módulo do .uplugin tem Source/<Nome>/<Nome>.Build.cs e cada pasta de Source/ está no .uplugin."""
    source = plugin_dir / "Source"
    manifest = f"{plugin_dir.name}/{plugin_dir.name}.uplugin"
    declared = {m.get("Name") for m in uplugin.get("Modules") or [] if isinstance(m, dict)}
    for name in sorted(n for n in declared if isinstance(n, str)):
        if not (source / name / f"{name}.Build.cs").is_file():
            result.fail(f"{manifest}: módulo {name!r} sem Source/{name}/{name}.Build.cs")
    if source.is_dir():
        for folder in sorted(p.name for p in source.iterdir() if p.is_dir()):
            if folder not in declared:
                result.fail(f"{manifest}: pasta Source/{folder} não está declarada em Modules")


def blank_comments(text: str) -> str:
    """Troca comentários por espaços, preservando as quebras de linha (números de linha continuam valendo)."""
    return CS_COMMENT_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)


def api_macro_for(rel: Path) -> str | None:
    """<MODULO>_API exigida em headers Public/ de plugin; None quando a regra 5 não se aplica."""
    parts = rel.parts
    if parts[0] != "Plugins" or rel.suffix != ".h" or "Source" not in parts:
        return None
    i = parts.index("Source")
    if "Public" not in parts[i + 2 : -1]:
        return None
    return f"{parts[i + 1].upper()}_API"


def lint_cpp_text(rel: str, text: str, api_macro: str | None = None) -> list[str]:
    """Regras de estilo 1–5 sobre um .h/.cpp (texto com quebras '\\n'). Mensagens 'arquivo:linha: ...'."""
    errors: list[str] = []
    is_header = rel.endswith(".h")
    lines = text.split("\n")
    code_lines = blank_comments(text).split("\n")

    if lines[0] != COPYRIGHT_LINE:
        errors.append(f"{rel}:1: primeira linha deve ser exatamente {COPYRIGHT_LINE!r}")
    if is_header and (len(lines) < 2 or lines[1].strip() != "#pragma once"):
        errors.append(f"{rel}:2: '#pragma once' deve vir logo após o copyright")

    includes: list[tuple[int, str]] = []
    first_uht_line = 0
    pending_macro: str | None = None  # macro UHT aguardando a linha class/struct que ela anota
    for no, (raw, code) in enumerate(zip(lines, code_lines, strict=True), start=1):
        if raw.startswith("    "):
            errors.append(f"{rel}:{no}: indentação com espaços; use TAB")
        if LOGTEMP_RE.search(code):
            errors.append(f"{rel}:{no}: LogTemp proibido; use a categoria do módulo (LogCatalogAnchor)")
        include = INCLUDE_RE.match(code)
        if include:
            includes.append((no, include.group(1)))
        macro = UHT_MACRO_RE.match(code)
        if macro:
            pending_macro = macro.group(1)
            first_uht_line = first_uht_line or no
        elif pending_macro and TYPE_DECL_RE.match(code):
            # UINTERFACE fica de fora: a classe U da interface usa MinimalAPI, não a macro de API.
            exported = pending_macro in ("UCLASS", "USTRUCT")
            if api_macro and exported and not re.search(rf"\b{api_macro}\b", code):
                errors.append(f"{rel}:{no}: {pending_macro} em header Public/ precisa de {api_macro}")
            pending_macro = None

    if is_header:
        expected = f"{PurePosixPath(rel).stem}.generated.h"
        uses_uht = first_uht_line or any(inc.endswith(".generated.h") for _, inc in includes)
        if uses_uht and (not includes or includes[-1][1] != expected):
            line = includes[-1][0] if includes else first_uht_line
            errors.append(f'{rel}:{line}: o último #include deve ser "{expected}"')
    return errors


def cpp_source_files(root: Path) -> list[Path]:
    found: set[Path] = set()
    for pattern in CPP_SOURCE_GLOBS:
        for path in root.glob(pattern):
            rel_parts = path.relative_to(root).parts
            if path.suffix in CPP_SUFFIXES and path.is_file() and not FORBIDDEN_DIR_NAMES.intersection(rel_parts):
                found.add(path)
    return sorted(found)


def lint_cpp_tree(root: Path) -> list[str]:
    """Lint de todos os .h/.cpp de Plugins/**/Source e SampleProject/Source."""
    errors: list[str] = []
    for path in cpp_source_files(root):
        rel = path.relative_to(root)
        try:
            # utf-8-sig: o Visual Studio às vezes grava BOM; universal newlines normaliza CRLF.
            text = path.read_text(encoding="utf-8-sig")
        except UnicodeDecodeError:
            errors.append(f"{rel.as_posix()}:1: arquivo não é UTF-8")
            continue
        if not text.isascii() and not path.read_bytes().startswith(UTF8_BOM):
            errors.append(f"{rel.as_posix()}:1: {NON_ASCII_WITHOUT_BOM}")
        errors += lint_cpp_text(rel.as_posix(), text, api_macro_for(rel))
    return errors


def cpp_texts(root: Path) -> dict[str, str]:
    """Caminho relativo -> texto sem comentários, de todos os .h/.cpp do lint (não-UTF-8 o lint já reporta)."""
    texts: dict[str, str] = {}
    for path in cpp_source_files(root):
        try:
            texts[path.relative_to(root).as_posix()] = blank_comments(path.read_text(encoding="utf-8-sig"))
        except UnicodeDecodeError:
            pass
    return texts


def line_of(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def automation_tests(texts: dict[str, str]) -> list[tuple[str, int, str]]:
    """(arquivo, linha, nome) de cada IMPLEMENT_*_AUTOMATION_TEST."""
    return [
        (rel, line_of(text, m.start()), m[1])
        for rel, text in texts.items()
        for m in AUTOMATION_TEST_RE.finditer(text)
    ]


def validate_automation_names(tests: list[tuple[str, int, str]], result: ValidationResult) -> None:
    seen: dict[str, str] = {}
    for rel, line, name in tests:
        where = f"{rel}:{line}"
        if not AUTOMATION_NAME_RE.match(name):
            result.fail(f"{where}: teste de automação {name!r} fora do padrão CatalogAnchor.<Área>.<Caso>")
        if name in seen:
            result.fail(f"{where}: teste de automação {name!r} duplicado (já em {seen[name]})")
        seen.setdefault(name, where)


def ufunction_specifiers(text: str, open_idx: int) -> tuple[set[str], int]:
    """Nomes (antes do '=') dos especificadores do UFUNCTION( ... ) aberto em open_idx e o índice após o ')'."""
    depth, in_str, cur, parts = 0, False, "", []
    for i in range(open_idx, len(text)):
        c = text[i]
        if in_str:
            in_str = c != '"'  # shortcut: sem aspas escapadas, UFUNCTION não usa
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
            if depth == 1:
                continue
        elif c == ")":
            depth -= 1
            if depth == 0:
                parts.append(cur)
                return {p.split("=")[0].strip() for p in parts}, i + 1
        elif c == "," and depth == 1:
            parts.append(cur)
            cur = ""
            continue
        cur += c
    return set(), len(text)  # parêntese sem fechar: o compilador reclama


def validate_server_rpcs(texts: dict[str, str], report: Callable[[str], None]) -> None:
    """Todo UFUNCTION(Server...) tem WithValidation e a função <Nome>_Validate definida."""
    all_code = "\n".join(texts.values())
    for rel, text in texts.items():
        if not rel.endswith(".h"):
            continue
        for m in UFUNCTION_RE.finditer(text):
            specs, end = ufunction_specifiers(text, m.end() - 1)
            decl = FUNCTION_NAME_RE.search(text, end)
            if "Server" not in specs or decl is None:
                continue
            name, where = decl[1], f"{rel}:{line_of(text, m.start())}"
            if "WithValidation" not in specs:
                report(f"{where}: UFUNCTION(Server) {name}() sem WithValidation (valide a entrada do cliente)")
            elif not re.search(rf"\b{name}_Validate\s*\(", all_code):
                report(f"{where}: {name}_Validate() não encontrada (exigida por WithValidation)")


def pytest_case_count(tests_dir: Path) -> int | None:
    """Casos que o pytest coletaria em tests/test_*.py, contados via AST; None quando não dá para contar.

    shortcut: parametrize só com lista/tupla literal e sem fixture com params, upgrade: pytest --collect-only.
    """
    total = 0
    for path in sorted(tests_dir.glob("test_*.py")):
        try:
            body = ast.parse(path.read_bytes()).body
        except SyntaxError:
            return None
        funcs = [n for n in body if isinstance(n, ast.FunctionDef)]
        for cls in (n for n in body if isinstance(n, ast.ClassDef) and n.name.startswith("Test")):
            funcs += [n for n in cls.body if isinstance(n, ast.FunctionDef)]
        for fn in funcs:
            if not fn.name.startswith("test"):
                continue
            cases = 1
            for dec in fn.decorator_list:
                if isinstance(dec, ast.Call) and getattr(dec.func, "attr", "") == "parametrize":
                    values = dec.args[1] if len(dec.args) > 1 else None
                    if not isinstance(values, ast.List | ast.Tuple):
                        return None
                    cases *= len(values.elts)
            total += cases
    return total


def doc_count_divergences(root: Path, automation: int, pytest_cases: int | None) -> list[str]:
    """Contagens de testes citadas no README/docs que não batem com a contagem real."""
    out: list[str] = []
    for doc in [root / "README.md", *sorted((root / "docs").glob("*.md"))]:
        if not doc.is_file():
            continue
        rel = doc.relative_to(root).as_posix()
        for no, line in enumerate(doc.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
            for m in DOC_AUTOMATION_COUNT_RE.finditer(line):
                if int(m[1]) != automation:
                    out.append(
                        f"{rel}:{no}: cita {m[1]} testes de automação, mas o código tem {automation} "
                        "(macros IMPLEMENT_*_AUTOMATION_TEST)"
                    )
            for m in DOC_PYTEST_COUNT_RE.finditer(line):
                cited = m[1] or m[2]
                if pytest_cases is not None and int(cited) != pytest_cases:
                    out.append(f"{rel}:{no}: cita {cited} casos de pytest, mas tests/ tem {pytest_cases}")
    return out


def validate_header_stub(path: Path, needle: str, result: ValidationResult) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")
    if needle not in text:
        result.fail(f"{path.name}: espere {needle!r}")
    if "#pragma once" not in text:
        result.fail(f"{path.name}: falta #pragma once")


def run_validation(
    root: Path | None = None, strict_docs: bool = False, strict_rpc: bool = True
) -> ValidationResult:
    root = repo_root_from(root)
    result = ValidationResult()

    paths = validate_tree(root, result)

    uplugin: dict[str, Any] | None = None
    if paths["uplugin"].is_file():
        try:
            data = load_json(paths["uplugin"])
            if not isinstance(data, dict):
                result.fail(".uplugin: raiz deve ser objeto JSON")
            else:
                uplugin = data
                validate_uplugin(uplugin, result)
                validate_module_coherence(paths["uplugin"].parent, uplugin, result)
        except ValueError as exc:  # JSONDecodeError e UnicodeDecodeError
            result.fail(f".uplugin JSON inválido: {exc}")

    if paths["uproject"].is_file():
        try:
            uproject = load_json(paths["uproject"])
            if not isinstance(uproject, dict):
                result.fail(".uproject: raiz deve ser objeto JSON")
            else:
                validate_uproject(uproject, result)
        except ValueError as exc:  # JSONDecodeError e UnicodeDecodeError
            result.fail(f".uproject JSON inválido: {exc}")

    build_texts: list[str] = []
    for key, module in (("runtime_build", "CatalogAnchor"), ("editor_build", "CatalogAnchorEditor")):
        if paths[key].is_file():
            validate_build_cs(paths[key], module, result)
            build_texts.append(paths[key].read_text(encoding="utf-8", errors="replace"))
    if uplugin is not None:
        validate_plugin_dependencies(uplugin, build_texts, result)

    if paths["interaction_h"].is_file():
        validate_header_stub(paths["interaction_h"], "UCatalogInteractionComponent", result)
    if paths["subsystem_h"].is_file():
        validate_header_stub(paths["subsystem_h"], "UCatalogGameplaySubsystem", result)

    forbidden, used_git = find_forbidden_dirs(root)
    for hit in forbidden:
        if used_git:
            result.fail(f"diretório proibido versionado: {hit}")
        else:
            result.fail(
                f"diretório proibido no disco: {hit} (git ausente, com erro ou fora de um repo: "
                "apague-o ou valide dentro de um repo git para o .gitignore valer)"
            )

    for doc_key in ("tecnologias", "instalacao"):
        doc_path = paths[doc_key]
        if doc_path.is_file():
            doc_text = doc_path.read_text(encoding="utf-8", errors="replace")
            if "5.8" not in doc_text:
                result.fail(f"{doc_path.name}: docs devem citar a versão alvo (Unreal Engine 5.8)")

    for err in lint_cpp_tree(root):
        result.fail(err)

    texts = cpp_texts(root)
    tests = automation_tests(texts)
    validate_automation_names(tests, result)
    # Aviso até o C++ do repo ganhar WithValidation/_Validate nos RPCs de servidor; depois, vira erro por padrão.
    validate_server_rpcs(texts, result.fail if strict_rpc else result.warn)
    report_doc = result.fail if strict_docs else result.warn
    for msg in doc_count_divergences(root, len(tests), pytest_case_count(root / "tests")):
        report_doc(msg)

    return result


def make_console_safe() -> None:
    # Console Windows (cp1252) não codifica "≥"/"→"/"✓": troca por escape em vez de quebrar.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(errors="backslashreplace")
        except (AttributeError, ValueError):
            pass  # stream substituído (ex.: captura do pytest)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Raiz do repo (default: pai de tools/)",
    )
    parser.add_argument(
        "--strict-docs",
        action="store_true",
        help="Contagens de testes citadas nos docs que divergem da real viram erro (default: aviso)",
    )
    parser.add_argument(
        "--strict-rpc",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="UFUNCTION(Server) sem WithValidation/_Validate é erro (default); --no-strict-rpc vira aviso",
    )
    args = parser.parse_args(argv)
    make_console_safe()
    result = run_validation(args.root, strict_docs=args.strict_docs, strict_rpc=args.strict_rpc)
    for warning in result.warnings:
        print(f"  ! aviso: {warning}", file=sys.stderr)
    if result.ok():
        print("validate_plugin: OK")
        return 0
    print("validate_plugin: FALHOU", file=sys.stderr)
    for err in result.errors:
        print(f"  - {err}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
