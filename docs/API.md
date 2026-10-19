# API de referência

> Gerado por `tools/gen_api_docs.py` a partir de `Plugins/CatalogAnchor/Source/CatalogAnchor/Public/*.h`. Não edite à mão: rode `py -3 tools/gen_api_docs.py` e commite. O CI confere com `--check`.

Cobre o que o UHT reflete (UCLASS, USTRUCT, UENUM, interfaces, delegates, UPROPERTY e UFUNCTION públicos). Helpers C++ estáticos e membros protegidos/privados ficam nos headers.

## Índice

- [Interação](#interação)
  - [`ICatalogInteractable`](#icataloginteractable) (interface)
  - [`FCatalogInteractionTargetChanged`](#fcataloginteractiontargetchanged) (delegate)
  - [`FCatalogInteractionCompleted`](#fcataloginteractioncompleted) (delegate)
  - [`ECatalogInteractionFailReason`](#ecataloginteractionfailreason) (enum)
  - [`FCatalogInteractionFailed`](#fcataloginteractionfailed) (delegate)
  - [`UCatalogInteractionComponent`](#ucataloginteractioncomponent) (classe)
- [Inventário](#inventário)
  - [`UCatalogItemDefinition`](#ucatalogitemdefinition) (classe)
  - [`FCatalogInventoryStack`](#fcataloginventorystack) (struct)
  - [`FCatalogInventoryEntry`](#fcataloginventoryentry) (struct)
  - [`FCatalogInventoryList`](#fcataloginventorylist) (struct)
  - [`FCatalogInventoryChanged`](#fcataloginventorychanged) (delegate)
  - [`FCatalogItemUsed`](#fcatalogitemused) (delegate)
  - [`UCatalogInventoryComponent`](#ucataloginventorycomponent) (classe)
  - [`ACatalogPickupActor`](#acatalogpickupactor) (classe)
- [Habilidades](#habilidades)
  - [`UCatalogAbilityDefinition`](#ucatalogabilitydefinition) (classe)
  - [`UCatalogAbility`](#ucatalogability) (classe)
  - [`UCatalogAbility_Dash`](#ucatalogability_dash) (classe)
  - [`UCatalogAbility_MeleeSweep`](#ucatalogability_meleesweep) (classe)
  - [`UCatalogDashAbilityStub`](#ucatalogdashabilitystub) (classe)
  - [`FCatalogAbilityCooldown`](#fcatalogabilitycooldown) (struct)
  - [`FCatalogAbilityActivated`](#fcatalogabilityactivated) (delegate)
  - [`FCatalogAbilityFailed`](#fcatalogabilityfailed) (delegate)
  - [`UCatalogAbilityComponent`](#ucatalogabilitycomponent) (classe)
- [Vida](#vida)
  - [`FCatalogHealthChanged`](#fcataloghealthchanged) (delegate)
  - [`FCatalogDeath`](#fcatalogdeath) (delegate)
  - [`UCatalogHealthComponent`](#ucataloghealthcomponent) (classe)
- [Save](#save)
  - [`FCatalogSaveHeader`](#fcatalogsaveheader) (struct)
  - [`FCatalogInventorySlot`](#fcataloginventoryslot) (struct)
  - [`FCatalogSaveMetadata`](#fcatalogsavemetadata) (struct)
  - [`FCatalogSaveRecord`](#fcatalogsaverecord) (struct)
  - [`FCatalogSavePayload`](#fcatalogsavepayload) (struct)
  - [`ECatalogSaveSlotState`](#ecatalogsaveslotstate) (enum)
  - [`FCatalogSaveSlotInfo`](#fcatalogsaveslotinfo) (struct)
  - [`UCatalogSaveGame`](#ucatalogsavegame) (classe)
  - [`UCatalogSaveTemplate`](#ucatalogsavetemplate) (classe)
  - [`ICatalogSaveParticipant`](#icatalogsaveparticipant) (interface)
  - [`FCatalogSaveCompleted`](#fcatalogsavecompleted) (delegate)
  - [`FCatalogLoadCompleted`](#fcatalogloadcompleted) (delegate)
  - [`FCatalogSaveLoaded`](#fcatalogsaveloaded) (delegate)
  - [`UCatalogSaveSubsystem`](#ucatalogsavesubsystem) (classe)
- [Núcleo](#núcleo)
  - [`UCatalogAnchorSettings`](#ucataloganchorsettings) (classe)
  - [`UCatalogGameplaySubsystem`](#ucataloggameplaysubsystem) (classe)
- [Blueprint Library](#blueprint-library)
  - [`UCatalogBlueprintLibrary`](#ucatalogblueprintlibrary) (classe)

## Interação

### ICatalogInteractable

*interface* · `ICatalogInteractable.h`

```cpp
class ICatalogInteractable
```

Contrato de interação (trace + interface) do catálogo §8. Implemente em atores estáticos / pickups; UCatalogInteractionComponent faz o trace no servidor. Chame sempre via ICatalogInteractable::Execute_* (funciona para implementações C++ e Blueprint). Sem override, o UHT devolve os defaults do tipo: prompt vazio, false, duração 0 (instantâneo), prioridade 0, não bloqueado, foco sem efeito.

**Funções**

- `FText GetInteractionPrompt() const`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - Texto curto do prompt para a UI (ex.: "Abrir baú"). Lido no cliente dono e no servidor.
- `bool OnInteract(AActor* InstigatorActor)`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - Executa a interação (só autoridade). Retorna true se consumiu.
- `float GetInteractionDuration() const`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - Segundos que o jogador precisa segurar. 0 (ou inválido) = instantâneo. Lido pelo servidor ao iniciar.
- `bool IsInteractionBlocked(AActor* InstigatorActor) const`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - true = existe mas não pode ser usado agora (trancado, sem chave...). O servidor checa antes de completar e durante o hold; o alvo continua recebendo foco para a UI mostrar "Trancado".
- `int32 GetInteractionPriority() const`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - Desempate entre candidatos do assist esférico do UCatalogInteractionComponent (bUseSphereAssist): maior vence; empate vai para o mais perto do centro da mira. Padrão 0. Só o servidor lê. Alvo que o trace de mira acerta direto não passa por aqui.
- `void OnFocusChanged(bool bHasFocus, AActor* InstigatorActor)`
  - Specifiers: `BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction"`
  - Hook cosmético (highlight/outline). Só roda na máquina do jogador que olha; nunca mude estado de jogo aqui.

### FCatalogInteractionTargetChanged

*delegate* · `CatalogInteractionComponent.h`

```cpp
void FCatalogInteractionTargetChanged(AActor* NewTarget)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam
```

### FCatalogInteractionCompleted

*delegate* · `CatalogInteractionComponent.h`

```cpp
void FCatalogInteractionCompleted(AActor* Target, bool bConsumed)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

### ECatalogInteractionFailReason

*enum* · `CatalogInteractionComponent.h`

```cpp
enum class ECatalogInteractionFailReason : uint8
```

Por que uma tentativa de interação foi recusada (OnInteractionFailed).

**Valores**

- `Blocked`
  - Há alvo, mas IsInteractionBlocked (trancado, sem chave...); também quando o bloqueio derruba um hold.
- `OutOfRange`
  - A mira está num interagível além do alcance (TraceDistance).
- `Cooldown`
  - Tentativa dentro do InteractionCooldownSeconds.
- `NoTarget`
  - Nada interagível sob a mira (ou o alvo do hold saiu dela).

### FCatalogInteractionFailed

*delegate* · `CatalogInteractionComponent.h`

```cpp
void FCatalogInteractionFailed(AActor* Target, ECatalogInteractionFailReason Reason)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

### UCatalogInteractionComponent

*classe* · `CatalogInteractionComponent.h`

```cpp
class UCatalogInteractionComponent : public UActorComponent
```

Componente de interação por trace (catálogo §8), v2 com hold-to-interact.
- O servidor faz o trace e decide; o cliente só pede via Server RPC e lê o estado replicado.
- CurrentTarget e o estado do hold replicam só para o dono (COND_OwnerOnly).
- Hold: o servidor guarda início (relógio do servidor) + duração e completa no tick;
o dono calcula o progresso localmente com AGameStateBase::GetServerWorldTimeSeconds(), sem replicar um float por frame.
- Cooldown anti-spam e IsInteractionBlocked são checados no servidor antes de completar.
- Assist esférico opcional (bUseSphereAssist) escolhe por GetInteractionPriority e depois pela mira.
- Recusas viram OnInteractionFailed; CVar catalog.Interaction.Debug desenha o trace (não-Shipping).

**Propriedades**

- `float TraceDistance`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "10.0")`
  - Alcance (cm), medido a partir do pawn controlado; a mira sai da câmera (GetPlayerViewPoint). Sem pawn, medido do ponto de visão (ou do próprio dono sem controller). Padrão: Project Settings.
- `float TraceRadius = 8.f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0")`
  - Raio do sphere-sweep (0 = line trace).
- `TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction"`
  - Canal de colisão usado no trace (padrão Visibility / Custom).
- `bool bUseSphereAssist = false`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction"`
  - Quando o trace de mira não acha interagível, varre uma esfera de AssistRadius ao longo da mira e escolhe entre os interagíveis visíveis: maior GetInteractionPriority, depois o mais perto do centro da mira.
- `float AssistRadius = 60.f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0", EditCondition = "bUseSphereAssist")`
  - Raio (cm) do assist esférico.
- `bool bAutoTrace = true`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction"`
  - Servidor atualiza CurrentTarget a cada tick (desligue se preferir poll manual; durante um hold o trace roda sempre).
- `float TraceIntervalSeconds = 0.05f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0")`
  - Intervalo (s) entre traces automáticos do servidor (bAutoTrace); 0 = todo tick. Hold e tentativas (TryInteract/BeginInteract) sempre fazem trace fresco; 0.05 s (20 Hz) não muda a sensação da mira.
- `float InteractionCooldownSeconds = 0.25f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0")`
  - Intervalo mínimo entre tentativas aceitas pelo servidor (s): barra spam de RPC/tecla.
- `TWeakObjectPtr<AActor> CurrentTarget`
  - Specifiers: `BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentTarget, Category = "Catalog|Interaction"`
  - Alvo atual, decidido pelo servidor (OwnerOnly — não precisa ir para sim proxy).
- `FCatalogInteractionTargetChanged OnTargetChanged`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Interaction"`
  - Alvo mudou (inclui nullptr). Dispara no servidor e no cliente dono.
- `FCatalogInteractionCompleted OnInteractionCompleted`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Interaction"`
  - OnInteract rodou no servidor. Dispara no servidor e, se o dono for remoto, no cliente dono (Client RPC).
- `FCatalogInteractionFailed OnInteractionFailed`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Interaction"`
  - Tentativa recusada (Reason). Target é o alvo envolvido e pode ser nulo (Cooldown sem alvo, NoTarget). Dispara no servidor e, se o dono for remoto, uma vez no cliente dono (Client RPC não confiável: é cosmético). Host listen e AI só recebem o broadcast local, sem duplicar.

**Funções**

- `bool TryInteract()`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Interaction"`
  - Interação instantânea. Servidor: refaz o trace e chama OnInteract; retorna o resultado dele. Alvo com duração > 0 é recusado (use BeginInteract) para o cliente não pular o hold. Cliente: envia ServerTryInteract e retorna se havia alvo replicado (o resultado real só existe no servidor).
- `bool BeginInteract()`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Interaction"`
  - Botão pressionado. Duração 0 completa já (igual TryInteract); duração > 0 inicia o hold, que o servidor completa sozinho no tick. Cliente: envia ServerBeginInteract e retorna se havia alvo replicado.
- `void EndInteract()`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Interaction"`
  - Botão solto: cancela o hold em andamento (no-op sem hold).
- `float GetInteractionProgress() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Interaction"`
  - Progresso 0..1 do hold atual (0 sem hold). Válido no servidor e no cliente dono.
- `bool IsInteracting() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Interaction"`
  - true enquanto há um hold em andamento (servidor e cliente dono).
- `AActor* RefreshTarget()`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Interaction"`
  - Refaz o trace e atualiza CurrentTarget. Sem autoridade, só devolve o alvo replicado.
- `FText GetCurrentPrompt() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Interaction"`

## Inventário

### UCatalogItemDefinition

*classe* · `CatalogItemDefinition.h`

```cpp
class UCatalogItemDefinition : public UPrimaryDataAsset
```

Definição estática de um item (dados de design, não estado do jogador). O inventário guarda ponteiros para estes assets; o save guarda só o ItemId.
Asset Manager: o load resolve ItemId → asset via UAssetManager, então o projeto PRECISA registrar o tipo "CatalogItem" em DefaultGame.ini ([/Script/Engine.AssetManagerSettings] PrimaryAssetTypesToScan, classe base CatalogItemDefinition, bHasBlueprintClasses=False). Sem isso o load só acha definições já carregadas em memória (ex.: criadas em runtime); as demais ele avisa e pula.

**Propriedades**

- `FText DisplayName`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Nome mostrado na UI e no prompt do pickup.
- `FText Description`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (MultiLine = true)`
- `TSoftObjectPtr<UTexture2D> Icon`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Soft: o ícone só carrega quando a UI pede, não junto com o inventário.
- `int32 MaxStackSize = 1`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "1")`
  - Máximo por pilha (1 = não empilha, ex.: armas).
- `FGameplayTagContainer ItemTags`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Tags livres para filtros de gameplay/UI (ex.: Item.Consumable).
- `FGameplayTag Category`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Categoria principal (ex.: Item.Category.Consumable) para filtros e abas de UI; vazia = sem categoria.
- `float Weight = 0.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "0")`
  - Peso por unidade; soma contra UCatalogInventoryComponent::MaxWeight (0 = não pesa).
- `bool bUsable = false`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory|Use"`
  - Pode ser usado por UCatalogInventoryComponent::UseItem.
- `bool bConsumeOnUse = true`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory|Use", meta = (EditCondition = "bUsable")`
  - Cada uso consome 1 unidade; quem consome é o UseItem (validado no servidor), não o OnUsed.
- `float UseCooldownSeconds = 0.25f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory|Use", meta = (ClampMin = "0", EditCondition = "bUsable && !bConsumeOnUse")`
  - Intervalo mínimo entre usos de um item que NÃO se consome (o estoque já limita os consumíveis); 0 = sem limite.

**Funções**

- `void OnUsed(AActor* User) const`
  - Specifiers: `BlueprintNativeEvent, Category = "Catalog|Inventory"`
  - Efeito de UMA unidade usada (chamado no servidor, já depois do consumo). User é o Pawn (ou o dono do inventário, se não houver Pawn). Padrão: nada. Const: a definição é asset compartilhado, não guarda estado.
- `FName GetItemId() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Id gravado em FCatalogInventorySlot::ItemId (== nome do asset): renomear o asset quebra saves antigos.

### FCatalogInventoryStack

*struct* · `CatalogInventoryTypes.h`

```cpp
struct FCatalogInventoryStack
```

Cópia de leitura de uma pilha para UI/Blueprint (alterá-la não mexe no inventário).

**Propriedades**

- `TObjectPtr<const UCatalogItemDefinition> Item = nullptr`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Inventory"`
- `int32 Quantity = 0`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Inventory"`

### FCatalogInventoryEntry

*struct* · `CatalogInventoryTypes.h`

```cpp
struct FCatalogInventoryEntry : public FFastArraySerializerItem
```

Uma pilha replicada. Os callbacks rodam só nos clientes, item a item, e avisam o componente dono para disparar OnInventoryChanged (o servidor dispara direto em Add/Remove).

**Propriedades**

- `TObjectPtr<const UCatalogItemDefinition> Item = nullptr`
- `int32 Quantity = 0`

### FCatalogInventoryList

*struct* · `CatalogInventoryTypes.h`

```cpp
struct FCatalogInventoryList : public FFastArraySerializer
```

Lista replicada por delta: só as pilhas marcadas como sujas trafegam (padrão Lyra).

**Propriedades**

- `TArray<FCatalogInventoryEntry> Entries`
- `TObjectPtr<UCatalogInventoryComponent> OwnerComponent = nullptr`
  - Specifiers: `NotReplicated, Transient`
  - Componente dono, usado pelos callbacks de cliente; local, nunca vai pela rede.

### FCatalogInventoryChanged

*delegate* · `CatalogInventoryComponent.h`

```cpp
void FCatalogInventoryChanged()  // DECLARE_DYNAMIC_MULTICAST_DELEGATE
```

### FCatalogItemUsed

*delegate* · `CatalogInventoryComponent.h`

```cpp
void FCatalogItemUsed(const UCatalogItemDefinition* Item, int32 Quantity)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

### UCatalogInventoryComponent

*classe* · `CatalogInventoryComponent.h`

```cpp
class UCatalogInventoryComponent : public UActorComponent, public ICatalogSaveParticipant
```

Inventário de pilhas com servidor autoritativo.
- Add/Remove só com autoridade; o cliente pede ao servidor (ex.: ServerDropItem, interação).
- Replica por FFastArraySerializer com COND_OwnerOnly: só o cliente dono recebe (menos
banda e nada de ler mochila alheia). Por isso o dono do ator precisa ser uma conexão de jogador (PlayerController, Pawn possuído, PlayerState); num baú nenhum cliente recebe.
- Salva via ICatalogSaveParticipant (chave "Inventory", grava em FCatalogSavePayload::Inventory).

**Propriedades**

- `int32 Capacity`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "1")`
  - Número máximo de pilhas (padrão: Project Settings → Catalog Anchor).
- `bool bSaveWithGame = true`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Registra no UCatalogSaveSubsystem. O payload tem UM Inventory, o do jogador LOCAL: só registra em Pawn/PlayerController/PlayerState cujo controller seja de jogador local. Baú/NPC nunca registram (sobrescreveriam o do jogador) e, num listen server, os remotos não entram no save do host.
- `TSubclassOf<ACatalogPickupActor> DropPickupClass`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
  - Pickup gerado por ServerDropItem. A classe base não tem malha: use um BP filho com mesh.
- `float MaxWeight = 0.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "0")`
  - Peso máximo somado (UCatalogItemDefinition::Weight x quantidade); 0 = ilimitado. AddItem entra parcial ao esbarrar nele. Load de save não enforça (não perde item se o máximo foi reduzido): só trava novas entradas.
- `FCatalogInventoryChanged OnInventoryChanged`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Inventory"`
  - Dispara no servidor após Add/Remove/Load e nos clientes quando a replicação chega.
- `FCatalogItemUsed OnItemUsed`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Inventory"`
  - Dispara só no servidor, ao fim de um UseItem bem-sucedido (depois do consumo e dos OnUsed).

**Funções**

- `int32 AddItem(const UCatalogItemDefinition* Item, int32 Quantity)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory"`
  - Completa pilhas do item e abre novas até Capacity. Devolve quanto entrou.
- `int32 RemoveItem(const UCatalogItemDefinition* Item, int32 Quantity)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory"`
  - Retira até Quantity (parcial se faltar). Devolve quanto saiu.
- `int32 GetItemCount(const UCatalogItemDefinition* Item) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
- `bool HasItem(const UCatalogItemDefinition* Item, int32 Quantity = 1) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - true se há pelo menos Quantity (mínimo 1) do item.
- `TArray<FCatalogInventoryStack> GetStacks() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
- `TArray<FCatalogInventoryStack> GetItemsByTag(FGameplayTag Tag) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Pilhas cujo item tem Tag em ItemTags (hierárquico: pai casa com filho) ou Category igual/filha dela. Tag inválida devolve vazio.
- `float GetTotalWeight() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Soma de Weight x quantidade de todas as pilhas (0 se nenhum item pesa).
- `int32 GetAddableQuantity(const UCatalogItemDefinition* Item, int32 Quantity) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Quanto de Quantity caberia agora em AddItem (pilhas, Capacity e MaxWeight), sem alterar nada.
- `bool UseItem(const UCatalogItemDefinition* Item, int32 Quantity = 1)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory"`
  - Usa Quantity unidades (1..FCatalogInventoryRules::MaxUseQuantity), tudo ou nada: item precisa ser bUsable e haver Quantity no inventário. Consome Quantity se bConsumeOnUse, chama OnUsed(User) uma vez por unidade e dispara OnItemUsed. Devolve false (sem efeito) se algo falhar.
- `void ServerUseItem(const UCatalogItemDefinition* Item, int32 Quantity)`
  - Specifiers: `Server, Reliable, WithValidation, BlueprintCallable, Category = "Catalog|Inventory"`
  - Cliente dono pede para usar: o servidor valida tudo em UseItem. Chamado no servidor, executa direto.
- `bool TransferTo(UCatalogInventoryComponent* Other, const UCatalogItemDefinition* Item, int32 Quantity)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory"`
  - Move Quantity de Item para Other, tudo ou nada: só move se este inventário tem a quantidade E Other comporta (pilhas, Capacity, MaxWeight). Exige autoridade nos dois. Sem RPC: transferir entre inventários é decisão de gameplay do servidor (baú, troca), não pedido direto do cliente.
- `void ServerDropItem(const UCatalogItemDefinition* Item, int32 Quantity)`
  - Specifiers: `Server, Reliable, WithValidation, BlueprintCallable, Category = "Catalog|Inventory"`
  - Cliente dono pede para soltar itens: o servidor remove só o que existe e gera DropPickupClass à frente do Pawn. Chamado no servidor, executa direto.

### ACatalogPickupActor

*classe* · `CatalogPickupActor.h`

```cpp
class ACatalogPickupActor : public AActor, public ICatalogInteractable
```

Item solto no mundo. Interagir põe Item no UCatalogInventoryComponent do instigador (no Pawn ou no Controller dele); some quando tudo entrou, senão fica com o resto. A malha bloqueia só Visibility (canal padrão do trace de interação): pawns atravessam. Highlight de foco usa Custom Depth: habilite-o no projeto e use um post-process. Dorme na rede (DORM_Initial) depois da replicação inicial: se mudar Item/Quantity em runtime fora de OnInteract, chame FlushNetDormancy() para os clientes receberem.

**Propriedades**

- `TObjectPtr<UStaticMeshComponent> Mesh`
  - Specifiers: `VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory"`
- `TObjectPtr<const UCatalogItemDefinition> Item = nullptr`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Replicated, Category = "Catalog|Inventory", meta = (ExposeOnSpawn = true)`
  - Replicado: o prompt é montado no cliente e pickups soltos em runtime não existem no mapa.
- `int32 Quantity = 1`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Replicated, Category = "Catalog|Inventory", meta = (ExposeOnSpawn = true, ClampMin = "1")`
  - Replicado: coleta parcial reduz o valor e o prompt dos clientes acompanha.

## Habilidades

### UCatalogAbilityDefinition

*classe* · `CatalogAbilityDefinition.h`

```cpp
class UCatalogAbilityDefinition : public UPrimaryDataAsset
```

Asset de uma habilidade: tag (identidade), nome, cooldown e o comportamento (Instanced). Concedido a atores via UCatalogAbilityComponent::GrantedAbilities. Para achá-lo pelo AssetManager, registre o tipo "CatalogAbility" em PrimaryAssetTypesToScan.

**Propriedades**

- `FGameplayTag AbilityTag`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability"`
  - Identidade da habilidade; é o que TryActivateAbility recebe. Ex.: Catalog.Ability.Dash.
- `FText DisplayName`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability"`
- `float CooldownSeconds = 1.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0")`
  - Segundos até poder ativar de novo, contados no relógio do servidor. 0 = sem cooldown.
- `TObjectPtr<UCatalogAbility> Behavior`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Instanced, Category = "Catalog|Ability"`
  - O que a habilidade faz. Compartilhado por todos os atores que usam este asset.

### UCatalogAbility

*classe* · `CatalogAbility.h`

```cpp
class UCatalogAbility : public UObject
```

Comportamento de uma habilidade "GAS-lite" (sem o plugin GameplayAbilities). Vive como subobjeto Instanced de um UCatalogAbilityDefinition: a MESMA instância atende todo ator que usa o asset (e todo mundo de PIE), então não guarde estado por ator aqui.
Fluxo, orquestrado por UCatalogAbilityComponent:
- CanActivate: consulta sem efeito colateral; no servidor e, como estimativa, no cliente.
- Activate: efeito de gameplay; só com autoridade sobre o avatar.
- PlayCosmetics: animação/VFX; em todas as máquinas, via multicast do componente.
Subclasses Blueprint: implemente os eventos K2_ (todos opcionais).

### UCatalogAbility_Dash

*classe* · `CatalogAbility.h`

```cpp
class UCatalogAbility_Dash : public UCatalogAbility
```

Dash: mesma física de UCatalogGameplaySubsystem::TriggerDash — ACharacter recebe LaunchCharacter; ator com física no root recebe impulso (mudança de velocidade).

**Propriedades**

- `float Strength`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0")`
  - Velocidade do dash (cm/s). Padrão: Project Settings → Catalog Anchor → DefaultDashStrength.

### UCatalogAbility_MeleeSweep

*classe* · `CatalogAbility.h`

```cpp
class UCatalogAbility_MeleeSweep : public UCatalogAbility
```

Golpe corpo a corpo: varre uma esfera à frente do avatar e aplica dano uma vez por ator. Activate (servidor) só faz o dano. A montagem toca em PlayCosmetics: tocada só no servidor ela não apareceria nos clientes, por isso o multicast do componente a chama em todos.

**Propriedades**

- `float Range = 150.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0")`
  - Distância (cm) do centro do avatar até o centro da esfera no fim da varredura.
- `float Radius = 60.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "1.0")`
  - Raio da esfera (cm).
- `float Damage = 20.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0")`
  - Dano por ator atingido (UGameplayStatics::ApplyDamage).
- `TSubclassOf<UDamageType> DamageTypeClass`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability"`
  - Vazio = UDamageType.
- `TSoftObjectPtr<UAnimMontage> Montage`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability"`
  - Opcional; só em ACharacter. Carregado sob demanda (LoadSynchronous): pré-carregue se o hitch importar.

### UCatalogDashAbilityStub

*classe* · `CatalogDashAbilityStub.h`

```cpp
class UCatalogDashAbilityStub : public UObject
```

LEGADO (v0.1): placeholder pedagógico da "Gameplay Ability (dash)" do MVP §8. Código novo: UCatalogAbilityDefinition + UCatalogAbility_Dash num UCatalogAbilityComponent (cooldown replicado, Gameplay Tags, Server RPC). Mantido para não quebrar Blueprints da v0.1. Consumido por UCatalogGameplaySubsystem::TryTriggerDashFromAbility.
Sem depender de GameplayAbilities neste scaffold (plugin opcional no .uplugin). Quando GAS estiver habilitado, troque esta UObject por UGameplayAbility e mova a lógica de cooldown/montagem para ActivateAbility + CommitAbility.
NÃO é um sistema de combate completo nem um cooked montage.

**Propriedades**

- `float CooldownSeconds = 1.2f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (ClampMin = "0.1")`
- `float ImpulseStrength = 1200.f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (ClampMin = "0.0")`
  - Velocidade do dash (cm/s).
- `FName MontageSlot = TEXT("FullBody")`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability"`
  - Reservado para a variante GAS (ainda não lido pelo stub).
- `FSoftObjectPath MontagePath`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (AllowedClasses = "/Script/Engine.AnimMontage")`
  - Montagem da variante GAS — asset real vive no Content do seu projeto, não neste repo.

### FCatalogAbilityCooldown

*struct* · `CatalogAbilityComponent.h`

```cpp
struct FCatalogAbilityCooldown
```

Quando uma habilidade volta a ficar pronta.

**Propriedades**

- `FGameplayTag AbilityTag`
- `double ReadyAtServerTime = 0.0`
  - Em AGameStateBase::GetServerWorldTimeSeconds(): o mesmo relógio no servidor e no cliente.

### FCatalogAbilityActivated

*delegate* · `CatalogAbilityComponent.h`

```cpp
void FCatalogAbilityActivated(FGameplayTag AbilityTag)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam
```

### FCatalogAbilityFailed

*delegate* · `CatalogAbilityComponent.h`

```cpp
void FCatalogAbilityFailed(FGameplayTag AbilityTag, FName Reason)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

### UCatalogAbilityComponent

*classe* · `CatalogAbilityComponent.h`

```cpp
class UCatalogAbilityComponent : public UActorComponent
```

Habilidades "GAS-lite": o servidor decide, cooldowns replicam só para o dono.
- TryActivateAbility no cliente envia ServerTryActivateAbility (o ator precisa ter
conexão dona, ex.: o pawn possuído) e devolve só uma estimativa local.
- Sucesso: cooldown + MulticastAbilityActivated (cosmético e OnAbilityActivated em todas as máquinas).
- Falha: OnAbilityFailed no servidor e no cliente dono. Reason: "NotGranted", "NoBehavior",
"Cooldown" ou "CannotActivate". GrantedAbilities não replica: vem dos defaults, iguais no servidor e nos clientes.

**Propriedades**

- `TArray<TObjectPtr<UCatalogAbilityDefinition>> GrantedAbilities`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability"`
- `FCatalogAbilityActivated OnAbilityActivated`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Ability"`
  - Servidor e clientes que veem o ator (multicast não confiável: pode faltar num cliente).
- `FCatalogAbilityFailed OnAbilityFailed`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Ability"`
  - Servidor e cliente dono.

**Funções**

- `bool TryActivateAbility(FGameplayTag AbilityTag)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Ability"`
  - Servidor: ativa de fato e devolve o resultado. Cliente: pede ao servidor e devolve se localmente parece possível (concedida, fora de cooldown, CanActivate).
- `float GetCooldownRemaining(FGameplayTag AbilityTag) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Ability"`
  - Segundos até poder ativar de novo (0 = pronta).
- `bool IsOnCooldown(FGameplayTag AbilityTag) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Ability"`
- `UCatalogAbilityDefinition* FindAbilityDefinition(FGameplayTag AbilityTag) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Ability"`
  - Definição concedida com essa tag exata, ou nullptr.

## Vida

### FCatalogHealthChanged

*delegate* · `CatalogHealthComponent.h`

```cpp
void FCatalogHealthChanged(float NewHealth, float Delta)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

### FCatalogDeath

*delegate* · `CatalogHealthComponent.h`

```cpp
void FCatalogDeath(AActor* Killer)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam
```

### UCatalogHealthComponent

*classe* · `CatalogHealthComponent.h`

```cpp
class UCatalogHealthComponent : public UActorComponent
```

Vida replicada do dono. Escuta AActor::OnTakeAnyDamage, então qualquer UGameplayStatics::ApplyDamage (ex.: UCatalogAbility_MeleeSweep) a reduz; ator com bCanBeDamaged = false não perde vida. O servidor decide; Health replica para todos (barra sobre inimigos), e OnHealthChanged/OnDeath disparam no servidor e nos clientes. OnDeath: uma vez por transição vivo→morto (ResetHealth revive). Cliente vê só o valor replicado: dano e respawn no mesmo envio (ex.: ResetHealth dentro de OnDeath) se fundem e o cliente pode não receber OnDeath; o estado final (GetHealth/IsDead) sempre converge, então reaja a IsDead() e não conte só com OnDeath. Na 1ª replicação recebida o cliente emite OnHealthChanged(Health, 0) (sem delta falso contra o default) e OnDeath(nullptr) se já nasce morto.

**Propriedades**

- `float MaxHealth = 100.f`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Health", meta = (ClampMin = "1.0")`
- `FCatalogHealthChanged OnHealthChanged`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Health"`
- `FCatalogDeath OnDeath`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Health"`
  - Vida chegou a 0. Killer = DamageCauser (pode ser nulo no cliente).

**Funções**

- `float GetHealth() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
- `float GetHealthFraction() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
- `bool IsDead() const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
- `float Heal(float Amount)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health"`
  - Cura (só autoridade; mortos não curam). Devolve quanto curou de fato.
- `void SetHealth(float NewHealth)`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health"`
  - Define a vida direto (ex.: restaurar um save). Só autoridade. Limita a [0, MaxHealth]; valor não finito é ignorado; não depende de bCanBeDamaged. Eventos com a mesma regra do dano (OnDeath ao chegar a 0); valor > 0 num morto o revive, como ResetHealth.
- `void ResetHealth()`
  - Specifiers: `BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health"`
  - Volta a MaxHealth (respawn). Só autoridade.

## Save

### FCatalogSaveHeader

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogSaveHeader
```

Cabeçalho de versionamento do save.

**Propriedades**

- `int32 SaveVersion = 0`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Versão inteira do schema (não confundir com VersionName do .uplugin).
  - O padrão PRECISA ser 0 ("sem versão"): o SaveGame da Unreal só grava campos diferentes do default. Com default 0, toda versão real (>= 1) vai para o disco, e um arquivo sem o campo é lido como v0 e migrado. Toda gravação pelo UCatalogSaveSubsystem carimba GCatalogSaveSchemaVersion, então não preencha à mão.
- `FGuid SaveId`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Identidade da partida: criada no primeiro save e mantida nos saves seguintes.
- `FDateTime Timestamp`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Horário UTC da gravação.

### FCatalogInventorySlot

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogInventorySlot
```

**Propriedades**

- `FName ItemId = NAME_None`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
- `int32 Quantity = 0`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`

### FCatalogSaveMetadata

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogSaveMetadata
```

Dados para o menu de saves, legíveis sem carregar o jogo (LoadPayloadFromSlot). Nasceu na v2: arquivos v1 chegam com estes defaults, que por isso significam "desconhecido".

**Propriedades**

- `FString MapName`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Mapa ativo ao salvar, sem prefixo de PIE (ex.: "Lvl_Forest"). Vazio = desconhecido.
- `double PlaytimeSeconds = 0.0`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Tempo de jogo acumulado ao longo de saves/loads (s). 0 = novo jogo ou desconhecido.
- `FString DisplayName`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Nome mostrado ao jogador. O subsystem preenche com o slot; participantes podem trocar. Vazio = use o slot.

### FCatalogSaveRecord

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogSaveRecord
```

Bloco opaco de um ICatalogSaveParticipant, achado pela Key. Deixa cada sistema evoluir o próprio formato (Version) sem subir o schema global.

**Propriedades**

- `FName Key = NAME_None`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Chave estável do dono (GetSaveRecordKey), ex.: "Inventory". Uma por payload.
- `int32 Version = 0`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Versão do formato de Data, controlada pelo participante. Mesma regra do SaveVersion: comece em 1, porque 0 (default) pode não ir para o disco e significa "sem versão".
- `TArray<uint8> Data`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - Bytes do participante (ex.: escritos com FMemoryWriter). O plugin não interpreta.

### FCatalogSavePayload

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogSavePayload
```

Tudo o que vai para um slot. Ver a regra de delta em GCatalogSaveSchemaVersion.

**Propriedades**

- `FCatalogSaveHeader Header`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
- `TArray<FCatalogInventorySlot> Inventory`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
- `FVector PlayerLocation = FVector::ZeroVector`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
- `float PlayerYaw = 0.f`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
- `FCatalogSaveMetadata Metadata`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - v2. Preenchido pelo UCatalogSaveSubsystem a cada SaveGame.
- `TArray<FCatalogSaveRecord> Records`
  - Specifiers: `EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save"`
  - v2. Um registro por participante; procure com CatalogFindSaveRecord.

### ECatalogSaveSlotState

*enum* · `CatalogSaveTypes.h`

```cpp
enum class ECatalogSaveSlotState : uint8
```

Estado de um slot em UCatalogSaveSubsystem::ListSaveSlots.

**Valores**

- `Ok`
  - Arquivo legível e íntegro.
- `Recovered`
  - O arquivo do slot estava corrompido; os dados vieram de um backup.
- `Unreadable`
  - Nem o arquivo nem os backups puderam ser lidos (corrompido, ou schema mais novo que o plugin).
- `Busy`
  - Gravação/leitura assíncrona em andamento: o arquivo não foi aberto.

### FCatalogSaveSlotInfo

*struct* · `CatalogSaveTypes.h`

```cpp
struct FCatalogSaveSlotInfo
```

Resumo leve de um slot (menu de saves): sem inventário nem registros.

**Propriedades**

- `FName SlotName = NAME_None`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
- `ECatalogSaveSlotState State = ECatalogSaveSlotState::Unreadable`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
- `int32 SaveVersion = 0`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
  - Schema do arquivo antes da migração (0 = sem o campo/legado). Só vale com State Ok ou Recovered.
- `FDateTime Timestamp`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
  - Horário UTC da gravação.
- `FString MapName`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
  - Mapa ativo ao salvar. Vazio = desconhecido.
- `double PlaytimeSeconds = 0.0`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
  - Tempo de jogo acumulado (s).
- `FString DisplayName`
  - Specifiers: `BlueprintReadOnly, Category = "Catalog|Save"`
  - Nome mostrado ao jogador; vazio = use SlotName.

### UCatalogSaveGame

*classe* · `CatalogSaveGame.h`

```cpp
class UCatalogSaveGame : public USaveGame
```

Envelope gravado pelo backend SaveGame da plataforma (UGameplayStatics::SaveGameToSlot).

**Propriedades**

- `uint32 PayloadCrc = 0`
  - Specifiers: `SaveGame`
  - CRC32 do Payload (CatalogComputePayloadCrc), conferido no load. Vem ANTES do Payload de propósito: um arquivo truncado perde o fim do Payload, não o CRC, e a conferência pega. 0 = arquivo de antes do CRC existir (v0.4): carrega sem conferência.
- `FCatalogSavePayload Payload`
  - Specifiers: `SaveGame`

### UCatalogSaveTemplate

*classe* · `CatalogSaveTemplate.h`

```cpp
class UCatalogSaveTemplate : public UDataAsset
```

Estado inicial de um save (ex.: "novo jogo") editável como Data Asset. Convenção de nome: CA_Save_\<Nome>.
Validado no Editor por UCatalogSaveDataValidator (módulo CatalogAnchorEditor): ItemId vazio, ItemId duplicado, Quantity \<= 0 ou SaveVersion fora de 0..GCatalogSaveSchemaVersion tornam o asset Invalid.
Uso ("novo jogo"): passe Payload para UCatalogSaveSubsystem::SavePayloadToSlot. Ele vira o estado corrente, então os SaveGame seguintes partem dele (e o tempo de jogo, de Metadata.PlaytimeSeconds). Metadata e Records podem ficar vazios no asset. UCatalogGameplaySubsystem::SaveToSlot continua funcionando (delega para o mesmo lugar).

**Propriedades**

- `FCatalogSavePayload Payload`
  - Specifiers: `EditAnywhere, BlueprintReadOnly, Category = "Catalog|Save"`

### ICatalogSaveParticipant

*interface* · `ICatalogSaveParticipant.h`

```cpp
class ICatalogSaveParticipant
```

Objeto que entra no save do UCatalogSaveSubsystem (ex.: UCatalogInventoryComponent). Só C++: as funções são virtuais puras, não UFUNCTION, para o subsystem chamar direto. Registre com UCatalogSaveSubsystem::RegisterParticipant (só na autoridade) e desregistre ao sair; o subsystem guarda ponteiro fraco, então esquecer de desregistrar não vaza.

**Funções**

- `virtual FName GetSaveRecordKey() const = 0`
  - Chave estável (ex.: "Inventory"); vira FCatalogSaveRecord::Key se você usar Records. Não mude entre versões. Única entre os registrados: registrar outro objeto com a mesma chave tira este do save.
- `virtual void WriteSaveData(FCatalogSavePayload& Payload) const = 0`
  - Chamado na autoridade antes de gravar: escreva seu estado no Payload (campos próprios ou um Record).
- `virtual void ReadSaveData(const FCatalogSavePayload& Payload) = 0`
  - Chamado após um load bem-sucedido e ao registrar um objeto que ainda não tem o estado ativo (uma vez por load); Payload já está migrado para GCatalogSaveSchemaVersion.

### FCatalogSaveCompleted

*delegate* · `CatalogSaveSubsystem.h`

```cpp
void FCatalogSaveCompleted(FName SlotName, bool bSuccess)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

Fim de uma gravação tentada (síncrona ou assíncrona).

### FCatalogLoadCompleted

*delegate* · `CatalogSaveSubsystem.h`

```cpp
void FCatalogLoadCompleted(FName SlotName, bool bSuccess)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
```

Fim de um load tentado (síncrono ou assíncrono). Em sucesso, os participantes já leram o payload.

### FCatalogSaveLoaded

*delegate* · `CatalogSaveSubsystem.h`

```cpp
void FCatalogSaveLoaded(const FCatalogSavePayload& Payload)  // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam
```

Payload já migrado. Também é o tipo de UCatalogGameplaySubsystem::OnSaveLoaded.

### UCatalogSaveSubsystem

*classe* · `CatalogSaveSubsystem.h`

```cpp
class UCatalogSaveSubsystem : public UGameInstanceSubsystem
```

Dono do save do jogo. Vive no GameInstance para atravessar troca de mapa (o autosave e o tempo de jogo não zeram a cada level).
Fluxo: SaveGame parte do estado corrente (último payload salvo ou carregado), carimba Metadata, pede WriteSaveData aos participantes e grava. LoadGame lê, migra uma cópia e entrega aos participantes via ReadSaveData. Partir do estado corrente evita apagar dados de quem não está registrado agora (ex.: inventário num mapa de menu).
Slots: ASCII [A-Za-z0-9_-], 1..128, sem diferenciar maiúsculas, user index 0. Um slot com operação assíncrona em andamento recusa qualquer outra operação até terminar. Chamadas recusadas na hora (nome inválido, slot ocupado) só retornam false. Gravações e loads aceitos (SaveGame, SavePayloadToSlot, LoadGame, Async*) sempre terminam em OnSaveCompleted/OnLoadCompleted; LoadPayloadFromSlot e DeleteSlot não disparam eventos.
Integridade: toda gravação leva o CRC32 do payload (UCatalogSaveGame::PayloadCrc) e todo load o confere. Antes de sobrescrever um slot, o arquivo atual vira `\<slot>_bak` (e os anteriores `_bak2`... até Settings → BackupCount), mas só se ele próprio estiver íntegro. Slot ilegível ou com CRC errado cai no backup mais novo íntegro (log Warning); schema mais novo que o plugin NÃO cai: não é corrupção. A rotação roda no game thread, antes da escrita assíncrona. Nomes terminados em `_bak`/`_bakN` são reservados aos backups (IsValidSlotName).

**Propriedades**

- `FCatalogSaveCompleted OnSaveCompleted`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Save"`
- `FCatalogLoadCompleted OnLoadCompleted`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Save"`
- `FCatalogSaveLoaded OnPayloadLoaded`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Save"`
  - Dispara em todo LoadGame/AsyncLoadGame bem-sucedido, depois dos participantes e antes de OnLoadCompleted.

**Funções**

- `bool SaveGame(FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Coleta os participantes e grava, de forma síncrona (trava o frame em saves grandes).
- `bool LoadGame(FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Lê, migra e entrega aos participantes, de forma síncrona.
- `bool AsyncSaveGame(FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - SaveGame com a escrita em disco fora do game thread. true = iniciou; resultado em OnSaveCompleted.
- `bool AsyncLoadGame(FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - LoadGame com a leitura fora do game thread. true = iniciou; resultado em OnLoadCompleted.
- `bool SavePayloadToSlot(const FCatalogSavePayload& Payload, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Grava o payload como veio (sem participantes nem Metadata), só carimbando o header. Ele vira o estado corrente: é assim que se começa um "novo jogo" a partir de um UCatalogSaveTemplate.
- `bool LoadPayloadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Só lê e migra (para menus de save): não chama participantes nem muda o estado corrente.
- `bool DeleteSlot(FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Apaga o arquivo do slot e todos os seus backups (senão o slot voltaria no fallback). false se o arquivo do slot não existia ou não pôde ser apagado, ou se o nome é inválido.
- `TArray<FCatalogSaveSlotInfo> ListSaveSlots()`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - Slots em disco, do mais recente para o mais antigo (ilegíveis e ocupados por último), com metadados leves para menus. Backups não entram. A lista vem de ISaveGameSystem::GetSaveGameNames; em plataforma sem esse suporte vale só o que este subsystem já gravou/leu nesta sessão mais o AutosaveSlotName. Abre cada arquivo: chame ao abrir o menu, não por frame.
- `bool DoesSlotExist(FName SlotName) const`
  - Specifiers: `BlueprintPure, Category = "Catalog|Save"`
- `static bool IsValidSlotName(FName SlotName)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Save"`
  - Letras/dígitos ASCII, '_' ou '-', 1..128 caracteres: o nome vira arquivo .sav e precisa ser portátil. Não diferencia maiúsculas: "Slot" e "slot" são o mesmo arquivo. Recusa nomes de dispositivo do Windows (CON, PRN, AUX, NUL, COM0-9, LPT0-9) e os que terminam em "_bak" ou "_bak" + dígitos, reservados aos backups.

## Núcleo

### UCatalogAnchorSettings

*classe* · `CatalogAnchorSettings.h`

```cpp
class UCatalogAnchorSettings : public UDeveloperSettings
```

Project Settings → Plugins → Catalog Anchor (gravado em DefaultGame.ini). Fonte única dos defaults do plugin: componentes e subsystems leem daqui em vez de repetir números no código.

**Propriedades**

- `float DefaultTraceDistance = 250.f`
  - Specifiers: `config, EditAnywhere, Category = "Interaction", meta = (ClampMin = "10.0")`
  - TraceDistance inicial de cada UCatalogInteractionComponent (cm).
- `int32 ExpectedSaveSchemaVersion = 2`
  - Specifiers: `config, EditAnywhere, Category = "Save", meta = (ClampMin = "1")`
  - Versão de schema de save que este projeto declara usar. Se uma atualização do plugin mudar GCatalogSaveSchemaVersion, o módulo avisa no log ao iniciar: revise as migrações e então atualize este valor.
- `float AutosaveIntervalSeconds = 0.f`
  - Specifiers: `config, EditAnywhere, Category = "Save", meta = (ClampMin = "0.0")`
  - Intervalo do autosave do UCatalogSaveSubsystem (s). 0 = desligado.
- `FName AutosaveSlotName = TEXT("Autosave")`
  - Specifiers: `config, EditAnywhere, Category = "Save"`
  - Slot usado pelo autosave.
- `int32 BackupCount = 1`
  - Specifiers: `config, EditAnywhere, Category = "Save", meta = (ClampMin = "0", ClampMax = "9")`
  - Quantas versões anteriores de cada slot o save mantém (`\<slot>_bak`, `\<slot>_bak2`...). Um slot corrompido (arquivo ilegível ou CRC que não confere) é lido do backup mais novo íntegro. 0 = sem backup nem fallback.
- `int32 DefaultInventoryCapacity = 24`
  - Specifiers: `config, EditAnywhere, Category = "Inventory", meta = (ClampMin = "1")`
  - Capacidade inicial (número de pilhas) de cada UCatalogInventoryComponent.
- `float DefaultDashCooldown = 1.2f`
  - Specifiers: `config, EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0")`
  - Cooldown usado por TryTriggerDashStub quando CooldownSeconds \< 0 (s).
- `float DefaultDashStrength = 1200.f`
  - Specifiers: `config, EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0")`
  - Velocidade aplicada por TryTriggerDashStub (cm/s).

### UCatalogGameplaySubsystem

*classe* · `CatalogGameplaySubsystem.h`

```cpp
class UCatalogGameplaySubsystem : public UWorldSubsystem
```

WorldSubsystem leve com o dash stub do plugin e a API de save antiga. Save/load moraram aqui na v1; hoje delegam ao UCatalogSaveSubsystem (GameInstance) e ficam só por compatibilidade. Código novo: use UCatalogSaveSubsystem direto.

**Propriedades**

- `FCatalogSaveLoaded OnSaveLoaded`
  - Specifiers: `BlueprintAssignable, Category = "Catalog|Save"`

**Funções**

- `bool SaveToSlot(const FCatalogSavePayload& Payload, FName SlotName = TEXT("Default"))`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - = UCatalogSaveSubsystem::SavePayloadToSlot: grava o payload como veio (sem participantes), carimbando versão de schema, SaveId (se ausente) e horário UTC.
- `bool LoadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save"`
  - = UCatalogSaveSubsystem::LoadPayloadFromSlot + OnSaveLoaded. Não chama ReadSaveData dos participantes; para restaurá-los use UCatalogSaveSubsystem::LoadGame.
- `static bool IsValidSlotName(FName SlotName)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Save"`
  - = UCatalogSaveSubsystem::IsValidSlotName (ASCII [A-Za-z0-9_-], 1..128, sem diferenciar maiúsculas, sem CON/NUL/COM1... nem sufixo _bak).
- `bool TryTriggerDashStub(AActor* Instigator, float CooldownSeconds = -1.f)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Ability"`
  - Dash sem GAS. Precisa de autoridade (servidor/standalone): ACharacter recebe LaunchCharacter; ator com física no root recebe impulso. Outros atores → false. CooldownSeconds \< 0 usa Project Settings → Catalog Anchor → DefaultDashCooldown.
- `bool TryTriggerDashFromAbility(AActor* Instigator, const UCatalogDashAbilityStub* Ability)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Ability"`
  - Mesmo dash, com cooldown e força vindos de um UCatalogDashAbilityStub.

## Blueprint Library

### UCatalogBlueprintLibrary

*classe* · `CatalogBlueprintLibrary.h`

```cpp
class UCatalogBlueprintLibrary : public UBlueprintFunctionLibrary
```

Atalhos de Blueprint sobre a API pública do plugin: save/load/lista/apagar sem precisar achar o subsystem no grafo, e consultas de vida/inventário a partir de qualquer ator. Não tem regra própria: cada função delega ao subsystem/componente e falha limpo (false/0/nullptr) quando o mundo, o GameInstance ou o componente não existem.

**Funções**

- `static UCatalogSaveSubsystem* GetSaveSubsystem(const UObject* WorldContextObject)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - UCatalogSaveSubsystem do GameInstance do mundo do contexto; nullptr sem mundo ou GameInstance.
- `static bool SaveCatalogGame(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::SaveGame.
- `static bool LoadCatalogGame(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::LoadGame.
- `static bool AsyncSaveCatalogGame(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::AsyncSaveGame (resultado em OnSaveCompleted).
- `static bool AsyncLoadCatalogGame(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::AsyncLoadGame (resultado em OnLoadCompleted).
- `static TArray<FCatalogSaveSlotInfo> ListCatalogSaveSlots(const UObject* WorldContextObject)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::ListSaveSlots. Vazio sem subsystem.
- `static bool DeleteCatalogSaveSlot(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::DeleteSlot (apaga também os backups).
- `static bool DoesCatalogSaveSlotExist(const UObject* WorldContextObject, FName SlotName)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject")`
  - = UCatalogSaveSubsystem::DoesSlotExist.
- `static UCatalogHealthComponent* GetHealthComponent(const AActor* Actor)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
  - Componente de vida do ator (para um Controller, o do pawn controlado). nullptr se não houver.
- `static bool GetActorHealthFraction(const AActor* Actor, float& OutFraction)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
  - true e a fração 0..1 se o ator tem vida.
- `static bool IsActorDead(const AActor* Actor)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Health"`
  - false também para quem não tem componente de vida.
- `static float HealActor(AActor* Actor, float Amount)`
  - Specifiers: `BlueprintCallable, Category = "Catalog|Health"`
  - = UCatalogHealthComponent::Heal (só na autoridade). Devolve o que foi curado; 0 sem componente.
- `static UCatalogInventoryComponent* GetInventoryComponent(const AActor* Actor)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Componente de inventário do ator (para um Controller, o do pawn controlado). nullptr se não houver.
- `static int32 GetActorItemCount(const AActor* Actor, const UCatalogItemDefinition* Item)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - Quantidade do item no inventário do ator; 0 sem inventário ou sem item.
- `static bool ActorHasItem(const AActor* Actor, const UCatalogItemDefinition* Item, int32 Quantity = 1)`
  - Specifiers: `BlueprintPure, Category = "Catalog|Inventory"`
  - false sem inventário.
