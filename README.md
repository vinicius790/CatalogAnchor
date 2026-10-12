# Catalog Anchor — plugin de gameplay para Unreal Engine 5.8+

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8%2B-313131?logo=unrealengine)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

Plugin **C++ Runtime + Editor** com sistemas de gameplay reutilizáveis, **prontos para multiplayer**:
interação com hold, inventário replicado, habilidades com cooldown, vida com dano/cura/morte, save versionado com migração
e Data Validation no Editor. Acompanha um **sample jogável montado 100% em C++**: nenhum
`.uasset`/`.umap` no repositório.

> Repositório-âncora da seção §8 (Unreal Engine 5) do meu catálogo de portfólio: *um plugin
> reutilizável, não "um jogo feito na UE5"*. Sem binários, sem FPS inventado — ver [LIMITATIONS](LIMITATIONS.md).

## Sistemas

| Sistema | Destaques |
|---|---|
| **Interação** | trace no servidor, **hold-to-interact** com progresso replicado ao dono, alvo bloqueado ("Trancado"), foco/highlight, cooldown anti-spam, Server/Client RPCs |
| **Inventário** | `UCatalogItemDefinition` (Primary Data Asset), lista em **`FFastArraySerializer`** (delta replication, `COND_OwnerOnly`), empilhamento por `MaxStackSize`, pickup interagível, drop por Server RPC |
| **Habilidades** | "GAS-lite" nativo: **Gameplay Tags nativas**, `UCatalogAbilityDefinition` + comportamento instanciado (Dash, Melee Sweep), cooldown replicado no relógio do servidor, cosmético por multicast |
| **Vida** | `UCatalogHealthComponent`: vida replicada para todos, escuta `ApplyDamage` (o Melee Sweep já causa dano), `Heal`, `ResetHealth`, `OnHealthChanged`/`OnDeath` no servidor e nos clientes |
| **Save** | `UCatalogSaveSubsystem` no GameInstance, **schema v2 com migração em cadeia** (v0→v1→v2), sync e **async**, autosave, participantes plugáveis (`ICatalogSaveParticipant`), metadata para menu de saves |
| **Editor** | validadores de itens, habilidades e templates de save; menu **Tools → Catalog Anchor → Validate Catalog Assets** |
| **Settings** | Project Settings → Plugins → Catalog Anchor (fonte única de todos os defaults) |

```mermaid
flowchart LR
    subgraph Pawn["Pawn do jogador"]
        IC[UCatalogInteractionComponent]
        INV[UCatalogInventoryComponent]
        AB[UCatalogAbilityComponent]
    end
    IC -- "trace (servidor)" --> I{{ICatalogInteractable}}
    I --> PK[ACatalogPickupActor]
    PK -- AddItem --> INV
    AB -- "Activate (servidor)" --> BH[UCatalogAbility_Dash / MeleeSweep]
    BH -- ApplyDamage --> HP[UCatalogHealthComponent]
    INV -- ICatalogSaveParticipant --> SS[(UCatalogSaveSubsystem<br/>GameInstance)]
    SS -- "SaveGame (.sav, schema v2)" --> DISK[(Disco)]
    ED[CatalogAnchorEditor<br/>Data Validation] -. valida .-> DA[Data Assets CA_*]
```

## Início rápido

**Usar no seu projeto:** copie `Plugins/CatalogAnchor/` para `SeuProjeto/Plugins/`, gere os project
files e compile o target Editor. Passo a passo em [docs/INSTALACAO.md](docs/INSTALACAO.md).

**Rodar o sample:** abra `SampleProject/SampleCatalog.uproject` (ele encontra o plugin via
`AdditionalPluginDirectories`) e aperte Play. Controles e detalhes em
[SampleProject/README.md](SampleProject/README.md).

**Compilar e testar com um comando** (máquina com UE 5.8+; outra versão instalada: `--version 5.9`):

```bash
py -3 tools/ue.py build-plugin
```

```bash
py -3 tools/ue.py test
```

## Qualidade

| Gate | Onde roda | O que cobre |
|---|---|---|
| `tools/validate_plugin.py` | CI (GitHub Actions) | manifestos `.uplugin`/`.uproject`, dependências de plugin, árvore, **lint de C++** (copyright, `#pragma once`, `.generated.h` por último, TABs, BOM em fonte com acento, `_API` em tipos exportados, sem `LogTemp`) |
| pytest (78 casos) | CI: Python 3.10–3.13, Ubuntu + Windows | validador, lint e `tools/ue.py` |
| ruff | CI | estilo do Python |
| Automation (18 testes C++) | sua máquina com UE: `tools/ue.py test` | migração de save, nomes de slot, empilhamento, cooldown, vida, progresso de hold, validadores |

> O CI **não** compila C++: não há Unreal Engine em runners públicos. A compilação e os testes de
> automação rodam na sua máquina com `tools/ue.py`.

## Estrutura

```
Plugins/CatalogAnchor/
  Source/CatalogAnchor/        Runtime: interação, inventário, habilidades, vida, save, settings
  Source/CatalogAnchorEditor/  Editor: validadores + menu
SampleProject/                 demo jogável 100% C++ (mapa Entry da engine)
docs/                          INSTALACAO · ARQUITETURA · TECNOLOGIAS
tools/                         validate_plugin.py · ue.py · dev.py
tests/                         pytest do tooling
```

## Documentação

- [docs/INSTALACAO.md](docs/INSTALACAO.md): instalação, compilação e checklist no Editor
- [docs/ARQUITETURA.md](docs/ARQUITETURA.md): fluxo de cada sistema e as decisões de rede e save
- [docs/TECNOLOGIAS.md](docs/TECNOLOGIAS.md): stack e decisões
- [CHANGELOG.md](CHANGELOG.md) · [CONTRIBUTING.md](CONTRIBUTING.md) · [LIMITATIONS.md](LIMITATIONS.md)

## Licença

MIT © 2026 Geraliza Castlevania
