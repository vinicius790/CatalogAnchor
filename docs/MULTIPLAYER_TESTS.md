# Testes de multiplayer (manual, reproduzível)

Roteiro para validar o plugin CatalogAnchor em rede no Unreal Engine 5.8 com o `SampleProject`.
Cada item tem passos, o resultado esperado (tirado do código atual) e um status.
Os testes automáticos (`CatalogAnchor.*`) cobrem a lógica sem rede; este roteiro cobre o que só
aparece com servidor e clientes separados.

> **Status geral (2026-10-13): parcial.** Executado de verdade, headless (`-nullrhi`, sem teclado nem mouse):
> servidor dedicado + 2 clientes e listen server + 1 cliente, com `catalog.Debug.Dump` (ver 2.0). Esse comando
> prova conexão, spawn, replicação de vida e o registro de participantes do save. Tudo que depende de input
> (segurar E, golpe, Shift, F5/F9) está marcado `bloqueado` com o motivo. UE 5.8.3, Win64 Development Editor.

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

Limite: o headless não tem input, então os inventários ficam vazios (`Items=0`) e nenhuma vida muda. Python
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

status: bloqueado: exige mira com câmera e interação (input). Só o pré-requisito foi visto: em cada cliente exatamente 1 pawn tem `Locally=1`; no dedicado nenhum (não há jogador local).

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

status: bloqueado: hold de 1,5 s no baú exige segurar E com mira; sem input no headless e sem comando de console para `BeginInteract`.

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

status: bloqueado: Shift/clique são input; sem comando de console para ativar habilidade.

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

status: bloqueado: F5/F9 são input e o fluxo pede reiniciar o PIE; `catalog.Save.Now/Load` existem mas não foram executados nesta rodada (sem estado a salvar sem input).

## 3. Testes automáticos de mundo relacionados

Rodam sem rede, num `UWorld` próprio (Session Frontend → Automation, filtro `CatalogAnchor`, ou
`py -3 tools/ue.py test`):
`CatalogAnchor.Health.ApplyDamageEvents`, `.ResetHealthRespawn`, `.ReentrantDamageSingleDeath`,
`.DestroyedDuringCallback`, `.RespectsCanBeDamaged`, `.SetHealth` e
`CatalogAnchor.Abilities.DeadAvatarCannotActivate`.

status: executado: testes automáticos de mundo rodaram via `py -3 tools/ue.py test`: 33 ok, 0 falhas, exit 0 (build + editor headless); lista completa em Saved/Reports/index.json.
