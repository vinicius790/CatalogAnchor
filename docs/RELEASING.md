# Checklist de release

Como publicar uma versão do CatalogAnchor. O que o `release.yml` faz é fato do workflow; o que é regra da Fab está marcado como
**a confirmar**, porque as diretrizes mudam e não foram consultadas ao escrever este documento.

## 0. O que um release é hoje

`release.yml` roda em `ubuntu-latest` quando se empurra uma tag `v*`: valida (`validate_plugin.py`), roda o pytest, confere
a tag contra o `VersionName` do `.uplugin`, extrai as notas do `CHANGELOG.md`, empacota `Plugins/CatalogAnchor` com
`git archive` (**só fonte, sem binários**) e cria o GitHub Release (`--prerelease` se a tag tiver `-`). **Ele não compila C++**
(não há Unreal Engine nos runners do GitHub): a prova de compilação precisa vir antes, do runner self-hosted ou de uma máquina com a engine.

## 1. Versão e changelog

- [ ] Decidir o número (SemVer). Em `Plugins/CatalogAnchor/CatalogAnchor.uplugin`: `VersionName` = `X.Y.Z` **igual à tag sem o `v`**
      (a tag `v1.2.3-rc1` compara com `1.2.3`; o sufixo marca pre-release) e `Version` (inteiro) incrementado.
- [ ] `CHANGELOG.md`: a seção da versão precisa existir com o cabeçalho exato `## [X.Y.Z]` (ex.: `## [0.6.0] — 2026-MM-DD`); sem
      "em preparação". O workflow extrai da linha `## [<versão>]` até o próximo `## [` e falha se vier vazio. Para pre-release a
      seção usa a tag inteira sem o `v` (ex.: `## [0.6.0-rc1]`), porque a extração usa o nome completo da tag.
- [ ] Mover o conteúdo de `[Unreleased]` e deixar `[Unreleased]` vazio.
- [ ] Decidir se `IsBetaVersion` continua `true` no `.uplugin`.
- [ ] Atualizar versões citadas nos docs (`ARQUITETURA.md` e `TECNOLOGIAS.md` trazem o número no título) e os selos do README.

## 2. Gates de host (qualquer máquina)

- [ ] `py -3 tools/validate_plugin.py --strict-docs` retorna OK (manifestos, lint de C++, todo Server RPC com `WithValidation`,
      contagens de testes nos docs).
- [ ] `py -3 tools/gen_api_docs.py --check` retorna OK; se não, regenere `docs/API.md` com o gerador e faça commit.
- [ ] `py -3 -m pytest -q tests/` verde e `ruff check .` limpo (o CI roda os dois).
- [ ] Contagens citadas em `README.md`, `README.pt-BR.md` e `docs/` (testes de automação, casos de pytest) batem com o código. O
      `--strict-docs` confere `README.md`, `README.pt-BR.md` e `docs/*.md` (não `docs/adr/`, `CHANGELOG.md` nem `LIMITATIONS.md`),
      nas formas "N testes C++", "N testes de automação", "N casos de pytest", "pytest (N casos", "N C++ tests",
      "N automation tests" e "pytest (N cases"; contagem escrita de outro jeito ou fora desses arquivos: revise à mão.

## 3. Prova com a engine (runner self-hosted ou máquina com UE 5.8+)

Esta é a etapa que o `release.yml` não faz. Não declare uma plataforma como verificada sem rodar nela.

- [ ] Em cada SO que o release vai declarar: `py -3 tools/ue.py build-plugin` (RunUAT `BuildPlugin`, saída em `dist/CatalogAnchor`) e
      `py -3 tools/ue.py test` (compila o editor do sample e roda os testes `CatalogAnchor.*`). O mesmo é o que o `ue.yml` roda
      quando as variáveis de repositório `UE_RUNNER_WINDOWS_ENABLED`, `UE_RUNNER_MACOS_ENABLED` e `UE_RUNNER_LINUX_ENABLED` estão
      `true` (ou por disparo manual) e há runner com os labels `[self-hosted, <os>, ue-5.8]` (detalhes em [PLATFORMS.md](PLATFORMS.md)).
- [ ] `py -3 tools/mp_run.py --project SampleProject/SampleCatalog.uproject --case all` (e `--mode listen`) sem predicado falho.
- [ ] Build **Shipping** do sample/projeto de teste e empacotamento, no Windows (precedente verificado até `876aec1`) e nos demais SOs
      declarados.
- [ ] Pickups em rede (R1 de [NETWORKING.md](NETWORKING.md)): o predicado de pickups do caso 03 roda no `--case all` dedicado
      acima e ainda não foi executado; drop e coleta parcial vistos pelos clientes seguem sem cobertura. Decidir se bloqueia o release.
- [ ] Atualizar o estado real: tabela de [PLATFORMS.md](PLATFORMS.md), [LIMITATIONS.md](../LIMITATIONS.md) e os dois READMEs
      (commit verificado, número de testes executados). **Mac e Linux nunca foram compilados**: recomendação deste checklist é só
      publicar suporte a eles depois de pelo menos um build e os testes passarem em cada um (decisão de quem publica).
- [ ] Pacote por plataforma: o `release.yml` gera só o zip de fonte. Se o release for distribuir plugin compilado, gere um zip por
      SO a partir da saída de `build-plugin` no runner correspondente e anexe manualmente ao release (não automatizado hoje).

## 4. Requisitos para a Fab (**a confirmar nas diretrizes vigentes**)

Nada abaixo foi submetido ou validado contra a Fab. Conferir a documentação atual de publicação de plugins de código antes de começar.

- [ ] Conta de vendedor e página do produto (descrição, capturas, vídeo se aplicável, categoria, preço/licença).
- [ ] Compatibilidade por versão de engine: o `.uplugin` hoje não tem `EngineVersion` de propósito; a compatibilidade com 5.9 e 6.0
      é intenção, não fato. Confirmar se a Fab exige build/teste por versão de engine submetida.
- [ ] Ícone: `Plugins/CatalogAnchor/Resources/Icon128.png` existe (PNG 128x128 na última conferência) e está versionado desde
      o commit `3632107`; confirmar que é arte própria.
- [ ] `.uplugin`: `CreatedBy` preenchido; `CreatedByURL`, `DocsURL`, `MarketplaceURL` e `SupportURL` estão **vazios** hoje. Preencher
      com URLs reais (autor, documentação, página do produto, suporte).
- [ ] Conteúdo do pacote: só fonte, sem `Binaries/`, `Intermediate/`, `Saved/` (o `git archive` já exclui o que não está versionado).
      Conferir se a Fab pede ou proíbe binários em plugins de código.
- [ ] Licença: o repositório é MIT. Verificar a compatibilidade com o modelo de licença da Fab e deixar claro, na página, o que o
      comprador recebe. Sem código de terceiros hoje; se entrar, incluir avisos.
- [ ] Declaração de uso de IA: parte do código e da documentação foi produzida com assistência de IA. Conferir se a Fab exige
      divulgação e, se sim, declarar.
- [ ] Marcas: manter o aviso "Unreal Engine is a trademark of Epic Games, Inc.; this project is not affiliated with or endorsed by Epic Games".
      O selo do README usa `logo=unrealengine` (shields.io): conferir a política de marca da Epic e remover o logo se necessário.
- [ ] Documentação em inglês: hoje só os READMEs têm versão em inglês; os demais docs estão em pt-BR. Avaliar traduzir
      QUICKSTART e INSTALACAO antes de publicar para público internacional.

## 5. URLs e metadados do repositório

- [ ] Substituir o placeholder do `git clone` pela URL real: `<url-deste-repositório>` no `README.pt-BR.md` e `<this-repository-url>` no `README.md`.
- [ ] Revisar `SECURITY.md` (canal de contato), `CODEOWNERS`, templates de issue/PR e `CONTRIBUTING.md` quanto a nomes e URLs.
- [ ] Descrição, tópicos e site do repositório no GitHub; nome sugerido: `CatalogAnchor`.

## 6. Publicar

1. Garantir a árvore limpa e a branch principal com os commits de release (versão, changelog, docs).
2. Criar a tag anotada (`git tag -a vX.Y.Z -m "vX.Y.Z"`) e empurrá-la (`git push origin vX.Y.Z`). Pre-release: `vX.Y.Z-rc1`.
3. Acompanhar o workflow *Release*: se o passo "Conferir tag x VersionName" ou "Extrair notas do CHANGELOG" falhar, corrija os
   arquivos, apague a tag (local e remota) e recrie-a no commit certo.
4. Abrir o release: as notas vieram do `CHANGELOG.md`; baixar o zip e conferir que contém só `CatalogAnchor/` com fonte
   (`.uplugin`, `Source/`, `Config/`, `Content/`, `Resources/`), sem binários.
5. Depois: anunciar o que foi **e o que não foi** verificado (use o texto de verificação do README), abrir issues para os
   pendentes (Mac/Linux, Iris/Push Model, medição de performance) e começar a próxima seção `[Unreleased]`.
