"""tools/ue.py e o atalho `dev.py build`: montagem de comandos, relatório de automação e descoberta da engine."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dev  # noqa: E402
import ue  # noqa: E402

UPROJECT = str(ROOT / "SampleProject" / "SampleCatalog.uproject")


_REAL_PROGRAM_FILES_LOOKUP = ue.program_files_engine_dir  # a fixture no_engine_env o substitui


def _fake_engine(base: Path, platform: str = "Linux") -> Path:
    """Instalação mínima: o marcador Engine/Build/BatchFiles + os executáveis que os comandos chamam."""
    engine = base / "UE_5.8"
    for exe in (
        ue.runuat_path(engine, platform),
        ue.build_script_path(engine, platform),
        ue.editor_cmd_path(engine, platform),
    ):
        exe.parent.mkdir(parents=True, exist_ok=True)
        exe.write_text("", encoding="utf-8")
    return engine


def _report(report_dir: Path, *states: str, bom: bool = True, **counters) -> None:
    """index.json no formato do FAutomatedTestPassResults (camelCase, enum como texto)."""
    data = {
        "succeeded": states.count("Success"),
        "succeededWithWarnings": 0,
        "failed": states.count("Fail"),
        "notRun": states.count("NotRun"),
        "inProcess": states.count("InProcess"),
        "tests": [{"fullTestPath": f"CatalogAnchor.T.{i}", "state": s} for i, s in enumerate(states)],
        **counters,
    }
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / "index.json").write_text(json.dumps(data), encoding="utf-8-sig" if bom else "utf-8")


def _fake_run(calls: list, codes: list[int], on_editor=None):
    """subprocess.run falso: devolve os exit codes em ordem; on_editor simula o editor gravando o relatório."""

    def run(cmd, **kwargs):
        calls.append((cmd, kwargs.get("env")))
        if on_editor and len(calls) == 2:
            on_editor()
        return subprocess.CompletedProcess(cmd, codes[len(calls) - 1])

    return run


@pytest.fixture
def no_engine_env(monkeypatch):
    """Isola a descoberta: sem UE_ROOT, registro ou Launcher (a máquina de quem roda pode ter a UE instalada)."""
    monkeypatch.delenv("UE_ROOT", raising=False)
    monkeypatch.setattr(ue, "registry_engine_dir", lambda version: None)
    monkeypatch.setattr(ue, "launcher_engine_dir", lambda version: None)
    monkeypatch.setattr(ue, "program_files_engine_dir", lambda version: None)
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


@pytest.mark.parametrize(
    ("platform", "script", "editor"),
    [
        ("Win64", ("Build.bat",), ("Binaries", "Win64", "UnrealEditor-Cmd.exe")),
        ("Mac", ("Mac", "Build.sh"), ("Binaries", "Mac", "UnrealEditor.app", "Contents", "MacOS", "UnrealEditor")),
        ("Linux", ("Linux", "Build.sh"), ("Binaries", "Linux", "UnrealEditor")),
    ],
)
def test_paths_per_platform(platform: str, script: tuple[str, ...], editor: tuple[str, ...]):
    engine = Path("/opt/UE_5.8")
    assert ue.build_script_path(engine, platform) == engine.joinpath("Engine", "Build", "BatchFiles", *script)
    assert ue.editor_cmd_path(engine, platform) == engine.joinpath("Engine", *editor)


def test_build_editor_command(tmp_path: Path):
    cmd = ue.build_editor_command(Path("/opt/UE_5.8"), "Mac", tmp_path)
    assert cmd[1:] == [
        "SampleCatalogEditor",
        "Mac",
        "Development",
        f"-Project={UPROJECT}",
        "-WaitMutex",
        f"-Log={tmp_path / 'UBT.log'}",
    ]


def test_automation_test_command(tmp_path: Path):
    engine = Path("/opt/UE_5.8")
    cmd = ue.automation_test_command(engine, "Linux", tmp_path)
    assert cmd[0] == str(engine / "Engine" / "Binaries" / "Linux" / "UnrealEditor")
    assert cmd[1] == UPROJECT
    for arg in (
        "-ExecCmds=Automation RunTests CatalogAnchor; Quit",
        "-TestExit=Automation Test Queue Empty",
        f"-ReportExportPath={tmp_path}",
        f"-ABSLOG={tmp_path / 'Editor.log'}",
        "-unattended",
        "-nullrhi",
        "-nosplash",
        "-NoSound",
        "-nopause",
        "-log",
    ):
        assert arg in cmd


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


def test_report_all_passed(tmp_path: Path):
    _report(tmp_path, "Success", "Success", "Skipped")
    ok, summary = ue.summarize_report(tmp_path)
    assert ok
    assert summary.startswith("2 ok, 0 falha(s), 0 não terminado(s), 1 pulado(s)")


def test_report_without_bom_is_read(tmp_path: Path):
    _report(tmp_path, "Success", bom=False)
    assert ue.summarize_report(tmp_path)[0]


def test_report_with_failure_names_the_test(tmp_path: Path):
    _report(tmp_path, "Success", "Fail")
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok
    assert "1 falha(s)" in summary and "FALHOU: CatalogAnchor.T.1" in summary


def test_report_failed_counter_counts_even_without_test_entries(tmp_path: Path):
    _report(tmp_path, "Success", failed=2)
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok and "2 falha(s)" in summary


@pytest.mark.parametrize("state", ["NotRun", "InProcess"])
def test_report_unfinished_test_fails(tmp_path: Path, state: str):
    _report(tmp_path, "Success", state)
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok and "NÃO TERMINOU: CatalogAnchor.T.1" in summary


def test_report_with_zero_tests_fails(tmp_path: Path):
    _report(tmp_path)
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok and "nenhum teste rodou" in summary


def test_report_missing_or_invalid_fails(tmp_path: Path):
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok and "relatório ausente" in summary
    (tmp_path / "index.json").write_text("{", encoding="utf-8")
    ok, summary = ue.summarize_report(tmp_path)
    assert not ok and "relatório ilegível" in summary


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


def test_launcher_dat_lookup(tmp_path: Path):
    dat = tmp_path / "LauncherInstalled.dat"
    dat.write_text(
        json.dumps(
            {
                "InstallationList": [
                    {"InstallLocation": "C:/Games/Fortnite", "AppName": "Fortnite"},
                    {"InstallLocation": "C:/Program Files/Epic Games/UE_5.8", "AppName": "UE_5.8"},
                ]
            }
        ),
        encoding="utf-8-sig",
    )
    assert ue.launcher_engine_dir("5.8", dat) == "C:/Program Files/Epic Games/UE_5.8"
    assert ue.launcher_engine_dir("5.9", dat) is None
    assert ue.launcher_engine_dir("5.8", tmp_path / "nao-existe.dat") is None
    dat.write_text("{quebrado", encoding="utf-8")
    assert ue.launcher_engine_dir("5.8", dat) is None


def test_find_engine_falls_back_to_launcher(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    no_engine_env.setattr(ue, "launcher_engine_dir", lambda version: str(engine) if version == "5.8" else None)
    assert ue.find_engine() == engine
    assert ue.find_engine(version="5.9") is None


def test_program_files_scan_finds_launcher_default_install(monkeypatch, tmp_path: Path):
    engine = tmp_path / "pf" / "Epic Games" / "UE_5.8"
    (engine / "Engine" / "Build" / "BatchFiles").mkdir(parents=True)
    monkeypatch.setenv("ProgramFiles", str(tmp_path / "vazio"))
    monkeypatch.setenv("ProgramFiles(x86)", str(tmp_path / "pf"))
    assert _REAL_PROGRAM_FILES_LOOKUP("5.8") == str(engine)
    assert _REAL_PROGRAM_FILES_LOOKUP("5.9") is None


def test_find_engine_falls_back_to_program_files(no_engine_env, tmp_path: Path):
    engine = _fake_engine(tmp_path)
    no_engine_env.setattr(ue, "program_files_engine_dir", lambda version: str(engine))
    assert ue.find_engine() == engine


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
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    no_engine_env.setattr(subprocess, "run", lambda *a, **k: pytest.fail("dry-run executou o comando"))
    assert ue.main(["build-plugin", "--dry-run", "--engine", str(engine)]) == 0
    assert ue.main(["test", "--dry-run", "--engine", str(engine), "--report-dir", str(report)]) == 0
    out = capsys.readouterr().out
    assert "BuildPlugin" in out and "RunUAT.sh" in out
    assert "Build.sh" in out and "SampleCatalogEditor Linux Development" in out and "-ReportExportPath=" in out
    assert not report.exists()


def test_main_test_builds_then_runs_and_passes_on_green_report(no_engine_env, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    calls: list = []
    no_engine_env.setattr(subprocess, "run", _fake_run(calls, [0, 0], lambda: _report(report, "Success")))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == 0
    assert [cmd for cmd, _ in calls] == [
        ue.build_editor_command(engine, "Linux", report),
        ue.automation_test_command(engine, "Linux", report),
    ]


def test_main_test_fails_on_red_report_even_with_exit_0(no_engine_env, capsys, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    no_engine_env.setattr(subprocess, "run", _fake_run([], [0, 0], lambda: _report(report, "Success", "Fail")))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == ue.EXIT_TESTS_FAILED
    assert "FALHOU: CatalogAnchor.T.1" in capsys.readouterr().err


def test_main_test_ignores_stale_report(no_engine_env, capsys, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    _report(report, "Success")  # execução anterior; desta vez o editor não grava nada
    no_engine_env.setattr(subprocess, "run", _fake_run([], [0, 0]))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == ue.EXIT_TESTS_FAILED
    assert "relatório ausente" in capsys.readouterr().err


def test_main_test_propagates_editor_exit_code(no_engine_env, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    no_engine_env.setattr(subprocess, "run", _fake_run([], [0, 255], lambda: _report(report, "Fail")))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == 255


def test_main_test_stops_on_build_failure_with_sdk_hint(no_engine_env, capsys, tmp_path: Path):
    engine, report = _fake_engine(tmp_path, "Win64"), tmp_path / "report"
    report.mkdir()
    (report / "UBT.log").write_text(
        "Platform Win64 is not a valid platform to build. SDK validation failed:\n  Sdk: not found.\n", encoding="utf-8"
    )
    no_engine_env.setattr(ue, "host_platform", lambda: "Win64")
    calls: list = []
    no_engine_env.setattr(subprocess, "run", _fake_run(calls, [6]))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == 6
    assert len(calls) == 1 and "SampleCatalogEditor" in calls[0][0]
    err = capsys.readouterr().err
    assert "Visual Studio 2022 17.14" in err and "Windows SDK" in err and "build falhou (exit 6)" in err


def test_main_build_failure_without_sdk_message_has_no_sdk_hint(no_engine_env, capsys, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    no_engine_env.setattr(subprocess, "run", _fake_run([], [6]))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == 6
    assert "SDK" not in capsys.readouterr().err


def test_main_build_plugin_sends_uat_logs_to_report_dir(no_engine_env, tmp_path: Path):
    engine, report = _fake_engine(tmp_path), tmp_path / "report"
    calls: list = []
    no_engine_env.setattr(subprocess, "run", _fake_run(calls, [3]))
    assert ue.main(["build-plugin", "--engine", str(engine), "--report-dir", str(report)]) == 3
    cmd, env = calls[0]
    assert cmd == ue.build_plugin_command(engine, "Linux")
    assert env["uebp_LogFolder"] == str(report / "UAT")


def test_main_windows_passes_ue_style_command_line(no_engine_env, tmp_path: Path):
    engine, report = _fake_engine(tmp_path, "Win64"), tmp_path / "report"
    no_engine_env.setattr(ue, "host_platform", lambda: "Win64")
    calls: list = []
    no_engine_env.setattr(subprocess, "run", _fake_run(calls, [0, 0], lambda: _report(report, "Success")))
    assert ue.main(["test", "--engine", str(engine), "--report-dir", str(report)]) == 0
    assert all(isinstance(cmd, str) for cmd, _ in calls)
    assert '-ExecCmds="Automation RunTests CatalogAnchor; Quit"' in calls[1][0]
    assert '-TestExit="Automation Test Queue Empty"' in calls[1][0]


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


@pytest.mark.parametrize("payload", ["[]", '{"tests": 5}', '{"tests": [1, "x"], "failed": "2"}', "null"])
def test_summarize_report_tolerates_unexpected_json_shapes(tmp_path: Path, payload: str):
    (tmp_path / "index.json").write_text(payload, encoding="utf-8")
    ok, summary = ue.summarize_report(tmp_path)  # sem AttributeError/ValueError
    assert not ok and summary
