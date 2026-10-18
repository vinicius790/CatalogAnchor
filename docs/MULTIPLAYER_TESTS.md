# Testes de multiplayer (manual, reproduzível)

Roteiro para validar o plugin CatalogAnchor em rede no Unreal Engine 5.8 com o `SampleProject`.
Cada item tem passos, o resultado esperado (tirado do código atual) e um status.
Os testes automáticos (`CatalogAnchor.*`) cobrem a lógica sem rede; este roteiro cobre o que só
aparece com servidor e clientes separados.

> **Status geral (2026-10-18): MP-03, MP-04, MP-06 e a restauração do MP-07 executados de verdade, headless.** `py -3 tools/mp_run.py --project <uproject> --case all`
> sobe servidor dedicado + 2 clientes (e um standalone para o MP-07), roda um roteiro de comandos de console que chamam os mesmos
> pontos de entrada do input (`BeginInteract`/`EndInteract`, `TryActivateAbility`) e imprime PASS/FAIL com a linha de log:
> 27 de 27 predicados passaram na última execução (UE 5.8.3, Win64 Development Editor, `-nullrhi`). Ainda sem input humano: F5/F9 pelas teclas, mouse/câmera reais, HUD e listen server para MP-03/04/06.

## 0. Preparação

1. Compile o editor do sample (Development Editor, Win64; sem Visual Studio: `UE_SDKS_ROOT`, ver `tools/get_msvc.py`): `py -3 tools/ue.py test` (compila e roda os testes) ou o fluxo de
   [INSTALACAO.md](INSTALACAO.md).
2. Variáveis usadas abaixo (PowerShell):

   ```powershell
   $UE = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
   $PROJ = "<repo>\SampleProject\SampleCatalog.uproject"
   ```

3. Em toda janela, ligue o log detalhado do plugin no console (tecla `` ` ``):
   `Log LogCatalogAnchor Verbose`. Mortes aparecem como `<ator> morreu (causador: <ator>)`.
4. Comandos de console usados para inspeção objetiva (imprimem no Output Log / console):
   - `GetAll CatalogInventoryComponent InventoryList`
   - `GetAll CatalogHealthComponent Health`
   - `GetAll CatalogAbilityComponent Cooldowns`
   - `GetAll CatalogInteractionComponent CurrentTarget`

   Em cada máquina, a lista mostra os componentes de **todos** os pawns que ela conhece; o
   valor vazio/padrão num pawn alheio é justamente o que prova o `COND_OwnerOnly`.

## 1. Modos de execução

### 1A. PIE, listen server

Play (seta ao lado de ▶) → *Multiplayer Options*: **Number of Players = 2**,
**Net Mode = Play As Listen Server**, *Run Under One Process* ligado. ▶ Play.
Janela 1 = host (servidor com jogador local), janela 2 = cliente.

### 1B. PIE, servidor dedicado

Mesmo menu: **Number of Players = 2**, **Net Mode = Play As Client**. O editor sobe um servidor
dedicado (sem jogador local, `NM_DedicatedServer`) e duas janelas de cliente.

### 1C. Linha de comando, servidor dedicado

```powershell
# servidor (porta padrão 7777); -server seleciona o modo dedicado no UnrealEditor.exe
& $UE $PROJ /Engine/Maps/Entry -server -log
# cada cliente (rode duas vezes para dois jogadores)
& $UE $PROJ 127.0.0.1 -game -log -windowed -resx=1280 -resy=720
```

Flags conferidas no código da engine 5.8: no binário de editor, `-server` liga o modo dedicado
(`StaticDedicatedServerCheck`, `Core/Private/Misc/CoreMisc.cpp`) e é avaliado antes de `-game`
(`LaunchEngineLoop.cpp`), então `-game -server` também funciona, mas o `-game` é ignorado.
`-game` sem `-server` roda o jogo sem o editor. O primeiro argumento que não começa com `-` é a URL
(mapa no servidor, IP no cliente).

### 1D. Linha de comando, listen server

```powershell
& $UE $PROJ "/Engine/Maps/Entry?listen" -game -log -windowed -resx=1280 -resy=720
& $UE $PROJ 127.0.0.1 -game -log -windowed -resx=1280 -resy=720
```

## 2. Casos

### 2.0 Evidência headless: `catalog.Debug.Dump`

Comando de diagnóstico do plugin (não-Shipping): `catalog.Debug.Dump` loga em `LogCatalogAnchor` uma linha por ator com
vida/inventário que **aquela máquina** enxerga e, no fim, `SaveParticipants=<n>`. Com `N T` repete N vezes a cada T s
(útil em `-ExecCmds`, sem input). Execução usada (cópia ASCII do repo; `UE_SDKS_ROOT` definido; `-Multiprocess` evita
bloquear no Turnkey/UBT):

```powershell
$UE = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$A  = "-log -nullrhi -unattended -nosplash -NoSound -nopause -Multiprocess"
Start-Process $UE "$PROJ /Engine/Maps/Entry -server $A -ExecCmds=`"catalog.Debug.Dump 10 6`" -abslog=srv.log"
Start-Process $UE "$PROJ 127.0.0.1 -game $A -ExecCmds=`"catalog.Debug.Dump 10 6`" -abslog=cli1.log"   # e cli2.log
```

Observado (linhas `[Dump]` dos logs):

| Máquina | NetMode | Pawns vistos (vida) | Pawn local | `SaveParticipants` |
| --- | --- | --- | --- | --- |
| Dedicado | DedicatedServer | 2 (100/100 cada) + 2 bonecos (75/75) | nenhum (`Locally=0`) | **0** |
| Cliente 1 e 2 (dedicado) | Client | os mesmos 2 pawns e 2 bonecos, todos replicados | exatamente 1 (`Locally=1`) | 0 |
| Listen (host) | ListenServer | 2 pawns + 2 bonecos | pawn do host (`Locally=1`) | **2** (inventário + `USamplePlayerSaveComponent` do host) |
| Cliente (listen) | Client | 2 pawns + 2 bonecos | exatamente 1 | 0 |

Comandos não-Shipping usados pelos roteiros (`CatalogDebugCommands.cpp`; `catalog.Save.Now|Load|List|Delete` em `CatalogConsoleCommands.cpp`):
`Dump [rótulo]` (cada linha ganha `Loc=`, `Target=`, `Focused=`, `Holding=`, `Cd=[tag:segundos]`, `Snap=<rótulo>`, e há uma linha `Interactable=` com `Prompt=`/`Blocked=`/`Duration=`),
`Hurt`, `GiveItem`, `Respawn`, `Interact`, `Aim <parte do nome> [jogador]` (servidor: leva o pawn a 150 cm e acha o pitch em que o raio da câmera passa pelo centro do alvo; `ClientSetRotation` mira o cliente dono),
`Hold Begin|End` e `Ability <tag|sufixo>` (rodam no pawn LOCAL: no cliente vão por Server RPC, o caminho do input real) e `Schedule <s ou instante Unix UTC> <cmd>`
(instante absoluto alinha servidor e clientes pelo relógio da máquina). Nomes de atores replicados (`SampleChestActor_0/1`) podem diferir entre servidor e cliente: o driver identifica o baú no cliente pelo prompt.

Limite: sem comandos o headless não tem input, então os inventários ficam vazios (`Items=0`) e nenhuma vida muda. Python
(`-ExecutePythonScript`/`EditorLevelLibrary.get_game_world`) não serve em `-game`/`-server`: o primeiro é recusado fora do editor
("cannot be used outside of the editor") e o segundo derruba o processo (access violation em EditorScriptingUtilities).

### MP-01 Replicação do inventário (COND_OwnerOnly)

Modos: 1A e 1B.

1. Cliente 2 pega a esfera de gemas à frente (segurar **E**).
2. Em cada janela: `GetAll CatalogInventoryComponent InventoryList`.

Esperado:
- O HUD do cliente 2 mostra as gemas; o HUD do outro jogador não muda.
- No cliente 2, só o componente do próprio pawn tem itens; o do outro pawn aparece vazio
  (o `InventoryList` replica só para a conexão dona).
- No servidor (host em 1A, log do dedicado em 1B) os dois componentes têm o estado real.
- **G** no cliente 2 solta 1 unidade (`ServerDropItem`): um pickup aparece para todos e a pilha
  diminui só no HUD do dono.

status: executado/parcial. Verificado via Dump: dedicado + 2 clientes conectam, cada cliente enxerga os 2 pawns e só 1 é local; inventários vazios nos 3 processos no spawn. NÃO verificado: pegar a esfera e o `COND_OwnerOnly` do conteúdo, soltar com **G** (bloqueado: exige segurar E / tecla; `BeginInteract` não é chamável por console).

### MP-02 Registro de participantes do save

Modos: 1A, 1B e 1C. Quem decide é `UCatalogInventoryComponent::UpdateSaveRegistration` (o
`USamplePlayerSaveComponent` do sample segue a mesma regra): registra só com autoridade **e**
quando o controller do pawn é `IsLocalPlayerController()`.

Esperado:
- **1A (listen):** só o inventário do host entra no save. Pegue gemas com os dois jogadores, aperte
  **F5** no host: `SampleProject/Saved/SaveGames/Quick.sav` é gravado e, após **F9**, o host volta ao
  estado salvo; o inventário do cliente não muda (não é participante).
- **Cliente remoto (1A/1B/1C):** **F5/F9** mostram "Save/load só no host (standalone ou listen
  server)" e nenhum arquivo é escrito (o componente nem registra: `BeginPlay` sem autoridade sai cedo).
- **Dedicado (1B/1C):** nenhum jogador é local no servidor, então nenhum inventário (nem o
  `SamplePlayer`) é registrado; o servidor não salva o inventário de ninguém. Se
  `AutosaveIntervalSeconds > 0` (Project Settings → Catalog Anchor; padrão 0 = desligado), o log
  do servidor mostra `Autosave pulado: nenhum participante registrado.` e nenhum `.sav` é criado.

status: executado/ok para o registro. Dedicado: `SaveParticipants=0` (nenhum jogador local, bate com o esperado). Listen: host `SaveParticipants=2` (inventário + `SamplePlayerSaveComponent`), cliente remoto `SaveParticipants=0`. NÃO verificado: F5/F9, arquivo `Quick.sav`, mensagem "Save/load só no host" e o log de autosave (bloqueado: teclas; `catalog.Save.Now` existe mas não foi rodado).

### MP-03 Foco de interação

Modos: 1A e 1B.

1. Cliente 2 mira na esfera de poções; o host/cliente 1 mira em outra coisa.

Esperado:
- Só a janela de quem mira mostra o prompt e o marcador do alvo (`OnFocusChanged` roda só na
  máquina do jogador local; `CurrentTarget` é OwnerOnly).
- `GetAll CatalogInteractionComponent CurrentTarget` no outro cliente: o componente do pawn alheio
  aparece com alvo vazio.
- No dedicado (1B) nenhum foco é anunciado no servidor (não há jogador local lá).
- Ao pegar o pickup, o foco some na hora para quem mirava (alvo destruído).

status: executado/ok (headless, `tools/mp_run.py --case 03`). Servidor dedicado: o pawn mirado tem `Target=<baú> Focused=0` (o dedicado não anuncia foco). Cliente dono: pawn local `Target=<baú> Focused=1`. Outro cliente: todos os pawns com `Target=None Focused=0` (OwnerOnly). NÃO verificado: o marcador visual/HUD e o foco sumir ao coletar o pickup (sem renderização).

### MP-04 Interação com hold (baú, 1,5 s)

Modos: 1A e 1B, com o cliente remoto e, em 1A, também com o host.

1. Segure **E** no baú aberto até completar.
2. Repita soltando antes de 1,5 s.
3. Repita desviando a mira no meio do hold.
4. No baú trancado, segure **E** sem a chave; depois pegue a chave e repita.

Esperado:
- (1) A barra de hold sobe no HUD do dono e o baú abre ao fim; o loot entra no inventário do
  dono (o servidor completa no tick). `OnInteractionCompleted` dispara uma vez no dono (Client RPC
  só para dono remoto; o host não recebe evento duplicado).
- (2) e (3) O hold é cancelado (`EndInteract` / alvo mudou) e nada é coletado.
- (4) Sem a chave: prompt "Trancado", hold não começa. Com a chave: abre normalmente.

status: executado/ok (`--case 04`): `Hold Begin` no cliente (Server RPC) devolveu `resultado=1`. Soltar em 1 s: baú segue "Abrir baú" `Blocked=0`, inventário vazio no servidor. Segurar 4 s: baú vira "Baú vazio" `Blocked=1` no servidor e nos 2 clientes, o dono tem 13 itens (3 poções + 10 gemas) no servidor e no cliente dono, o outro cliente vê o pawn do dono com `Items=0` (OwnerOnly). Baú trancado sem chave: "Trancado (precisa de Chave)", `Blocked=1`, inventário continua em 13. NÃO verificado: barra de hold no HUD, desviar a mira no meio do hold, abrir o trancado com a chave.

### MP-05 Vida, morte e respawn

Modos: 1A e 1B.

1. Com o cliente 2, golpeie (botão esquerdo, 25 de dano) um boneco de treino (75 de vida).
2. Golpeie o outro jogador até zerar a vida (100 → 4 golpes).
3. Em cada janela: `GetAll CatalogHealthComponent Health` antes e depois.

Esperado:
- (1) A barra do boneco cai 25 por golpe em **todas** as janelas (`Health` replica para todos).
  No 3º golpe o boneco some para todos, o log do servidor mostra uma única linha `morreu`, e
  3 s depois ele volta cheio (`ResetHealth` no servidor). Golpes no boneco morto não fazem nada.
- (2) Em todas as janelas a vida do jogador atingido vai a 0 e o cliente recebe `OnDeath`; ele não
  anda, pula, interage nem solta itens. 2 s depois volta ao ponto de respawn com vida cheia. O log
  do servidor mostra uma única linha `morreu` por morte.
- Morto não ativa habilidade: durante os 2 s, Shift/clique não fazem nada (`TryActivateAbility`
  falha com `CannotActivate`).
- No dedicado (1B) o comportamento é o mesmo; nada depende de controller local no servidor.

status: executado/parcial. Vida inicial replica certo: pawns 100/100 e bonecos 75/75 idênticos no servidor e nos clientes (dedicado e listen). NÃO verificado: dano, morte única, respawn (bloqueado: o golpe é input; não há comando de dano).

### MP-06 Habilidades e cooldown

Modos: 1A e 1B.

1. Cliente 2: **Shift** (dash, cooldown 1,2 s) e logo **Shift** de novo.
2. Em cada janela: `GetAll CatalogAbilityComponent Cooldowns`.

Esperado:
- O dash acontece no servidor e aparece para todos; a 2ª tentativa dentro de 1,2 s falha com
  `Cooldown` (log Verbose do servidor: `Habilidade 'Catalog.Ability.Dash' falhou ...`).
- O HUD de cooldown aparece só para o dono; no outro cliente o componente alheio tem `Cooldowns`
  vazio (OwnerOnly). Cooldowns sobrevivem ao respawn no mesmo pawn (o componente não é recriado).
- A montagem/efeito do golpe aparece nas outras janelas (multicast não confiável: perda ocasional
  sob perda de pacote é esperada).

status: executado/ok (`--case 06`). Dash nos 2 clientes: 1ª ativação `resultado=1`, 2ª `resultado=0`; log do servidor `Habilidade 'Catalog.Ability.Dash' falhou ... Cooldown.`; os 2 pawns andaram > 100 cm no servidor; o pawn local mostra `Cd=[Catalog.Ability.Dash:>0]` e o pawn alheio `0.00` (OwnerOnly). Melee a 150 cm do boneco: vida 75 -> 50 igual no servidor e nos 2 clientes. NÃO verificado: HUD de cooldown, efeito/montagem multicast, cooldown após respawn.

### MP-07 Persistência (save/load)

Modo: 1A (o save só existe no host) e standalone.

1. Host pega itens, toma dano, anda até outro ponto e aperta **F5**.
2. Gasta itens, se cura/toma dano, anda e aperta **F9**.
3. Feche o PIE, rode de novo e aperte **F9**.

Esperado:
- (2) e (3) Inventário, posição e vida do host voltam ao salvo (`SamplePlayer` usa
  `UCatalogHealthComponent::SetHealth`; vida salva inválida ou 0 vira vida cheia).
- O cliente 2 mantém o próprio inventário; o save do host não o altera.
- O arquivo `SampleProject/Saved/SaveGames/Quick.sav` tem data de modificação do passo 1.

status: executado/ok para a restauração (standalone `-game`, `--case 07`): `GiveItem Gem 5` + `Hurt 30` + `Aim` no baú, `catalog.Save.Now MPT`; depois `GiveItem Potion 4`, `Hurt 20` e outra posição (Items 9, Health 50/100, Loc diferente); `catalog.Save.Load MPT` devolveu Health 70/100, Items 5 e a posição salva. Antes: `Save.Now`/`List`/`Load` gravaram `mpt.sav` (schema v2, `SaveParticipants=2`). NÃO verificado: F5/F9 por tecla, reiniciar o PIE e o cliente remoto no listen.

## 3. Testes automáticos de mundo relacionados

Rodam sem rede, num `UWorld` próprio (Session Frontend → Automation, filtro `CatalogAnchor`, ou
`py -3 tools/ue.py test`):
`CatalogAnchor.Health.ApplyDamageEvents`, `.ResetHealthRespawn`, `.ReentrantDamageSingleDeath`,
`.DestroyedDuringCallback`, `.RespectsCanBeDamaged`, `.SetHealth` e
`CatalogAnchor.Abilities.DeadAvatarCannotActivate`.

status: executado: na reconferência de 2026-10-18 a suíte completa `CatalogAnchor` deu 67 de 67 Success, exit 0 (`py -3 tools/ue.py test`; relatório em Saved/Reports/index.json).
