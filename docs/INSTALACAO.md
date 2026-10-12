# Instalação

**Engine:** Unreal Engine **5.8** ou mais nova (5.9, 6.0: compatibilidade pretendida, ainda não verificada) +
Visual Studio com o workload *Game development with C++* na versão que as release notes da engine pedem
(Windows), ou o toolchain equivalente no Linux/macOS.

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

`tools/ue.py` acha a engine por `--engine`, pela variável `UE_ROOT` ou pelo registro do Windows
(chave da 5.8; para outra versão instalada, `--version 5.9`).

```bash
py -3 tools/ue.py build-plugin
```

Roda `RunUAT BuildPlugin` e compila Runtime + Editor num projeto temporário; a saída vai para `dist/`.

```bash
py -3 tools/ue.py test
```

Roda os 18 testes de automação `CatalogAnchor.*` no SampleProject (`-nullrhi`). Compile o
`SampleCatalogEditor` antes (Caminho B): com `-unattended` o editor não se oferece para compilar módulos ausentes.

Use `--dry-run` para ver o comando sem executar.

## Checklist no Editor

- [ ] Output Log: `LogCatalogAnchor: CatalogAnchor Runtime online`
- [ ] Project Settings → Plugins → **Catalog Anchor** mostra Interaction / Save / Inventory / Ability
- [ ] Session Frontend → Automation → `CatalogAnchor` → 18 testes verdes
- [ ] Sample: pegar pickups, abrir o baú segurando E, pegar a chave e abrir o baú trancado
- [ ] Sample: F5 → fechar o PIE → Play → F9 restaura o inventário
- [ ] Sample em **2 jogadores (Listen Server)**: o cliente vê pickups/baús, coleta e segura E
- [ ] Data Asset `CatalogItemDefinition` sem DisplayName → **Validate** falha com mensagem clara
- [ ] **Tools → Catalog Anchor → Validate Catalog Assets** mostra o resumo

## O que o CI deste repositório faz (e não faz)

| Ação | CI (GitHub Actions) | Sua máquina com UE 5.8+ |
|---|---|---|
| Manifestos, dependências, lint de C++ | ✅ `tools/validate_plugin.py` | ✅ |
| pytest + ruff do tooling | ✅ | ✅ |
| Compilar C++ (UBT) | ❌ sem engine | ✅ `tools/ue.py build-plugin` |
| Testes de automação / PIE | ❌ | ✅ `tools/ue.py test` |
| Medir FPS | ❌ nunca inventamos números | seu profiler |
