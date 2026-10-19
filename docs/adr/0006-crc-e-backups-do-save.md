# 0006. CRC32 do payload e backups do save

**Status:** Aceito

## Contexto

Um `.sav` pode ficar truncado (queda de energia, processo morto), corrompido ou travado por antivírus/sincronização. Sem
conferência, o load aceitaria dados quebrados; com um fallback ingênuo, um arquivo apenas travado voltaria um save antigo e o
próximo save apagaria o progresso recente.

## Decisão

- **CRC32 do payload.** `UCatalogSaveGame::PayloadCrc` guarda o CRC32 calculado por `CatalogComputePayloadCrc` (nunca 0: um
  cálculo 0 vira 1) e todo load o confere. O campo vem **antes** do `Payload` no struct, de propósito: um arquivo truncado perde
  o fim do payload, não o CRC, e a conferência pega. `PayloadCrc == 0` significa arquivo anterior ao CRC (0.4): carrega sem
  conferência. O CRC cobre cada campo do payload; campo novo exige atualizar a função (o teste
  `CatalogAnchor.Save.PayloadCrcCoversEveryField` falha até lá). O CRC é detecção de corrupção, não assinatura: quem edita o
  arquivo pode recalcular o valor.
- **Rotação de backups.** Antes de gravar, o arquivo atual vira `<slot>_bak` (depois `_bak2`... até `BackupCount`, padrão 1,
  limitado a 0..9), **só se ele próprio estiver íntegro**: um corrompido não vira backup nem empurra um bom para fora. Os
  backups antigos são movidos/renomeados e o atual é copiado (permanece no lugar até o save novo).
- **Load com fallback controlado.** Arquivo corrompido (CRC, versão negativa, ilegível como `UCatalogSaveGame`) tenta o backup
  mais novo legível, e o resultado fica como `Recovered` na lista de slots. **Não** cai no backup: (a) schema futuro (o arquivo
  está bom, é o plugin que é velho); (b) falha ao LER os bytes do principal (arquivo travado, não corrupção); (c) slot principal
  ausente (backup solto não ressuscita um slot apagado).
- **Nomes reservados.** Nomes de slot terminados em `_bak` ou `_bak<dígitos>` são recusados (`IsValidSlotName`), para que o
  jogador não colida com um arquivo de backup.
- **`DeleteSlot`** apaga o principal primeiro e depois todos os backups possíveis (até 9), não só os configurados agora.

## Consequências

- (+) Truncamento e lixo são detectados; progresso recente não é perdido por falha transitória de leitura.
- (+) Coberto por testes de automação em `CatalogAnchor.Save.Integrity.*` (round trip com CRC, queda no backup, corrompido nunca
  vira backup, schema futuro não cai no backup, principal travado, arquivo sem CRC carrega, remoção de backups).
- (-) Só as últimas `BackupCount` versões íntegras são guardadas; um slot sem CRC (anterior à 0.5) carrega sem conferência.
- (-) Não protege contra adulteração deliberada; save com assinatura/criptografia está fora do escopo.
- (-) O backup costuma ser mais velho que o principal: `Recovered` significa perda de progresso recente, e a UI deve avisar.
