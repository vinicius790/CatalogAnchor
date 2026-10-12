"""Lint de estilo C++ do validador: regras 1–5 do contrato, cada uma com caso bom e ruim."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import validate_plugin as vp  # noqa: E402

PUBLIC_H = "Plugins/CatalogAnchor/Source/CatalogAnchor/Public/Foo.h"
PRIVATE_CPP = "Plugins/CatalogAnchor/Source/CatalogAnchor/Private/Foo.cpp"

GOOD_HEADER = "\n".join(
    [
        vp.COPYRIGHT_LINE,
        "#pragma once",
        "",
        '#include "CoreMinimal.h"',
        '#include "Foo.generated.h"',
        "",
        "/** Doc. Nunca use LogTemp aqui: comentário não conta. */",
        "UCLASS(BlueprintType)",
        "class CATALOGANCHOR_API UFoo : public UObject",
        "{",
        "\tGENERATED_BODY()",
        "};",
        "",
    ]
)

GOOD_CPP = "\n".join(
    [
        vp.COPYRIGHT_LINE,
        '#include "Foo.h"',
        '#include "CatalogAnchor.h"',
        "",
        "void Foo()",
        "{",
        '\tUE_LOG(LogCatalogAnchor, Log, TEXT("ok")); // LogTemp em comentário é permitido',
        "}",
        "",
    ]
)


def _lint(root: Path, rel: str, text: str, encoding: str = "utf-8-sig") -> list[str]:
    """Padrão com BOM, como o contrato pede para fontes com acento (os textos abaixo têm acento)."""
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode(encoding))  # bytes: o Windows não traduz LF -> CRLF
    return vp.lint_cpp_tree(root)


def test_good_header_and_cpp_pass(tmp_path: Path):
    _lint(tmp_path, PUBLIC_H, GOOD_HEADER)
    assert _lint(tmp_path, PRIVATE_CPP, GOOD_CPP) == []


def test_bom_and_crlf_are_tolerated(tmp_path: Path):
    assert _lint(tmp_path, PUBLIC_H, GOOD_HEADER.replace("\n", "\r\n"), encoding="utf-8-sig") == []


def test_non_ascii_without_bom_fails(tmp_path: Path):
    assert _lint(tmp_path, PRIVATE_CPP, GOOD_CPP, encoding="utf-8") == [f"{PRIVATE_CPP}:1: {vp.NON_ASCII_WITHOUT_BOM}"]


def test_ascii_only_file_needs_no_bom(tmp_path: Path):
    text = GOOD_CPP.replace(" // LogTemp em comentário é permitido", "")
    assert text.isascii()
    assert _lint(tmp_path, PRIVATE_CPP, text, encoding="utf-8") == []


def test_rule1_copyright_must_be_first_line(tmp_path: Path):
    errors = _lint(tmp_path, PRIVATE_CPP, "// Copyright Epic Games\n" + GOOD_CPP)
    assert errors == [f"{PRIVATE_CPP}:1: primeira linha deve ser exatamente {vp.COPYRIGHT_LINE!r}"]


def test_rule2_header_needs_pragma_once_after_copyright(tmp_path: Path):
    errors = _lint(tmp_path, PUBLIC_H, GOOD_HEADER.replace("#pragma once\n", "", 1))
    assert any(e.startswith(f"{PUBLIC_H}:2:") and "#pragma once" in e for e in errors), errors


def test_rule2_does_not_apply_to_cpp(tmp_path: Path):
    assert _lint(tmp_path, PRIVATE_CPP, GOOD_CPP) == []


def test_rule3_generated_include_must_be_last(tmp_path: Path):
    text = GOOD_HEADER.replace('#include "Foo.generated.h"', '#include "Foo.generated.h"\n#include "Late.h"')
    errors = _lint(tmp_path, PUBLIC_H, text)
    assert errors == [f'{PUBLIC_H}:6: o último #include deve ser "Foo.generated.h"']


def test_rule3_uht_header_without_generated_include_fails(tmp_path: Path):
    errors = _lint(tmp_path, PUBLIC_H, GOOD_HEADER.replace('#include "Foo.generated.h"\n', ""))
    assert any("Foo.generated.h" in e for e in errors), errors


def test_rule3_plain_header_needs_no_generated_include(tmp_path: Path):
    text = f'{vp.COPYRIGHT_LINE}\n#pragma once\n\n#include "CoreMinimal.h"\n\nstruct FPlain\n{{\n}};\n'
    assert _lint(tmp_path, PUBLIC_H, text) == []


def test_rule4_four_space_indent_fails(tmp_path: Path):
    errors = _lint(tmp_path, PUBLIC_H, GOOD_HEADER.replace("\tGENERATED_BODY()", "    GENERATED_BODY()"))
    assert errors == [f"{PUBLIC_H}:11: indentação com espaços; use TAB"]


@pytest.mark.parametrize(
    ("line", "bad"),
    [
        ('\tUE_LOG(LogTemp, Warning, TEXT("x"));', True),
        ("\t/* LogTemp só em comentário */", False),
        ('\tUE_LOG(LogTemplateThing, Log, TEXT("x"));', False),
    ],
)
def test_rule4b_logtemp_forbidden_in_code_only(tmp_path: Path, line: str, bad: bool):
    errors = _lint(tmp_path, PRIVATE_CPP, GOOD_CPP.replace("}\n", f"{line}\n}}\n", 1))
    assert any("LogTemp" in e and ":8:" in e for e in errors) is bad, errors


@pytest.mark.parametrize("macro", ["UCLASS()", "USTRUCT(BlueprintType)"])
def test_rule5_public_type_without_api_macro_fails(tmp_path: Path, macro: str):
    keyword = "struct" if macro.startswith("USTRUCT") else "class"
    text = GOOD_HEADER.replace("UCLASS(BlueprintType)", macro).replace(
        "class CATALOGANCHOR_API UFoo : public UObject", f"{keyword} UFoo"
    )
    errors = _lint(tmp_path, PUBLIC_H, text)
    assert errors == [f"{PUBLIC_H}:9: {macro.split('(')[0]} em header Public/ precisa de CATALOGANCHOR_API"]


def test_rule5_multiline_macro_is_followed_to_the_class(tmp_path: Path):
    text = GOOD_HEADER.replace("UCLASS(BlueprintType)", "UCLASS(\n\tBlueprintType)").replace("CATALOGANCHOR_API ", "")
    errors = _lint(tmp_path, PUBLIC_H, text)
    assert any(":10:" in e and "CATALOGANCHOR_API" in e for e in errors), errors


def test_rule5_uinterface_class_is_exempt(tmp_path: Path):
    text = GOOD_HEADER.replace("UCLASS(BlueprintType)", "UINTERFACE(MinimalAPI, Blueprintable)").replace(
        "class CATALOGANCHOR_API UFoo : public UObject", "class UFoo : public UInterface"
    )
    assert _lint(tmp_path, PUBLIC_H, text) == []


def test_rule5_private_header_is_exempt(tmp_path: Path):
    rel = "Plugins/CatalogAnchor/Source/CatalogAnchor/Private/Foo.h"
    assert _lint(tmp_path, rel, GOOD_HEADER.replace("CATALOGANCHOR_API ", "")) == []


def test_rule5_uses_the_header_module_macro(tmp_path: Path):
    rel = "Plugins/CatalogAnchor/Source/CatalogAnchorEditor/Public/Foo.h"
    errors = _lint(tmp_path, rel, GOOD_HEADER)
    assert errors == [f"{rel}:9: UCLASS em header Public/ precisa de CATALOGANCHOREDITOR_API"]


def test_sample_project_sources_are_linted(tmp_path: Path):
    rel = "SampleProject/Source/SampleCatalog/SampleCatalog.cpp"
    assert _lint(tmp_path, rel, '#include "SampleCatalog.h"\n') == [
        f"{rel}:1: primeira linha deve ser exatamente {vp.COPYRIGHT_LINE!r}"
    ]


def test_build_output_dirs_are_skipped(tmp_path: Path):
    assert _lint(tmp_path, "Plugins/CatalogAnchor/Intermediate/Source/Gen.h", "lixo gerado\n") == []


def test_non_utf8_file_is_reported(tmp_path: Path):
    path = tmp_path / PRIVATE_CPP
    path.parent.mkdir(parents=True)
    path.write_bytes(vp.COPYRIGHT_LINE.encode() + b"\n// coment\xe1rio cp1252\n")
    assert vp.lint_cpp_tree(tmp_path) == [f"{PRIVATE_CPP}:1: arquivo não é UTF-8"]


def test_api_macro_for():
    assert vp.api_macro_for(Path(PUBLIC_H)) == "CATALOGANCHOR_API"
    assert vp.api_macro_for(Path("Plugins/CatalogAnchor/Source/CatalogAnchor/Private/Foo.h")) is None
    assert vp.api_macro_for(Path("SampleProject/Source/SampleCatalog/Public/Foo.h")) is None
