# Limitações (honestidade do âncora)

## O que não está verificado

- **O C++ nunca foi compilado por este repositório.** Não há Unreal Engine no CI nem na máquina
  onde ele foi escrito. As APIs mais arriscadas foram conferidas na documentação oficial da 5.8,
  mas compilação, testes de automação e PIE só valem depois de rodar `tools/ue.py build-plugin`
  e `tools/ue.py test` numa máquina com UE 5.8.
- Compatibilidade com 5.9 e 6.0 é intenção (`.uplugin` sem `EngineVersion`), não fato: essas
  versões ainda não saíram.
- Comportamento de rede (PIE com 2+ jogadores) não foi testado.

## Não entregue / não inventado

- Nenhum cooked game, `.pak`, shipping build ou trailer.
- Nenhum número de FPS nem benchmark.
- Sem `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` versionados.
- Sem malha de personagem, animações, materiais ou mapas (`.uasset`/`.umap`).

## Limites de design conhecidos

| Limite | Por quê / contorno |
|---|---|
| Save em **listen server** guarda só o inventário do host | inventários de jogadores remotos não entram no save, porque o payload tem um único `Inventory`. Persistência por jogador fica para quem integrar |
| Save **só no host** | o subsystem vive no GameInstance de quem tem autoridade. Servidor dedicado precisa de persistência própria |
| Cosmético de habilidade por multicast **unreliable** | um cliente pode perder uma montage ocasionalmente |
| Sem predição de cliente nas habilidades | o dash é aplicado no servidor e corrigido no cliente. Predição é trabalho do GAS |
| `Capacity` do inventário não replica | mudar em runtime não atualiza a UI do cliente |
| Highlight de foco só liga Custom Depth | o contorno visual exige um material de pós-processo (asset) |
| `ListSlots` ausente | a API de enumeração da plataforma mudou entre versões; não foi incluída sem verificação |

## Versão completa (fora do escopo)

GAS real (attributes, effects, prediction) · CMC estendido (sprint/slide/mantle) ·
Automation Spec de rede · UI em UMG.
