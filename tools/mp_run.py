#!/usr/bin/env python3
r"""Driver dos testes multiplayer headless (docs/MULTIPLAYER_TESTS.md: MP-03, MP-04, MP-06, MP-07 e teclas/HUD 08/09).

Sobe servidor dedicado + 2 clientes (03/04/06; `--mode listen`: host com jogador local + 1 cliente) ou um standalone
(07 = save/load por comando; 08/09 = teclas reais (Enhanced Input) e estado do HUD UMG), cada processo com um roteiro de
`catalog.Debug.Schedule <s> <cmd>` em -ExecCmds, espera, mata só os processos que ele criou, lê os logs
(`[Dump] ... Snap=<rótulo>` e `[Debug] ...`) e imprime PASS/FAIL por predicado com a linha de log como evidência.

  py -3 tools/mp_run.py --project SampleProject/SampleCatalog.uproject --case all [--mode dedicated|listen]

Engine: --engine > env UE_ROOT > descoberta do tools/ue.py. Saída ≠ 0 se algum predicado falhar.
Comandos de console usados só existem fora de Shipping (CatalogDebugCommands.cpp, SampleDebugCommands.cpp).
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
    (54, "S", "catalog.Debug.GiveItem Key 1"),
    (55, "S", "catalog.Debug.Aim ChestActor_1 0"),
    (61, "D", "key_ready"),
    (62, "C", "catalog.Debug.Hold Begin"),
    (65, "C", "catalog.Debug.Hold End"),
    (68, "D", "key_open"),
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
# Teclas reais (caso 08) + HUD (caso 09), standalone: catalog.Debug.Key passa pelo PlayerController/Enhanced Input.
KEYS: list[tuple[float, str, str]] = [
    (0, "S", "catalog.Debug.Hurt 40"),
    (0.5, "S", "catalog.Debug.Aim ChestActor_0 0"),
    (4, "D", "k_base"),
    (4.1, "S", "sample.Debug.Hud k_base"),
    (5, "S", "catalog.Debug.Key E Press"),
    (5.4, "S", "sample.Debug.Hud k_hold1"),
    (5.9, "S", "sample.Debug.Hud k_hold2"),
    (6, "S", "catalog.Debug.Key E Release"),
    (6.6, "S", "sample.Debug.Hud k_early_rel"),
    (7, "D", "k_early"),
    (8, "S", "catalog.Debug.Key E Press"),
    (8.6, "S", "sample.Debug.Hud k_hold3"),
    (9.2, "S", "sample.Debug.Hud k_hold4"),
    (11, "S", "sample.Debug.Hud k_full_held"),
    (11.1, "D", "k_full"),
    (11.5, "S", "catalog.Debug.Key E Release"),
    (12.5, "S", "sample.Debug.Hud k_full_rel"),
    (13, "S", "catalog.Debug.Key Q Tap"),
    (15, "D", "k_potion"),
    (15.1, "S", "sample.Debug.Hud k_potion"),
    (16, "S", "catalog.Debug.Key G Tap"),
    (18, "D", "k_drop"),
    (19, "S", "catalog.Debug.Key LeftShift Tap"),
    (19.5, "D", "k_dash"),
    (22, "S", "catalog.Debug.Key F5 Tap"),
    (23.5, "D", "k_saved"),
    (24, "S", "catalog.Debug.GiveItem Gem 3"),
    (24.2, "S", "catalog.Debug.Hurt 20"),
    (25.5, "D", "k_changed"),
    (26, "S", "catalog.Debug.Key F9 Tap"),
    (28, "D", "k_loaded"),
    (29, "S", "catalog.Debug.Key Tab Tap"),
    (30, "S", "sample.Debug.Hud k_menu"),
    (31, "S", "catalog.Debug.Key Tab Tap"),
    (32, "S", "sample.Debug.Hud k_menu_closed"),
    (33, "S", "catalog.Debug.Aim Dummy 0"),
    (37, "D", "k_melee_before"),
    (38, "S", "catalog.Debug.Key LeftMouseButton Tap"),
    (40, "D", "k_melee_after"),
]
# Listen server: S = comandos de servidor no host (Aim/GiveItem, jogador 0 = host, 1 = cliente), H = pawn local do host.
LISTEN: list[tuple[float, str, str]] = [
    (0, "D", "base"),
    (1, "H", "catalog.Debug.Ability Dash"),
    (1, "C", "catalog.Debug.Ability Dash"),
    (1.3, "H", "catalog.Debug.Ability Dash"),
    (1.3, "C", "catalog.Debug.Ability Dash"),
    (2, "D", "dash"),
    (6, "S", "catalog.Debug.Aim ChestActor_0 0"),
    (6.2, "S", "catalog.Debug.Aim ChestActor_1 1"),
    (13, "D", "focus"),
    (14, "H", "catalog.Debug.Hold Begin"),
    (14, "C", "catalog.Debug.Hold Begin"),
    (15, "H", "catalog.Debug.Hold End"),
    (15, "C", "catalog.Debug.Hold End"),
    (18, "D", "hold_early"),
    (19, "S", "catalog.Debug.GiveItem Key 1 1"),
    (21, "H", "catalog.Debug.Hold Begin"),
    (21, "C", "catalog.Debug.Hold Begin"),
    (25, "H", "catalog.Debug.Hold End"),
    (25, "C", "catalog.Debug.Hold End"),
    (28, "D", "hold_full"),
    (30, "S", "catalog.Debug.Aim Dummy 1"),
    (36, "D", "melee_before"),
    (37, "C", "catalog.Debug.Ability Melee"),
    (40, "D", "melee_after"),
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


def g(r: Rec | None, key: str) -> str | None:
    return r["f"].get(key) if r else None


def num(r: Rec | None, key: str, default: float = -1.0) -> float:
    try:
        return float(g(r, key))  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return default


def hp(r: Rec | None) -> float:
    """Vida atual do campo Health=`atual/máx` do Dump (-1 se ausente)."""
    return num({"f": {"h": (g(r, "Health") or "").split("/")[0]}}, "h")


def local(recs: list[Rec], label: str) -> Rec | None:
    return next((r for r in pawns(recs, label) if r["f"]["Locally"] == "1"), None)


def hud(recs: list[Rec], label: str) -> Rec | None:
    """`[Debug] Hud ... Snap=<rótulo>` (sample.Debug.Hud)."""
    return next((r for r in recs if r["words"][:1] == ["Hud"] and r["f"].get("Snap") == label), None)


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
        ready = find(srv, "key_ready", "Interactable", "ChestActor_1")
        out.append(
            check(
                "04 baú trancado com Item_Key no inventário: prompt vira Abrir e Blocked=0",
                bool(ready) and "Abrir" in ready["f"]["Prompt"] and ready["f"]["Blocked"] == "0",
                ready and ready["raw"],
            )
        )
        opened = find(srv, "key_open", "Interactable", "ChestActor_1")
        owner = [r for r in pawns(srv, "key_open") if r["f"]["Items"] != "0"]
        out.append(
            check(
                "04 hold no baú trancado com a chave: abre ('vazio'), dono com 39 itens (13+1+25; chave não consumida)",
                bool(opened)
                and "vazio" in opened["f"]["Prompt"]
                and len(owner) == 1
                and owner[0]["f"]["Items"] == "39",
                owner[0]["raw"] if owner else (opened and opened["raw"]),
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
    elif case in ("08", "09"):
        out += evaluate_keys(case, srv, raw["srv"])
    return out


def evaluate_keys(case: str, srv: list[Rec], raw: str) -> list[tuple[str, bool, str]]:
    """Casos 08 (teclas -> efeitos no jogo) e 09 (HUD UMG e menu), standalone com `catalog.Debug.Key`."""
    out: list[tuple[str, bool, str]] = []
    names = ("base", "early", "full", "potion", "drop", "dash", "saved", "changed", "loaded")
    p = {n: local(srv, f"k_{n}") for n in names}
    ch = {n: find(srv, f"k_{n}", "Interactable", "ChestActor_0") for n in ("early", "full")}
    if case == "08":
        held = hud(srv, "k_hold1")
        out.append(
            check(
                "08 tecla E (Press): Enhanced Input inicia o hold (Holding=1 no pawn local)",
                num(held, "CompHolding") == 1,
                held and held["raw"],
            )
        )
        out.append(
            check(
                "08 E solto antes de 1,5 s: baú segue 'Abrir baú' (Blocked=0) e inventário vazio",
                bool(ch["early"] and p["early"])
                and "Abrir" in g(ch["early"], "Prompt")
                and g(ch["early"], "Blocked") == "0"
                and g(p["early"], "Items") == "0",
                p["early"] and p["early"]["raw"],
            )
        )
        out.append(
            check(
                "08 E segurado > 1,5 s: baú abre ('vazio', Blocked=1) e o loot (13 itens) entra no inventário",
                bool(ch["full"] and p["full"])
                and "vazio" in g(ch["full"], "Prompt")
                and g(ch["full"], "Blocked") == "1"
                and g(p["full"], "Items") == "13",
                p["full"] and p["full"]["raw"],
            )
        )
        out.append(
            check(
                "08 Q: poção usada (vida +35, itens -1)",
                bool(p["full"] and p["potion"])
                and hp(p["potion"]) - hp(p["full"]) == 35
                and num(p["full"], "Items") - num(p["potion"], "Items") == 1,
                p["potion"] and p["potion"]["raw"],
            )
        )
        out.append(
            check(
                "08 G: solta 1 item (itens -1)",
                bool(p["potion"] and p["drop"]) and num(p["potion"], "Items") - num(p["drop"], "Items") == 1,
                p["drop"] and p["drop"]["raw"],
            )
        )
        out.append(
            check(
                "08 LeftShift: Dash (cooldown > 0 e deslocamento > 50 cm em 0,5 s)",
                bool(p["drop"] and p["dash"])
                and cd(p["dash"], "Dash") > 0
                and math.dist(loc(p["drop"]), loc(p["dash"])) > 50,
                p["dash"] and p["dash"]["raw"],
            )
        )
        saved = re.search(r".*CatalogSave: slot 'Quick' gravado[^\n]*", raw)
        out.append(check("08 F5: grava o slot Quick", bool(saved), saved and saved.group(0)[-240:]))
        out.append(
            check(
                "08 F9: estado muda depois do save e volta ao salvo (vida, itens, posição)",
                all(p[n] for n in ("saved", "changed", "loaded"))
                and all(g(p["saved"], k) != g(p["changed"], k) for k in ("Health", "Items"))
                and all(g(p["loaded"], k) == g(p["saved"], k) for k in ("Health", "Items", "Loc")),
                p["loaded"] and p["loaded"]["raw"],
            )
        )
        dummy = aimed(srv, "Dummy")
        before, after = find(srv, "k_melee_before", "Actor", dummy), find(srv, "k_melee_after", "Actor", dummy)
        out.append(
            check(
                f"08 LeftMouseButton: Melee no boneco {dummy} (vida 75 -> 50)",
                bool(before and after) and g(before, "Health") == "75/75" and g(after, "Health") == "50/75",
                after and after["raw"],
            )
        )
        return out
    hn = (
        "base",
        "hold1",
        "hold2",
        "hold3",
        "hold4",
        "early_rel",
        "full_held",
        "full_rel",
        "potion",
        "menu",
        "menu_closed",
    )
    h = {n: hud(srv, f"k_{n}") for n in hn}
    if not h["base"] or g(h["base"], "Widget") != "1":
        out.append(
            check(
                "09 bloqueado: USampleHUDWidget não existe neste modo (sample.Debug.Hud sem linha ou Widget=0)",
                False,
                h["base"] and h["base"]["raw"],
            )
        )
        return out
    closed = ("MenuOpen", "IgnoreInput", "Cursor")
    ticked = any(g(h[n], "Health") for n in hn if h[n])
    # Verdade do componente (o que a barra desenha): progresso do hold via GetInteractionProgress, sempre disponível.
    prog = [num(h[n], "CompProgress") for n in ("hold1", "hold2", "hold3", "hold4")]
    out.append(
        check(
            "09 progresso do hold (componente) sobe em cada hold (0<p<1) e volta a 0 ao soltar/abrir",
            all(0 < x < 1 for x in prog)
            and prog[0] < prog[1]
            and prog[2] < prog[3]
            and all(
                num(h[n], "CompProgress") == 0 and num(h[n], "CompHolding") == 0 for n in ("early_rel", "full_held")
            ),
            h["hold4"] and h["hold4"]["raw"],
        )
    )
    if not ticked:
        out.append(
            check(
                "09 BLOQUEADO: NativeTick do USampleHUDWidget não roda em -nullrhi (vida/prompt/inventário/barra ficam "
                "vazios com o pawn em 60/100); textos e barra do HUD não verificáveis headless",
                False,
                h["hold4"] and h["hold4"]["raw"],
            )
        )
    else:
        out += hud_text_checks(h)
    out.append(
        check(
            "09 menu de saves fechado no início (MenuOpen=0, IgnoreInput=0, Cursor=0)",
            all(g(h["base"], k) == "0" for k in closed),
            h["base"]["raw"],
        )
    )
    out.append(
        check(
            "09 Tab abre o menu e o modo de input vira UI (MenuOpen=1, IgnoreInput=1, Cursor=1)",
            bool(h["menu"]) and all(g(h["menu"], k) == "1" for k in closed),
            h["menu"] and h["menu"]["raw"],
        )
    )
    out.append(
        check(
            "09 segundo Tab fecha o menu e o input volta ao jogo (MenuOpen=0, IgnoreInput=0, Cursor=0)",
            bool(h["menu_closed"]) and all(g(h["menu_closed"], k) == "0" for k in closed),
            h["menu_closed"] and h["menu_closed"]["raw"],
        )
    )
    return out


def hud_text_checks(h: dict[str, Rec | None]) -> list[tuple[str, bool, str]]:
    """Textos e barra do USampleHUDWidget (precisam do NativeTick do widget)."""
    out: list[tuple[str, bool, str]] = []
    out.append(
        check(
            "09 HUD: texto de vida (Vida 60/100) e inventário vazio no início",
            "Vida 60/100" in g(h["base"], "Health") and "vazio" in g(h["base"], "Inventory"),
            h["base"]["raw"],
        )
    )
    out.append(
        check(
            "09 prompt do baú no HUD: '(E) Abrir baú'",
            "Abrir baú" in (g(h["hold1"], "Prompt") or ""),
            h["hold1"] and h["hold1"]["raw"],
        )
    )
    bars = [num(h[n], "Bar") for n in ("hold2", "hold3", "hold4")]
    out.append(
        check(
            "09 barra de hold durante o hold: visível, 0 < progresso < 1 e crescente",
            all(num(h[n], "BarVisible") == 1 for n in ("hold2", "hold3", "hold4"))
            and all(0 < b < 1 for b in bars)
            and bars == sorted(bars),
            h["hold4"] and h["hold4"]["raw"],
        )
    )
    out.append(
        check(
            "09 barra de hold some ao soltar antes do fim e depois de abrir (BarVisible=0; Bar guarda o último valor)",
            all(num(h[n], "BarVisible") == 0 for n in ("early_rel", "full_held", "full_rel")),
            h["full_held"] and h["full_held"]["raw"],
        )
    )
    out.append(
        check(
            "09 HUD depois de abrir: prompt 'Baú vazio' e inventário listado",
            "Baú vazio" in (g(h["full_rel"], "Prompt") or "")
            and "vazio" not in (g(h["full_rel"], "Inventory") or "vazio"),
            h["full_rel"] and h["full_rel"]["raw"],
        )
    )
    out.append(
        check(
            "09 HUD depois da poção: texto de vida (Vida 95/100)",
            "Vida 95/100" in (g(h["potion"], "Health") or ""),
            h["potion"] and h["potion"]["raw"],
        )
    )
    return out


def evaluate_listen(logs: dict[str, list[Rec]], raw: dict[str, str]) -> list[tuple[str, bool, str]]:
    """Listen server: srv = host (jogador local), cli1 = cliente remoto. Mesmos temas dos casos 03/04/06."""
    out: list[tuple[str, bool, str]] = []
    host, cli = logs["srv"], logs["cli1"]

    def other(recs: list[Rec], label: str) -> Rec | None:
        return next((r for r in pawns(recs, label) if r["f"]["Locally"] == "0"), None)

    hp0, cp0, remote, alien = local(host, "focus"), local(cli, "focus"), other(host, "focus"), other(cli, "focus")
    out.append(
        check(
            "L03 host: pawn local com Target=baú e Focused=1 (listen anuncia foco ao host)",
            bool(hp0) and "ChestActor_0" in g(hp0, "Target") and g(hp0, "Focused") == "1",
            hp0 and hp0["raw"],
        )
    )
    out.append(
        check(
            "L03 host: pawn do cliente remoto tem Target=baú mas Focused=0",
            bool(remote) and "Chest" in g(remote, "Target") and g(remote, "Focused") == "0",
            remote and remote["raw"],
        )
    )
    out.append(
        check(
            "L03 cliente: pawn local com Target=baú e Focused=1",
            bool(cp0) and "Chest" in g(cp0, "Target") and g(cp0, "Focused") == "1",
            cp0 and cp0["raw"],
        )
    )
    out.append(
        check(
            "L03 cliente: pawn do host sem Target/foco (OwnerOnly)",
            bool(alien) and g(alien, "Target") == "None" and g(alien, "Focused") == "0",
            alien and alien["raw"],
        )
    )
    locked = [r for r in snap(cli, "hold_early") if "Trancado" in r["f"].get("Prompt", "")]
    early_items = [r for lg in (host, cli) for r in pawns(lg, "hold_early") if r["f"]["Items"] != "0"]
    chest0 = find(host, "hold_early", "Interactable", "ChestActor_0")
    out.append(
        check(
            "L04 hold de 1 s (host) e trancado sem chave (cliente): sem itens; cliente vê Trancado/Blocked=1",
            bool(locked)
            and locked[0]["f"]["Blocked"] == "1"
            and not early_items
            and bool(chest0)
            and "Abrir" in chest0["f"]["Prompt"],
            locked[0]["raw"] if locked else None,
        )
    )
    h_full, c_on_host = local(host, "hold_full"), other(host, "hold_full")
    c_full, h_on_cli = local(cli, "hold_full"), other(cli, "hold_full")
    chest_host = find(host, "hold_full", "Interactable", "ChestActor_0")
    out.append(
        check(
            "L04 host abre o baú com hold local: 13 itens no host e baú 'vazio'",
            bool(h_full and chest_host) and g(h_full, "Items") == "13" and "vazio" in chest_host["f"]["Prompt"],
            h_full and h_full["raw"],
        )
    )
    out.append(
        check(
            "L04 cliente (Server RPC) abre o trancado com a chave: 26 itens (1 chave + 25 gemas) no host e no cliente",
            g(c_on_host, "Items") == "26" and g(c_full, "Items") == "26",
            c_full and c_full["raw"],
        )
    )
    out.append(
        check(
            "L04 cliente não vê o inventário do host (OwnerOnly: Items=0 no pawn alheio)",
            bool(h_on_cli) and g(h_on_cli, "Items") == "0",
            h_on_cli and h_on_cli["raw"],
        )
    )
    for who, recs in (("host", host), ("cliente", cli)):
        results = [r["f"]["resultado"] for r in recs if r["words"][:1] == ["Ability"] and "Dash" in r["words"][1]]
        mine = local(recs, "dash")
        out.append(
            check(
                f"L06 {who}: Dash 1ª ativação ok, 2ª recusada; cooldown > 0 no pawn local",
                bool(mine) and results[:2] == ["1", "0"] and cd(mine, "Dash") > 0,
                mine and mine["raw"],
            )
        )
    h_alien = other(cli, "dash")
    out.append(
        check(
            "L06 cliente: cooldown do Dash do pawn do host = 0 (OwnerOnly)",
            bool(h_alien) and cd(h_alien, "Dash") == 0,
            h_alien and h_alien["raw"],
        )
    )
    moved = [
        a
        for a in pawns(host, "base")
        for b in pawns(host, "dash")
        if a["f"]["Actor"] == b["f"]["Actor"] and math.dist(loc(a), loc(b)) > 100
    ]
    out.append(check("L06 host: Dash moveu os 2 pawns > 100 cm", len(moved) == 2, moved and moved[0]["raw"]))
    dummy = aimed(host, "Dummy")
    for key, recs in (("host", host), ("cliente", cli)):
        b, a = find(recs, "melee_before", "Actor", dummy), find(recs, "melee_after", "Actor", dummy)
        out.append(
            check(
                f"L06 {key}: boneco {dummy} vida 75 -> 50 (Melee do cliente via Server RPC)",
                bool(a and b) and g(b, "Health") == "75/75" and g(a, "Health") == "50/75",
                a and a["raw"],
            )
        )
    return out


def schedule_args(script: list[tuple[float, str, str]], role: str, t0: float) -> str:
    """-ExecCmds do papel (S servidor/standalone, C clientes, H pawn local do host; "SH" = host listen); D = Dump."""
    cmds = []
    for t, who, cmd in script:
        if who == "D":
            cmd = f"catalog.Debug.Dump {cmd}"
        elif who not in role:
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
    args: argparse.Namespace,
    cmd: Path,
    name: str,
    script: list,
    n_clients: int,
    lead: float,
    out: Path,
    listen: bool = False,
) -> dict[str, str]:
    port = args.port
    procs: list[subprocess.Popen] = []
    logs = {"srv": out / f"{name}_srv.log"}
    for stale in out.glob(f"{name}_*.log"):  # log de uma execução anterior enganaria a espera pelo rótulo final
        stale.unlink()
    try:
        t0 = time.monotonic()
        t_zero = time.time() + lead  # T=0 do roteiro: igual para todos os processos
        if n_clients:
            procs.append(
                launch(
                    cmd,
                    args.project,
                    "/Engine/Maps/Entry?listen" if listen else "/Engine/Maps/Entry",
                    ["-game" if listen else "-server", f"-port={port}"],
                    logs["srv"],
                    schedule_args(script, "SH" if listen else "S", t_zero),
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
            # Um processo só: o roteiro é relativo ao -ExecCmds (roda depois de carregar o mapa); o boot da engine varia
            # com a carga da máquina e um T0 absoluto fixo perdia os primeiros comandos.
            procs.append(
                launch(cmd, args.project, "/Engine/Maps/Entry", ["-game"], logs["srv"], schedule_args(script, "S", 5.0))
            )
            last = next(c for _, w, c in reversed(script) if w == "D")
            while f"Snap={last}" not in (logs["srv"].read_text("utf-8", "replace") if logs["srv"].exists() else ""):
                if time.monotonic() - t0 > lead + script[-1][0] + 120:
                    raise RuntimeError(f"standalone não chegou ao rótulo '{last}'")
                time.sleep(1)
            time.sleep(2)
            return {k: p.read_text("utf-8", "replace") for k, p in logs.items()}
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
    ap.add_argument("--case", default="all", choices=["03", "04", "06", "07", "08", "09", "all"])
    ap.add_argument(
        "--mode",
        default="dedicated",
        choices=["dedicated", "listen"],
        help="rede dos casos 03/04/06 (padrão dedicated)",
    )
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
    if args.mode == "listen":
        cases = ["03", "04", "06"]  # 07/08/09 são standalone; no listen os predicados L03/L04/L06 rodam juntos
        if args.case not in ("all", *cases):
            print("mp_run: --mode listen só cobre os casos 03/04/06 (--case all)", file=sys.stderr)
            return 2
    else:
        cases = ["03", "04", "06", "07", "08", "09"] if args.case == "all" else [args.case]
    results: list[tuple[str, bool, str]] = []
    try:
        if args.mode == "listen":
            raw = run_session(args, cmd, "listen", LISTEN, 1, args.lead, out, listen=True)
            results += evaluate_listen({k: parse_log(v) for k, v in raw.items()}, raw)
        else:
            if set(cases) & {"03", "04", "06"}:
                raw = run_session(args, cmd, "dedicated", DEDICATED, 2, args.lead, out)
                parsed = {k: parse_log(v) for k, v in raw.items()}
                for c in (c for c in cases if c in ("03", "04", "06")):
                    results += evaluate(c, parsed, raw)
            if "07" in cases:
                raw = run_session(args, cmd, "standalone", STANDALONE, 0, 40, out)
                results += evaluate("07", {k: parse_log(v) for k, v in raw.items()}, raw)
            if set(cases) & {"08", "09"}:
                raw = run_session(args, cmd, "keys", KEYS, 0, 40, out)
                parsed = {k: parse_log(v) for k, v in raw.items()}
                for c in (c for c in cases if c in ("08", "09")):
                    results += evaluate(c, parsed, raw)
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
