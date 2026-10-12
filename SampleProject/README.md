# SampleCatalog — demo jogável sem nenhum asset binário

Projeto UE **5.8+** que demonstra todos os sistemas do plugin `CatalogAnchor`. A cena inteira é
montada em C++ sobre o mapa vazio `/Engine/Maps/Entry`: **zero `.uasset`/`.umap` no repositório**.

## O que aparece ao apertar Play

| Elemento | Classe | Demonstra |
|---|---|---|
| Arena com chão, paredes, sol, céu | `ASampleArenaActor` | ator replicado montado no construtor |
| Personagem 3ª pessoa | `ASampleCatalogCharacter` | 3 componentes do plugin + Enhanced Input criado em código |
| Esferas coletáveis | `ASampleCatalogPickup` | `ACatalogPickupActor` + inventário em FastArray |
| Baú aberto | `ASampleChestActor` | hold-to-interact (1,5 s), loot, foco |
| Bonecos de treino | `ASampleTrainingDummy` | `UCatalogHealthComponent`: barra de vida, morte e respawn |
| Baú trancado | `ASampleChestActor` | `IsInteractionBlocked` ("Trancado") até pegar a chave |
| HUD em Canvas | `ASampleCatalogHUD` | prompt, barra de hold, inventário, cooldowns, avisos de save |

## Controles

| Tecla | Ação |
|---|---|
| WASD / mouse | mover / olhar |
| Espaço | pular |
| **E (segurar)** | interagir (pegar, abrir baú) |
| Shift | dash (`Catalog.Ability.Dash`) |
| Botão esquerdo | golpe em área (`Catalog.Ability.Melee`) |
| G | soltar 1 unidade do primeiro item (Server RPC) |
| Q | usar poção: cura 35 de vida (Server RPC revalidado) |
| F5 / F9 | salvar / carregar o slot `Quick` (só no host) |

## Como abrir

1. Clone o repositório inteiro: o `.uproject` acha o plugin em `../Plugins` via
   `AdditionalPluginDirectories` (não copie o plugin para `SampleProject/Plugins/`).
2. Botão direito em `SampleCatalog.uproject` → **Generate Visual Studio project files**.
3. Compile `SampleCatalogEditor` (Development Editor, Win64) e abra o projeto.
4. **Play**. Para multiplayer: Play → *Number of Players* = 2, *Net Mode* = **Play As Listen Server**.

## Decisões que valem ler

- **Itens e habilidades sem assets** (`USampleItemCatalog`, `USampleAbilityCatalog`): são default
  subobjects de um CDO. Isso dá a eles **nome estável para a rede**; um `NewObject` no pacote
  transient chegaria **nulo** nos clientes, no FastArray e nos RPCs.
- **Piso em Z = −200**: sem PlayerStart, o jogador nasce na origem *antes* do `BeginPlay` do GameMode;
  com o piso abaixo, ele cai alguns centímetros em vez de nascer atravessado no chão.
- **Save só no host**: o `UCatalogSaveSubsystem` vive no GameInstance de quem tem autoridade.
- **Highlight**: `OnFocusChanged` liga Custom Depth (`r.CustomDepth=3`). O contorno visual pede
  um material de pós-processo (asset), que o sample não traz; o HUD marca o alvo com um quadrado.

## O que não há

`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, malha de personagem, animações,
números de FPS ou "benchmark PIE".
