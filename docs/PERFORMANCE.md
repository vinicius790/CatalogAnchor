# Performance: orçamento proposto e como medir

> **Estado: nada aqui foi medido.** O repositório não publica números de FPS nem benchmark (LIMITATIONS.md). Os valores abaixo
> são **hipóteses de trabalho** (metas iniciais para refutar ou confirmar com um perfil), não resultados. O que é fato vem
> do código e está marcado como tal. Nenhuma plataforma além do Windows foi compilada.

## O que o código faz hoje (fatos lidos do código)

| Ponto | Comportamento atual |
|---|---|
| Único componente que faz tick | `UCatalogInteractionComponent` (`bCanEverTick = true`). Vida, habilidades e inventário têm `bCanEverTick = false` |
| Trace automático (servidor) | a cada `TraceIntervalSeconds` (padrão 0,05 s, ou seja, 20 Hz; `0` = todo tick), só com `bAutoTrace` e pawn possuído |
| Trace durante o hold | todo tick, sem intervalo, enquanto `IsInteracting()` (desviar o olhar precisa cancelar) |
| Assist de esfera | `bUseSphereAssist`: um sweep a mais por trace quando a mira não acha interagível |
| Cliente | o tick também roda no cliente (`SyncFocus` todo tick: foco local, destruição do alvo sem `OnRep`) |
| Save assíncrono | `AsyncSaveGame` monta o payload (participantes) e a serialização no game thread, e **também chama `RotateBackups` no game thread** antes de despachar; só a escrita do arquivo vai para outra thread. Não foi movido: `RotateBackups` move/copia os mesmos arquivos que a escrita assíncrona vai sobrescrever, então rodar os dois em threads diferentes cria concorrência entre rotação e escrita; e, sem a engine, a mudança não teria como ser validada |
| `RotateBackups` | lê o arquivo atual, desserializa e confere o CRC antes de rotacionar; depois move/copia arquivos |
| Trava de save | uma trava por slot (`BusySlots`) |
| Replicação | inventário por delta e só ao dono; vida a todos; pickups dormentes (ver [NETWORKING.md](NETWORKING.md)) |
| Instrumentação | cinco escopos `TRACE_CPUPROFILER_EVENT_SCOPE` (ver [Escopos de CPU no código](#escopos-de-cpu-no-código)); **não há** `SCOPE_CYCLE_COUNTER` nem CSV stats. Escritos sem compilar (*não verificado*) |

## Orçamento proposto (hipóteses a medir)

Medir sempre em build **Development ou Test** do jogo (não Editor, não PIE) e anotar máquina, número de jogadores e mapa. As
colunas "meta" são pontos de partida sem base empírica; ajuste depois do primeiro perfil e registre o resultado real no lugar da meta.

| Sistema | O que medir | Hipótese / meta inicial (não medida) | Como refutar |
|---|---|---|---|
| Interação, servidor, ocioso | custo de `TickComponent` por jogador com `bAutoTrace` e `TraceIntervalSeconds` = 0,05 | custo médio baixo o bastante para ser invisível frente ao resto do tick de gameplay (alvo de trabalho: dezenas de microssegundos por jogador) | Insights, escopo `CatalogInteraction_Tick` (existe; `RefreshTarget` ainda não tem escopo próprio), N = 1, 8, 32 jogadores/bots |
| Interação, servidor, hold | custo por tick com trace todo frame | pior caso = 1 line trace (+1 sweep com assist) por jogador segurando; deve escalar linearmente com o número de jogadores em hold | N jogadores segurando ao mesmo tempo; comparar com o caso ocioso |
| Efeito de `TraceIntervalSeconds` | 0 (todo tick), 0,05, 0,1, 0,2 | o custo médio cai aproximadamente na razão do intervalo; a latência de foco sobe até o intervalo | mesmo cenário, 4 valores, comparar custo e atraso do prompt |
| Interação, cliente | custo do tick (`SyncFocus`) | desprezível | Insights no cliente |
| Save síncrono | tempo do `SaveGame` (payload + `.sav`) | proporcional ao tamanho do payload; sem hitch perceptível com o inventário padrão (24 pilhas) | escopo em `SaveGame`, variar inventário e `BackupCount` (0, 1, 9) |
| Save assíncrono | custo **no game thread** de `AsyncSaveGame` | menor que o síncrono, mas não zero: payload e `RotateBackups` rodam no game thread (ver tabela acima); `RotateBackups` pode dominar | escopos `CatalogSave_BuildPayload` e `CatalogSave_RotateBackups` (existem); `CatalogComputePayloadCrc` ainda sem escopo; olhar o pior frame durante o autosave |
| Load | tempo de `LoadGame`/`ApplyLoadedPayload` | pico único aceitável em tela de carregamento | escopo `CatalogSave_ApplyLoadedPayload` (existe; a leitura do disco fica fora dele) |
| Banda, ocioso | bytes/s por jogador de propriedades do plugin sem ação | próximo de zero: nada muda, nada é enviado (só o que a engine envia por conexão) | Net Insights, `stat net`, jogador parado |
| Banda, ativo | bytes/s ao coletar, usar item, segurar interação, tomar dano | proporcional ao que mudou: delta de pilha no inventário (só ao dono), `Health` a todos, 3 propriedades `OwnerOnly` da interação | Net Insights por classe/propriedade, cenário roteirizado pelo `mp_run` |
| Banda, pickups | custo de pickups dormentes vs. ativos | pickup parado a 2 Hz de frequência e dormente custa pouco; confirmar que dormência realmente vale | N pickups, comparar com `NetDormancy = DORM_Never` |
| Memória do save | tamanho do `.sav` | cresce com `Records.Data` dos participantes e com as pilhas | medir o arquivo para payloads pequeno/médio/grande |

## Como medir

### Unreal Insights (CPU e rede)

1. Rode um build Development/Test do jogo (ou o Editor em `-game`) com o trace ligado:
   `UnrealEditor.exe SampleProject/SampleCatalog.uproject -game -trace=cpu,net` (adicione `-tracehost=127.0.0.1` com o Unreal
   Insights aberto, ou deixe gravar o `.utrace` em disco). **Conferir na documentação da 5.8** se o canal `net` ainda exige
   `-NetTrace=1` e qual verbosidade usar (`-trace=net` sozinho pode não bastar); isto não foi testado aqui.
2. Para o servidor dedicado, use o mesmo `-trace` no processo do servidor; para os clientes, outro `.utrace` por processo.
3. No Insights: *Timing Insights* para os escopos de CPU (ordene por tempo exclusivo) e *Networking Insights* para bytes por
   ator, propriedade e RPC. Procure `ACatalogPickupActor`, `UCatalogInventoryComponent` (`InventoryList`), `UCatalogHealthComponent` (`Health`) e os RPCs `Server*`.

### Comandos `stat`

- `stat unit` e `stat unitgraph`: frame, game, draw e GPU.
- `stat game`: custo de tick de ator/componente (procure `UCatalogInteractionComponent` entre os tick groups).
- `stat net`: taxas de entrada/saída e pacotes (cliente e servidor).
- `stat startfile` e `stat stopfile`: gravam um arquivo de stats para análise posterior; `stat namedevents` ajuda a ler escopos em ferramentas externas.
- Emulação de rede: `NetEmulation.PktLag=100`, `NetEmulation.PktLoss=2` (e `NetEmulation.Off`), para ver o efeito de latência/perda no foco, hold e cooldown.

### CSV Profiler

`csvprofile start` e `csvprofile stop` no console (ou `-csvprofile` na linha de comando) geram um `.csv` em `Saved/Profiling/CSV`.
Para ter números do plugin no CSV é preciso declarar categorias e escopos (`CSV_DEFINE_CATEGORY`, `CSV_SCOPED_TIMING_STAT`),
o que o código ainda não faz. Use para comparar rodadas (antes/depois de uma mudança) no mesmo cenário.

### Cenário reproduzível

O `tools/mp_run.py` já sobe servidor dedicado + clientes com roteiro (ver [MULTIPLAYER_TESTS.md](MULTIPLAYER_TESTS.md) e o
[ADR 0005](adr/0005-comandos-de-depuracao-e-mp-run.md)). Para perfil, reaproveite o roteiro dos casos 03/04/06 adicionando
`-trace=cpu,net` aos processos; os predicados do `mp_run` servem de "o cenário rodou de verdade". Isto é uma sugestão de uso:
a combinação não foi executada.

## Escopos de CPU no código

`TRACE_CPUPROFILER_EVENT_SCOPE` (include `ProfilingDebugging/CpuProfilerTrace.h`) na primeira linha de cada função abaixo.
Foram escritos sem compilar nem abrir no Insights (*não verificado*).

| Escopo | Função | O que entra (do código) |
|---|---|---|
| `CatalogInteraction_Tick` | `UCatalogInteractionComponent::TickComponent` | o tick inteiro: trace automático/hold (`RefreshTarget`), `TickHoldWithAuthority` e `SyncFocus`. `RefreshTarget` chamado pelos RPCs de interação fica fora |
| `CatalogSave_BuildPayload` | `UCatalogSaveSubsystem::BuildPayload` | `WriteSaveData` de todos os participantes; chamado por `SaveGame` e `AsyncSaveGame` |
| `CatalogSave_ApplyLoadedPayload` | `UCatalogSaveSubsystem::ApplyLoadedPayload` | `ReadSaveData` dos participantes e `OnPayloadLoaded`; chamado por `LoadGame` e pelo fim do `AsyncLoadGame` |
| `CatalogSave_RotateBackups` | `CatalogSaveIO::RotateBackups` | leitura e conferência do CRC do arquivo atual + move/copia dos backups; no save síncrono e no `AsyncSaveGame` (game thread) |
| `CatalogAbility_TryActivate` | `UCatalogAbilityComponent::TryActivateAbility` | no cliente: estimativa local e envio do RPC; com autoridade: `ActivateWithAuthority`. A ativação que chega por `ServerTryActivateAbility` vai direto a `ActivateWithAuthority` e fica **fora** do escopo |

Como ver (Unreal Insights, Timing Insights; procure por `Catalog`):

- Desde o início: rode com `-trace=cpu` (ou `-trace=cpu,net`, ver acima) e abra o `.utrace` gravado, ou use `-tracehost=127.0.0.1`
  com o Insights aberto.
- Só um trecho: no console, `Trace.Start` (com o canal, ex.: `Trace.Start cpu`) antes do cenário e `Trace.Stop` depois.
  **Conferir na 5.8** a sintaxe exata e onde o arquivo é gravado; nada disto foi executado aqui.

Escopos de `TRACE_CPUPROFILER_EVENT_SCOPE` só têm custo relevante com o canal `cpu` ligado.

Ainda sem escopo (sugestões, conferir a assinatura antes de aplicar): `UCatalogInteractionComponent::RefreshTarget` e `SyncFocus`,
`UCatalogInventoryComponent::AddInternal` e `FlushReplicatedBroadcast` (flush de `OnInventoryChanged` no cliente), o tratamento
de dano de `UCatalogHealthComponent`, `CatalogComputePayloadCrc` e a gravação/leitura do `.sav` em `SaveGame`/`LoadGame`.

## Registro de resultados

Quando medir, acrescente aqui (nunca estime): data, commit, máquina, build (Development/Test), número de jogadores, valores
de `TraceIntervalSeconds` e `BackupCount`, e o número observado por linha da tabela de orçamento.

| Data | Commit | Cenário | Resultado |
|---|---|---|---|
| (nenhum ainda) | | | |
