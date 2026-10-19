# 0002. Inventário em FFastArraySerializer replicado só ao dono

**Status:** Aceito

## Contexto

O inventário é um array de pilhas que muda com frequência (coleta, uso, transferência). Replicar um `TArray` comum reenvia o
array inteiro a cada mudança, e replicar a mochila de um jogador para todos os outros gasta banda e expõe dado que só o dono
precisa ver.

## Decisão

- `FCatalogInventoryList : FFastArraySerializer` com `FCatalogInventoryEntry : FFastArraySerializerItem`: replicação por delta,
  só das pilhas alteradas (`MarkItemDirty` / `MarkArrayDirty` no servidor).
- `DOREPLIFETIME_CONDITION(UCatalogInventoryComponent, InventoryList, COND_OwnerOnly)`: só o cliente dono recebe. Consequência
  direta: o dono do ator precisa ser uma conexão de jogador (PlayerController, Pawn possuído ou PlayerState); num baú, nenhum
  cliente recebe a lista.
- No cliente, os callbacks do FastArray (add/change/remove) são agrupados num único `OnInventoryChanged` no tick seguinte, já com
  o array final (em `PreReplicatedRemove` o item ainda está no array).
- Regras de pilha, peso e capacidade são funções puras (`FCatalogInventoryRules`), testáveis sem UObject. `Capacity` e
  `MaxWeight` não replicam (LIMITATIONS.md).
- Mesmo princípio em outros estados do dono: `Cooldowns` (habilidades) e `CurrentTarget`/`HoldStartServerTime`/`HoldDuration`
  (interação) usam `COND_OwnerOnly`; a vida replica a todos (`COND_None`) porque barras sobre inimigos precisam dela.

## Consequências

- (+) Menos banda por mudança e nenhum vazamento de mochila alheia.
- (+) `catalog.Debug.Dump` no `mp_run` compara o que cada máquina enxerga (valor vazio num pawn alheio prova o `COND_OwnerOnly`).
- (-) Quem integrar precisa garantir um owner de conexão de jogador; um inventário em ator sem dono não chega a ninguém.
- (-) Mostrar a mochila de outro jogador (inspecionar, trocar) exige canal próprio (RPC ou outra condição); não é suportado.
- Não medido: o ganho de banda é a expectativa do mecanismo, não um número (ver [../PERFORMANCE.md](../PERFORMANCE.md)).
