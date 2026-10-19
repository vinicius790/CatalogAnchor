# Instalação

**Engine:** Unreal Engine **5.8** ou mais nova (5.9, 6.0: compatibilidade pretendida, ainda não verificada) +
Visual Studio 2022 17.14 (MSVC 14.44) ou 2026 (MSVC 14.50) com o workload *Game development with C++* e
Windows SDK ≥ 10.0.19041 (Windows), ou o toolchain equivalente no Linux/macOS (não verificado). Sem eles o
UBT falha com "Platform Win64 is not a valid platform to build".

Depois de instalar, siga o [QUICKSTART.md](QUICKSTART.md) (baú interativo + save em 10 passos) e consulte [API.md](API.md).

## Caminho A — usar o plugin no seu projeto

1. Copie `Plugins/CatalogAnchor/` para `SeuProjeto/Plugins/CatalogAnchor/`.
2. Feche o Editor → botão direito no `.uproject` → **Generate project files**.
3. Compile o target `*Editor` e abra o projeto. Em **Edit → Plugins**, confirme que
   **Catalog Anchor (UE5)** está ligado. Ele liga junto o plugin **Data Validation** da engine.
4. Para itens/habilidades como Data Assets, registre os tipos no Asset Manager
   (copie o bloco `[/Script/Engine.AssetManagerSettings]` de `SampleProject/Config/DefaultGame.ini`).
5. No seu Pawn: adicione `Catalog Interaction Component`, `Catalog Inventory Component` e
   `Catalog Ability Component`. Para segurar a interação, ligue a tecla Pressed→`Begin Interact`
   e Released→`End Interact`.

## Caminho B — sample jogável deste repositório

1. Clone o repositório **inteiro**: o sample usa `../Plugins` via `AdditionalPluginDirectories`.
   Não copie o plugin para `SampleProject/Plugins/` (plugin duplicado impede o projeto de abrir).
2. Botão direito em `SampleProject/SampleCatalog.uproject` → **Generate project files**.
3. Compile `SampleCatalogEditor`, abra e aperte **Play**.

> Há relatos de `AdditionalPluginDirectories` falhar ao **depurar o target Game direto do Visual
> Studio**. Se acontecer, remova a entrada do `.uproject` e copie o plugin para
> `SampleProject/Plugins/`.

## Compilar e testar sem abrir o Editor

`tools/ue.py` acha a engine por `--engine`, pela variável `UE_ROOT`, pelo registro do Windows ou, por
último, pelo `%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat` (entrada `UE_5.8`; para outra
versão instalada, `--version 5.9`). A primeira fonte informada decide: um `--engine` errado não cai nas outras.

```bash
py -3 tools/ue.py build-plugin
```

Roda `RunUAT BuildPlugin` e compila Runtime + Editor num projeto temporário; a saída vai para `dist/`.

```bash
py -3 tools/ue.py test
```

Compila o `SampleCatalogEditor` e roda os 76 testes de automação `CatalogAnchor.*` no editor headless
(`-nullrhi`). Logs e relatório (`index.json`) vão para `--report-dir` (default `Saved/Reports`); sai ≠ 0 se
algum teste falhar ou se nenhum teste rodar.

Use `--dry-run` para ver o comando sem executar.

**Sem Visual Studio:** `py -3 tools/get_msvc.py` baixa MSVC 14.44 + Windows SDK 22621 + NetFx 4.6.2 (pacotes
oficiais) no layout AutoSDK; depois defina `UE_SDKS_ROOT` com o diretório impresso no final e rode `tools/ue.py`.
Compile a partir de um caminho ASCII.

## Checklist no Editor

- [ ] Output Log: `LogCatalogAnchor: CatalogAnchor Runtime online`
- [ ] Project Settings → Plugins → **Catalog Anchor** mostra Interaction / Save / Inventory / Ability
- [ ] Session Frontend → Automation → `CatalogAnchor` → 76 testes verdes (com o PIE parado)
- [ ] Sample: pegar pickups, abrir o baú segurando E, pegar a chave e abrir o baú trancado
- [ ] Sample: F5 → fechar o PIE → Play → F9 restaura inventário, posição e vida
- [ ] Sample: Tab abre o menu de saves (salvar/carregar/apagar com metadados)
- [ ] Sample em **2 jogadores (Listen Server)**: o cliente vê pickups/baús, coleta e segura E
- [ ] Data Asset `CatalogItemDefinition` sem DisplayName → **Validate** falha com mensagem clara
- [ ] **Tools → Catalog Anchor → Validate Catalog Assets** mostra o resumo
- [ ] **Tools → Catalog Anchor → Report Catalog Assets** escreve contagens e problemas no Output Log
- [ ] Add (+) no Content Browser lista a categoria **Catalog Anchor** (Item e Ability Definition)
- [ ] PIE: console `catalog.Save.Now`, `catalog.Save.List`, `catalog.Save.Load`, `catalog.Save.Delete`
- [ ] PIE: `catalog.Interaction.Debug 1` desenha o trace de interação (não-Shipping)

## O que o CI deste repositório faz (e não faz)

| Ação | CI (GitHub Actions) | Sua máquina com UE 5.8+ |
|---|---|---|
| Manifestos, dependências, lint de C++, RPCs com validação | ✅ `tools/validate_plugin.py` | ✅ |
| pytest (Python 3.10–3.14, JUnit) + ruff do tooling | ✅ `ci.yml` | ✅ |
| Compilar C++ (UBT) | ❌ sem engine (✅ `ue.yml` em runner self-hosted) | ✅ `tools/ue.py build-plugin` |
| Testes de automação | ❌ (✅ `ue.yml` em runner self-hosted, ainda não rodou) | ✅ `tools/ue.py test` |
| Release (tag `v*`) | ✅ `release.yml`: valida, testa, empacota o plugin | n/a |
| PIE / multiplayer | ❌ | roteiro em [MULTIPLAYER_TESTS.md](MULTIPLAYER_TESTS.md) |
| Medir FPS | ❌ nunca inventamos números | seu profiler |
