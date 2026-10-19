"""tools/gen_api_docs.py: parser dos headers públicos, saída determinística e --check no repo real."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import gen_api_docs as gen  # noqa: E402

HEADER = '''\ufeff// Copyright 2026 X. MIT License.
#pragma once
#include "CoreMinimal.h"
#include "CatalogFoo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogFooDone, AActor*, Who, TMap<int32, int32>, Map);

/** Motivo da falha. */
UENUM(BlueprintType)
enum class ECatalogFooReason : uint8
{
	/** Sem alvo. */
	None,
	Far = 3 UMETA(DisplayName = "Longe"),
	Last
};

/** Dados soltos. */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogFooData
{
	GENERATED_BODY()

	/** Quantidade. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1; x"))
	int32 Count = 1;

	int32 Hidden = 0;
};

UINTERFACE(MinimalAPI)
class UCatalogFooable : public UInterface
{
	GENERATED_BODY()
};

/** Contrato foo. */
class CATALOGANCHOR_API ICatalogFooable
{
	GENERATED_BODY()
public:
	/** Faz foo. */
	UFUNCTION(BlueprintNativeEvent)
	bool DoFoo(AActor* Who) const;
};

/**
 * Componente foo.
 * Segunda linha <junto>.
 *
 * - item um
 * - item dois
 */
UCLASS(ClassGroup = (Catalog), meta = (BlueprintSpawnableComponent))
class CATALOGANCHOR_API UCatalogFoo : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Vida. */
	UPROPERTY(BlueprintReadOnly, Category = "Foo")
	float Life = 1.f;

	/** Inline com corpo. */
	UFUNCTION(BlueprintPure, Category = "Foo")
	float GetLife() const { return Life; }

	/** Sem UFUNCTION: fora da API. */
	static float Helper(float X);

	UFUNCTION(BlueprintCallable)
	void Fire(FName Slot = NAME_None);

protected:
	UPROPERTY()
	int32 Secret = 0;

	UFUNCTION(Server, Reliable)
	void ServerSecret();

private:
	UFUNCTION()
	void Hidden();
};
'''


def types_by_name(text: str) -> dict[str, gen.TypeDoc]:
    return {t.name: t for t in gen.parse_header(text.replace("\ufeff", ""), "CatalogFoo.h")}


def test_parse_kinds_and_docs():
    t = types_by_name(HEADER)
    assert {n: x.kind for n, x in t.items()} == {
        "FCatalogFooDone": "delegate",
        "ECatalogFooReason": "enum",
        "FCatalogFooData": "struct",
        "ICatalogFooable": "interface",  # o stub UCatalogFooable do UHT não entra
        "UCatalogFoo": "class",
    }
    assert t["ECatalogFooReason"].doc == "Motivo da falha."
    assert t["UCatalogFoo"].signature == "class UCatalogFoo : public UActorComponent"
    assert t["UCatalogFoo"].doc.splitlines() == [r"Componente foo. Segunda linha \<junto>.", "- item um", "- item dois"]


def test_delegate_params_with_template_commas():
    d = types_by_name(HEADER)["FCatalogFooDone"]
    assert d.signature.startswith("void FCatalogFooDone(AActor* Who, TMap<int32, int32> Map)")


def test_enum_values_with_umeta_and_last_without_comma():
    values = types_by_name(HEADER)["ECatalogFooReason"].values
    assert [(v.decl, v.doc) for v in values] == [("None", "Sem alvo."), ("Far = 3", ""), ("Last", "")]


def test_struct_members_and_string_with_semicolon_in_specifier():
    s = types_by_name(HEADER)["FCatalogFooData"]
    assert [(p.decl, p.specs, p.doc) for p in s.props] == [
        ("int32 Count = 1", 'EditAnywhere, meta = (ClampMin = "1; x")', "Quantidade.")
    ]


def test_only_public_reflected_members():
    c = types_by_name(HEADER)["UCatalogFoo"]
    assert [p.decl for p in c.props] == ["float Life = 1.f"]
    assert [f.decl for f in c.funcs] == ["float GetLife() const", "void Fire(FName Slot = NAME_None)"]
    assert c.funcs[0].doc == "Inline com corpo."


def test_interface_function():
    f = types_by_name(HEADER)["ICatalogFooable"].funcs
    expected = [("bool DoFoo(AActor* Who) const", "BlueprintNativeEvent", "Faz foo.")]
    assert [(m.decl, m.specs, m.doc) for m in f] == expected


def _repo(tmp_path: Path, files: dict[str, str]) -> Path:
    pub = tmp_path / gen.PUBLIC_DIR
    pub.mkdir(parents=True)
    for name, text in files.items():
        (pub / name).write_bytes(text.encode("utf-8"))
    return tmp_path


def test_render_groups_by_area_and_is_deterministic(tmp_path):
    other = HEADER.replace("Foo", "Bar")
    repo = _repo(tmp_path, {"CatalogHealthComponent.h": HEADER, "CatalogUnknown.h": other, "CatalogSaveGame.h": other})
    out = gen.generate(repo)
    assert out == gen.generate(repo)
    assert "\r" not in out and out.endswith("\n") and not out.endswith("\n\n")
    assert out.index("## Vida") < out.index("## Save") < out.index("## Outros")  # ordem de AREAS; "Outros" por último
    assert "- [`UCatalogFoo`](#ucatalogfoo) (classe)" in out


def test_crlf_and_bom_headers_give_same_output(tmp_path):
    a = gen.generate(_repo(tmp_path / "a", {"CatalogHealthComponent.h": HEADER}))
    b = gen.generate(_repo(tmp_path / "b", {"CatalogHealthComponent.h": HEADER.replace("\n", "\r\n")}))
    assert a == b


def test_check_detects_stale_and_missing(tmp_path, capsys):
    repo = _repo(tmp_path, {"CatalogHealthComponent.h": HEADER})
    args = ["--root", str(repo)]
    assert gen.main([*args, "--check"]) == 1  # sem API.md
    assert gen.main(args) == 0
    assert gen.main([*args, "--check"]) == 0
    api = repo / gen.DEFAULT_OUT
    api.write_bytes(api.read_bytes().replace(b"\n", b"\r\n"))  # checkout com CRLF não conta como defasado
    assert gen.main([*args, "--check"]) == 0
    api.write_bytes(api.read_bytes() + b"x\n")
    assert gen.main([*args, "--check"]) == 1
    assert "desatualizado" in capsys.readouterr().err


def test_missing_public_dir_is_error(tmp_path):
    assert gen.main(["--root", str(tmp_path)]) == 2


def test_real_repo_api_md_is_up_to_date():
    assert gen.main(["--root", str(ROOT), "--check"]) == 0


def test_real_repo_covers_every_reflected_type():
    out = gen.generate(ROOT)
    for name in ("UCatalogInteractionComponent", "ICatalogInteractable", "UCatalogSaveSubsystem", "FCatalogDeath",
                 "ECatalogSaveSlotState", "UCatalogBlueprintLibrary"):
        assert f"### {name}\n" in out
