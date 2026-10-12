"""tools/ue.py e o atalho `dev.py build`: montagem de comandos e descoberta da engine (sem Unreal instalada)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dev  # noqa: E402
import ue  # noqa: E402


def _fake_engine(base: Path, platform: str = "Linux") -> Path:
    """Instalação mínima: o marcador Engine/Build/BatchFiles + os executáveis que os comandos chamam."""
    engine = base / "UE_5.8"
    ue.runuat_path(engine, platform).parent.mkdir(parents=True)
    ue.runuat_path(engine, platform).write_text("", encoding="utf-8")
    ue.editor_cmd_path(engine, platform).parent.mkdir(parents=True)
    ue.editor_cmd_path(engine, platform).write_text("", encoding="utf-8")
    return engine


@pytest.fixture
def no_engine_env(monkeypatch):
    """Isola a descoberta: sem UE_ROOT e sem registro (a máquina de quem roda pode ter a UE instalada)."""
    monkeypatch.delenv("UE_ROOT", raising=False)
    monkeypatch.setattr(ue, "registry_engine_dir", lambda version: None)
    monkeypatch.setattr(ue, "host_platform", lambda: "Linux")
    return monkeypatch


@pytest.mark.parametrize(("system", "expected"), [("win32", "Win64"), ("darwin", "Mac"), ("linux", "Linux")])
def test_host_platform(system: str, expected: str):
    assert ue.host_platform(system) == expected


def test_build_plugin_command_windows():
    engine = Path("C:/UE_5.8")
    cmd = ue.build_plugin_command(engine, "Win64")
    assert cmd == [
        str(engine / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"),
        "BuildPlugin",
        f"-Plugin={ROOT / 'Plugins' / 'CatalogAnchor' / 'CatalogAnchor.uplugin'}",
        f"-Package={ROOT / 'dist' / 'CatalogAnchor'}",
        "-TargetPlatforms=Win64",
    ]
    assert Path(cmd[2].split("=", 1)[1]).is_absolute()


def test_build_plugin_command_posix_uses_runuat_sh():
    cmd = ue.build_plugin_command(Path("/opt/UE_5.8"), "Mac")
    assert cmd[0].endswith("RunUAT.sh")
    assert cmd[-1] == "-TargetPlatforms=Mac"


def test_automation_test_command():
    engine = Path("/opt/UE_5.8")
    cmd = ue.automation_test_command(engine, "Linux")
    assert cmd[0] == str(engine / "Engine" / "Binaries" / "Linux" / "UnrealEditor-Cmd")
    assert cmd[1] == str(ROOT / "SampleProject" / "SampleCatalog.uproject")
    assert "-ExecCmds=Automation RunTests CatalogAnchor; Quit" in cmd
    for flag in ("-unattended", "-nullrhi", "-nosplash", "-nopause", "-log"):
        assert flag in cmd
    assert ue.automation_test_command(engine, "Win64")[0].endswith(str(Path("Win64") / "UnrealEditor-Cmd.exe"))


def test_windows_command_line_quotes_values_ue_style():
    line = ue.windows_command_line(
        [
            "C:/Program Files/UE/RunUAT.bat",
            "BuildPlugin",
            "-Plugin=H:/Meu Drive/x.uplugin",
            "-ExecCmds=Automation RunTests CatalogAnchor; Quit",
            "-log",
        ]
    )
    assert line == (
        '"C:/Program Files/UE/RunUAT.bat" BuildPlugin -Plugin="H:/Meu Drive/x.uplugin" '
        '-ExecCmds="Automation RunTests CatalogAnchor; Quit" -log'
    )


def test_find_engine_from_arg(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    assert ue.find_engine(str(engine)) == engine


def test_find_engine_from_env(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    no_engine_env.setenv("UE_ROOT", str(engine))
    assert ue.find_engine() == engine


def test_find_engine_from_registry(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    asked = []
    no_engine_env.setattr(ue, "registry_engine_dir", lambda version: asked.append(version) or str(engine))
    assert ue.find_engine() == engine
    assert ue.find_engine(version="5.9") == engine
    assert asked == ["5.8", "5.9"]


def test_find_engine_arg_wins_and_bad_arg_does_not_fall_back(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    no_engine_env.setenv("UE_ROOT", str(engine))
    assert ue.find_engine(str(tmp_path / "nao-existe")) is None


def test_registry_lookup_of_unknown_version_is_none():
    assert ue.registry_engine_dir("0.0-inexistente") is None


@pytest.mark.parametrize("command", ["build-plugin", "test"])
def test_main_without_engine_exits_2(no_engine_env, capsys, command: str):
    assert ue.main([command]) == 2
    assert "Unreal Engine 5.8 não encontrada" in capsys.readouterr().err


def test_main_version_flag_selects_registry_key(no_engine_env, capsys):
    asked = []
    no_engine_env.setattr(ue, "registry_engine_dir", lambda version: asked.append(version))
    assert ue.main(["test", "--version", "6.0"]) == 2
    assert asked == ["6.0"]
    assert "Unreal Engine 6.0 não encontrada" in capsys.readouterr().err


def test_main_dry_run_prints_and_does_not_execute(no_engine_env, capsys, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    no_engine_env.setattr(subprocess, "run", lambda *a, **k: pytest.fail("dry-run executou o comando"))
    assert ue.main(["build-plugin", "--dry-run", "--engine", str(engine)]) == 0
    out = capsys.readouterr().out
    assert "BuildPlugin" in out and "RunUAT.sh" in out


def test_main_runs_command_and_returns_its_exit_code(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    calls = []
    no_engine_env.setattr(
        subprocess, "run", lambda cmd, **k: calls.append(cmd) or subprocess.CompletedProcess(cmd, 3)
    )
    assert ue.main(["test", "--engine", str(engine)]) == 3
    assert calls == [ue.automation_test_command(engine, "Linux")]


def test_main_windows_passes_ue_style_command_line(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path, "Win64")
    no_engine_env.setattr(ue, "host_platform", lambda: "Win64")
    calls = []
    no_engine_env.setattr(
        subprocess, "run", lambda cmd, **k: calls.append(cmd) or subprocess.CompletedProcess(cmd, 0)
    )
    assert ue.main(["test", "--engine", str(engine)]) == 0
    assert isinstance(calls[0], str) and '-ExecCmds="Automation RunTests CatalogAnchor; Quit"' in calls[0]


def test_dev_build_without_engine_keeps_message(no_engine_env, capsys):
    with pytest.raises(SystemExit) as exc:
        dev.main(["build"])
    assert exc.value.code == 2
    assert "Unreal Engine 5.8" in capsys.readouterr().err


def test_dev_build_delegates_to_ue_build_plugin(no_engine_env, tmp_path: Path):
    calls = []
    no_engine_env.setattr(ue, "find_engine", lambda *a: tmp_path)
    no_engine_env.setattr(ue, "main", lambda argv: calls.append(argv) or 0)
    assert dev.main(["build"]) == 0
    assert calls == [["build-plugin"]]
