# 0003. Regra de delta do SaveGame e migração v0, v1, v2

**Status:** Aceito

## Contexto

O `USaveGame` da Unreal só grava propriedades diferentes do default (delta). Se o campo de versão tivesse default 1 e o save fosse
v1, o valor não iria ao disco e um arquivo antigo seria lido como "versão atual", sem migração. O scaffold anterior do plugin
já teve esse problema (default 1 nunca serializado).

## Decisão

- `FCatalogSaveHeader::SaveVersion` tem default **0** ("sem versão"). Toda versão real (>= 1) difere do default e vai ao disco; um
  arquivo sem o campo é lido como v0 e migrado. O `UCatalogSaveSubsystem` carimba `GCatalogSaveSchemaVersion` (hoje 2) em toda
  gravação; ninguém preenche à mão.
- Regra para **todo campo** dos structs do payload: (1) o default de um campo novo significa "ausente/desconhecido", porque é isso
  que um arquivo antigo lê; (2) nunca mudar o default de um campo existente (arquivos já gravados omitiram o valor antigo e
  passariam a ler o novo em silêncio). Vale também para `FCatalogSaveRecord::Version`, controlada pelo participante.
- `CatalogApplySaveMigration` migra em cadeia, um degrau por iteração: v0 para v1 (gera `SaveId` se faltar), v1 para v2 (Metadata e
  Records ficam no default, "desconhecido"; descarta pilhas de `Inventory` sem `ItemId` ou com `Quantity <= 0`) e, no fim, preenche
  um `Timestamp` ausente. Versão maior que a do plugin, ou negativa, recusa o load (`false`). O load
  migra uma cópia: o payload de saída só muda se tudo der certo.
- Mudar o layout do payload: incrementar `GCatalogSaveSchemaVersion`, adicionar o `case` em `CatalogApplySaveMigration` e incluir o
  campo novo no CRC (ADR 0006). O módulo avisa no startup se `ExpectedSaveSchemaVersion` (Project Settings) diverge da versão do código.
- Histórico: v0 sem `SaveVersion`; v1 Header, Inventory e PlayerLocation/PlayerYaw; v2 acrescenta Metadata (menu de saves) e Records
  (blocos por `ICatalogSaveParticipant`).

## Consequências

- (+) Saves antigos continuam abrindo; um save de plugin mais novo é recusado em vez de lido pela metade.
- (+) Cobertos por testes de automação: `CatalogAnchor.Save.MigrateV0ToV2`, `MigrateV1ToV2`, `CurrentVersionIsNoOp`,
  `RejectsFutureAndNegativeVersion`.
- (-) Disciplina obrigatória a cada campo novo; um default errado só aparece como dado "novo" lido em silêncio de arquivo antigo.
- (-) A versão de schema vive em dois lugares (constante no código e Project Settings); só há aviso, não correção automática.
