#!/usr/bin/env python3
r"""Comandos que exigem Unreal Engine 5.8+ instalada: empacotar o plugin e rodar os testes de automação.

Engine: --engine <pasta> > env UE_ROOT > registro Windows (HKLM\SOFTWARE\EpicGames\Unreal Engine\<--version>,
default 5.8). A pasta é a raiz da instalação (a que contém Engine/), ex.: "C:/Program Files/Epic Games/UE_5.8".

Exemplos:
  py -3 tools/ue.py build-plugin --dry-run
  py -3 tools/ue.py test --engine "C:/Program Files/Epic Games/UE_5.8"
  py -3 tools/ue.py test --version 5.9
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

from validate_plugin import make_console_safe

ROOT = Path(__file__).resolve().parents[1]
ENGINE_VERSION = "5.8"
UPLUGIN = ROOT / "Plugins" / "CatalogAnchor" / "CatalogAnchor.uplugin"
UPROJECT = ROOT / "SampleProject" / "SampleCatalog.uproject"
PACKAGE_DIR = ROOT / "dist" / "CatalogAnchor"
TEST_FILTER = "CatalogAnchor"
EXIT_NO_ENGINE = 2


def host_platform(system: str = sys.platform) -> str:
    """Nome de plataforma do UBT para o SO."""
    if system.startswith("win"):
        return "Win64"
    if system == "darwin":
        return "Mac"
    return "Linux"


def registry_engine_dir(version: str = ENGINE_VERSION) -> str | None:
    """InstalledDirectory gravado pelo Epic Games Launcher (só existe no Windows)."""
    try:
        import winreg
    except ImportError:
        return None
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, rf"SOFTWARE\EpicGames\Unreal Engine\{version}") as key:
            return str(winreg.QueryValueEx(key, "InstalledDirectory")[0])
    except OSError:
        return None


def find_engine(explicit: str | None = None, version: str = ENGINE_VERSION) -> Path | None:
    """Raiz da engine. A primeira fonte informada decide: um --engine errado não cai silenciosamente no registro."""
    raw = explicit or os.environ.get("UE_ROOT") or registry_engine_dir(version)
    if not raw:
        return None
    path = Path(raw).expanduser()
    return path if (path / "Engine" / "Build" / "BatchFiles").is_dir() else None


def runuat_path(engine: Path, platform: str) -> Path:
    script = "RunUAT.bat" if platform == "Win64" else "RunUAT.sh"
    return engine / "Engine" / "Build" / "BatchFiles" / script


def editor_cmd_path(engine: Path, platform: str) -> Path:
    exe = "UnrealEditor-Cmd.exe" if platform == "Win64" else "UnrealEditor-Cmd"
    return engine / "Engine" / "Binaries" / platform / exe


def build_plugin_command(engine: Path, platform: str) -> list[str]:
    return [
        str(runuat_path(engine, platform)),
        "BuildPlugin",
        f"-Plugin={UPLUGIN}",
        f"-Package={PACKAGE_DIR}",
        f"-TargetPlatforms={platform}",
    ]


def automation_test_command(engine: Path, platform: str) -> list[str]:
    return [
        str(editor_cmd_path(engine, platform)),
        str(UPROJECT),
        f"-ExecCmds=Automation RunTests {TEST_FILTER}; Quit",
        "-unattended",
        "-nullrhi",
        "-nosplash",
        "-nopause",
        "-log",
    ]


def windows_command_line(argv: list[str]) -> str:
    """Linha de comando no estilo UE: -Chave="valor com espaço".

    No Windows o UE lê a linha crua (FParse::Value) e corta "-Chave=valor com espaço" no primeiro espaço,
    que é como subprocess.list2cmdline citaria o argumento.
    """
    parts = []
    for arg in argv:
        key, sep, value = arg.partition("=")
        if " " not in arg:
            parts.append(arg)
        elif sep and key.startswith("-") and " " not in key:
            parts.append(f'{key}="{value}"')
        else:
            parts.append(f'"{arg}"')
    return " ".join(parts)


def main(argv: list[str] | None = None) -> int:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--engine", help="raiz da engine (pasta com Engine/); default: UE_ROOT ou registro")
    common.add_argument("--dry-run", action="store_true", help="só imprime o comando")
    common.add_argument("--version", default=ENGINE_VERSION, help=f"versão no registro (default: {ENGINE_VERSION})")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("build-plugin", parents=[common], help="RunUAT BuildPlugin -> dist/CatalogAnchor")
    sub.add_parser("test", parents=[common], help="testes de automação CatalogAnchor no SampleProject")
    args = parser.parse_args(argv)
    make_console_safe()

    engine = find_engine(args.engine, args.version)
    if engine is None:
        print(
            f"ue.py: Unreal Engine {args.version} não encontrada. Passe --engine <pasta>, defina UE_ROOT "
            "ou instale pelo Epic Games Launcher (a pasta precisa conter Engine/Build/BatchFiles).",
            file=sys.stderr,
        )
        return EXIT_NO_ENGINE

    platform = host_platform()
    make = build_plugin_command if args.command == "build-plugin" else automation_test_command
    argv_cmd = make(engine, platform)
    cmd: str | list[str] = windows_command_line(argv_cmd) if platform == "Win64" else argv_cmd
    print(cmd if isinstance(cmd, str) else shlex.join(cmd))
    if args.dry_run:
        return 0
    if not Path(argv_cmd[0]).is_file():
        print(f"ue.py: executável ausente: {argv_cmd[0]} (engine compilada do fonte?)", file=sys.stderr)
        return EXIT_NO_ENGINE
    return subprocess.run(cmd, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
