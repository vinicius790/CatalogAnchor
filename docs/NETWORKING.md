# Rede: modelo, riscos a verificar e prontidão para Iris e Push Model

> Fatos abaixo vêm do código e dos testes headless (`tools/mp_run.py`). Tudo marcado como **hipótese**, **a verificar** ou
> **incerto** não foi executado nem confirmado na engine. Decisões de fundo: [adr/](adr/README.md) (0001, 0002, 0004, 0005).

## 1. Modelo de rede

Servidor autoritativo ([ADR 0001](adr/0001-servidor-autoritativo-e-rpcs-validados.md)): o cliente pede por Server RPC e lê estado
replicado; trace, cooldown, dano e alteração de inventário só rodam com autoridade.

### Estado replicado

| Classe | Propriedade | Condição | Quem recebe |
|---|---|---|---|
| `UCatalogInventoryComponent` | `InventoryList` (FastArray) | `COND_OwnerOnly` | só o cliente dono ([ADR 0002](adr/0002-inventario-fastarray-owner-only.md)) |
| `UCatalogInteractionComponent` | `CurrentTarget`, `HoldStartServerTime`, `HoldDuration` | `COND_OwnerOnly` | só o dono |
| `UCatalogAbilityComponent` | `Cooldowns` | `COND_OwnerOnly` | só o dono (relógio do servidor) |
| `UCatalogHealthComponent` | `Health` | `COND_None`, `REPNOTIFY_OnChanged` | todos (barras sobre inimigos) |
| `ACatalogPickupActor` | `Item`, `Quantity` | padrão | todos que o têm relevante |

### RPCs

| Tipo | Funções |
|---|---|
| Server, Reliable, `WithValidation` (6) | `ServerTryActivateAbility`, `ServerTryInteract`, `ServerBeginInteract`, `ServerEndInteract`, `ServerUseItem`, `ServerDropItem` |
| Client, Reliable | `ClientInteractionCompleted` |
| Client, Unreliable | `ClientInteractionFailed`, `ClientAbilityFailed` (cosméticos) |
| NetMulticast, Unreliable | `MulticastAbilityActivated` (cosmético: um cliente pode perder uma montage) |

### Frequência e dormência dos atores (valores do código)

| Ator | Onde | NetUpdateFrequency | Outros |
|---|---|---|---|
| `ACatalogPickupActor` | plugin | 2 Hz | `NetDormancy = DORM_Initial`; `FlushNetDormancy()` em coleta parcial |
| `ASampleChestActor` | sample | 10 Hz | |
| `ASampleTrainingDummy` | sample | 20 Hz | |
| `ASampleArenaActor` | sample | 1 Hz | `bAlwaysRelevant` |
| Pawn do sample | sample | não definido no sample (padrão da engine; valor **não conferido**) | |

Nenhum desses valores foi medido nem ajustado por perfil: são escolhas de projeto do sample (ver risco R2).

## 2. Riscos a verificar

### R1. Pickups com `DORM_Initial` gerados em runtime (**coberto pelo predicado 03 do `mp_run`, ainda não executado**)

**Fatos do código:**

- O construtor de `ACatalogPickupActor` define `NetDormancy = DORM_Initial` e 2 Hz de `NetUpdateFrequency`; `Item` e `Quantity`
  replicam com `DOREPLIFETIME` (sem condição).
- Todo pickup do sample nasce em runtime, no servidor, por `SpawnActorDeferred`: `ASampleCatalogGameMode::SpawnPickup` (quatro
  pickups no `BeginPlay` do GameMode) e `UCatalogInventoryComponent::ServerDropItem` (drop). Nos dois, `Item` e `Quantity` são
  atribuídos **antes** de `FinishSpawning`, ou seja, antes do `BeginPlay` e da primeira replicação.
- Coleta parcial (`OnInteract` com inventário quase cheio) reduz `Quantity` e chama `FlushNetDormancy()` (`CatalogPickupActor.cpp`,
  por volta da linha 104); coleta total chama `Destroy()`. Pelo comentário do header, mudar `Item`/`Quantity` fora de `OnInteract`
  exige `FlushNetDormancy()`.
- O teste de automação `CatalogAnchor.Inventory.World.PickupDormancy` (sem rede) confere o `DORM_Initial` padrão e que a coleta
  parcial acorda o ator; é um dos testes ainda não executados.

**Hipótese de risco (não confirmada):** a engine descreve `DORM_Initial` como "dormente desde o início", pensado para atores
colocados no nível. Num ator gerado dinamicamente é **incerto** que o estado inicial (`Item`/`Quantity`) chegue aos clientes; se
não chegar, o item só existe no servidor.

**Cobertura escrita, pendente de execução:** o caso 03 do `mp_run` (modo dedicado) tem um predicado novo: cada cliente precisa
listar, no snapshot `focus`, os mesmos pickups que o servidor, comparando o `Prompt` (`Pegar <item> x<qtd>`, montado no cliente a
partir de `Item` e `Quantity` replicados). Os clientes conectam depois que o GameMode gerou os pickups, então o predicado também
cobre o late join do estado inicial. `tests/test_mp_run.py` confere só a lógica do predicado sobre linhas de log sintéticas.
**Não executado:** precisa da engine, que está sendo reinstalada. Até lá o risco continua aberto.

**Ainda sem cobertura** (dedicado + 2 clientes, `-nullrhi` basta):

1. Pickup gerado por drop (cliente 2 solta com `catalog.Debug.Key G Tap`) aparecendo nos clientes: o predicado só vê os pickups do GameMode.
2. Coleta parcial: `Quantity` deve diminuir **nos clientes** (valida o `FlushNetDormancy`).
3. Cliente que entra depois de uma mudança (coleta parcial antes do join).
4. `--mode listen`: o predicado de pickups existe só no caso 03 do modo dedicado.
5. Pickups colocados no nível (o sample não tem), para separar "colocado" de "gerado".

**Se falhar, correções a avaliar** (não aplicadas): `NetDormancy = DORM_Never` com a frequência baixa já existente (custo: o pickup
parado entra na lista de consideração), ou acordar o ator no spawn (`FlushNetDormancy()` após `FinishSpawning`) e só depois deixá-lo
dormir. Decidir com medição ([PERFORMANCE.md](PERFORMANCE.md), linha "Banda, pickups").

### R2. `NetUpdateFrequency` do sample

Hipótese: as frequências acima (1 a 20 Hz) foram escolhidas sem perfil. Pontos a checar: (a) latência percebida do estado do baú
e do prompt do pickup (um pickup a 2 Hz pode levar até ~0,5 s para refletir `Quantity`; **incerto** se `FlushNetDormancy` acelera
a primeira atualização); (b) o efeito de `net.UseAdaptiveNetUpdateFrequency` (**nome do CVar a conferir na 5.8**) sobre atores sem
mudança; (c) o custo do `ASampleArenaActor` como `bAlwaysRelevant` para todos os clientes. Teste: variar a frequência e medir o
atraso entre `Dump` do servidor e do cliente, com e sem `NetEmulation.PktLag`.

### R3. Cosméticos não confiáveis

`MulticastAbilityActivated`, `ClientInteractionFailed` e `ClientAbilityFailed` são `Unreliable`: sob perda de pacote o
cliente pode não ver a montage nem o motivo da recusa. Não afeta estado (cooldown replica por propriedade). Teste: `NetEmulation.PktLoss`.

### R4. Validação por rede dos RPCs

Os `_Validate` não têm teste que envie argumentos inválidos pela rede (LIMITATIONS.md). Candidato: um caso de `mp_run` que force
`ServerUseItem` com quantidade 0 e confirme a desconexão (comportamento da engine, a confirmar).

### R5. Save e listen server

O save vive no GameInstance de quem tem autoridade e guarda só o jogador local do host (LIMITATIONS.md, [ADR 0004](adr/0004-geracoes-de-payload-e-chave-unica-de-participantes.md)). Dedicado precisa de persistência própria.

## 3. Prontidão para Iris e Push Model

Estado atual (fatos): o código usa a replicação clássica: `DOREPLIFETIME`/`DOREPLIFETIME_CONDITION` em `GetLifetimeReplicatedProps`,
componentes com `SetIsReplicatedByDefault(true)`, um `FFastArraySerializer` e `NetCore` como dependência do módulo. **Não há** uso de
Push Model (`DOREPLIFETIME_WITH_PARAMS_FAST`, `MARK_PROPERTY_DIRTY_FROM_NAME`) nem código específico de Iris, e nada foi testado com
Iris ligado. Nenhum item abaixo foi executado.

### Push Model (checklist)

- [ ] Listar todos os pontos que escrevem propriedade replicada: `Health` (SetHealth, dano, cura, reset), `Cooldowns`,
      `CurrentTarget`/`HoldStartServerTime`/`HoldDuration`, `Item`/`Quantity` do pickup, `InventoryList`.
- [ ] Trocar o registro por `DOREPLIFETIME_WITH_PARAMS_FAST` com `bIsPushBased = true` e marcar sujo (`MARK_PROPERTY_DIRTY_FROM_NAME`)
      em **cada** escrita; uma escrita sem marca vira estado que não replica.
- [ ] Decidir o tratamento do `FFastArraySerializer` (`MarkItemDirty`/`MarkArrayDirty` já existem; **a verificar** na 5.8 se a
      propriedade-struct também precisa ser marcada).
- [ ] Ligar o Push Model (CVar `net.IsPushModelEnabled`, **nome a confirmar**) e rodar `mp_run --case all` nos dois modos.
- [ ] Comparar custo no Insights: o ganho esperado é não varrer propriedades sem mudança (hipótese a medir).

### Iris (checklist)

- [ ] Confirmar na 5.8 como habilitar (plugin e CVar, **a confirmar**) e se o projeto de teste exige configuração extra.
- [ ] Verificar o suporte a `FFastArraySerializer` no Iris da 5.8 (**incerto**: pode exigir registro/macro adicional de NetSerializer).
- [ ] Verificar `COND_OwnerOnly` em componentes e a semântica de dormência (`DORM_Initial`, `FlushNetDormancy`) no Iris; revisitar R1
      sob Iris.
- [ ] Os componentes são replicados como `UActorComponent` (não há subobjetos replicados manuais): conferir se algo precisa de registro
      explícito.
- [ ] Rodar o `mp_run` completo e comparar os predicados de `Dump` (foco só no dono, `OwnerOnly`, vida em todos) com a replicação clássica.

## 4. O que o `mp_run` cobre (e o que não cobre)

Cobertura executada (UE 5.8.3, Windows, até o commit `876aec1`; detalhes e evidências em [MULTIPLAYER_TESTS.md](MULTIPLAYER_TESTS.md)):

- foco só no pawn do dono (servidor dedicado não anuncia foco; outro cliente vê `Target=None`);
- hold por Server RPC, baú trancado que abre com a chave (a chave é checada, não consumida);
- habilidades: Dash com cooldown `OwnerOnly`, Melee com dano visível no servidor e nos clientes;
- save e load por comando, teclas reais via `PlayerController`/Enhanced Input, estado do HUD, menu Tab;
- os mesmos casos em servidor dedicado + 2 clientes e em listen server.

Escrito depois de `876aec1` e **ainda não executado**: no caso 03 (dedicado), os clientes veem os mesmos pickups que o servidor,
inclusive quem conectou depois do spawn (R1).

**Não coberto:** pickups de drop e coleta parcial vistos pelos clientes (R1), perda/latência de pacote, muitos jogadores, reconexão e
travel, argumentos inválidos nos RPCs (R4), predição, Iris e Push Model, medidas de banda, mouse e câmera reais, gamepad, aparência
do HUD. O `mp_run` precisa da engine; o CI público não o executa.
