#!/usr/bin/env python3
"""MAC-LEGACY: check that every symbol a Mach-O binary needs exists on an older macOS.

    check_against_sdk.py <old SDK, e.g. MacOSX10.9.sdk> <binary> [<binary> ...]

For each non-weak undefined symbol (nm -m: "(undefined) external _x (from AppKit)") the library it is bound to is
looked up in the old SDK (its Mach-O stub dylibs, plus everything they re-export), and the symbol must be exported
there. dyld on that macOS would refuse to load the binary for every symbol reported missing.
Exit code 1 when something is missing.
"""
import os
import re
import subprocess
import sys

sdk = sys.argv[1]
bins = sys.argv[2:]


def run(*args):
    return subprocess.run(args, capture_output=True, text=True).stdout


def short_name(install_name):
    base = os.path.basename(install_name)
    if ".framework/" in install_name:
        return base                                  # .../AppKit.framework/Versions/C/AppKit -> AppKit
    return re.sub(r"(\.[A-Z0-9]+)?\.dylib$", "", base)  # libc++.1.dylib -> libc++, libSystem.B.dylib -> libSystem


def sdk_path(install_name):
    p = os.path.join(sdk, install_name.lstrip("/"))
    if os.path.exists(p):
        return p
    # frameworks: try Versions/Current and the top-level link
    m = re.match(r"(.*\.framework)/Versions/[^/]+/(.+)$", install_name)
    if m:
        for cand in (f"{m.group(1)}/Versions/Current/{m.group(2)}", f"{m.group(1)}/{m.group(2)}"):
            p = os.path.join(sdk, cand.lstrip("/"))
            if os.path.exists(p):
                return p
    for ext in (".tbd",):
        p = os.path.join(sdk, os.path.splitext(install_name.lstrip("/"))[0] + ext)
        if os.path.exists(p):
            return p
    return None


_export_cache = {}


def exports(install_name, seen=None):
    """All symbols exported by install_name in the SDK, following LC_REEXPORT_DYLIB. None if not in the SDK."""
    if install_name in _export_cache:
        return _export_cache[install_name]
    seen = seen or set()
    if install_name in seen:
        return set()
    seen.add(install_name)
    p = sdk_path(install_name)
    if p is None:
        _export_cache[install_name] = None
        return None
    syms = set()
    if p.endswith(".tbd"):
        txt = open(p, errors="replace").read()
        for block in re.findall(r"(?:symbols|objc-classes|weak-def-symbols|thread-local-symbols):\s*\[([^\]]*)\]", txt):
            for s in block.replace("\n", " ").split(","):
                s = s.strip().strip("'\"")
                if s:
                    syms.add(s)
        for c in re.findall(r"objc-classes:\s*\[([^\]]*)\]", txt):
            for s in c.replace("\n", " ").split(","):
                s = s.strip().strip("'\"")
                if s:
                    syms.add("_OBJC_CLASS_$_" + s.lstrip("_"))
                    syms.add("_OBJC_METACLASS_$_" + s.lstrip("_"))
        for r in re.findall(r"re-exports:\s*\[([^\]]*)\]", txt):
            for s in r.replace("\n", " ").split(","):
                s = s.strip().strip("'\"")
                if s:
                    sub = exports(s, seen)
                    if sub:
                        syms |= sub
    else:
        for line in run("nm", "-gUj", "-arch", "x86_64", p).splitlines():
            syms.add(line.strip())
        if not syms:   # not a fat file / no -arch support
            for line in run("nm", "-gUj", p).splitlines():
                syms.add(line.strip())
        lc = run("otool", "-l", p)
        for m in re.finditer(r"cmd LC_REEXPORT_DYLIB\n\s+cmdsize \d+\n\s+name (\S+)", lc):
            sub = exports(m.group(1), seen)
            if sub:
                syms |= sub
    _export_cache[install_name] = syms
    return syms


bad = 0
for b in bins:
    print(f"== {b}")
    libs = [l.strip().split(" (")[0] for l in run("otool", "-L", b).splitlines()[1:]]
    lc = run("otool", "-l", b)
    weak_libs = set(re.findall(r"cmd LC_LOAD_WEAK_DYLIB\n\s+cmdsize \d+\n\s+name (\S+)", lc))
    by_short = {short_name(l): l for l in libs}
    missing_libs = [l for l in libs if l not in weak_libs and not l.startswith("@") and exports(l) is None]
    for l in missing_libs:
        print(f"   LIBRARY NOT ON THIS macOS: {l}")
        bad += 1
    missing = {}
    for line in run("nm", "-m", "-u", b).splitlines():
        m = re.match(r"\s*\(undefined\) (weak )?external (\S+) \(from ([^)]+)\)", line)
        if not m or m.group(1):
            continue                       # weak references may be missing
        sym, lib = m.group(2), m.group(3)
        inst = by_short.get(lib)
        if inst is None or inst in weak_libs or inst.startswith("@"):
            continue
        ex = exports(inst)
        if ex is None or sym in ex:
            continue
        missing.setdefault(lib, []).append(sym)
    for lib, syms in sorted(missing.items()):
        for s in sorted(syms):
            dem = run("c++filt", s).strip() if s.startswith("__Z") else s
            print(f"   MISSING in {lib}: {s}" + (f"   = {dem}" if dem != s else ""))
            bad += 1
    if not missing and not missing_libs:
        print("   all symbols found")
print(f"{bad} problem(s)")
sys.exit(1 if bad else 0)
