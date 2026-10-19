# 0005. Comandos de depuração só fora de Shipping e o driver mp_run

**Status:** Aceito

## Contexto

O comportamento de rede (foco só no dono, hold por RPC, cooldown `OwnerOnly`, dano replicado) só aparece com servidor e clientes
separados. Os testes de automação usam um `UWorld` próprio, sem conexão de rede. Havia necessidade de um teste de rede
reproduzível sem input humano, mas sem deixar comandos que dão item, causam dano ou injetam teclas dentro de um build de jogo.

## Decisão

- **Comandos de depuração só fora de Shipping.** `CatalogDebugCommands.cpp` (`catalog.Debug.Dump|Hurt|GiveItem|Respawn|Aim|Hold|Ability|Interact|Key|Schedule`),
  `CatalogConsoleCommands.cpp` (`catalog.Save.List|Now|Load|Delete`) e o `SampleDebugCommands.cpp` do sample ficam dentro de
  `#if !UE_BUILD_SHIPPING`; as dependências extras (`InputCore`, `ApplicationCore`) também só entram fora de Shipping
  (`CatalogAnchor.Build.cs`). Os comandos que alteram estado exigem rodar no servidor.
- **`tools/mp_run.py` é o driver.** Sobe servidor dedicado + 2 clientes (ou host listen + 1 cliente com `--mode listen`, ou um
  standalone para save/load, teclas e HUD), dá a cada processo um roteiro de `catalog.Debug.Schedule <s> <comando>` em
  `-ExecCmds`, espera, mata só os processos que ele criou, lê os logs (`[Dump] ... Snap=<rótulo>` e `[Debug] ...`) e imprime
  PASS/FAIL por predicado com **a linha de log como evidência**. Sai com código diferente de 0 se algum predicado falhar.
- **O log é a fonte de verdade.** `catalog.Debug.Dump` imprime o que *cada* máquina enxerga; o predicado compara servidor e
  clientes (ex.: foco só no dono, valor vazio em pawn alheio).
- Casos de rede usam `-nullrhi`; os de HUD (UMG) usam `-RenderOffscreen`. O input de teclas passa por
  `APlayerController::InputKey` com evento simulado, ou seja, exercita o Enhanced Input, mas não mouse e câmera reais.

## Consequências

- (+) Teste de rede reproduzível e auditável (cada PASS aponta a linha de log) sem input humano; os resultados constam em
  [../MULTIPLAYER_TESTS.md](../MULTIPLAYER_TESTS.md).
- (+) Nenhum comando de trapaça em Shipping.
- (-) Os comandos existem em **qualquer** build não-Shipping do projeto que usa o plugin (incluindo Development): não devem vazar
  para um build entregue a jogadores fora de Shipping.
- (-) O driver depende do formato das linhas de log e de atrasos fixos no roteiro; mudar o texto do log exige atualizar os
  predicados, e uma máquina muito lenta pode exigir ajustar os tempos (não medido).
- (-) Não é verificação visual: só o estado dos widgets é lido; aparência do HUD, mouse/câmera reais e gamepad ficam sem cobertura.
- (-) O que roda em Shipping (os comandos não existem) não é exercitado por este driver.
