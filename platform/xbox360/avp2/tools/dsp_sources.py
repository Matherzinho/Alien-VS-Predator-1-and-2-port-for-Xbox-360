#!/usr/bin/env python3
"""avp360: lista os .cpp/.c que um projeto .dsp do VC6 compila na configuracao Release (ou a escolhida).

Um arquivo fica de fora quando o bloco dele tem "# PROP Exclude_From_Build 1" sob a configuracao pedida.
Uso: dsp_sources.py <arquivo.dsp> [trecho do nome da configuracao, padrao "Release"]
Imprime um caminho por linha, relativo a pasta do .dsp, com '/'.
"""
import re
import sys
from pathlib import Path


def sources(dsp, config="Release"):
    text = Path(dsp).read_text(encoding="latin-1").splitlines()
    out, cur, cur_cfg, excluded = [], None, None, False
    for line in text:
        m = re.match(r'^SOURCE=(.+?)\s*$', line)
        if m:
            path = m.group(1).strip('"')
            cur = path if re.search(r'\.(cpp|c)$', path, re.I) else None
            excluded = False
            continue
        m = re.match(r'^!(?:ELSE)?IF\s+"\$\(CFG\)"\s*==\s*"(.+)"', line)
        if m:
            cur_cfg = m.group(1)
            continue
        if line.startswith("!ENDIF"):
            cur_cfg = None
            continue
        if "Exclude_From_Build 1" in line and cur and (cur_cfg is None or config.lower() in cur_cfg.lower()):
            excluded = True
            continue
        if line.startswith("# End Source File"):
            if cur and not excluded:
                out.append(cur.replace("\\", "/").removeprefix("./"))
            cur = None
    return out


if __name__ == "__main__":
    cfg = sys.argv[2] if len(sys.argv) > 2 else "Release"
    for s in sources(sys.argv[1], cfg):
        print(s)
