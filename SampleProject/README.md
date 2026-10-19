# SampleCatalog — demo jogável sem nenhum asset binário

Projeto UE **5.8+** que demonstra todos os sistemas do plugin `CatalogAnchor`. A cena inteira é
montada em C++ sobre o mapa vazio `/Engine/Maps/Entry`: **zero `.uasset`/`.umap` no repositório**.

## O que aparece ao apertar Play

| Elemento | Classe | Demonstra |
|---|---|---|
| Arena com chão, paredes, sol, céu | `ASampleArenaActor` | ator replicado montado no construtor |
| Personagem 3ª pessoa | `ASampleCatalogCharacter` | componentes do plugin + Enhanced Input criado em código (contexto removido ao perder o controller) |
| Esferas coletáveis | `ASampleCatalogPickup` | `ACatalogPickupActor` + inventário em FastArray |
| Baú aberto | `ASampleChestActor` | hold-to-interact (1,5 s), loot, foco |
| Bonecos de treino | `ASampleTrainingDummy` | `UCatalogHealthComponent`: barra de vida, morte e respawn |
| Baú trancado | `ASampleChestActor` | `IsInteractionBlocked` ("Trancado") até pegar a chave |
| HUD em UMG (código) | `USampleHUDWidget` | vida, prompt + barra de hold, inventário, cooldowns, avisos |
| Menu de saves (Tab) | `USampleSaveMenuWidget` | listar/salvar/carregar/apagar slots com metadados do `UCatalogSaveSubsystem` |
| Posição e vida no save | `USamplePlayerSaveComponent` | `ICatalogSaveParticipant` com `FCatalogSaveRecord` próprio |

O `ASampleCatalogHUD` cria os widgets e só desenha no Canvas o que é projetado do mundo: mira,
marcador do alvo e barras de vida sobre os bonecos.

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
| **Tab** | abre o menu de saves; Tab ou Esc fecham (no PIE, Esc também para a sessão: prefira Tab) |

Gamepad (layout Xbox; outros controles usam os botões equivalentes): stick esquerdo move, stick direito olha,
A pula, X (segurar) interage, RB dash, RT golpe, Y poção, D-pad baixo solta item, D-pad esquerda/direita
salva/carrega, Menu abre o menu de saves (Menu ou B fecham).

Plataformas: Windows, Mac (Metal) e Linux (Vulkan); `Config/DefaultEngine.ini` define o RHI por plataforma e a
escala de UI por DPI (menor lateral), sem tamanho de tela fixo.

## Menu de saves

- Lista sempre `Quick`, `Slot1`, `Slot2`, `Slot3` e, além deles, todo `.sav` do disco que o
  `UCatalogSaveSubsystem` consegue ler (ex.: `Autosave`). Cada linha mostra nome, mapa, data e tempo de
  jogo (`FCatalogSaveMetadata`, lidos com `LoadPayloadFromSlot`, sem aplicar o save).
- **Salvar** grava no slot; **Carregar** aplica (inventário, posição, vida); **Apagar** pede um
  segundo clique ("Confirmar?"). O resultado de salvar/carregar aparece no topo da tela.
- Com o menu aberto o input é só de UI (o personagem para) e o cursor aparece; ao fechar volta ao jogo.
- Num cliente remoto os botões ficam desabilitados: save/load só no host.

## O que vai no save

| Dado | Quem grava | Regras no load |
|---|---|---|
| Inventário | `UCatalogInventoryComponent` (plugin) | itens resolvidos por `ItemId` |
| Posição, yaw, vida | `USamplePlayerSaveComponent`, registro `SamplePlayer` v1 | posição NaN/infinita, abaixo do `KillZ` ou fora do mundo é ignorada (fica onde está); se não há espaço livre, idem; vida NaN ou ≤ 0 volta cheia, acima do máximo é limitada |

Os dois participam só na autoridade e só para o pawn do PlayerController local. A morte do sample
trava o personagem por 2 s (vida 0 replica e os clientes veem a morte) e depois o
teleporta ao centro com vida cheia (mesmo pawn): o próximo save grava esse estado. Morto, o personagem não
anda, pula, interage, golpeia, dá dash, solta item nem usa poção, também no cliente remoto (a vida replicada
bloqueia o input local; o servidor revalida). Olhar em volta continua livre. Se um
jogo trocar isso por um pawn novo, o subsystem entrega o save ativo ao novo participante, que volta à
última posição salva.

## Localização (pt-BR → en)

Todo texto visível usa `LOCTEXT` no namespace `SampleCatalog` (fonte em pt-BR). Para gerar inglês:

1. Editor → **Tools → Localization Dashboard** → target **Game**: em *Gather Text*, marque
   *Gather from Text Files* com o diretório `Source` (extensões `h`, `cpp`).
2. *Cultures*: native **pt-BR**; adicione **en**.
3. **Gather Text** → **Export Text** (gera `.po` em `Content/Localization/Game/en/`) → traduza →
   **Import Text** → **Compile Text** (gera o `.locres`).
4. Para testar: Editor Preferences → *Region & Language* → *Preview Game Language* = en, ou
   rode o jogo com `-culture=en`. Ao empacotar, inclua `en` em *Project Settings → Packaging →
   Localizations to Package*.

O Dashboard grava a configuração em `Config/Localization/*.ini` e em `DefaultEditor.ini`; versione
esses arquivos e o `Content/Localization/` gerado.

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
- **UI sem Widget Blueprint**: os `UUserWidget` montam a árvore com `WidgetTree->ConstructWidget` em
  `NativeOnInitialized` (roda na criação, com o PlayerController dono). Módulos `UMG`, `Slate`, `SlateCore`.
- **Highlight**: `OnFocusChanged` liga Custom Depth (`r.CustomDepth=3`). O contorno visual pede
  um material de pós-processo (asset), que o sample não traz; o HUD marca o alvo com um quadrado.

## O que não há

`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, malha de personagem, animações,
números de FPS ou "benchmark PIE". O único personagem animado que vem com a engine
(`/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP`) é conteúdo dos tutoriais do
editor, sem garantia de existir ou ser cozido em outras versões; por isso o boneco segue de formas básicas.
