#!/usr/bin/env python3
"""Gera docs/API.md a partir dos headers públicos do plugin (stdlib, determinístico, sem engine).

Documenta o que o UHT reflete: UCLASS/USTRUCT/UENUM/UINTERFACE, DECLARE_*DELEGATE*, UPROPERTY e UFUNCTION
públicos, com o comentário `/** */` que vem logo antes. `--check` sai ≠ 0 se o arquivo estiver desatualizado.

shortcut: parser textual (não é um parser de C++), upgrade: libclang se um header usar construção que ele não entenda.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

PUBLIC_DIR = Path("Plugins/CatalogAnchor/Source/CatalogAnchor/Public")
DEFAULT_OUT = Path("docs/API.md")
REGEN_CMD = "py -3 tools/gen_api_docs.py"

# Área -> headers, na ordem de exibição. Header fora da tabela cai em "Outros".
AREAS: list[tuple[str, list[str]]] = [
    ("Interação", ["ICatalogInteractable.h", "CatalogInteractionComponent.h"]),
    (
        "Inventário",
        ["CatalogItemDefinition.h", "CatalogInventoryTypes.h", "CatalogInventoryComponent.h", "CatalogPickupActor.h"],
    ),
    (
        "Habilidades",
        [
            "CatalogGameplayTags.h",
            "CatalogAbilityDefinition.h",
            "CatalogAbility.h",
            "CatalogDashAbilityStub.h",
            "CatalogAbilityComponent.h",
        ],
    ),
    ("Vida", ["CatalogHealthComponent.h"]),
    (
        "Save",
        [
            "CatalogSaveTypes.h",
            "CatalogSaveGame.h",
            "CatalogSaveTemplate.h",
            "ICatalogSaveParticipant.h",
            "CatalogSaveSubsystem.h",
        ],
    ),
    ("Núcleo", ["CatalogAnchor.h", "CatalogAnchorSettings.h", "CatalogGameplaySubsystem.h"]),
    ("Blueprint Library", ["CatalogBlueprintLibrary.h"]),
]
OTHER_AREA = "Outros"

ACCESS = ("public", "protected", "private")
API_RE = r"(?:[A-Z][A-Z0-9_]*_API\s+)?"
TYPE_RE = re.compile(rf"\b(class|struct)\s+{API_RE}(\w+)\s*(?:final\s*)?(?::\s*(.*))?$")
ENUM_RE = re.compile(r"\benum\s+(?:class\s+|struct\s+)?(\w+)\s*(?::\s*([\w:]+))?")
DELEGATE_RE = re.compile(r"^(DECLARE_\w*DELEGATE\w*)\s*\(")


@dataclass
class Member:
    decl: str  # declaração sem macro e sem ';'
    specs: str
    doc: str


@dataclass
class TypeDoc:
    kind: str  # class | struct | enum | interface | delegate
    name: str
    signature: str
    doc: str
    header: str
    props: list[Member] = field(default_factory=list)
    funcs: list[Member] = field(default_factory=list)
    values: list[Member] = field(default_factory=list)


class Scanner:
    """Quebra o fonte em instruções ('stmt'/'open'/'close'/'access') guardando o último `/** */` solto."""

    def __init__(self, text: str) -> None:
        self.t = text
        self.i = 0
        self.buf: list[str] = []
        self.depth = 0
        self.doc = ""
        self.enum_mode = False

    def _skip_string(self, quote: str) -> str:
        j = self.i + 1
        while j < len(self.t) and self.t[j] != quote:
            j += 2 if self.t[j] == "\\" else 1
        out = self.t[self.i : j + 1]
        self.i = j + 1
        return out

    def _skip_comment(self) -> str | None:
        """Consome comentário em self.i; devolve o texto se for `/** */`, senão ''. None se não é comentário."""
        t, i = self.t, self.i
        if t.startswith("//", i):
            end = t.find("\n", i)
            self.i = len(t) if end < 0 else end
            return ""
        if t.startswith("/*", i):
            end = t.find("*/", i + 2)
            end = len(t) if end < 0 else end + 2
            body = t[i:end]
            self.i = end
            return body if body.startswith("/**") and body != "/**/" else ""
        return None

    def _skip_directive(self) -> bool:
        t, i = self.t, self.i
        start = t.rfind("\n", 0, i) + 1
        if t[start:i].strip() or t[i] != "#":
            return False
        while True:
            end = t.find("\n", i)
            if end < 0:
                self.i = len(t)
                return True
            if not t[start:end].rstrip().endswith("\\"):
                self.i = end
                return True
            i = end + 1
            start = i

    def skip_block(self) -> None:
        """Pula um `{ ... }` já aberto (corpo inline, struct não refletido...)."""
        level = 1
        self.enum_mode = False
        while self.i < len(self.t) and level:
            c = self.t[self.i]
            if c in "\"'":
                self._skip_string(c)
            elif c == "/" and self._skip_comment() is not None:
                pass
            else:
                level += (c == "{") - (c == "}")
                self.i += 1

    def _take(self, kind: str):
        text = "".join(self.buf)
        self.buf = []
        doc, self.doc = self.doc, ""
        return kind, text, doc

    def next(self):
        t = self.t
        while self.i < len(t):
            c = t[self.i]
            if c in "\"'":
                self.buf.append(self._skip_string(c))
                continue
            if c == "#" and self._skip_directive():
                continue
            if c == "/":
                doc = self._skip_comment()
                if doc is not None:
                    if doc and not self._buf_text():
                        self.doc = doc
                    self.buf.append(" ")
                    continue
            self.i += 1
            if c == "(":
                self.depth += 1
            elif c == ")":
                self.depth = max(0, self.depth - 1)
                if self.depth == 0 and re.fullmatch(r"GENERATED\w*BODY\s*\(\s*", self._buf_text()):
                    self.buf = []  # GENERATED_BODY() não tem ';' e grudaria no próximo membro
                    continue
            elif self.depth == 0:
                tok = self._structural(c)
                if tok:
                    return tok
                continue
            self.buf.append(c)
        return None

    def _buf_text(self) -> str:
        return "".join(self.buf).strip()

    def _structural(self, c: str):
        text = self._buf_text()
        if c == ":" and text in ACCESS:
            self.buf = []
            return "access", text, ""
        if c == ";":
            return self._take("stmt") if text else None
        if c == "{":
            return self._take("open")
        if c == "}":
            if text:
                self.i -= 1  # reemite o '}' depois da instrução pendente
                return self._take("stmt")
            self.doc = ""
            self.enum_mode = False
            return "close", "", ""
        if c == "," and self.enum_mode:
            return self._take("stmt") if text else None
        self.buf.append(c)
        return None


def balanced(text: str, start: int) -> int:
    """Índice do ')' que fecha o '(' em text[start], respeitando strings; -1 se não fecha."""
    level = 0
    i = start
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i += 1
            while i < len(text) and text[i] != c:
                i += 2 if text[i] == "\\" else 1
        elif c == "(":
            level += 1
        elif c == ")":
            level -= 1
            if level == 0:
                return i
        i += 1
    return -1


def split_macro(stmt: str, macro: str) -> tuple[str, str] | None:
    """('args da macro', 'resto') se stmt começa com MACRO(...)."""
    m = re.match(rf"\s*{macro}\s*\(", stmt)
    if not m:
        return None
    end = balanced(stmt, m.end() - 1)
    if end < 0:
        return None
    return stmt[m.end() : end], stmt[end + 1 :]


def norm(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def clean_doc(raw: str) -> str:
    """`/** ... */` -> texto; linhas seguidas viram parágrafo, itens '-' ficam um por linha."""
    body = raw.strip()
    body = re.sub(r"^/\*+", "", body)
    body = re.sub(r"\*+/$", "", body)
    lines = [re.sub(r"^\s*\*\s?", "", ln).strip() for ln in body.splitlines()]
    out: list[str] = []
    para: list[str] = []

    def flush() -> None:
        if para:
            out.append(" ".join(para))
            para.clear()

    for ln in lines:
        if not ln:
            flush()
        elif ln.startswith(("- ", "* ")):
            flush()
            out.append(ln)
        else:
            para.append(ln)
    flush()
    return "\n".join(out).replace("<", "\\<")


def split_args(text: str) -> list[str]:
    parts, level, cur = [], 0, []
    for c in text:
        if c in "(<":
            level += 1
        elif c in ")>":
            level -= 1
        if c == "," and level == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
    if "".join(cur).strip():
        parts.append("".join(cur).strip())
    return parts


def parse_delegate(stmt: str, doc: str, header: str) -> TypeDoc | None:
    m = DELEGATE_RE.match(stmt)
    end = balanced(stmt, m.end() - 1) if m else -1
    if not m or end < 0:
        return None
    args = split_args(stmt[m.end() : end])
    has_ret = "_RetVal" in m[1]
    ret = args.pop(0) if has_ret and args else "void"
    if not args:
        return None
    name, rest = args[0], args[1:]
    params = ", ".join(f"{rest[k]} {rest[k + 1]}" for k in range(0, len(rest) - 1, 2))
    return TypeDoc("delegate", name, f"{ret} {name}({params})  // {m[1]}", clean_doc(doc), header)


def parse_header(text: str, header: str) -> list[TypeDoc]:
    sc = Scanner(text)
    types: list[TypeDoc] = []
    cur: TypeDoc | None = None
    access = "public"
    scopes: list[str] = []  # 'type' | 'enum' | 'ns'
    while True:
        tok = sc.next()
        if tok is None:
            break
        kind, raw, doc = tok
        if kind == "access":
            access = raw
            continue
        if kind == "close":
            if scopes:
                top = scopes.pop()
                if top in ("type", "enum"):
                    cur = None
            continue
        stmt = norm(raw)
        stmt = re.sub(r"^GENERATED\w*BODY\s*\(\s*\)\s*", "", stmt)
        if not stmt:
            continue
        in_type = bool(scopes) and scopes[-1] == "type" and cur is not None
        in_enum = bool(scopes) and scopes[-1] == "enum" and cur is not None
        if in_enum:
            m = re.fullmatch(r"(\w+)(?:\s*=\s*(.+))?", re.sub(r"\s*UMETA\s*\(.*$", "", stmt))
            if m and cur is not None:
                label = m[1] + (f" = {m[2]}" if m[2] else "")
                cur.values.append(Member(label, "", clean_doc(doc)))
            continue
        if in_type and cur is not None:
            if kind == "open":
                sc.skip_block()
            if access != "public":
                continue
            for macro, bucket in (("UPROPERTY", cur.props), ("UFUNCTION", cur.funcs)):
                parts = split_macro(stmt, macro)
                if parts:
                    decl = norm(parts[1])
                    if decl:
                        bucket.append(Member(decl, norm(parts[0]), clean_doc(doc)))
                    break
            else:
                if cur.kind == "interface" and re.match(r"virtual\b.*=\s*0$", stmt):
                    cur.funcs.append(Member(stmt, "", clean_doc(doc)))
            continue
        # escopo de arquivo
        if kind == "stmt":
            d = parse_delegate(stmt, doc, header)
            if d:
                types.append(d)
            continue
        # kind == "open"
        if re.match(r"namespace\b", stmt):
            scopes.append("ns")
            continue
        uenum = split_macro(stmt, "UENUM")
        if uenum:
            m = ENUM_RE.search(uenum[1])
            if m:
                sig = f"enum class {m[1]}" + (f" : {m[2]}" if m[2] else "")
                cur = TypeDoc("enum", m[1], sig, clean_doc(doc), header)
                types.append(cur)
                scopes.append("enum")
                sc.enum_mode = True
                access = "public"
                continue
        refl = None
        for macro, kind_name in (("UCLASS", "class"), ("USTRUCT", "struct")):
            parts = split_macro(stmt, macro)
            if parts:
                refl = (kind_name, norm(parts[1]))
                break
        if refl is None and split_macro(stmt, "UINTERFACE"):
            sc.skip_block()  # a classe U* do UHT é só um stub
            continue
        decl = refl[1] if refl else stmt
        m = TYPE_RE.search(decl)
        is_iface = refl is None and bool(m) and m[1] == "class" and re.match(r"I[A-Z]", m[2] or "") is not None
        if m and (refl or is_iface):
            bases = norm(m[3]) if m[3] else ""
            sig = f"{m[1]} {m[2]}" + (f" : {bases}" if bases else "")
            cur = TypeDoc("interface" if is_iface else refl[0] if refl else "class", m[2], sig, clean_doc(doc), header)
            types.append(cur)
            scopes.append("type")
            access = "public" if m[1] == "struct" else "private"
            continue
        sc.skip_block()  # qualquer outro bloco (struct auxiliar, template, enum comum...)
    return types


def area_of(header: str) -> tuple[int, int]:
    for ai, (_, files) in enumerate(AREAS):
        if header in files:
            return ai, files.index(header)
    return len(AREAS), 0


def slug(text: str) -> str:
    return re.sub(r"[^\w\- ]", "", text.lower()).replace(" ", "-")


KIND_LABEL = {
    "class": "classe",
    "struct": "struct",
    "enum": "enum",
    "interface": "interface",
    "delegate": "delegate",
}


def render_member(m: Member) -> list[str]:
    out = [f"- `{m.decl}`"]
    if m.specs:
        out.append(f"  - Specifiers: `{m.specs}`")
    if m.doc:
        out += [f"  - {ln}" for ln in m.doc.splitlines()]
    return out


def render_type(t: TypeDoc) -> list[str]:
    out = [f"### {t.name}", "", f"*{KIND_LABEL[t.kind]}* · `{t.header}`", "", f"```cpp\n{t.signature}\n```", ""]
    if t.doc:
        out += [t.doc, ""]
    for title, items in (("Valores", t.values), ("Propriedades", t.props), ("Funções", t.funcs)):
        if items:
            out += [f"**{title}**", ""]
            for m in items:
                out += render_member(m)
            out.append("")
    return out


def collect(root: Path) -> list[TypeDoc]:
    pub = root / PUBLIC_DIR
    types: list[TypeDoc] = []
    headers = sorted(p.name for p in pub.glob("*.h"))
    headers.sort(key=lambda h: (*area_of(h), h))
    for name in headers:
        text = (pub / name).read_bytes().decode("utf-8-sig").replace("\r\n", "\n")
        types += parse_header(text, name)
    return types


def render(types: list[TypeDoc]) -> str:
    groups: dict[str, list[TypeDoc]] = {}
    names = [a for a, _ in AREAS] + [OTHER_AREA]
    for t in types:
        groups.setdefault(names[area_of(t.header)[0]], []).append(t)
    out = [
        "# API de referência",
        "",
        f"> Gerado por `tools/gen_api_docs.py` a partir de `{PUBLIC_DIR.as_posix()}/*.h`. "
        f"Não edite à mão: rode `{REGEN_CMD}` e commite. O CI confere com `--check`.",
        "",
        "Cobre o que o UHT reflete (UCLASS, USTRUCT, UENUM, interfaces, delegates, UPROPERTY e UFUNCTION públicos). "
        "Helpers C++ estáticos e membros protegidos/privados ficam nos headers.",
        "",
        "## Índice",
        "",
    ]
    for area in names:
        items = groups.get(area)
        if not items:
            continue
        out.append(f"- [{area}](#{slug(area)})")
        out += [f"  - [`{t.name}`](#{slug(t.name)}) ({KIND_LABEL[t.kind]})" for t in items]
    out.append("")
    for area in names:
        items = groups.get(area)
        if not items:
            continue
        out += [f"## {area}", ""]
        for t in items:
            out += render_type(t)
    return "\n".join(out).rstrip("\n") + "\n"


def generate(root: Path) -> str:
    return render(collect(root))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--out", type=Path, default=None, help=f"padrão: <root>/{DEFAULT_OUT.as_posix()}")
    parser.add_argument("--check", action="store_true", help="não escreve; sai 1 se o arquivo estiver desatualizado")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    out = args.out or root / DEFAULT_OUT
    if not (root / PUBLIC_DIR).is_dir():
        print(f"erro: {root / PUBLIC_DIR} não existe", file=sys.stderr)
        return 2
    new = generate(root)
    if args.check:
        old = out.read_bytes().decode("utf-8").replace("\r\n", "\n") if out.is_file() else None
        if old != new:
            print(f"{out.name} desatualizado: rode `{REGEN_CMD}`", file=sys.stderr)
            return 1
        print(f"{out.name} em dia")
        return 0
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(new.encode("utf-8"))
    print(f"escrito {out.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
