# Changelog

Formato: [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) · versões: [SemVer](https://semver.org/lang/pt-BR/).

## [Unreleased]

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
