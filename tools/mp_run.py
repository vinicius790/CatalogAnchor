#!/usr/bin/env python3
r"""Driver dos testes multiplayer headless (docs/MULTIPLAYER_TESTS.md: MP-03, MP-04, MP-06, MP-07).

Sobe servidor dedicado + 2 clientes (03/04/06) ou um jogo standalone (07), cada processo com um roteiro de
`catalog.Debug.Schedule <s> <cmd>` em -ExecCmds, espera, mata só os processos que ele criou, lê os logs
(`[Dump] ... Snap=<rótulo>` e `[Debug] ...`) e imprime PASS/FAIL por predicado com a linha de log como evidência.

  py -3 tools/mp_run.py --project SampleProject/SampleCatalog.uproject --case all [--engine <raiz>] [--out <pasta>]

Engine: --engine > env UE_ROOT > descoberta do tools/ue.py. Saída ≠ 0 se algum predicado falhar.
Comandos de console usados só existem fora de Shipping (CatalogDebugCommands.cpp).
"""

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ue  # noqa: E402

FIELD = re.compile(r"(\w+)=(\[[^\]]*\]|\S*)")
LINE = re.compile(r"LogCatalogAnchor: (?:\w+: )?\[(Dump|Debug)\] (.*)$")
Rec = dict  # {"kind", "words", "f" (campos), "raw"}

# Roteiro (segundos desde T=0, um instante absoluto comum): (t, quem, comando). "D" = Dump com rótulo em todos.
DEDICATED: list[tuple[float, str, str]] = [
    (0, "D", "base"),
    (1, "C", "catalog.Debug.Ability Dash"),
    (1.3, "C", "catalog.Debug.Ability Dash"),
    (2, "D", "dash"),
    (6, "S", "catalog.Debug.Aim ChestActor_0 0"),
    (13, "D", "focus"),
    (14, "C", "catalog.Debug.Hold Begin"),
    (15, "C", "catalog.Debug.Hold End"),
    (18, "D", "hold_early"),
    (19, "C", "catalog.Debug.Hold Begin"),
    (23, "C", "catalog.Debug.Hold End"),
    (26, "D", "hold_full"),
    (28, "S", "catalog.Debug.Aim ChestActor_1 0"),
    (34, "C", "catalog.Debug.Hold Begin"),
    (37, "C", "catalog.Debug.Hold End"),
    (40, "D", "locked"),
    (42, "S", "catalog.Debug.Aim Dummy 0"),
    (48, "D", "melee_before"),
    (49, "C", "catalog.Debug.Ability Melee"),
    (52, "D", "melee_after"),
]
STANDALONE: list[tuple[float, str, str]] = [
    (0, "S", "catalog.Debug.GiveItem Gem 5"),
    (0.2, "S", "catalog.Debug.Hurt 30"),
    (0.4, "S", "catalog.Debug.Aim ChestActor_0 0"),
    (4, "D", "saved"),
    (5, "S", "catalog.Save.Now MPT"),
    (8, "S", "catalog.Debug.GiveItem Potion 4"),
    (8.2, "S", "catalog.Debug.Hurt 20"),
    (8.4, "S", "catalog.Debug.Aim Dummy 0"),
    (12, "D", "changed"),
    (13, "S", "catalog.Save.Load MPT"),
    (19, "D", "loaded"),
]


def parse_line(line: str) -> Rec | None:
    m = LINE.search(line)
    if not m:
        return None
    body = m.group(2)
    first = FIELD.search(body)
    words = (body[: first.start()] if first else body).split()
    fields = {k: v.strip("[]") if v.startswith("[") else v for k, v in FIELD.findall(body)}
    return {"kind": m.group(1), "words": words, "f": fields, "raw": line.strip()[-240:]}


def parse_log(text: str) -> list[Rec]:
    return [r for r in map(parse_line, text.splitlines()) if r]


def snap(recs: list[Rec], label: str) -> list[Rec]:
    return [r for r in recs if r["kind"] == "Dump" and r["f"].get("Snap") == label]


def actors(recs: list[Rec], label: str) -> list[Rec]:
    return [r for r in snap(recs, label) if "Actor" in r["f"]]


def pawns(recs: list[Rec], label: str) -> list[Rec]:
    return [r for r in actors(recs, label) if "Target" in r["f"]]


def find(recs: list[Rec], label: str, key: str, name: str) -> Rec | None:
    """Linha do snapshot cujo campo `key` (Actor/Interactable) contém `name`."""
    return next((r for r in snap(recs, label) if name in r["f"].get(key, "\0")), None)


def loc(r: Rec) -> tuple[float, ...]:
    return tuple(float(v) for v in r["f"]["Loc"].split(","))


def cd(r: Rec, ability: str) -> float:
    m = re.search(rf"Ability\.{ability}:([\d.]+)", r["f"].get("Cd", ""))
    return float(m.group(1)) if m else -1.0


def aimed(recs: list[Rec], needle: str) -> str:
    """Nome do ator-alvo do último `[Debug] Aim ... alvo=<nome>` cujo alvo contém `needle`."""
    names = [r["f"]["alvo"] for r in recs if r["words"][:1] == ["Aim"] and needle in r["f"].get("alvo", "")]
    return names[-1] if names else "\0"


def check(name: str, ok: bool, evidence: str | None) -> tuple[str, bool, str]:
    return name, bool(ok), evidence or "(sem linha de log correspondente)"


def evaluate(case: str, logs: dict[str, list[Rec]], raw: dict[str, str]) -> list[tuple[str, bool, str]]:
    """Predicados de um caso. `logs`: srv/cli1/cli2 (dedicado) ou srv (standalone)."""
    out: list[tuple[str, bool, str]] = []
    srv = logs["srv"]
    clis = [logs[k] for k in ("cli1", "cli2") if k in logs]
    if case == "03":
        sp = [r for r in pawns(srv, "focus") if "Chest" in r["f"]["Target"]]
        out.append(
            check(
                "03 servidor: exatamente 1 pawn com Target=baú, Focused=0 (dedicado não anuncia)",
                len(sp) == 1 and sp[0]["f"]["Focused"] == "0",
                sp[0]["raw"] if sp else None,
            )
        )
        owners = [
            (c, r) for c in clis for r in pawns(c, "focus") if r["f"]["Locally"] == "1" and "Chest" in r["f"]["Target"]
        ]
        out.append(
            check(
                "03 cliente dono: Target=baú e Focused=1 no pawn local",
                len(owners) == 1 and owners[0][1]["f"]["Focused"] == "1",
                owners[0][1]["raw"] if owners else None,
            )
        )
        others = [c for c in clis if not owners or c is not owners[0][0]]
        leak = [r for c in others for r in pawns(c, "focus") if r["f"]["Target"] != "None" or r["f"]["Focused"] != "0"]
        seen = [r for c in others for r in pawns(c, "focus")]
        out.append(
            check(
                "03 outro cliente: nenhum pawn com Target/foco (OwnerOnly)",
                bool(seen) and not leak,
                (leak or seen or [None])[0] and (leak or seen)[0]["raw"],
            )
        )
    elif case == "04":
        chest = aimed(srv, "ChestActor_0")
        for label in ("hold_early", "hold_full"):
            for key, c in (("srv", srv), ("cli1", logs.get("cli1", [])), ("cli2", logs.get("cli2", []))):
                # Nomes de atores replicados diferem entre máquinas: nos clientes o baú se acha pelo prompt.
                chests = [x for x in snap(c, label) if "Chest" in x["f"].get("Interactable", "")]
                r = find(c, label, "Interactable", chest) if key == "srv" else None
                want = "Abrir" if label == "hold_early" else "vazio"
                if key != "srv" and not (label == "hold_full" and any("Abrir" in x["f"]["Prompt"] for x in chests)):
                    r = next((x for x in chests if want in x["f"]["Prompt"]), None)
                blocked = "0" if label == "hold_early" else "1"
                out.append(
                    check(
                        f"04 {label} {key}: baú {chest} {'inalterado' if blocked == '0' else 'aberto'}",
                        bool(r) and want in r["f"]["Prompt"] and r["f"]["Blocked"] == blocked,
                        r and r["raw"],
                    )
                )
        items = {k: [r for r in pawns(c, "hold_full") if r["f"]["Items"] != "0"] for k, c in logs.items()}
        out.append(
            check(
                "04 servidor: loot (13 itens) no inventário de 1 pawn",
                len(items["srv"]) == 1 and items["srv"][0]["f"]["Items"] == "13",
                items["srv"][0]["raw"] if items["srv"] else None,
            )
        )
        mine = [r for c in clis for r in pawns(c, "hold_full") if r["f"]["Locally"] == "1" and r["f"]["Items"] == "13"]
        out.append(
            check(
                "04 cliente dono vê o loot; o outro cliente não (OwnerOnly)",
                len(mine) == 1 and sum(len(items[k]) for k in ("cli1", "cli2")) == 1,
                mine[0]["raw"] if mine else None,
            )
        )
        lock = find(srv, "locked", "Interactable", "ChestActor_1")
        kept = [r for r in pawns(srv, "locked") if r["f"]["Items"] == "13"]
        out.append(
            check(
                "04 baú trancado sem chave: Trancado/Blocked=1 e o inventário não muda (13)",
                bool(lock) and "Trancado" in lock["f"]["Prompt"] and lock["f"]["Blocked"] == "1" and len(kept) == 1,
                lock and lock["raw"],
            )
        )
        early = [r for r in pawns(srv, "hold_early") if r["f"]["Items"] != "0"]
        out.append(
            check(
                "04 soltar antes de 1,5 s: inventário vazio no servidor",
                not early and bool(pawns(srv, "hold_early")),
                (pawns(srv, "hold_early") or [None])[0] and pawns(srv, "hold_early")[0]["raw"],
            )
        )
        begins = [r for c in clis for r in c if r["words"][:2] == ["Hold", "Begin"] and r["f"].get("resultado") == "1"]
        out.append(
            check(
                "04 cliente chamou BeginInteract com alvo replicado (Server RPC)",
                len(begins) >= 2,
                begins and begins[0]["raw"],
            )
        )
    elif case == "06":
        for i, c in enumerate(clis, 1):
            mine = [r for r in pawns(c, "dash") if r["f"]["Locally"] == "1"]
            alien = [r for r in pawns(c, "dash") if r["f"]["Locally"] == "0"]
            out.append(
                check(
                    f"06 cli{i}: cooldown do Dash no pawn local > 0, 0 no alheio (OwnerOnly)",
                    len(mine) == 1 and cd(mine[0], "Dash") > 0 and all(cd(a, "Dash") == 0 for a in alien),
                    mine and mine[0]["raw"],
                )
            )
            results = [r["f"]["resultado"] for r in c if r["words"][:1] == ["Ability"] and "Dash" in r["words"][1]]
            out.append(
                check(f"06 cli{i}: 1ª ativação ok, 2ª recusada", results[:2] == ["1", "0"], f"resultado={results}")
            )
        moved = [
            (a, b)
            for a in pawns(srv, "base")
            for b in pawns(srv, "dash")
            if a["f"]["Actor"] == b["f"]["Actor"] and math.dist(loc(a), loc(b)) > 100
        ]
        out.append(check("06 servidor: Dash moveu os 2 pawns > 100 cm", len(moved) == 2, moved and moved[0][1]["raw"]))
        fail = re.search(r".*Habilidade 'Catalog\.Ability\.Dash' falhou[^\n]*Cooldown[^\n]*", raw["srv"])
        out.append(check("06 servidor: 2ª ativação falhou por Cooldown", bool(fail), fail and fail.group(0)[-240:]))
        dummy = aimed(srv, "Dummy")
        for key, c in (("srv", srv), ("cli1", logs.get("cli1", [])), ("cli2", logs.get("cli2", []))):
            b, a = find(c, "melee_before", "Actor", dummy), find(c, "melee_after", "Actor", dummy)
            out.append(
                check(
                    f"06 {key}: boneco {dummy} vida 75 -> 50 (Melee 25)",
                    bool(a and b) and b["f"]["Health"] == "75/75" and a["f"]["Health"] == "50/75",
                    a and a["raw"],
                )
            )
    elif case == "07":
        s, c, ld = (pawns(srv, x) for x in ("saved", "changed", "loaded"))
        if not (s and c and ld):
            out.append(check("07 snapshots saved/changed/loaded presentes", False, None))
            return out
        s, c, ld = s[0], c[0], ld[0]
        out.append(
            check(
                "07 estado mudou depois do save (vida, itens, posição)",
                all(s["f"][k] != c["f"][k] for k in ("Health", "Items", "Loc")),
                c["raw"],
            )
        )
        for key in ("Health", "Items", "Loc"):
            out.append(
                check(
                    f"07 após Save.Load: {key} volta ao salvo ({s['f'][key]})", ld["f"][key] == s["f"][key], ld["raw"]
                )
            )
    return out


def schedule_args(script: list[tuple[float, str, str]], role: str, t0: float) -> str:
    """-ExecCmds para o papel (S servidor/standalone, C clientes); D = Dump rotulado em todos."""
    cmds = []
    for t, who, cmd in script:
        if who == "D":
            cmd = f"catalog.Debug.Dump {cmd}"
        elif who != role:
            continue
        cmds.append(f"catalog.Debug.Schedule {t0 + t:.2f} {cmd}")  # instante absoluto (relógio comum)
    return ",".join(cmds)


def launch(cmd: Path, project: Path, url: str, mode: list[str], log: Path, execcmds: str) -> subprocess.Popen:
    argv = [
        str(cmd),
        str(project),
        url,
        *mode,
        "-log",
        f"-abslog={log}",
        "-nullrhi",
        "-unattended",
        "-nosplash",
        "-NoSound",
        "-nopause",
        "-Multiprocess",
        "-forcelogflush",
        "-LogCmds=LogCatalogAnchor Verbose",
        f"-ExecCmds={execcmds}",
    ]
    return subprocess.Popen(ue.windows_command_line(argv), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)  # noqa: S603


def kill(procs: list[subprocess.Popen], kill_all: bool = False) -> None:
    # Só os PIDs próprios (o agente principal pode ter outro UnrealEditor-Cmd rodando); /IM só com --kill-all.
    # No Git Bash, MSYS2_ARG_CONV_EXCL="*" evita que "/F" vire caminho.
    targets = [["/PID", str(p.pid)] for p in procs] if not kill_all else [["/IM", "UnrealEditor-Cmd.exe"]]
    for t in targets:
        subprocess.run(["taskkill", "/F", "/T", *t], capture_output=True, check=False)  # noqa: S603, S607
    for p in procs:
        p.wait(timeout=30)


def run_session(
    args: argparse.Namespace, cmd: Path, name: str, script: list, n_clients: int, lead: float, out: Path
) -> dict[str, str]:
    port = args.port
    procs: list[subprocess.Popen] = []
    logs = {"srv": out / f"{name}_srv.log"}
    try:
        t0 = time.monotonic()
        t_zero = time.time() + lead  # T=0 do roteiro: igual para todos os processos
        if n_clients:
            procs.append(
                launch(
                    cmd,
                    args.project,
                    "/Engine/Maps/Entry",
                    ["-server", f"-port={port}"],
                    logs["srv"],
                    schedule_args(script, "S", t_zero),
                )
            )
            while "listening on port" not in (
                logs["srv"].read_text("utf-8", "replace") if logs["srv"].exists() else ""
            ):
                if time.monotonic() - t0 > 120:
                    raise RuntimeError("servidor não abriu a porta em 120 s")
                time.sleep(1)
            for i in range(1, n_clients + 1):
                logs[f"cli{i}"] = out / f"{name}_cli{i}.log"
                procs.append(
                    launch(
                        cmd,
                        args.project,
                        f"127.0.0.1:{port}",
                        ["-game"],
                        logs[f"cli{i}"],
                        schedule_args(script, "C", t_zero),
                    )
                )
        else:
            procs.append(
                launch(
                    cmd, args.project, "/Engine/Maps/Entry", ["-game"], logs["srv"], schedule_args(script, "S", t_zero)
                )
            )
        time.sleep(max(1.0, t_zero + script[-1][0] + 6 - time.time()))
    finally:
        kill(procs, args.kill_all)
    return {k: p.read_text("utf-8", "replace") if p.exists() else "" for k, p in logs.items()}


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):  # consoles cp1252 não imprimem os acentos do --help
        stream.reconfigure(errors="backslashreplace")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project", type=Path, required=True, help="caminho do .uproject")
    ap.add_argument("--engine", help="raiz da engine (default: UE_ROOT / descoberta do ue.py)")
    ap.add_argument("--case", default="all", choices=["03", "04", "06", "07", "all"])
    ap.add_argument("--out", type=Path, help="pasta dos logs (default: temporária)")
    ap.add_argument("--port", type=int, default=17777)
    ap.add_argument("--lead", type=float, default=75, help="s entre subir o servidor e o T=0 do roteiro (padrão 75)")
    ap.add_argument(
        "--kill-all", action="store_true", help="taskkill /IM UnrealEditor-Cmd.exe em vez de só os PIDs próprios"
    )
    args = ap.parse_args(argv)
    engine = ue.find_engine(args.engine)
    if not engine:
        print("mp_run: engine não encontrada (--engine ou UE_ROOT)", file=sys.stderr)
        return 2
    cmd = ue.editor_cmd_path(engine, ue.host_platform())
    out = args.out or Path(tempfile.mkdtemp(prefix="mp_run_"))
    out.mkdir(parents=True, exist_ok=True)
    cases = ["03", "04", "06", "07"] if args.case == "all" else [args.case]
    results: list[tuple[str, bool, str]] = []
    try:
        if set(cases) & {"03", "04", "06"}:
            raw = run_session(args, cmd, "dedicated", DEDICATED, 2, args.lead, out)
            parsed = {k: parse_log(v) for k, v in raw.items()}
            for c in (c for c in cases if c != "07"):
                results += evaluate(c, parsed, raw)
        if "07" in cases:
            raw = run_session(args, cmd, "standalone", STANDALONE, 0, 40, out)
            results += evaluate("07", {k: parse_log(v) for k, v in raw.items()}, raw)
    except (RuntimeError, OSError, subprocess.SubprocessError) as exc:
        print(f"mp_run: erro de execução: {exc}", file=sys.stderr)
        return 2
    for name, ok, evidence in results:
        print(f"{'PASS' if ok else 'FAIL'} {name}\n     {evidence}")
    failed = sum(not ok for _, ok, _ in results)
    print(f"mp_run: {len(results) - failed}/{len(results)} predicados passaram (logs em {out})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
