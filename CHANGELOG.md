# Changelog

Formato: [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) · versões: [SemVer](https://semver.org/lang/pt-BR/).

## [Unreleased]

## [0.6.0] — em preparação

Entradas abaixo: só o que a documentação e o gerador entregam; o restante da 0.6.0 (cross-platform no C++, otimização,
gamepad, matriz de CI) é registrado pelo agente principal quando for entregue e verificado.

### Adicionado
- `tools/gen_api_docs.py` (stdlib): gera `docs/API.md` a partir dos headers públicos (UCLASS, USTRUCT, UENUM, interfaces,
  delegates, UPROPERTY e UFUNCTION públicos, com o comentário `/** */`), agrupado por área e com índice; saída
  determinística. `--check` sai ≠ 0 se o arquivo estiver desatualizado (coberto pelo pytest, que roda no CI).
- `docs/API.md`: referência de API gerada.
- `docs/QUICKSTART.md`: do zero a "abrir um baú e salvar" em 10 passos, em C++ e Blueprint.

### Alterado
- `README.md` reescrito como vitrine (recursos, plataformas, instalação em poucas linhas, estado real da verificação);
  nome sugerido do repositório: `CatalogAnchor`.
- `LIMITATIONS.md`: shipping e empacotamento em Windows passam a constar como verificados; Mac/Linux seguem não executados.

## [0.5.0] — 2026-10-13

### Adicionado
- **Multiplayer headless:** `catalog.Debug.Aim|Hold|Ability|Dump` + `tools/mp_run.py` executam MP-03, 04, 06 e 07 (dedicado + 2 clientes): 27/27 predicados com evidência de log.
- Itens usáveis que não se consomem têm `UseCooldownSeconds` (servidor); `ServerUseItem` não repete o efeito sem limite.
- Save: rotação de backups por mover/copiar arquivos (sem reler e regravar), `DeleteSlot` apaga o principal antes dos backups, e falha de LEITURA do principal não cai mais no backup (evita rollback silencioso).
- Vida: `SetHealth` antes do `BeginPlay` é mantido, `Heal` devolve o que curou e `SetHealth` restaura o valor exato. Editor: `Weight`, `UseCooldownSeconds` e `Dash->Strength` inválidos viram erro.
- Tooling: `get_msvc.py` bloqueia `..` no zip e faz hash em blocos; `release.yml` aceita pré-release (`-rc1`); validador e `ue.py` resistem a entradas malformadas.
- Vida: o 1º valor replicado não gera delta falso; testes das `_Validate` (habilidade, interação, inventário) e `catalog.Debug.Dump`/comandos de depuração. 67 testes de automação passando.
- **Verificação real (Windows, UE 5.8.3, MSVC 14.44, Windows SDK 22621):** build do `SampleCatalogEditor` OK e
  **64/64 testes de automação** passando no `UnrealEditor-Cmd`; pytest com 118 casos (Python 3.13 e 3.14).
- **Inventário:** `UseItem` / `ServerUseItem` (RPC validado) e `OnItemUsed`; `UCatalogItemDefinition::bUsable`,
  `bConsumeOnUse` e `OnUsed(User)`; `TransferTo` atômico entre inventários; `MaxWeight` / `Weight`
  (`GetTotalWeight`, `GetAddableQuantity`); `Category` e `GetItemsByTag`.
- **Save:** `PayloadCrc` (CRC32 do payload) conferido no load, com fallback para o backup (`<slot>_bak`,
  `_bak2`...; `BackupCount` nas Settings); nomes `_bak` reservados; `ListSaveSlots` / `FCatalogSaveSlotInfo`
  / `ECatalogSaveSlotState`; `DeleteSlot` apaga também os backups; `UCatalogBlueprintLibrary`; comandos de console
  `catalog.Save.List`, `catalog.Save.Now`, `catalog.Save.Load` e `catalog.Save.Delete`.
- **Interação:** `ICatalogInteractable::GetInteractionPriority`, assist de esfera (`bUseSphereAssist`,
  `AssistRadius`), `OnInteractionFailed(ECatalogInteractionFailReason)`, CVar `catalog.Interaction.Debug` e
  `TraceChannel` configurável. A mira pela câmera e o alcance do pawn já vinham da 0.4.0.
- **Editor:** validadores de nome de asset duplicado, ícone/mesh nulos e cooldown; factories na categoria
  "Catalog Anchor"; menu **Report Catalog Assets**.
- **RPCs:** todo Server RPC é `WithValidation`; o validador Python reprova a falta (`--no-strict-rpc` vira aviso).
- **Repo:** `release.yml` (release por tag `v*`), templates de issue/PR, Dependabot, `CODEOWNERS`, `SECURITY.md`,
  `CODE_OF_CONDUCT.md`, `tools/get_msvc.py` (+ `UE_SDKS_ROOT`) e CI de pytest em Python 3.10–3.14.
- `validate_plugin.py --strict-docs`: contagens de testes citadas nos docs que divergem do código viram erro.

### Alterado
- `CatalogAnchor.uplugin`: `VersionName` 0.5.0 (`Version` 5).

### Corrigido
- **Vida:** a primeira replicação no cliente não gera mais `OnHealthChanged` falso (delta contra o default 100).
- **Habilidades:** `TryActivateAbility` com tag inválida não envia RPC (`OnAbilityFailed`); avatar morto não ativa.
- **Melee Sweep** ignora alvo destruído por um dano anterior da mesma varredura.

## [0.4.0] — 2026-10-12

### Adicionado
- `tools/get_msvc.py`: baixa MSVC 14.44 + Windows SDK 22621 + NetFx 4.6.2 (canal oficial) no layout AutoSDK, sem instalar nada; `UE_SDKS_ROOT` aponta o UBT para ele.
- **Primeira compilação real:** build do `SampleCatalogEditor` OK e 33/33 testes de automação passando (UE 5.8.3).
- **Vida:** `SetHealth` (restaurar save; limita, ignora não finito, revive com valor > 0).
- **Save:** gerações de load: cada load entrega o payload a todos os registrados, e o mesmo objeto
  re-registrando na mesma geração não relê. Registrar outro objeto com a mesma chave o substitui (respawn).
- **Sample:** HUD e menu de saves (Tab) em UMG feitos em C++; `USamplePlayerSaveComponent` grava posição,
  yaw e vida (registro `SamplePlayer` v1); todo texto em `LOCTEXT` (namespace `SampleCatalog`).
- **Testes de automação de mundo** (vida, interação, save, save do sample): 33 testes C++ no total.
- **CI:** pytest em Python 3.10–3.14 com relatório JUnit; workflow `ue.yml` para runner self-hosted
  (variáveis `UE_RUNNER_ENABLED`, `UE_ROOT`) que compila e roda a automação.
- `tools/ue.py`: `--report-dir` (logs de UAT/UBT/editor e `index.json`), sai ≠ 0 com teste falho ou zero testes,
  aponta o SDK que falta quando o UBT recusa a plataforma, e acha a engine no `LauncherInstalled.dat`
  do Epic Launcher depois de `--engine`/`UE_ROOT`/registro.

### Alterado
- **Interação:** mira pelo ponto de vista do controller (`GetPlayerViewPoint`) com alcance medido do pawn
  (`ComputeTraceSegment`); o foco resolve o controller pela cadeia de owners (`FindOwningController`) e só
  liga para `PlayerController` local.
- **Vida:** respeita `bCanBeDamaged`; `OnDeath` uma vez por morte mesmo com dano reentrante.
- **Habilidades:** avatar morto não ativa habilidade.
- **Sample:** respawn do jogador atrasado 2 s (timer, limpo no `EndPlay`), sem movimento nem ações enquanto
  morto; o Enhanced Input remove o mapping context ao trocar de controller e no `EndPlay`.
- `CatalogAnchor.uplugin`: `VersionName` 0.4.0.

### Corrigido
- Sample: include do teste `SamplePlayerSaveTests.cpp` (único erro de compilação real encontrado).
- Sample: clientes podiam nunca ver a morte do jogador (`ResetHealth` no mesmo frame fundia 0 e 100 no OnRep).
- Melee Sweep ignora alvo destruído por um dano anterior da mesma varredura.

## [0.3.0] — 2026-10-11

### Adicionado
- **Vida:** `UCatalogHealthComponent` (replicado, `ApplyDamage`, `Heal`, `ResetHealth`, `OnHealthChanged`, `OnDeath`) + teste `CatalogAnchor.Health.ComputeNewHealth`.
- **Sample:** bonecos de treino com barra de vida e respawn, poção usável (Q, Server RPC revalidado no servidor), vida e respawn do jogador, vida no HUD.
- `tools/ue.py --version <x.y>` escolhe a chave do registro (padrão 5.8).
- Lint de C++: fonte com caractere não-ASCII sem BOM UTF-8 é reprovado (a regra já constava no CONTRIBUTING).

### Alterado
- **Alvo passa a ser Unreal Engine 5.8** (antes 5.4), mirando compatibilidade com 5.9 e 6.0: `.uplugin` sem
  `EngineVersion`, sample com `EngineAssociation` 5.8 e `BuildSettingsVersion.Latest`.
- O validador aceita `EngineAssociation` ≥ 5.8 e `EngineVersion` opcional (se presente, ≥ 5.8).
- Testes de automação: `EAutomationTestFlags::ApplicationContextMask` não existe na 5.8 (não compilaria);
  agora usam os contextos explícitos `EditorContext | ClientContext | ServerContext | CommandletContext`.
- Tooling suporta Python 3.10+: CI testa 3.10–3.13 e o ruff mira `py310`.
- Runtime não depende mais de `InputCore`, `Slate` nem `SlateCore` (nenhum código os usava).
- Inventário em baú/NPC **nunca** entra no save; antes, com `bSaveWithGame` ligado (o padrão), sobrescrevia o do jogador.

### Corrigido
- **Save em listen server misturava inventários:** um inventário em Pawn só entra no save quando é possuído por um jogador local (`ReceiveControllerChangedDelegate`); o dos jogadores remotos não sobrescreve nem duplica o do host. Vale também para inventário em PlayerState.
- **Perda de itens ao perder o controller** (ex.: entrar num veículo): o Pawn saía do save e, ao voltar, relia o
  save por cima do inventário atual. Agora mantém o registro.
- Slot com nome de dispositivo do Windows (`CON`, `NUL`, `COM1`…) é recusado: `nul.sav` "gravava" sem criar arquivo.
- `GetItemCount` não estoura `int32` com pilhas enormes.
- Melee Sweep ignora dano não finito (NaN) em vez de repassá-lo a `ApplyDamage`.
- Highlight de foco preso no cliente após unpossess (corpo que fica no mundo, veículo): o anúncio do foco é
  reavaliado a cada tick, sem duplicar `OnTargetChanged`.
- Sample: quem entra na partida via boneco de treino já morto como vivo (o OnRep inicial chegava antes do bind).
- Sample: o baú consulta `IsInteractionBlocked` via `Execute_` e respeita override em subclasse Blueprint.
- `KINDA_SMALL_NUMBER` → `UE_KINDA_SMALL_NUMBER`.
- Includes explícitos (`Engine/HitResult.h`, `CollisionShape.h`, `Engine/TimerHandle.h`) no lugar de includes
  transitivos que podem sumir em 5.9/6.0.
- Validador: `.uplugin`/`.uproject` com BOM UTF-8 ou em UTF-16 (formatos que a UE lê) não são mais recusados, e
  bytes inválidos viram erro de validação em vez de traceback.

## [0.2.0] — 2026-10-11

### Adicionado
- **Interação v2:** hold-to-interact com progresso replicado ao dono, `IsInteractionBlocked`,
  `OnFocusChanged`, `GetInteractionDuration`, cooldown anti-spam, delegates `OnTargetChanged` /
  `OnInteractionCompleted`.
- **Inventário:** `UCatalogItemDefinition`, `UCatalogInventoryComponent` com `FFastArraySerializer`,
  `ACatalogPickupActor`, `ServerDropItem`, persistência como participante de save.
- **Habilidades:** `UCatalogAbilityComponent`, `UCatalogAbilityDefinition`, `UCatalogAbility_Dash`,
  `UCatalogAbility_MeleeSweep`, Gameplay Tags nativas `Catalog.Ability.*`, cooldown replicado.
- **Save v2:** `UCatalogSaveSubsystem` (GameInstance), async, autosave, `ICatalogSaveParticipant`,
  `Metadata`/`Records`, migrador v1→v2.
- **Editor:** validadores de itens e habilidades, menu *Validate Catalog Assets*.
- **Sample jogável** 100% C++: arena, personagem, baús, pickups, HUD, Enhanced Input em código.
- **Tooling:** lint de C++ no validador, `tools/ue.py` (build-plugin/test), ruff, CI em matriz,
  `.editorconfig`, `.gitattributes`, `.clang-format`.
- 17 testes de automação C++ e 61 casos de pytest.

### Alterado
- `GCatalogSaveSchemaVersion` = 2; `ExpectedSaveSchemaVersion` padrão = 2.
- `UCatalogGameplaySubsystem::SaveToSlot/LoadFromSlot` delegam ao `UCatalogSaveSubsystem`.
- `UCatalogDashAbilityStub` marcado como legado.

### Corrigido
- Perda de save ao trocar de mapa após um load: participantes que se registram depois recebem o estado ativo.
- Foco/highlight preso no alvo quando o pawn morre.
- Itens criados sem asset chegavam nulos nos clientes (agora subobjetos de CDO no sample).

## [0.1.0] — 2026-10-10

### Adicionado
- Plugin Runtime + Editor: interação por trace, save versionado em disco com migração,
  subsystem, dash stub, settings, validador de `UCatalogSaveTemplate`; sample mínimo; validador Python e CI.
