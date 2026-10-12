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

## Convenções de C++ (o validador reprova violações)

- Primeira linha: `// Copyright 2026 Geraliza Castlevania. MIT License.`
- Headers: `#pragma once` logo depois; `#include "X.generated.h"` sempre por último.
- Indentação com TAB (estilo Epic; ver `.clang-format`). Fontes com acento em UTF-8 **com BOM**.
- Tipos exportados em `Public/` com `CATALOGANCHOR_API` / `CATALOGANCHOREDITOR_API`.
- Log em `LogCatalogAnchor`, nunca `LogTemp`.
- Gameplay é decidido no servidor; o cliente pede por Server RPC.
- Save: **nunca mude o default de um campo existente** (regra de delta, ver `CatalogSaveTypes.h`).
  Mudou o layout? Incremente `GCatalogSaveSchemaVersion` e adicione o `case` de migração com teste.
- Lógica nova não trivial vem com teste de automação (`Private/Tests/`, `CatalogAnchor.<Sistema>.<Caso>`).

## Commits

Mensagens curtas no imperativo, e uma mudança lógica por commit. Atualize o `CHANGELOG.md` em `[Unreleased]`.
