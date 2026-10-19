# Decisões de arquitetura (ADR)

Registros curtos (Contexto, Decisão, Consequências, Status) das decisões que moldam o plugin. Escritos em pt-BR, a partir
do código e da documentação do repositório; onde algo não foi verificado, o ADR diz. Visão geral dos sistemas: [../ARQUITETURA.md](../ARQUITETURA.md).

| ADR | Decisão | Status |
|---|---|---|
| [0001](0001-servidor-autoritativo-e-rpcs-validados.md) | Servidor autoritativo; todo Server RPC com `WithValidation` | Aceito |
| [0002](0002-inventario-fastarray-owner-only.md) | Inventário em `FFastArraySerializer` replicado só ao dono (`COND_OwnerOnly`) | Aceito |
| [0003](0003-regra-de-delta-e-migracao-do-save.md) | Regra de delta do SaveGame: `SaveVersion` default 0 e migração v0, v1, v2 | Aceito |
| [0004](0004-geracoes-de-payload-e-chave-unica-de-participantes.md) | Gerações de payload e substituição por chave no registro de participantes | Aceito |
| [0005](0005-comandos-de-depuracao-e-mp-run.md) | Comandos de depuração só fora de Shipping e o driver `mp_run` (log como evidência) | Aceito |
| [0006](0006-crc-e-backups-do-save.md) | CRC32 do payload e rotação de backups do save | Aceito |

Novo ADR: copie o formato de um existente, use o próximo número e adicione a linha acima.
