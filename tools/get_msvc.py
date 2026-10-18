#!/usr/bin/env python3
"""Baixa MSVC 14.44 + Windows SDK 22621 + .NET FX SDK 4.6.2 do canal oficial da Microsoft (VS 2022 17.14)
para a estrutura AutoSDK da Unreal, sem Visual Studio, sem instalador e sem admin.

Uso:  py -3 tools/get_msvc.py [--dest C:\\dev\\sdks]
Depois: set UE_SDKS_ROOT=C:\\dev\\sdks  (a UE 5.8 acha o compilador em HostWin64/Win64/VS2022).
~1 GB de download, ~5 GB em disco. Só Windows. O repo deve estar em caminho ASCII para o UBT.
"""
import argparse
import hashlib
import itertools
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile
from concurrent.futures import ThreadPoolExecutor

CHANNEL = "https://aka.ms/vs/17/release/channel"
MSVC_PKG = "microsoft.vc.14.44.17.14."
MSVC_PARTS = ("tools.hostx64.targetx64", "tools.hostx64.targetx64.res", "crt.headers", "crt.x64.desktop",
    "crt.x64.store", "atl.headers", "atl.x64")
SDK_PKG = "win11sdk_10.0.22621"
SDK_MSIS = ("Windows SDK for Windows Store Apps Tools-x86_en-us.msi",
    "Windows SDK for Windows Store Apps Headers-x86_en-us.msi",
    "Windows SDK for Windows Store Apps Libs-x86_en-us.msi", "Windows SDK Desktop Headers x64-x86_en-us.msi",
    "Windows SDK Desktop Headers x86-x86_en-us.msi", "Windows SDK Desktop Libs x64-x86_en-us.msi",
    "Windows SDK Desktop Tools x64-x86_en-us.msi", "Universal CRT Headers Libraries and Sources-x86_en-us.msi")
NETFX_PKG = "Microsoft.Net.4.6.2.SDK"


def get(url):
    return urllib.request.urlopen(url, timeout=120).read()


def sha256_file(path):
    """sha256 em blocos de 1 MiB: os payloads têm centenas de MB e não cabem inteiros na memória."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while block := f.read(1 << 20):
            h.update(block)
    return h.hexdigest()


def safe_dest(base, *parts):
    """Caminho absoluto de base/parts, ou None se escapar de base (zip slip: '..', absoluto, drive, barra invertida)."""
    base = os.path.abspath(base)
    out = os.path.abspath(os.path.join(base, *parts))
    try:
        inside = os.path.normcase(os.path.commonpath([base, out])) == os.path.normcase(base)
    except ValueError:  # drives diferentes (Windows)
        return None
    return out if inside and os.path.normcase(out) != os.path.normcase(base) else None


def download(url, dst):
    with urllib.request.urlopen(url, timeout=120) as r, open(dst, "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)


def fetch(pl, cache, progress=None):
    dst = os.path.join(cache, pl["fileName"].replace("\\", os.sep))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    sha = pl.get("sha256", "").lower()
    cached = os.path.exists(dst) and (not sha or sha256_file(dst) == sha)
    if not cached:
        download(pl["url"], dst + ".part")
        if sha and sha256_file(dst + ".part") != sha:
            os.remove(dst + ".part")
            raise SystemExit("sha256 inválido: " + pl["fileName"])
        os.replace(dst + ".part", dst)
    if progress:
        progress(pl["fileName"].split("\\")[-1], cached)
    return dst


def pool(payloads, cache):
    total, done = len(payloads), itertools.count(1)  # next() é atômico no CPython: seguro entre threads
    mb = sum(p.get("size", 0) for p in payloads) / 1e6
    print(f"{total} arquivo(s), {mb:.0f} MB", flush=True)

    def progress(name, cached):
        print(f"  [{next(done)}/{total}] {name}{' (cache)' if cached else ''}", flush=True)

    with ThreadPoolExecutor(4) as ex:
        return list(ex.map(lambda p: fetch(p, cache, progress), payloads))


def msi_extract(msi, target):
    r = subprocess.run(["msiexec", "/a", msi, "/qn", "TARGETDIR=" + target], capture_output=True)
    if r.returncode:
        raise SystemExit(f"msiexec falhou ({r.returncode}): {msi}")


def main():
    if sys.platform != "win32":
        raise SystemExit("get_msvc.py só funciona no Windows (usa msiexec).")
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dest", default=r"C:\dev\sdks")
    dest = os.path.abspath(ap.parse_args().dest)
    cache, host = os.path.join(dest, "_dl"), os.path.join(dest, "HostWin64", "Win64")
    ch = json.loads(get(CHANNEL).decode("utf-8-sig"))
    item = next(i for i in ch["channelItems"] if i["id"] == "Microsoft.VisualStudio.Manifests.VisualStudio")
    pk = {}
    for p in json.loads(get(item["payloads"][0]["url"]).decode("utf-8-sig"))["packages"]:
        pk.setdefault(p["id"].lower(), p)

    vsix = [pl for part in MSVC_PARTS for pl in pk[f"{MSVC_PKG}{part}.base"]["payloads"]
            if pl["fileName"].endswith(".vsix")]
    for f in pool(vsix, cache):
        with zipfile.ZipFile(f) as z:
            for n in z.namelist():
                m = re.match(r"Contents/VC/Tools/MSVC/([\d.]+)/(.+)", urllib.parse.unquote(n))
                if m and not n.endswith("/"):
                    out = safe_dest(os.path.join(host, "VS2022"), m.group(1), *m.group(2).split("/"))
                    if out is None:
                        raise SystemExit("entrada de .vsix fora do destino (zip slip): " + n)
                    os.makedirs(os.path.dirname(out), exist_ok=True)
                    with z.open(n) as s, open(out, "wb") as d:
                        shutil.copyfileobj(s, d)

    sdk = pk[SDK_PKG]["payloads"]
    byname = {p["fileName"].split("\\")[-1]: p for p in sdk}
    msis = pool([byname[n] for n in SDK_MSIS], cache)
    cabs = set()
    for f in msis:
        with open(f, "rb") as fh:
            cabs |= set(re.findall(rb"[0-9a-f]{32}\.cab", fh.read()))
    pool([p for p in sdk if p["fileName"].split("\\")[-1].encode() in cabs], cache)
    tmp = os.path.join(dest, "_sdkx")
    shutil.rmtree(tmp, ignore_errors=True)
    for f in msis:
        msi_extract(f, tmp)
    kits = os.path.join(host, "Windows Kits")
    shutil.rmtree(kits, ignore_errors=True)
    shutil.move(os.path.join(tmp, "Windows Kits"), kits)

    netfx = pool([p for p in pk[NETFX_PKG.lower()]["payloads"]], cache)
    tmp = os.path.join(dest, "_netfx")
    shutil.rmtree(tmp, ignore_errors=True)
    msi_extract(next(f for f in netfx if f.endswith(".msi")), tmp)
    shutil.copytree(os.path.join(tmp, "Program Files", "Windows Kits", "NETFXSDK", "4.6.2"),
        os.path.join(kits, "NETFXSDK", "4.6.2"), dirs_exist_ok=True)
    shutil.rmtree(os.path.join(dest, "_sdkx"), ignore_errors=True)
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"OK. Defina UE_SDKS_ROOT={dest}")


if __name__ == "__main__":
    sys.exit(main())
