# Contribuindo

## Antes de abrir um PR

```bash
python -m pip install -r requirements-dev.txt
```

```bash
python tools/validate_plugin.py
```

```bash
python -m pytest -q tests/
```

```bash
python -m ruff check .
```

Com Unreal Engine 5.8+ instalada, também rode `py -3 tools/ue.py build-plugin` e `py -3 tools/ue.py test`.
Sem Visual Studio, `py -3 tools/get_msvc.py` baixa o toolchain e `UE_SDKS_ROOT` aponta o UBT para ele.

Antes do PR, rode também `python tools/validate_plugin.py --strict-docs`: as contagens de testes citadas no
README e em `docs/` precisam bater com o código.

Mexeu em header público (`Public/*.h`) ou nos comentários `/** */`? Regenere a referência com
`python tools/gen_api_docs.py` e commite `docs/API.md` (não edite à mão); `--check` confere sem escrever, e o pytest
reprova um `API.md` defasado. Escreva o comentário `/** */` logo antes de cada tipo, propriedade e função pública.

## Convenções de C++ (o validador reprova violações)

- Primeira linha: `// Copyright 2026 Geraliza Castlevania. MIT License.`
- Headers: `#pragma once` logo depois; `#include "X.generated.h"` sempre por último.
- Indentação com TAB (estilo Epic; ver `.clang-format`). Fontes com acento em UTF-8 **com BOM**.
- Tipos exportados em `Public/` com `CATALOGANCHOR_API` / `CATALOGANCHOREDITOR_API`.
- Log em `LogCatalogAnchor`, nunca `LogTemp`.
- Gameplay é decidido no servidor; o cliente pede por Server RPC, sempre `WithValidation` com `_Validate`
  (o validador reprova a falta; `--no-strict-rpc` só para investigar).
- Save: **nunca mude o default de um campo existente** (regra de delta, ver `CatalogSaveTypes.h`).
  Mudou o layout? Incremente `GCatalogSaveSchemaVersion` e adicione o `case` de migração com teste.
- Lógica nova não trivial vem com teste de automação (`Private/Tests/`, `CatalogAnchor.<Sistema>.<Caso>`).

## Commits

Mensagens curtas no imperativo, e uma mudança lógica por commit. Atualize o `CHANGELOG.md` em `[Unreleased]`.

## Pull requests, segurança e conduta

Os templates de PR e de issue estão em `.github/`. Vulnerabilidades: [SECURITY.md](SECURITY.md) (relato privado,
sem issue pública). Conduta: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Uma tag `v*` dispara `release.yml`, e ela
precisa bater com o `VersionName` do `.uplugin`.
