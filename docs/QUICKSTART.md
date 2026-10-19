# Quickstart: do zero a "abrir um baú e salvar"

Objetivo: um Pawn que mira num baú, **segura** a tecla para abrir, recebe o item no inventário e **grava** o jogo.
Cada passo tem a versão C++ e a Blueprint; os nomes vêm dos headers (referência completa em [API.md](API.md)).
Precisa de UE 5.8+ e do toolchain C++ ([INSTALACAO.md](INSTALACAO.md)).

1. **Instale o plugin.** Copie `Plugins/CatalogAnchor/` para `SeuProjeto/Plugins/`, gere os project files, compile
   o target `*Editor` e confirme **Catalog Anchor (UE5)** em Edit → Plugins.
   No seu `*.Build.cs`, só para C++: `PublicDependencyModuleNames.Add("CatalogAnchor");`

2. **Crie o item.** Content Browser → botão direito → categoria *Catalog Anchor* → item (`UCatalogItemDefinition`),
   por exemplo `DA_Chave`. Preencha `DisplayName` e `MaxStackSize`. Registre os tipos no Asset Manager
   (bloco `[/Script/Engine.AssetManagerSettings]` de `SampleProject/Config/DefaultGame.ini`).

3. **Dê os componentes ao Pawn.** Adicione `Catalog Interaction Component` e `Catalog Inventory Component`.
   - C++: `Interaction = CreateDefaultSubobject<UCatalogInteractionComponent>(TEXT("Interaction"));` e o mesmo para
     `UCatalogInventoryComponent`. Ajuste `TraceDistance` se precisar.
   - Blueprint: *Add Component* nos dois.

4. **Ligue a tecla de interagir.** Pressed chama `BeginInteract`, Released chama `EndInteract`
   (o hold usa a duração do baú; duração 0 completa no Pressed).
   - C++: `Interaction->BeginInteract();` e `Interaction->EndInteract();` nos handlers do Enhanced Input.
   - Blueprint: Pressed → `Begin Interact`, Released → `End Interact`, ambos no componente.

5. **Crie o baú.** Um Actor que implementa `ICatalogInteractable` e põe o item no inventário de quem interagiu.
   - C++:

     ```cpp
     UCLASS()
     class AMeuBau : public AActor, public ICatalogInteractable
     {
         GENERATED_BODY()
     public:
         UPROPERTY(EditAnywhere) TObjectPtr<const UCatalogItemDefinition> Loot;

         virtual FText GetInteractionPrompt_Implementation() const override { return INVTEXT("Abrir baú"); }
         virtual float GetInteractionDuration_Implementation() const override { return 1.0f; } // segura 1 s
         virtual bool OnInteract_Implementation(AActor* InstigatorActor) override
         {
             UCatalogInventoryComponent* Inv = UCatalogBlueprintLibrary::GetInventoryComponent(InstigatorActor);
             return HasAuthority() && Inv && Loot && Inv->AddItem(Loot, 1) > 0;
         }
     };
     ```

   - Blueprint: nova classe Actor com malha e colisão que bloqueie **Visibility**; em Class Settings → Interfaces
     adicione **Catalog Interactable**; implemente `Get Interaction Prompt` (texto), `Get Interaction Duration`
     (segundos) e `On Interact`: `Get Inventory Component` (Catalog Library) com o Instigator → `Add Item`.

6. **Posicione o baú e o Pawn no mapa.** Coloque o `AMeuBau`/Blueprint, defina `Loot` com `DA_Chave`, e rode o
   Pawn como Default Pawn do GameMode. Nenhum outro setup é necessário: o servidor decide, o cliente só pede.

7. **Jogue e abra.** Mire no baú (o prompt vem de `GetCurrentPrompt`) e segure a tecla. Para um HUD de progresso use
   `GetInteractionProgress` (0..1) e `IsInteracting`. Recusas (trancado, longe, cooldown) chegam em
   `OnInteractionFailed` com `ECatalogInteractionFailReason`. Para bloquear o baú, sobrescreva `IsInteractionBlocked`.

8. **Confira o item.** Leia o inventário pelo atalho:
   - C++: `UCatalogBlueprintLibrary::ActorHasItem(Pawn, Loot)` ou `GetActorItemCount(Pawn, Loot)`.
   - Blueprint: `Actor Has Item` / `Get Actor Item Count` (categoria *Catalog|Inventory*).

9. **Salve.** O inventário já é participante do save (`ICatalogSaveParticipant`): não precisa registrar nada.
   - C++: `UCatalogBlueprintLibrary::SaveCatalogGame(this, TEXT("Slot1"));`
     (ou `GetSaveSubsystem(this)->SaveGame(TEXT("Slot1"))`; `AsyncSaveGame` não trava o frame).
   - Blueprint: nó `Save Catalog Game` com `Slot Name = Slot1`.
   - Atalho de console para testar: `catalog.Save.Now Slot1`.

10. **Carregue e confirme.** `LoadCatalogGame(this, TEXT("Slot1"))` (Blueprint: `Load Catalog Game`) ou
    `catalog.Save.Load Slot1`; `catalog.Save.List` mostra os slots. O inventário volta como estava ao salvar.
    Para um menu de saves use `ListCatalogSaveSlots`, `DoesCatalogSaveSlotExist` e `DeleteCatalogSaveSlot`.

Em multiplayer o baú roda no servidor (`OnInteract` só com autoridade) e o inventário replica para o dono. O save
vive no host: veja os limites em [LIMITATIONS.md](../LIMITATIONS.md). Um exemplo completo (baú trancado com chave,
HUD, menu de saves) está em `SampleProject/` (`ASampleChestActor`).
