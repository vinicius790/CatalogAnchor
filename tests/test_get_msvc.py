"""tools/get_msvc.py: checagem de caminho do .vsix (zip slip), hash em blocos e fetch com verificação."""

from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import get_msvc  # noqa: E402

BS = chr(92)


@pytest.mark.parametrize(
    "parts",
    [
        ("14.44.1", "bin", "cl.exe"),
        ("14.44.1", "include", "x", "..", "y.h"),  # '..' que continua dentro
    ],
)
def test_safe_dest_accepts_paths_inside(tmp_path: Path, parts: tuple[str, ...]):
    out = get_msvc.safe_dest(tmp_path, *parts)
    assert out is not None and os.path.commonpath([str(tmp_path), out]) == str(tmp_path)


@pytest.mark.parametrize(
    "parts",
    [
        ("..", "x"),  # versão ".." (casa [\d.]+)
        ("14.44.1", "..", "..", "evil.dll"),
        ("14.44.1", f"..{BS}..{BS}evil.dll"),  # separador do Windows dentro de um componente
        ("14.44.1", "..", "..", "..", "..", "..", "..", "..", "..", "..", "evil.dll"),
        ("14.44.1", os.path.abspath(os.sep + "evil.dll")),  # absoluto descarta a base no os.path.join
        ("14.44.1", ".."),  # o próprio diretório-base não é um arquivo
        (),
    ],
)
def test_safe_dest_rejects_escapes(tmp_path: Path, parts: tuple[str, ...]):
    base = tmp_path / "VS2022"
    base.mkdir()
    out = get_msvc.safe_dest(base, *parts)
    if sys.platform != "win32" and parts == ("14.44.1", f"..{BS}..{BS}evil.dll"):
        # no POSIX '\' é nome de arquivo válido: fica dentro (a extração só roda no Windows)
        assert out is not None
    else:
        assert out is None


@pytest.mark.skipif(sys.platform != "win32", reason="drive só existe no Windows")
def test_safe_dest_rejects_other_drive(tmp_path: Path):
    other = "D:" if tmp_path.drive.upper() != "D:" else "E:"
    assert get_msvc.safe_dest(tmp_path, "14.44.1", other + BS + "evil.dll") is None


def test_sha256_file_reads_in_blocks(tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
    data = os.urandom((1 << 20) * 2 + 123)
    f = tmp_path / "big.bin"
    f.write_bytes(data)
    reads: list[int] = []
    real_open = open

    def spy_open(*a, **k):
        fh = real_open(*a, **k)
        real_read = fh.read
        fh.read = lambda n=-1: (reads.append(n), real_read(n))[1]
        return fh

    monkeypatch.setattr("builtins.open", spy_open)
    assert get_msvc.sha256_file(f) == hashlib.sha256(data).hexdigest()
    assert reads and all(n == 1 << 20 for n in reads)  # nunca read() inteiro


def _payload(src: Path, sha: str | None = None) -> dict:
    data = src.read_bytes()
    return {
        "fileName": f"Microsoft.X{BS}x.vsix",
        "url": src.as_uri(),
        "sha256": (sha or hashlib.sha256(data).hexdigest()).upper(),  # a Microsoft publica em maiúsculas
        "size": len(data),
    }


def test_fetch_downloads_verifies_and_reports_cache(tmp_path: Path):
    src = tmp_path / "src.bin"
    src.write_bytes(b"conteudo" * 1000)
    calls: list[tuple[str, bool]] = []
    cache = tmp_path / "cache"
    dst = get_msvc.fetch(_payload(src), str(cache), lambda name, cached: calls.append((name, cached)))
    assert Path(dst).read_bytes() == src.read_bytes() and not Path(dst + ".part").exists()
    get_msvc.fetch(_payload(src), str(cache), lambda name, cached: calls.append((name, cached)))
    assert calls == [("x.vsix", False), ("x.vsix", True)]


def test_fetch_bad_hash_exits_and_leaves_no_part(tmp_path: Path):
    src = tmp_path / "src.bin"
    src.write_bytes(b"abc")
    with pytest.raises(SystemExit, match="sha256 inválido"):
        get_msvc.fetch(_payload(src, sha="0" * 64), str(tmp_path / "cache"))
    assert not list((tmp_path / "cache").rglob("*.part")) and not list((tmp_path / "cache").rglob("*.vsix"))
