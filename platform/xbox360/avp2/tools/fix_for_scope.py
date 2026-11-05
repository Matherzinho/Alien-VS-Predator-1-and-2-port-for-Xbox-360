#!/usr/bin/env python3
"""avp360: corrige o escopo de `for` do VC6 no codigo do AvP2.

No VC6, `for (int i = 0; ...)` deixa `i` vivo depois do laco, e o codigo do AvP2 usa isso. O GCC nao. A partir dos
erros "'i' was not declared in this scope" de um log de build, o script acha o `for (TIPO i =` mais proximo acima
e tira a declaracao para a linha anterior (`TIPO i;`), no mesmo bloco: exatamente o escopo que o VC6 dava.

Uso: fix_for_scope.py <log do ninja> <raiz do repositorio> [--map PREFIXO_GERADO=PREFIXO_ORIGINAL:DESLOCAMENTO ...]
--map: erros num arquivo gerado (copia com linhas inseridas no topo) vao para o original, com a linha deslocada.
Mantem a codificacao (latin-1) e os fins de linha dos arquivos.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ERR = re.compile(r"^(?P<file>[^:\s]+\.(?:cpp|h)):(?P<line>\d+):\d+: error: '(?P<var>\w+)' was not declared in this scope")


def main():
    log, root = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
    maps = []
    args = sys.argv[3:]
    for i, a in enumerate(args):
        if a == "--map" and i + 1 < len(args):
            src, rest = args[i + 1].split("=", 1)
            dst, off = rest.rsplit(":", 1)
            maps.append((src, dst, int(off)))
    wanted = defaultdict(set)
    for line in log.read_text(errors="replace").splitlines():
        m = ERR.match(line)
        if not m:
            continue
        f, ln = m.group("file"), int(m.group("line"))
        for src, dst, off in maps:
            if f.startswith(src):
                f, ln = dst + f[len(src):], ln + off
                break
        else:
            if "/proj/" not in f and "/nolfsrc-master/" not in f:
                continue
        wanted[Path(f).resolve()].add((ln, m.group("var")))
    fixed = 0
    for path, errs in wanted.items():
        raw = path.read_bytes().decode("latin-1")
        lines = raw.splitlines(keepends=True)
        changed = {}
        for lineno, var in sorted(errs):
            decl = re.compile(r"for\s*\(\s*((?:const\s+|unsigned\s+|signed\s+)*[A-Za-z_][\w:]*(?:<[^;()]*>)?\s*\**)\s*\b" + re.escape(var) + r"\s*=")
            for k in range(min(lineno - 1, len(lines) - 1), max(lineno - 400, -1), -1):
                m = decl.search(lines[k])
                if not m:
                    continue
                if k in changed:
                    break
                indent = re.match(r"\s*", lines[k]).group(0)
                eol = "\r\n" if lines[k].endswith("\r\n") else "\n"
                typ = m.group(1).strip()
                new_for = lines[k][:m.start()] + "for (" + var + " =" + lines[k][m.end():]
                changed[k] = indent + typ + " " + var + "; // avp360: escopo de for do VC6" + eol + new_for
                fixed += 1
                break
        for k, text in changed.items():
            lines[k] = text
        if changed:
            path.write_bytes("".join(lines).encode("latin-1"))
            print("%3d %s" % (len(changed), path.relative_to(root)))
    print("fix_for_scope: %d declaracoes movidas" % fixed)


if __name__ == "__main__":
    main()
