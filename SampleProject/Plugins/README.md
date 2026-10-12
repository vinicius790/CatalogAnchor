# Plugins locais do sample

**Não coloque o `CatalogAnchor` aqui.**

`SampleCatalog.uproject` já encontra o plugin canônico da raiz do repositório via:

```json
"AdditionalPluginDirectories": ["../Plugins"]
```

Uma cópia ou symlink em `SampleProject/Plugins/CatalogAnchor` duplicaria o plugin
e o Editor recusaria abrir o projeto (`tools/validate_plugin.py` também acusa).

Esta pasta fica disponível para outros plugins que só o sample use.
