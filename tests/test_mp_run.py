"""tools/mp_run.py: parsing dos logs do plugin, montagem do roteiro e predicados (sem rodar a engine)."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import mp_run  # noqa: E402

PREFIX = "[2026.10.18-10.00.00:123][ 45]LogCatalogAnchor: Display: "
PAWN = (
    "[Dump] NetMode=Client Actor=Char_0 Health=100/100 "
    "Dead=0 Items=13 Locally=1 Authority=0 Stacks=[Item_Gem x10 Item_Potion x3] "
    "Loc=800,-340,88 Target=Chest_0 Focused=1 Holding=0 "
    "Cd=[Catalog.Ability.Dash:0.85 Catalog.Ability.Melee:0.00] Snap=focus"
)
CHEST = "[Dump] NetMode=Client Interactable=Chest_0 Prompt=[Baú vazio] Blocked=1 Duration=1.5 Snap=hold_full"


def test_parse_line_fields_and_brackets():
    rec = mp_run.parse_line(PREFIX + PAWN)
    assert rec["kind"] == "Dump" and rec["f"]["Stacks"] == "Item_Gem x10 Item_Potion x3"
    assert rec["f"]["Target"] == "Chest_0" and mp_run.loc(rec) == (800.0, -340.0, 88.0)
    assert mp_run.cd(rec, "Dash") == 0.85 and mp_run.cd(rec, "Nada") == -1.0
    assert mp_run.parse_line(PREFIX + CHEST)["f"]["Prompt"] == "Baú vazio"


def test_parse_debug_words_and_ignores_noise():
    rec = mp_run.parse_line(PREFIX + "[Debug] Hold Begin NetMode=Client pawn=Char_0 alvo=Chest_0 resultado=1")
    assert rec["words"] == ["Hold", "Begin"] and rec["f"]["resultado"] == "1"
    assert mp_run.parse_line("LogNet: Warning: algo qualquer") is None
    assert mp_run.parse_line(PREFIX + "[Dump] NetMode=Client Atores=4 SaveParticipants=0 Snap=")["f"]["Snap"] == ""


def test_schedule_args_roles_and_absolute_time():
    script = [(10, "D", "x"), (12, "S", "catalog.A"), (14, "C", "catalog.B 1")]
    srv = mp_run.schedule_args(script, "S", 1000)
    assert srv == "catalog.Debug.Schedule 1010.00 catalog.Debug.Dump x,catalog.Debug.Schedule 1012.00 catalog.A"
    assert mp_run.schedule_args(script, "C", 1000).endswith("catalog.Debug.Schedule 1014.00 catalog.B 1")


def test_focus_predicate_distinguishes_owner_and_other():
    owner = mp_run.parse_log(PREFIX + PAWN)
    other = mp_run.parse_log(
        PREFIX + PAWN.replace("Locally=1", "Locally=0").replace("Target=Chest_0 Focused=1", "Target=None Focused=0")
    )
    srv = mp_run.parse_log(PREFIX + PAWN.replace("Locally=1", "Locally=0").replace("Focused=1", "Focused=0"))
    results = mp_run.evaluate("03", {"srv": srv, "cli1": owner, "cli2": other}, {})
    assert [ok for _, ok, _ in results] == [True, True, True]
    leaked = mp_run.evaluate("03", {"srv": srv, "cli1": owner, "cli2": owner}, {})
    assert not all(ok for _, ok, _ in leaked)


def test_schedule_args_listen_host_runs_server_and_local_roles():
    script = [(1, "S", "catalog.A"), (2, "H", "catalog.B"), (3, "C", "catalog.C")]
    host = mp_run.schedule_args(script, "SH", 1000).split(",")
    assert [c.split()[-1] for c in host] == ["catalog.A", "catalog.B"]
    assert mp_run.schedule_args(script, "C", 1000).endswith("catalog.C")


HUD = (
    "[Debug] Hud Widget=1 Created=0 Health=[Vida 60/100] Prompt=[(E) Abrir baú] Bar=0.450 BarVisible=1 "
    "Inventory=[Inventário (0/16) |   (vazio)] Message=[] CompProgress=0.450 CompHolding=1 "
    "MenuOpen=0 IgnoreInput=0 Cursor=0 Snap=k_hold3"
)


def test_parse_hud_line_without_nested_brackets():
    rec = mp_run.hud(mp_run.parse_log(PREFIX + HUD), "k_hold3")
    assert rec["f"]["Prompt"] == "(E) Abrir baú" and mp_run.num(rec, "Bar") == 0.45
    assert rec["f"]["Inventory"] == "Inventário (0/16) |   (vazio)" and rec["f"]["Message"] == ""
    assert mp_run.hud(mp_run.parse_log(PREFIX + HUD), "outro") is None


def test_keys_case_09_blocked_without_widget_and_bar_predicate():
    blocked = mp_run.evaluate(
        "09",
        {"srv": mp_run.parse_log(PREFIX + HUD.replace("Widget=1", "Widget=0").replace("k_hold3", "k_base"))},
        {"srv": ""},
    )
    assert len(blocked) == 1 and not blocked[0][1] and blocked[0][0].startswith("09 bloqueado")
    no_log = mp_run.evaluate("09", {"srv": []}, {"srv": ""})
    assert not no_log[0][1]


def test_health_and_num_helpers():
    rec = mp_run.parse_line(PREFIX + PAWN)
    assert mp_run.hp(rec) == 100.0 and mp_run.hp(None) == -1.0 and mp_run.num(rec, "Nada") == -1.0
    assert mp_run.local([rec], "x") is None  # rótulo diferente
    assert mp_run.local([rec], "focus") is rec


class _Proc:
    def __init__(self, pid: int = 4242):
        self.pid, self.killed, self.waited = pid, False, False

    def kill(self):
        self.killed = True

    def wait(self, timeout=None):
        self.waited = True


@pytest.mark.parametrize(("platform", "windows"), [("Win64", True), ("Mac", False), ("Linux", False)])
def test_spawn_uses_command_line_on_windows_and_argv_session_on_posix(monkeypatch, platform: str, windows: bool):
    seen = {}
    monkeypatch.setattr(mp_run.subprocess, "Popen", lambda cmd, **kw: seen.update(cmd=cmd, kw=kw))
    mp_run.spawn(["/ue/UnrealEditor", "-ExecCmds=a b"], platform)
    assert isinstance(seen["cmd"], str) is windows
    assert ("start_new_session" in seen["kw"]) is (not windows)


def test_kill_windows_terminates_own_processes_without_taskkill(monkeypatch):
    runs: list = []
    monkeypatch.setattr(mp_run.subprocess, "run", lambda cmd, **kw: runs.append(cmd))
    p = _Proc()
    mp_run.kill([p], platform="Win64")
    assert p.killed and p.waited and not runs


@pytest.mark.parametrize("platform", ["Mac", "Linux"])
def test_kill_posix_kills_process_group_and_pkill_only_with_kill_all(monkeypatch, platform: str):
    runs, groups = [], []
    monkeypatch.setattr(mp_run.subprocess, "run", lambda cmd, **kw: runs.append(cmd))
    monkeypatch.setattr(mp_run.signal, "SIGKILL", 9, raising=False)  # Windows não tem SIGKILL
    monkeypatch.setattr(mp_run.os, "getpgid", lambda pid: pid + 1, raising=False)
    monkeypatch.setattr(mp_run.os, "killpg", lambda pg, sig: groups.append((pg, sig)), raising=False)
    p = _Proc(100)
    mp_run.kill([p], platform=platform)
    assert groups == [(101, 9)] and not runs and p.waited
    mp_run.kill([p], kill_all=True, platform=platform)
    assert runs == [["pkill", "-KILL", "-x", "UnrealEditor"]]


def test_kill_kill_all_on_windows_uses_image_name(monkeypatch):
    runs: list = []
    monkeypatch.setattr(mp_run.subprocess, "run", lambda cmd, **kw: runs.append(cmd))
    mp_run.kill([], kill_all=True, platform="Win64")
    assert runs == [["taskkill", "/F", "/IM", "UnrealEditor-Cmd.exe"]]


def test_kill_posix_ignores_already_dead_process(monkeypatch):
    def gone(pid):
        raise ProcessLookupError

    monkeypatch.setattr(mp_run.os, "getpgid", gone, raising=False)
    monkeypatch.setattr(mp_run.os, "killpg", lambda pg, sig: None, raising=False)
    monkeypatch.setattr(mp_run.signal, "SIGKILL", 9, raising=False)
    p = _Proc()
    mp_run.kill([p], platform="Linux")
    assert p.waited
