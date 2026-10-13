# Política de segurança

## Versões com correção

Só a versão minor mais recente recebe correção de segurança (hoje, `0.5.x`).

## Como reportar

**Não abra issue pública.** Use o relato privado do GitHub: aba **Security** do repositório,
**Report a vulnerability**.

Inclua:

- versão do plugin e da Unreal Engine;
- o que é afetado (por exemplo, leitura de save malformado, caminho de arquivo, script em `tools/`);
- passos mínimos para reproduzir e, se tiver, o impacto esperado.

## O que esperar

- Confirmação do recebimento em até 7 dias.
- Avaliação e plano de correção depois da confirmação; aviso quando houver release com a correção.
- Crédito no `CHANGELOG.md`, se você quiser.

Este é um projeto mantido por uma pessoa, em horário livre: os prazos são metas, não garantia.

## Escopo

Dentro: código C++ em `Plugins/CatalogAnchor`, o `SampleProject` e os scripts em `tools/`
(por exemplo, `get_msvc.py` baixa binários da Microsoft; `ue.py` executa o editor).

Fora: vulnerabilidades da própria Unreal Engine, do Windows SDK/MSVC e de dependências de terceiros
(reporte aos respectivos mantenedores).
