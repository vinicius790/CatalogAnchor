# Arquitetura — Catalog Anchor 0.5

Princípio de todos os sistemas: **o servidor decide, o cliente pede e lê o estado replicado.**
Nenhum cliente executa gameplay localmente nem escolhe alvo, item ou habilidade por conta própria.

## Módulos

| Módulo | Tipo | Conteúdo |
|---|---|---|
| `CatalogAnchor` | Runtime | interação, inventário, habilidades, vida, save, settings, Gameplay Tags nativas, `UCatalogBlueprintLibrary`, comandos de console |
| `CatalogAnchorEditor` | Editor | `UCatalogItemDefinitionValidator`, `UCatalogAbilityDefinitionValidator`, `UCatalogSaveDataValidator`, factories, menu Tools |

**RPCs:** todo `UFUNCTION(Server)` é `WithValidation`; o `_Validate` reprova argumentos impossíveis (nulo, quantidade ≤ 0,
tag inválida) e o lint do `validate_plugin.py` falha se faltar (`--no-strict-rpc` rebaixa a aviso).

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

- **Mira e alcance:** o trace segue o raio da câmera do controller (`GetPlayerViewPoint`), mas o alcance é
  medido do pawn: `ComputeTraceSegment` (pura, testada) recorta o raio dentro da esfera de alcance em volta
  do dono. Câmera em 3ª pessoa não aumenta nem encurta o alcance.
- **Canal e assist:** o trace usa `TraceChannel` (padrão `ECC_Visibility`). Com `bUseSphereAssist`, se a mira não acha
  interagível, uma esfera de `AssistRadius` varre a mira e escolhe entre os visíveis o de maior
  `GetInteractionPriority` (`ICatalogInteractable`), depois o mais perto do centro da mira. A varredura para no
  primeiro oclusor.
- **Falhas:** recusa vira `OnInteractionFailed(Target, ECatalogInteractionFailReason)` (`Blocked`, `OutOfRange`,
  `Cooldown`, `NoTarget`), no servidor e no dono remoto (cosmético, Client RPC não confiável).
- **Debug:** CVar `catalog.Interaction.Debug 1` desenha o trace (não-Shipping).
- `CurrentTarget`, `HoldStartServerTime` e `HoldDuration` replicam com `COND_OwnerOnly`.
- `TryInteract` recusa alvos com hold, senão um cliente pularia o tempo de segurar.
- Foco (`OnFocusChanged`) é cosmético e roda só na máquina de quem olha: o controller vem da cadeia de
  owners (`FindOwningController`: Pawn, o próprio Controller ou o primeiro em `GetOwner()`), e só um
  `PlayerController` local liga o highlight. É desligado pelo que
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
- **Peso e categoria:** `UCatalogItemDefinition::Weight` soma contra `MaxWeight` (0 = ilimitado); `AddItem` entra
  parcial até caber. `GetTotalWeight`, `GetAddableQuantity` e `GetItemsByTag` (tag em `ItemTags` ou `Category`,
  hierárquico) são consultas sem efeito colateral.
- **Uso:** `UseItem` (autoridade) ou `ServerUseItem` (RPC validado) exige `bUsable`, consome 1 por uso se
  `bConsumeOnUse`, chama `UCatalogItemDefinition::OnUsed(User)` por unidade e dispara `OnItemUsed` só no servidor.
- **Transferência:** `TransferTo(Other, Item, Qty)` é atômica: simula o destino (pilhas, `Capacity`, `MaxWeight`) e só
  então remove da origem e adiciona no destino; sem espaço, nada muda. Exige autoridade nos dois e não tem RPC.
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
- Ator com `bCanBeDamaged = false` não perde vida por dano. `SetHealth` (ex.: restaurar save) ignora a flag,
  limita a `[0, MaxHealth]`, ignora valor não finito e revive um morto se o valor for > 0.
- `Health` replica para todos (barras sobre inimigos). Os eventos disparam no servidor direto e nos
  clientes pelo `OnRep` (com valor antigo). `OnDeath` dispara **uma vez** por transição vivo→morto, mesmo
  com dano reentrante (listener de `OnDeath` que aplica mais dano não gera segunda morte).
- Morto não ativa habilidade: `CanActivate` consulta o componente de vida do avatar.
- `TryActivateAbility` com tag inválida não sai do cliente (`OnAbilityFailed` com `NotGranted`, o `_Validate` do servidor desconectaria quem só errou o Blueprint); o Melee Sweep ignora alvo destruído no meio da varredura.
- **Respawn atrasado:** o cliente só vê o valor replicado; `ResetHealth` no frame da morte funde 0 e
  `MaxHealth` num envio e o cliente pode nunca ver a morte. O sample espera 2 s (timer limpo no `EndPlay`),
  com movimento e ações bloqueados, antes de reviver e teleportar.
- `ComputeNewHealth` é lógica pura testada (clamp e entradas inválidas).
- **Primeira replicação:** o `OnRep` inicial no cliente não gera `OnHealthChanged` falso (sem delta contra o default 100
  quando `MaxHealth` ≠ 100).

## Save (schema v2)

```
SaveGame(slot): CurrentPayload + Metadata ─▶ participantes.WriteSaveData ─▶ carimba versão/SaveId/UTC/PayloadCrc
                ─▶ arquivo atual (se íntegro) vira <slot>_bak ─▶ .sav
LoadGame(slot): .sav ─▶ confere CRC ─▶ (corrompido? tenta <slot>_bak, _bak2...) ─▶ cópia
                ─▶ CatalogApplySaveMigration (v0→v1→v2) ─▶ participantes.ReadSaveData
RegisterParticipant: recebe o estado ativo uma vez por geração (cada load abre uma)
```

- **Regra de delta**: o SaveGame da Unreal omite campos iguais ao default. Por isso
  `SaveVersion` tem default **0** ("sem versão"), e nenhum default existente pode mudar.
- O save parte do **último estado salvo/carregado**. Um participante ausente naquele mapa não
  tem seus dados apagados.
- Um participante que se registra **depois** do load (troca de mapa, respawn) recebe o estado
  ativo. Sem isso, o autosave gravaria um inventário vazio por cima do save carregado.
- **Integridade e backup:** `UCatalogSaveGame::PayloadCrc` guarda o CRC32 do payload (`CatalogComputePayloadCrc`) e todo
  load o confere. Antes de sobrescrever, o arquivo atual vira `<slot>_bak` (depois `_bak2`... até `BackupCount` nas
  Settings, padrão 1), só se ele próprio estiver íntegro. Load de arquivo corrompido cai no backup mais novo legível
  (`Recovered`). Nomes `_bak`/`_bakN` são reservados e recusados como nome de slot.
- **Slots:** `ListSaveSlots()` devolve `FCatalogSaveSlotInfo` (`SlotName`, `State`, `SaveVersion`, `Timestamp`,
  `MapName`, `PlaytimeSeconds`, `DisplayName`; `ECatalogSaveSlotState`: `Ok`, `Recovered`, `Unreadable`, `Busy`),
  sem inventário nem registros, do mais recente ao mais antigo, via `GetSaveGameNames` (onde a plataforma não lista:
  só os slots tocados na sessão e o autosave). Backups não entram. `DeleteSlot` apaga também os backups.
- **Blueprint e console:** `UCatalogBlueprintLibrary` (`SaveCatalogGame`, `LoadCatalogGame`, `AsyncSaveCatalogGame`, `AsyncLoadCatalogGame`,
  `ListCatalogSaveSlots`, `DeleteCatalogSaveSlot`, vida e inventário) e `catalog.Save.List`, `catalog.Save.Now [slot]`,
  `catalog.Save.Load [slot]`, `catalog.Save.Delete [slot]` (padrão `Quick`).
- Async (`AsyncSaveGame`/`AsyncLoadGame`) com trava por slot. Autosave por timer do GameInstance.
- Nomes de slot: ASCII `[A-Za-z0-9_-]`, 1–128 caracteres, arquivo sempre em minúsculas (FName
  não preserva caixa de forma confiável entre plataformas). Nomes de dispositivo do Windows
  (`CON`, `PRN`, `AUX`, `NUL`, `COM0-9`, `LPT0-9`) são recusados.
- **Gerações:** cada load bem-sucedido abre uma geração e entrega o payload a todos os registrados. O
  mesmo objeto desregistrando e registrando de novo na mesma geração **não** relê o save.
- **Chave única:** registrar outro objeto vivo com a mesma `GetSaveRecordKey` substitui o anterior
  (respawn com o pawn velho ainda no mundo). `UnregisterParticipant` tira só aquele objeto.
- `UCatalogGameplaySubsystem` mantém a API da v0.1 (`SaveToSlot`/`LoadFromSlot`, dash stub)
  delegando para o sistema novo.

## Sample

- UI em UMG montada em C++ (`USampleHUDWidget`, `USampleSaveMenuWidget`, sem Widget Blueprint); menu no Tab.
- `USamplePlayerSaveComponent` grava posição, yaw e vida no registro `SamplePlayer` (v1) do payload.
- Enhanced Input criado em código; o mapping context sai do LocalPlayer ao trocar de controller e no `EndPlay`.
- Texto visível em `LOCTEXT` (namespace `SampleCatalog`). Detalhes em
  [SampleProject/README.md](../SampleProject/README.md).

## Editor

Validadores rodam ao salvar o asset, em *Validate Assets* e no commandlet `DataValidation`.
O menu **Tools → Catalog Anchor → Validate Catalog Assets** valida todos os itens, habilidades e
templates do projeto, e se recusa a rodar enquanto o Asset Registry ainda indexa (um
"0 inválidos" seria falso).

- **Regras:** `ItemId`/`PrimaryAssetId` repetido entre assets (erro), ícone/mesh nulos por reflexão (aviso),
  cooldown não finito ou negativo (erro) e zero (aviso).
- **Factories:** itens e habilidades aparecem em Add (+) na categoria **Catalog Anchor**.
- **Report Catalog Assets:** escreve no Output Log a contagem por tipo, cada erro/aviso e nomes duplicados.

## Limitações conhecidas

Ver [LIMITATIONS.md](../LIMITATIONS.md).
