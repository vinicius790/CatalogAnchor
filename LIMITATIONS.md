# Limitações (honestidade do âncora)

## Estado real da verificação

- **O C++ nunca foi compilado.** A máquina de desenvolvimento tem a UE 5.8.3, mas o UBT para em
  "Platform Win64 is not a valid platform to build. SDK validation failed: Sdk: not found": falta o
  MSVC 14.44 (VS 2022 17.14) ou 14.50 (VS 2026) e o Windows SDK ≥ 10.0.19041. O CI público não tem engine,
  e o workflow `ue.yml` (runner self-hosted) ainda não rodou.
- **Testes de automação (33) não executados.** Só o tooling Python roda de fato (validador, lint, pytest).
- **Testes de rede MP-01..MP-07** ([docs/MULTIPLAYER_TESTS.md](docs/MULTIPLAYER_TESTS.md)) não executados.
- As assinaturas de API mais arriscadas foram conferidas lendo os headers da 5.8.3
  ([docs/TECNOLOGIAS.md](docs/TECNOLOGIAS.md)); isso não substitui compilar.
- **Mac e Linux** não verificados (nem o tooling de build `tools/ue.py` nessas plataformas com engine real).
- Compatibilidade com 5.9 e 6.0 é intenção (`.uplugin` sem `EngineVersion`), não fato.

## Riscos conhecidos (a confirmar ao compilar/rodar)

- **OnRep inicial com `MaxHealth` ≠ 100:** o cliente parte do default 100 e o primeiro `OnRep_Health`
  dispara um `OnHealthChanged` espúrio (sem dano real).
- **Testes de mundo que usam GameInstance** (save) não devem rodar com uma sessão PIE aberta: podem pegar
  o GameInstance/subsystem errado. Rode-os com o PIE parado ou via `tools/ue.py test`.
- **Foco/highlight real** depende de `PlayerController` local com câmera: só é verificável no PIE; os testes
  de mundo cobrem o trace (`ComputeTraceSegment`, `FindOwningController`), não o highlight na tela.
- **Menu de saves do sample** lista slots extras do disco só onde a plataforma implementa
  `ISaveGameSystem::GetSaveGameNames`; nas outras aparecem só `Quick` e `Slot1..3`.
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
| `Capacity` do inventário não replica | mudar em runtime não atualiza a UI do cliente |
| Highlight de foco só liga Custom Depth | o contorno visual exige um material de pós-processo (asset) |
| Respawn no mesmo frame da morte | o cliente pode não ver a morte (OnRep coalesce); atrase o `ResetHealth`, como o sample (2 s) |
| Sem `ListSlots` no plugin | enumeração depende da plataforma; o sample usa `GetSaveGameNames` direto |

## Versão completa (fora do escopo)

GAS real (attributes, effects, prediction) · CMC estendido (sprint/slide/mantle) · Automation Spec de rede.
