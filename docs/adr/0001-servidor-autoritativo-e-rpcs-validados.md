# 0001. Servidor autoritativo e Server RPCs com validação

**Status:** Aceito

## Contexto

O plugin é pensado para multiplayer. Um cliente nunca deve escolher alvo, item, quantidade ou habilidade por conta própria,
e um cliente adulterado (ou um Blueprint com erro) não pode corromper o estado do servidor. A Unreal permite `UFUNCTION(Server)`
sem validação, o que deixa a checagem de argumentos a cargo de cada autor e fácil de esquecer.

## Decisão

- O servidor decide; o cliente pede por Server RPC e lê estado replicado. Gameplay (trace, cooldown, dano, adição/remoção de
  item) roda só com autoridade.
- Todo `UFUNCTION(Server, ...)` é `WithValidation`. Hoje são seis: `ServerTryActivateAbility`, `ServerTryInteract`,
  `ServerBeginInteract`, `ServerEndInteract`, `ServerUseItem` e `ServerDropItem`. O `_Validate` rejeita só o que um cliente
  legítimo nunca envia (ponteiro nulo, quantidade fora de `[1, MaxUseQuantity]`, tag inválida) e, na Unreal, reprovar derruba a
  conexão. Casos de "não tem o item" ou "não é usável" não são validação: ficam na `_Implementation` (ex.: `UseItem` revalida
  tudo, porque o pedido do cliente não é confiável).
- RPCs sem parâmetros (`ServerTryInteract`, `ServerBeginInteract`, `ServerEndInteract`) têm `_Validate` que retorna `true`: o
  servidor refaz trace, alcance e cooldown por conta própria.
- No cliente, o erro de uso comum é recusado antes de sair (ex.: `TryActivateAbility` com tag inválida dispara
  `OnAbilityFailed` em vez de mandar o RPC cujo `_Validate` desconectaria o jogador).
- Cosméticos (montage, foco, `OnInteractionFailed` remoto) vão por multicast/Client RPC não confiável e não alteram estado.
- `tools/validate_plugin.py` falha se algum `UFUNCTION(Server)` não tiver `WithValidation` e a função `_Validate`
  (`--no-strict-rpc` rebaixa a aviso).

## Consequências

- (+) Superfície de ataque pequena e uniforme; a regra é checada automaticamente no CI.
- (+) O mesmo código de servidor atende dedicado, listen e standalone.
- (-) Sem predição no cliente: o Dash é aplicado no servidor e corrigido no cliente; predição seria trabalho do GAS (LIMITATIONS.md).
- (-) `_Validate` agressivo derruba a conexão de quem só errou um Blueprint, então a API pública filtra argumentos óbvios antes do RPC.
- Não coberto: nenhum teste exercita o `_Validate` com argumentos inválidos pela rede (LIMITATIONS.md, riscos conhecidos); o
  `mp_run` exercita os RPCs de interação e habilidade ponta a ponta com argumentos válidos.
