# Plataformas: Windows, macOS e Linux (Unreal Engine 5.8)

O plugin declara `SupportedTargetPlatforms` Win64, Mac e Linux (o validador exige). As ferramentas Python
(`tools/ue.py`, `tools/dev.py`, `tools/mp_run.py`) rodam nos três SOs com Python 3.10+. Windows é a plataforma
verificada em máquina real; Mac e Linux têm o tooling testado por pytest (caminhos, comandos, matar processos) e o
código C++ protegido pelo validador, mas **não foram executados com a engine** (ver tabela do CI abaixo).

## Binários e scripts usados (UE 5.8)

| Item | Windows | macOS | Linux |
|---|---|---|---|
| Editor (`CommandletUtils.GetEditorCommandletExe`) | `Engine/Binaries/Win64/UnrealEditor-Cmd.exe` | `Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor` | `Engine/Binaries/Linux/UnrealEditor` |
| UAT | `Engine/Build/BatchFiles/RunUAT.bat` | `RunUAT.sh` | `RunUAT.sh` |
| UBT | `Build/BatchFiles/Build.bat` | `Build/BatchFiles/Mac/Build.sh` | `Build/BatchFiles/Linux/Build.sh` |
| Raiz da engine | registro, `LauncherInstalled.dat`, `Program Files\Epic Games\UE_5.8` | `UE_ROOT`/`--engine`; `/Users/Shared/Epic Games/UE_5.8` | `UE_ROOT`/`--engine`; `~/UnrealEngine` |
| Processos | `Popen.kill`; `--kill-all` = `taskkill /IM UnrealEditor-Cmd.exe` | grupo de processos (`killpg`); `--kill-all` = `pkill -KILL -x UnrealEditor` | idem Mac |

`ue.py` sempre pesquisa a engine nesta ordem: `--engine`, `UE_ROOT`, registro (só Windows), `LauncherInstalled.dat`
(Win `%ProgramData%\Epic`, Mac `/Users/Shared/Epic`, Linux `~/.config/Epic`) e pastas padrão. `--version` escolhe
a versão procurada (padrão 5.8).

## Windows

- Visual Studio 2022 17.14+ (MSVC **14.44**, `14.44.35211` ou superior) ou Visual Studio 2026 (MSVC **14.50**,
  `14.50.35723` ou superior; 14.50.0 a 14.50.35722 têm erros internos de compilador) com a carga "Desenvolvimento para
  desktop com C++". Windows SDK **10.0.22621** (`Engine/Config/Windows/Windows_SDK.json`).
- Sem Visual Studio: `py -3 tools/get_msvc.py` baixa MSVC 14.44 + SDK 22621 portáteis; defina `UE_SDKS_ROOT=<destino>`
  e o UBT/UAT usam esse toolchain.
- `py -3 tools/ue.py build-plugin`, `py -3 tools/ue.py test`, `py -3 tools/mp_run.py --project SampleProject/SampleCatalog.uproject --case all`.

## macOS

- Xcode **15.2 a 27.x** (`Engine/Config/Apple/Apple_SDK.json`: Min 15.2.0, Main 26.1.1, Max 27.9.0), Command Line Tools
  e `sudo xcode-select -s /Applications/Xcode.app`. Rode o Xcode uma vez para aceitar a licença.
- Engine pelo Epic Games Launcher (`/Users/Shared/Epic Games/UE_5.8`) ou compilada do fonte.
- `python3 tools/ue.py build-plugin` e `python3 tools/ue.py test`. O executável do editor é o do bundle `.app`;
  não há `UnrealEditor-Cmd` no Mac.
- `python3 tools/mp_run.py --project SampleProject/SampleCatalog.uproject --case all --kill-all`.

## Linux

- Toolchain exigido: `v26_clang-20.1.8-rockylinux8` (`Engine/Config/Linux/Linux_SDK.json`). Engine do fonte:
  `./Setup.sh && ./GenerateProjectFiles.sh`. Baixe o toolchain oficial da Epic para a UE 5.8 (ou instale clang 20
  compatível) e defina `LINUX_MULTIARCH_ROOT` se não for o padrão do `Setup.sh`.
- Pacotes típicos (Debian/Ubuntu): `build-essential clang cmake git python3 mono-devel libxcb1 libxcursor1 libxi6
  libxrandr2 libxss1 libgbm1 libvulkan1 mesa-vulkan-drivers libnss3 libasound2 libpulse0`. Mesa/Vulkan é necessário
  para `-RenderOffscreen` (HUD dos casos 08/09); sem GPU use `lavapipe` (`mesa-vulkan-drivers`) ou rode só os casos de
  rede (`--case 03` etc., que usam `-nullrhi`).
- `python3 tools/ue.py build-plugin`, `python3 tools/ue.py test`, `python3 tools/mp_run.py ...`.
- Em CI/container sem display, `-RenderOffscreen` e `-nullrhi` (já usados pelas ferramentas) bastam; não é preciso X11.

## Caminhos de save

`UCatalogSaveSubsystem` grava slots do `USaveGame` da engine, em `<Saved>/SaveGames/<Slot>.sav`:

| | Editor/`-game` (projeto) | Build empacotado |
|---|---|---|
| Windows | `<Projeto>\Saved\SaveGames` | `%LOCALAPPDATA%\<Projeto>\Saved\SaveGames` |
| macOS | `<Projeto>/Saved/SaveGames` | `~/Library/Application Support/Epic/<Projeto>/Saved/SaveGames` |
| Linux | `<Projeto>/Saved/SaveGames` | `~/.config/Epic/<Projeto>/Saved/SaveGames` |

O código usa só `FPaths`/`UGameplayStatics` (nunca `\` literal). Os testes de automação e o `mp_run.py` (caso 07)
gravam e leem pelo subsistema, então não dependem do caminho físico.

## Sistemas de arquivos com diferença de caixa (Linux)

Linux (e volumes macOS sensíveis à caixa) distingue `Foo.h` de `foo.h`. O `tools/validate_plugin.py` falha quando um
`#include "X"` só casa com o arquivo real ignorando a caixa, quando há API só de Windows (`windows.h`, `Windows/`,
`_WIN32`, `__declspec`, `_MSC_VER`, `"\\"` em caminho) fora de `#if PLATFORM_WINDOWS` e quando o `.uplugin` não declara
Win64, Mac e Linux em `SupportedTargetPlatforms`. Rode-o antes de abrir PR.

## O que o CI cobre

| Verificação | Windows | macOS | Linux |
|---|---|---|---|
| ruff + validador + pytest (Python 3.10 a 3.14, `ci.yml`) | sim | sim | sim |
| Comandos/caminhos/kill por plataforma (pytest com `monkeypatch`, roda em qualquer SO) | sim | sim | sim |
| `ue.py build-plugin` e `ue.py test` com a engine (`ue.yml`, runner self-hosted `[self-hosted, <os>, ue-5.8]`) | se `UE_RUNNER_WINDOWS_ENABLED=true` | se `UE_RUNNER_MACOS_ENABLED=true` | se `UE_RUNNER_LINUX_ENABLED=true` |
| Verificado em máquina real nesta revisão | sim | não | não |

Variáveis opcionais por SO: `UE_ROOT_WINDOWS`, `UE_ROOT_MACOS`, `UE_ROOT_LINUX` (ou `UE_ROOT` para todos).
