# Limitações (honestidade do âncora)

## Estado real da verificação

- **Compilado e testado (Windows, UE 5.8.3, MSVC 14.44.35229, Windows SDK 22621):** `SampleCatalogEditor`
  (plugin Runtime + Editor + sample) compila sem erros e os **73 testes de automação** passam no
  `UnrealEditor-Cmd`. A toolchain veio de `tools/get_msvc.py` (pacotes oficiais, sem Visual Studio) via
  `UE_SDKS_ROOT`; o repo foi compilado a partir de um caminho ASCII. pytest: 144 casos (Python 3.13 e 3.14).
- Os 14 "Condition failed" no startup do editor vêm da engine 5.8.3 (provado num projeto vazio), não do plugin.
- O CI público não tem engine; o workflow `ue.yml` (runner self-hosted) ainda não rodou num runner real.
- **Multiplayer:** MP-03, 04, 06 e 07 rodam headless com `py -3 tools/mp_run.py --project <uproject> --case all` (dedicado + 2 clientes;
  27/27 predicados, com a linha de log como evidência; detalhes em [docs/MULTIPLAYER_TESTS.md](docs/MULTIPLAYER_TESTS.md)). **Não verificado:** HUD e
  barra de hold, F5/F9 por tecla, listen server nos casos 03/04/06 e abrir o baú trancado com a chave. Os testes de automação
  usam um `UWorld` próprio, sem conexão de rede.
- **Foco/highlight real** só é verificável no PIE (precisa de `PlayerController` local com câmera): a automação
  cobre o trace (`ComputeTraceSegment`, `FindOwningController`), não o highlight na tela.
- As assinaturas de API mais arriscadas foram conferidas lendo os headers da 5.8.3
  ([docs/TECNOLOGIAS.md](docs/TECNOLOGIAS.md)).
- **Mac e Linux** não verificados (nem o tooling de build `tools/ue.py` nessas plataformas com engine real).
- Compatibilidade com 5.9 e 6.0 é intenção (`.uplugin` sem `EngineVersion`), não fato.

## Riscos conhecidos

- **`_Validate` e RPCs sem teste de rede:** todo Server RPC tem `WithValidation`, mas a validação e o caminho
  cliente→servidor não são exercitados por nenhum teste automatizado (só lógica e mundo local).
- **Assist de esfera de interação** (`bUseSphereAssist`) para no primeiro oclusor: um interagível atrás de outro
  objeto no mesmo raio não é candidato.
- **Testes de mundo que usam GameInstance** (save) não devem rodar com uma sessão PIE aberta: podem pegar
  o GameInstance/subsystem errado. Rode-os com o PIE parado ou via `tools/ue.py test`.
- **Lista de saves** depende de `ISaveGameSystem::GetSaveGameNames`; onde a plataforma não implementa, `ListSaveSlots`
  e o menu do sample mostram só o que o subsystem gravou/leu na sessão (mais o slot de autosave).
- **Backup de save** guarda só as últimas `BackupCount` versões (padrão 1) e só de arquivo que estava íntegro; um
  slot sem CRC (gravado antes da 0.5) carrega sem conferência de integridade.
- **Cooldown persiste após o respawn** no mesmo pawn (intencional: o componente não é recriado).

## Não entregue / não inventado

- Nenhum cooked game, `.pak`, shipping build ou trailer.
- Nenhum número de FPS nem benchmark.
- Sem `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` versionados.
- Sem malha de personagem, animações, materiais ou mapas (`.uasset`/`.umap`).

## Limites de design conhecidos

| Limite | Por quê / contorno |
|---|---|
| Save em **listen server** guarda só o jogador do host | o payload tem um único `Inventory` e um registro `SamplePlayer`; jogadores remotos não entram. Persistência por jogador fica para quem integrar |
| Save **só no host** | o subsystem vive no GameInstance de quem tem autoridade. Servidor dedicado precisa de persistência própria |
| Cosmético de habilidade por multicast **unreliable** | um cliente pode perder uma montage ocasionalmente |
| Sem predição de cliente nas habilidades | o dash é aplicado no servidor e corrigido no cliente. Predição é trabalho do GAS |
| `Capacity` e `MaxWeight` do inventário não replicam | mudar em runtime não atualiza a UI do cliente |
| `TransferTo` sem RPC | só roda com autoridade nos dois inventários; um cliente que queira transferir precisa de um RPC seu |
| Highlight de foco só liga Custom Depth | o contorno visual exige um material de pós-processo (asset) |
| Respawn no mesmo frame da morte | o cliente pode não ver a morte (OnRep coalesce); atrase o `ResetHealth`, como o sample (2 s) |

## Versão completa (fora do escopo)

GAS real (attributes, effects, prediction) · CMC estendido (sprint/slide/mantle) · Automation Spec de rede.
