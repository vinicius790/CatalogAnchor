#!/usr/bin/env python3
r"""Comandos que exigem Unreal Engine 5.8+ instalada: empacotar o plugin e rodar os testes de automação.

Engine: --engine <pasta> > env UE_ROOT > registro Windows (HKLM\SOFTWARE\EpicGames\Unreal Engine\<--version>,
default 5.8) > %ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat (AppName "UE_<--version>")
> pastas padrão: Win %ProgramFiles%\Epic Games\UE_<--version>; Mac /Users/Shared/Epic Games; Linux ~/UnrealEngine.
No Mac/Linux use --engine ou UE_ROOT (docs/PLATFORMS.md).
A pasta é a raiz da instalação (a que contém Engine/), ex.: "C:/Program Files/Epic Games/UE_5.8".

UE_SDKS_ROOT (opcional): se estiver no ambiente, o UBT/UAT herda e usa <UE_SDKS_ROOT>/HostWin64/Win64/VS2022 como
toolchain MSVC portátil (gerado por tools/get_msvc.py), sem Visual Studio instalado; o ue.py não altera o ambiente.

build-plugin: RunUAT BuildPlugin -> dist/CatalogAnchor (logs do UAT/UBT em <report-dir>/UAT).
test: compila o target SampleCatalogEditor (Build.bat/Build.sh, log em <report-dir>/UBT.log), roda os testes
      CatalogAnchor no editor headless e lê <report-dir>/index.json; sai != 0 se algo falhou ou se nenhum teste rodou.

Exemplos:
  py -3 tools/ue.py build-plugin --dry-run
  py -3 tools/ue.py test --engine "C:/Program Files/Epic Games/UE_5.8" --report-dir Saved/Reports
  py -3 tools/ue.py test --version 5.9
"""

from __future__ import annotations

import argparse
import json
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
EDITOR_TARGET = "SampleCatalogEditor"
PACKAGE_DIR = ROOT / "dist" / "CatalogAnchor"
REPORT_DIR = ROOT / "Saved" / "Reports"
TEST_FILTER = "CatalogAnchor"
EXIT_NO_ENGINE = 2
EXIT_TESTS_FAILED = 1
# UnrealBuildTool/Configuration/UEBuildPlatform.cs (UE 5.8): "Platform X is not a valid platform to build. ..."
SDK_ERROR = "is not a valid platform to build"
SDK_HINTS = {
    "Win64": "rode `py -3 tools/get_msvc.py` e defina UE_SDKS_ROOT=<destino> (MSVC 14.44 + Windows SDK 22621 "
    "portáteis, sem instalar nada), ou instale o Visual Studio 2022 17.14 / Build Tools com a carga "
    "'Desenvolvimento para desktop com C++'; requisitos em Engine/Config/Windows/Windows_SDK.json",
    "Mac": "instale o Xcode 15.2 a 27.x (Engine/Config/Apple/Apple_SDK.json) e aponte `xcode-select -s` para ele",
    "Linux": "instale o clang do Linux_SDK.json (v26_clang-20.1.8 na UE 5.8) e rode Setup.sh da engine; "
    "veja docs/PLATFORMS.md",
}


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


def launcher_engine_dir(version: str = ENGINE_VERSION, dat: Path | None = None) -> str | None:
    """InstallLocation de "UE_<versão>" no LauncherInstalled.dat (instalações do Launcher que não vão ao registro)."""
    if dat is None:
        system = host_platform()
        if system == "Win64":
            base = Path(os.environ.get("ProgramData", r"C:\ProgramData")) / "Epic"
        elif system == "Mac":
            base = Path("/Users/Shared/Epic")
        else:
            base = Path.home() / ".config" / "Epic"
        dat = base / "UnrealEngineLauncher" / "LauncherInstalled.dat"
    try:
        installs = json.loads(dat.read_text(encoding="utf-8-sig")).get("InstallationList", [])
    except (OSError, ValueError, AttributeError):
        return None
    for item in installs:
        if isinstance(item, dict) and item.get("AppName") == f"UE_{version}" and item.get("InstallLocation"):
            return str(item["InstallLocation"])
    return None


def program_files_engine_dir(version: str = ENGINE_VERSION) -> str | None:
    """Último recurso: pastas padrão (Win: Program Files do Launcher; Mac: /Users/Shared; Linux: ~/UnrealEngine)."""
    candidates = [
        Path(base) / "Epic Games" / f"UE_{version}"
        for base in (os.environ.get(v) for v in ("ProgramFiles", "ProgramFiles(x86)"))
        if base
    ]
    candidates += [
        Path("/Users/Shared/Epic Games") / f"UE_{version}",
        Path.home() / "UnrealEngine" / f"UE_{version}",
        Path.home() / "UnrealEngine",
    ]
    for candidate in candidates:
        if (candidate / "Engine" / "Build" / "BatchFiles").is_dir():
            return str(candidate)
    return None


def find_engine(explicit: str | None = None, version: str = ENGINE_VERSION) -> Path | None:
    """Raiz da engine. A primeira fonte informada decide: um --engine errado não cai silenciosamente no registro."""
    raw = (
        explicit
        or os.environ.get("UE_ROOT")
        or registry_engine_dir(version)
        or launcher_engine_dir(version)
        or program_files_engine_dir(version)
    )
    if not raw:
        return None
    path = Path(raw).expanduser()
    return path if (path / "Engine" / "Build" / "BatchFiles").is_dir() else None


def runuat_path(engine: Path, platform: str) -> Path:
    script = "RunUAT.bat" if platform == "Win64" else "RunUAT.sh"
    return engine / "Engine" / "Build" / "BatchFiles" / script


def build_script_path(engine: Path, platform: str) -> Path:
    """Build.bat na raiz de BatchFiles (Win); Mac/Build.sh e Linux/Build.sh nas subpastas."""
    batch = engine / "Engine" / "Build" / "BatchFiles"
    return batch / "Build.bat" if platform == "Win64" else batch / platform / "Build.sh"


def editor_cmd_path(engine: Path, platform: str) -> Path:
    """Mesmo executável que o UAT usa (CommandletUtils.GetEditorCommandletExe, UE 5.8).

    Mac: o bundle .app (sem UnrealEditor-Cmd); Linux: UnrealEditor. Mac e Linux não verificados em máquina real.
    """
    binaries = engine / "Engine" / "Binaries" / platform
    if platform == "Win64":
        return binaries / "UnrealEditor-Cmd.exe"
    if platform == "Mac":
        return binaries / "UnrealEditor.app" / "Contents" / "MacOS" / "UnrealEditor"
    return binaries / "UnrealEditor"


def build_plugin_command(engine: Path, platform: str, strict_includes: bool = False) -> list[str]:
    cmd = [
        str(runuat_path(engine, platform)),
        "BuildPlugin",
        f"-Plugin={UPLUGIN}",
        f"-Package={PACKAGE_DIR}",
        f"-TargetPlatforms={platform}",
    ]
    if strict_includes:
        # Sem unity nem PCH: acha include faltando que o build unificado mascara (clang no Mac/Linux é intolerante).
        # Parâmetro do BuildPlugin do UAT; não conferido contra a engine instalada nesta sessão.
        cmd.append("-StrictIncludes")
    return cmd


def build_editor_command(engine: Path, platform: str, report_dir: Path) -> list[str]:
    """UBT do target editor do sample (compila plugin + módulo do sample); -unattended não deixa o editor compilar."""
    return [
        str(build_script_path(engine, platform)),
        EDITOR_TARGET,
        platform,
        "Development",
        f"-Project={UPROJECT}",
        "-WaitMutex",
        f"-Log={report_dir / 'UBT.log'}",
    ]


def automation_test_command(engine: Path, platform: str, report_dir: Path) -> list[str]:
    return [
        str(editor_cmd_path(engine, platform)),
        str(UPROJECT),
        f"-ExecCmds=Automation RunTests {TEST_FILTER}; Quit",
        # Rede de segurança: o Quit acima já encerra com o exit code dos testes.
        "-TestExit=Automation Test Queue Empty",
        f"-ReportExportPath={report_dir}",
        f"-ABSLOG={report_dir / 'Editor.log'}",
        "-unattended",
        "-nullrhi",
        "-nosplash",
        "-NoSound",
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


def summarize_report(report_dir: Path) -> tuple[bool, str]:
    """Veredito do index.json do -ReportExportPath (FAutomatedTestPassResults, chaves em camelCase).

    Falha se o relatório faltar, se algum teste falhou/não terminou ou se nenhum teste passou.
    """
    index = report_dir / "index.json"
    try:
        data = json.loads(index.read_text(encoding="utf-8-sig"))  # gravado com ForceUTF8 (BOM)
    except FileNotFoundError:
        return False, f"relatório ausente: {index} (o editor caiu antes de exportar? veja Editor.log)"
    except (OSError, ValueError) as exc:
        return False, f"relatório ilegível: {index}: {exc}"
    if not isinstance(data, dict) or not isinstance(data.get("tests") or [], list):
        return False, f"relatório ilegível: {index}: formato inesperado (esperado objeto com lista 'tests')"
    by_state: dict[str, list[str]] = {}
    for test in (t for t in data.get("tests") or [] if isinstance(t, dict)):
        by_state.setdefault(test.get("state", "NotRun"), []).append(test.get("fullTestPath", "?"))
    passed = len(by_state.get("Success", []))
    failed = by_state.get("Fail", [])
    unfinished = by_state.get("NotRun", []) + by_state.get("InProcess", [])
    reported = data.get("failed")
    n_failed = max(len(failed), reported if isinstance(reported, int) else 0)
    summary = (
        f"{passed} ok, {n_failed} falha(s), {len(unfinished)} não terminado(s), "
        f"{len(by_state.get('Skipped', []))} pulado(s)"
    )
    lines = [summary] + [f"  FALHOU: {name}" for name in failed] + [f"  NÃO TERMINOU: {name}" for name in unfinished]
    if passed == 0 and not n_failed:
        lines.append("  nenhum teste rodou (filtro sem match ou plugin não carregou)")
    return passed > 0 and n_failed == 0 and not unfinished, "\n".join(lines)


def sdk_missing(logs: list[Path]) -> bool:
    """O UBT recusou a plataforma por falta de SDK/toolchain (procura nos logs desta execução)."""
    for log in logs:
        if log.is_file() and SDK_ERROR in log.read_text(encoding="utf-8", errors="replace"):
            return True
    return False


def run_step(argv: list[str], platform: str, dry_run: bool, env: dict[str, str] | None = None) -> int:
    cmd: str | list[str] = windows_command_line(argv) if platform == "Win64" else argv
    print(cmd if isinstance(cmd, str) else shlex.join(cmd), flush=True)
    if dry_run:
        return 0
    if not Path(argv[0]).is_file():
        print(f"ue.py: executável ausente: {argv[0]} (engine compilada do fonte?)", file=sys.stderr)
        return EXIT_NO_ENGINE
    return subprocess.run(cmd, check=False, env=env).returncode


def build_failed(rc: int, platform: str, logs: list[Path]) -> int:
    if sdk_missing(logs):
        print(f"ue.py: SDK/toolchain de {platform} não encontrado pelo UBT; {SDK_HINTS[platform]}.", file=sys.stderr)
    print(f"ue.py: build falhou (exit {rc}); logs: {', '.join(str(p) for p in logs)}", file=sys.stderr)
    return rc


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):  # consoles cp1252 não imprimem os acentos do --help
        stream.reconfigure(errors="backslashreplace")
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument(
        "--engine",
        help="raiz da engine (pasta com Engine/); default: UE_ROOT, registro, LauncherInstalled.dat ou Program Files",
    )
    common.add_argument("--dry-run", action="store_true", help="só imprime os comandos")
    common.add_argument(
        "--version", default=ENGINE_VERSION, help=f"versão no registro/Launcher (default: {ENGINE_VERSION})"
    )
    common.add_argument(
        "--report-dir", default=str(REPORT_DIR), help=f"logs e relatório de automação (default: {REPORT_DIR})"
    )
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    plugin_parser = sub.add_parser("build-plugin", parents=[common], help="RunUAT BuildPlugin -> dist/CatalogAnchor")
    plugin_parser.add_argument(
        "--strict-includes",
        action="store_true",
        help="sem unity/PCH: detecta include faltando (BuildPlugin -StrictIncludes)",
    )
    sub.add_parser("test", parents=[common], help="compila o editor do sample e roda os testes CatalogAnchor")
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
    report_dir = Path(args.report_dir).resolve()
    if not args.dry_run:
        report_dir.mkdir(parents=True, exist_ok=True)

    if args.command == "build-plugin":
        uat_logs = report_dir / "UAT"  # o UAT esvazia essa pasta ao iniciar: não aponte para o report-dir inteiro
        env = {**os.environ, "uebp_LogFolder": str(uat_logs)}
        rc = run_step(build_plugin_command(engine, platform, args.strict_includes), platform, args.dry_run, env)
        logs = sorted(uat_logs.rglob("*.txt")) if uat_logs.is_dir() else []
        return build_failed(rc, platform, logs) if rc else 0

    rc = run_step(build_editor_command(engine, platform, report_dir), platform, args.dry_run)
    if rc:
        return build_failed(rc, platform, [report_dir / "UBT.log"])
    if not args.dry_run:
        (report_dir / "index.json").unlink(missing_ok=True)  # não julgar pelo relatório da execução anterior
    rc = run_step(automation_test_command(engine, platform, report_dir), platform, args.dry_run)
    if args.dry_run:
        return 0
    ok, summary = summarize_report(report_dir)
    print(f"ue.py: testes de automação: {summary}", file=sys.stdout if ok and not rc else sys.stderr)
    if rc:
        return rc  # o editor sai != 0 quando algum teste falha (GIsCriticalError)
    return 0 if ok else EXIT_TESTS_FAILED


if __name__ == "__main__":
    raise SystemExit(main())
