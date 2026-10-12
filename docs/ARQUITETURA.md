# Arquitetura — Catalog Anchor 0.3

Princípio de todos os sistemas: **o servidor decide, o cliente pede e lê o estado replicado.**
Nenhum cliente executa gameplay localmente nem escolhe alvo, item ou habilidade por conta própria.

## Módulos

| Módulo | Tipo | Conteúdo |
|---|---|---|
| `CatalogAnchor` | Runtime | interação, inventário, habilidades, vida, save, settings, Gameplay Tags nativas |
| `CatalogAnchorEditor` | Editor | `UCatalogItemDefinitionValidator`, `UCatalogAbilityDefinitionValidator`, `UCatalogSaveDataValidator`, menu Tools |

Dependências de engine (Runtime): Core, CoreUObject, Engine, NetCore, GameplayTags, DeveloperSettings.
Plugin de engine declarado no `.uplugin`: `DataValidation` (Editor). GAS **não** é necessário.

## Interação

```
cliente: BeginInteract() ──ServerBeginInteract──▶ servidor: trace fresco + cooldown + IsInteractionBlocked
                                                    ├─ duração 0 → OnInteract → OnInteractionCompleted
                                                    └─ duração > 0 → grava início (relógio do servidor)
                                                         tick: alvo mudou/bloqueou/soltou? cancela
                                                               atingiu duração? completa
dono: progresso = f(HoldStartServerTime, GetServerWorldTimeSeconds, HoldDuration)   ← sem float por frame
```

- `CurrentTarget`, `HoldStartServerTime` e `HoldDuration` replicam com `COND_OwnerOnly`.
- `TryInteract` recusa alvos com hold, senão um cliente pularia o tempo de segurar.
- Foco (`OnFocusChanged`) é cosmético e roda só na máquina de quem olha. É desligado pelo que
  foi anunciado, não pelo controller atual: na morte o pawn perde o controller antes do `EndPlay`.
- `OnInteractionCompleted` chega ao dono remoto por Client RPC, enviado só quando há conexão
  remota (evita evento duplicado no host de listen server).

## Inventário

- `UCatalogItemDefinition : UPrimaryDataAsset`. O `ItemId` é o nome do asset. Tipo
  `CatalogItem` no Asset Manager.
- `FCatalogInventoryList : FFastArraySerializer`: replicação **por delta**, só das pilhas que
  mudaram. Os callbacks de cliente juntam tudo num único `OnInventoryChanged` no tick seguinte
  (em `PreReplicatedRemove` o item ainda está no array).
- Regras de pilha são funções puras (`FCatalogInventoryRules`), testadas sem UObject.
- Save: participante com chave `"Inventory"` (o payload tem **um** inventário, o do jogador local).
  Só registra em Pawn/PlayerController/PlayerState controlado por **jogador local** (decidido na
  possessão via `ReceiveControllerChangedDelegate`). Um Pawn que perde o controller, por exemplo ao
  entrar num veículo, mantém o registro. Baú/NPC nunca registram. Num listen server, o
  inventário dos jogadores remotos não entra no save do host. No load, o `ItemId` é resolvido pelo Asset Manager;
  se não achar, o fallback são as definições já em memória (necessário para itens sem asset).

## Habilidades ("GAS-lite")

- `UCatalogAbilityDefinition` (Primary Data Asset): `AbilityTag` (Gameplay Tag), `CooldownSeconds`
  e `Behavior` instanciado (`UCatalogAbility_Dash`, `UCatalogAbility_MeleeSweep` ou subclasse Blueprint).
- `TryActivateAbility(Tag)` no cliente envia um Server RPC. O servidor checa concessão, cooldown e
  `CanActivate`, executa `Activate` e grava `ReadyAtServerTime`.
- Cooldowns replicam ao dono no **relógio do servidor**, então o HUD do cliente mostra o tempo
  restante correto mesmo com latência.
- O cosmético (montage) vai por multicast unreliable. Dano e física só no servidor.
- O Melee usa *sweep por tipo de objeto* (`ECC_Pawn`): um sweep por canal pararia na primeira cápsula.

## Vida

- `UCatalogHealthComponent` escuta `AActor::OnTakeAnyDamage`, então qualquer `ApplyDamage` (ex.: Melee
  Sweep) reduz a vida. Dano ≤ 0 ou não finito (NaN/∞) é ignorado; curar só por `Heal`.
- `Health` replica para todos (barras sobre inimigos). Os eventos disparam no servidor direto e nos
  clientes pelo `OnRep` (com valor antigo), e `OnDeath` dispara na transição para 0.
- `ComputeNewHealth` é lógica pura testada (clamp e entradas inválidas).

## Save (schema v2)

```
SaveGame(slot): CurrentPayload + Metadata ─▶ participantes.WriteSaveData ─▶ carimba versão/SaveId/UTC ─▶ .sav
LoadGame(slot): .sav ─▶ cópia ─▶ CatalogApplySaveMigration (v0→v1→v2) ─▶ participantes.ReadSaveData
RegisterParticipant (depois de um load): recebe o estado ativo na hora
```

- **Regra de delta**: o SaveGame da Unreal omite campos iguais ao default. Por isso
  `SaveVersion` tem default **0** ("sem versão"), e nenhum default existente pode mudar.
- O save parte do **último estado salvo/carregado**. Um participante ausente naquele mapa não
  tem seus dados apagados.
- Um participante que se registra **depois** do load (troca de mapa, respawn) recebe o estado
  ativo. Sem isso, o autosave gravaria um inventário vazio por cima do save carregado.
- Async (`AsyncSaveGame`/`AsyncLoadGame`) com trava por slot. Autosave por timer do GameInstance.
- Nomes de slot: ASCII `[A-Za-z0-9_-]`, 1–128 caracteres, arquivo sempre em minúsculas (FName
  não preserva caixa de forma confiável entre plataformas). Nomes de dispositivo do Windows
  (`CON`, `PRN`, `AUX`, `NUL`, `COM0-9`, `LPT0-9`) são recusados.
- `RegisterParticipant` só relê o estado ativo para quem **não** estava registrado. Desregistrar e
  registrar de novo faz o participante receber o último save por cima do estado atual.
- `UCatalogGameplaySubsystem` mantém a API da v0.1 (`SaveToSlot`/`LoadFromSlot`, dash stub)
  delegando para o sistema novo.

## Editor

Validadores rodam ao salvar o asset, em *Validate Assets* e no commandlet `DataValidation`.
O menu **Tools → Catalog Anchor → Validate Catalog Assets** valida todos os itens, habilidades e
templates do projeto, e se recusa a rodar enquanto o Asset Registry ainda indexa (um
"0 inválidos" seria falso).

## Limitações conhecidas

Ver [LIMITATIONS.md](../LIMITATIONS.md).
