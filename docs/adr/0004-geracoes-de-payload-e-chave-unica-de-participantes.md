# 0004. Gerações de payload e substituição por chave no registro de participantes

**Status:** Aceito

## Contexto

Quem salva é um conjunto de objetos vivos (`ICatalogSaveParticipant`: inventário, o componente de save do sample, objetos do
jogo) que entram e saem do mundo: troca de mapa, respawn, possessão. Dois problemas reais surgiram:

1. Um participante que se registra **depois** de um load (mapa novo, respawn) nunca receberia o estado carregado, e o autosave
   gravaria um inventário vazio por cima do save que acabou de ser carregado.
2. No respawn o pawn antigo ainda existe por um instante; dois objetos com a mesma `GetSaveRecordKey` gravariam o mesmo campo e a
   ordem de registro decidiria quem vence.

## Decisão

- **Geração:** todo load bem-sucedido abre uma geração (`ApplyLoadedPayload`): o subsistema marca quem está registrado como "em
  sincronia" (`ParticipantsInSync`) e entrega o payload a esses participantes via `ReadSaveData`. Quem se registra depois, com um
  payload ativo e fora do conjunto, recebe o estado ao registrar, **uma vez** por geração. O mesmo objeto que desregistra e
  registra de novo dentro da mesma geração **não** relê o save (senão o save velho cairia por cima do estado atual). Save não abre
  geração: o payload saiu dos próprios participantes.
- **Chave única:** `RegisterParticipant` remove qualquer outro objeto com a mesma `GetSaveRecordKey`; o novo substitui o antigo
  (cada campo do payload tem um só escritor). `UnregisterParticipant` tira só aquele objeto, então um substituto com a mesma
  chave continua registrado.
- Referências fracas (`TWeakObjectPtr`): objeto destruído sai da lista e do conjunto de sincronia, que não acumula.
- O save parte do **último estado salvo/carregado**; um participante ausente naquele mapa não tem seus dados apagados.
- O inventário só registra para jogador **local** (decidido na possessão); baú e NPC nunca registram.

## Consequências

- (+) Troca de mapa e respawn não perdem nem sobrescrevem o save carregado.
- (+) Cobertos por testes de automação: `CatalogAnchor.Save.LoadStartsNewGeneration`, `ReRegisterSameGenerationDoesNotReread`,
  `RespawnReplacesByKey` e `CatalogAnchor.Inventory.NonPlayerOwnerNotSaved`.
- (-) Duas instâncias legítimas com a mesma chave não coexistem: quem precisa de mais de um registro tem de usar chaves distintas.
- (-) Num listen server o payload tem um único `Inventory` (o do jogador local do host); jogadores remotos não entram no save
  (LIMITATIONS.md). Persistência por jogador fica para quem integrar.
