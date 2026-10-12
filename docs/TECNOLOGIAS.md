# Tecnologias — Catalog Anchor 0.3

| Tecnologia | Uso |
|---|---|
| Unreal Engine **5.8** (mirando 5.9/6.0) | alvo do plugin (`.uplugin` sem `EngineVersion`) e do sample (`EngineAssociation` 5.8) |
| C++20 (subset UE) | módulos Runtime, Editor e do sample |
| UObject / UActorComponent / Subsystems | componentes de gameplay, `UCatalogSaveSubsystem` (GameInstance), `UCatalogGameplaySubsystem` (World) |
| Replicação: `FFastArraySerializer`, `COND_OwnerOnly`, Server/Client/Multicast RPC | inventário, interação, habilidades |
| Gameplay Tags nativas (`FNativeGameplayTag`) | identidade das habilidades sem asset de tags |
| Asset Manager / Primary Data Assets | `UCatalogItemDefinition`, `UCatalogAbilityDefinition` |
| USaveGame + `UGameplayStatics` (sync e async) | persistência com schema versionado |
| DeveloperSettings | Project Settings do plugin |
| DataValidation + ToolMenus + Asset Registry | validadores e menu do Editor |
| Enhanced Input (só no sample) | ações e mapping context criados em código |
| Automation Framework | 18 testes C++ de lógica pura |
| Python ≥ 3.10, pytest, ruff | validador, lint de C++, `tools/ue.py`, testes do tooling |
| GitHub Actions | ruff + validador + pytest (Ubuntu/Windows, Python 3.10–3.13) |

## Decisões

- **GAS-lite nativo em vez de GameplayAbilities:** o plugin não força o GAS em todo projeto. As
  tags, a definição em Data Asset e o cooldown replicado cobrem dash e melee. O `.uplugin` lista
  o GAS como dependência **opcional**, e o validador acusa se ele for ligado no `Build.cs` sem ser habilitado.
- **Save no GameInstance:** sobrevive à troca de mapa. O payload v2 tem `Records` opacos por
  participante para cada sistema evoluir o próprio formato sem subir o schema global.
- **FastArray no inventário:** replica só o que mudou e tem callbacks por item no cliente.
- **Sample sem assets:** tudo nasce em construtores (arena, malhas da engine, input). Itens e
  habilidades são subobjetos de CDO, com nome estável para a rede sem `.uasset`.
- **Sample acha o plugin via `AdditionalPluginDirectories`:** sem symlink nem cópia.
- **`.uplugin` sem `EngineVersion`:** com ela, cada engine nova marcaria o plugin como feito para outra
  versão. O mínimo (5.8) fica na descrição, nos docs e no validador (`EngineAssociation` ≥ 5.8).
- **BOM UTF-8 nos fontes com acento:** sem ele, o MSVC lê o arquivo na codepage do Windows.
- **Validador Python no CI:** os runners públicos não têm Unreal. O lint pega erros de convenção
  que quebrariam o UHT/UBT (ex.: `.generated.h` fora do lugar, falta de `_API`).
