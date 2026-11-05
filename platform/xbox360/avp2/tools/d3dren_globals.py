#!/usr/bin/env python3
"""avp360: define os globais que o d3d.ren do AVP2 Reconstructed so declara (extern), gerando xbox/d3dren_globals.cpp.

No projeto original eles vem do binario na religacao (tools/relink_data.py). Aqui:
  1. cada simbolo indefinido do link (lista em argv[1], um por linha) e procurado como `extern TIPO NOME[...];` nos
     fontes e cabecalhos do d3dren;
  2. o endereco vem do nome (DAT_xxxxxxxx) ou de config/d3dren/symbols.csv / names_proposal.csv;
  3. com o d3d.ren original (opcional, argv[3]), escalares em .data recebem o valor inicial do binario (convertido de
     little-endian); o resto comeca zerado, como .bss.
Uso: d3dren_globals.py <indefinidos.txt> <saida.cpp> [d3d.ren]
"""
import csv
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
DECOMP = os.path.join(ROOT, "NOLF-Reconstructed-master")
SRC_DIRS = [os.path.join(DECOMP, "src", "d3dren"), os.path.join(DECOMP, "include", "d3dren")]

SCALARS = {
    "float": ("f", 4), "int": ("i", 4), "int32": ("i", 4), "uint32": ("I", 4), "DWORD": ("I", 4), "LTBOOL": ("i", 4),
    "BOOL": ("i", 4), "long": ("i", 4), "unsigned long": ("I", 4), "unsigned int": ("I", 4), "uint16": ("H", 2),
    "WORD": ("H", 2), "short": ("h", 2), "int16": ("h", 2), "uint8": ("B", 1), "BYTE": ("B", 1), "char": ("b", 1),
    "unsigned char": ("B", 1), "bool": ("B", 1), "D3DCOLOR": ("I", 4), "HRESULT": ("i", 4),
}


def find_externs(names):
    want = set(names)
    found = {}
    pat = re.compile(r"^\s*extern\s+(?!\"C\")(.+?)\s*\b([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*;")
    # Ponteiros de funcao: extern void (__fastcall *NOME)(args);
    fnptr = re.compile(r"^\s*extern\s+(.+?)\(\s*([\w\s]*?)\*\s*([A-Za-z_]\w*)\s*\)\s*(\(.*\))\s*;")
    for d in SRC_DIRS:
        for dirpath, _, files in os.walk(d):
            for fn in files:
                if not fn.endswith((".h", ".cpp")):
                    continue
                path = os.path.join(dirpath, fn)
                with open(path, encoding="latin-1") as f:
                    for line in f:
                        m = pat.match(line)
                        if m and m.group(2) in want and m.group(2) not in found:
                            found[m.group(2)] = (m.group(1).strip(), m.group(3), os.path.relpath(path, DECOMP))
                            continue
                        m = fnptr.match(line)
                        if m and m.group(3) in want and m.group(3) not in found:
                            # Guardado como tipo completo: "RET (CONV *" + nome + ")(ARGS)"
                            found[m.group(3)] = ("%s(%s*" % (m.group(1).strip(), m.group(2)),
                                                 ")" + m.group(4), os.path.relpath(path, DECOMP))
    return found


def symbol_sizes():
    """va -> bytes, de config/d3dren/symbols.csv (addr,end)."""
    sizes = {}
    path = os.path.join(DECOMP, "config", "d3dren", "symbols.csv")
    with open(path, encoding="latin-1") as f:
        for row in csv.reader(f):
            try:
                a, e = int(row[0], 16), int(row[1], 16)
            except (ValueError, IndexError):
                continue
            if e > a:
                sizes[a] = e - a
    return sizes


def proposal_addresses():
    """Enderecos dos dados com nome em config/d3dren/names_proposal.csv."""
    out = []
    path = os.path.join(DECOMP, "config", "d3dren", "names_proposal.csv")
    with open(path, encoding="latin-1") as f:
        for row in csv.reader(f):
            if len(row) > 2 and row[2] == "data":
                try:
                    out.append(int(row[0], 16))
                except ValueError:
                    pass
    return out


def symbol_addresses():
    addr = {}
    for fn in ("symbols.csv", "names_proposal.csv"):
        path = os.path.join(DECOMP, "config", "d3dren", fn)
        if not os.path.exists(path):
            continue
        with open(path, encoding="latin-1") as f:
            for row in csv.reader(f):
                if len(row) < 3:
                    continue
                try:
                    va = int(row[0], 16)
                except ValueError:
                    continue
                name = row[3] if fn == "symbols.csv" and len(row) > 3 else row[1]
                addr.setdefault(name.strip('"'), va)
    return addr


def load_pe(path):
    with open(path, "rb") as f:
        img = f.read()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    optsize = struct.unpack_from("<H", img, pe + 20)[0]
    base = struct.unpack_from("<I", img, pe + 24 + 28)[0]
    secs = []
    off = pe + 24 + optsize
    for i in range(nsec):
        name = img[off:off + 8].rstrip(b"\0").decode("latin-1")
        vsize, va, rawsize, raw = struct.unpack_from("<IIII", img, off + 8)
        secs.append((name, base + va, vsize, raw, rawsize))
        off += 40
    return img, secs


def read_initial(pe, va, size):
    if not pe:
        return None
    img, secs = pe
    for name, sva, vsize, raw, rawsize in secs:
        if sva <= va < sva + max(vsize, rawsize):
            rel = va - sva
            if rel + size > rawsize:
                return None  # parte so virtual: zerada
            return img[raw + rel: raw + rel + size]
    return None


def main():
    undef_path, out_path = sys.argv[1], sys.argv[2]
    pe = load_pe(sys.argv[3]) if len(sys.argv) > 3 and os.path.exists(sys.argv[3]) else None
    with open(undef_path) as f:
        names = [l.strip() for l in f if l.strip() and re.match(r"^[A-Za-z_]\w*$", l.strip())]
    externs = find_externs(names)
    addrs = symbol_addresses()
    sizes = symbol_sizes()
    # So globais com nome: o symbols.csv tem rotulos dentro dos arrays.
    known = sorted(set(proposal_addresses() +
                       [int(n[4:], 16) for n in names if re.match(r"^DAT_[0-9a-f]{8}$", n)]))
    lines, missing, nonzero_aggregates, init_count = [], [], [], 0
    unsized = []
    for name in sorted(names):
        if name not in externs:
            missing.append(name)
            continue
        typ, dims, where = externs[name]
        va = int(name[4:], 16) if re.match(r"^DAT_[0-9a-f]{8}$", name) else addrs.get(name)
        if dims.startswith("[]"):
            # Tamanho: ate o proximo global conhecido (o fim no symbols.csv as vezes so cobre o primeiro elemento),
            # dividido pelo tamanho do elemento no x86 (aqui ponteiros e escalares: o mesmo no PowerPC de 32 bits).
            elem = typ + "".join("[%s]" % d for d in re.findall(r"\[([^\]]+)\]", dims[2:]))
            nxt = next((a for a in known if a > va), None) if va is not None else None
            span = (nxt - va) if nxt else sizes.get(va, 0)
            if span:
                dims = "[%d / sizeof(%s)]%s" % (span, elem, dims[2:])
            else:
                dims = "[256]" + dims[2:]
                unsized.append(name)
        init = ""
        scalar = SCALARS.get(re.sub(r"\s+", " ", typ.replace("volatile", "").replace("static", "")).strip())
        if scalar and not dims and va is not None:
            raw = read_initial(pe, va, scalar[1])
            if raw is not None:
                val = struct.unpack("<" + scalar[0], raw)[0]
                if val:
                    init = " = %r" % val if scalar[0] == "f" else " = %d" % val
                    if scalar[0] == "f":
                        init += "f"
                    init_count += 1
        elif va is not None and pe:
            raw = read_initial(pe, va, 4)
            if raw and any(raw):
                nonzero_aggregates.append("%s (%s%s)" % (name, typ, dims))
        lines.append("%s %s%s%s;\t// %s%s" % (typ, name, dims, init, where, " @%08x" % va if va else ""))
    with open(out_path, "w", newline="\n") as f:
        f.write("// Gerado por tools/d3dren_globals.py: os globais que o d3d.ren do AVP2 Reconstructed so declara.\n")
        f.write("// Valores iniciais dos escalares tirados do d3d.ren original; o resto zerado.\n")
        f.write("#include \"d3dren_globals_includes.h\"\n\n")
        f.write("\n".join(lines) + "\n")
    print("definidos: %d (com valor inicial: %d); sem declaracao extern: %d" % (len(lines), init_count, len(missing)))
    if missing:
        print("  sem extern: " + " ".join(missing))
    if unsized:
        print("  arrays sem tamanho no symbols.csv (256 elementos): " + " ".join(unsized))
    if nonzero_aggregates:
        print("  agregados/ponteiros nao zerados no binario (revisar): " + "; ".join(nonzero_aggregates))


if __name__ == "__main__":
    main()
