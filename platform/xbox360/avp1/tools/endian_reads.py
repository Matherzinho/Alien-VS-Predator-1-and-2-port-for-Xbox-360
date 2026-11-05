#!/usr/bin/env python3
"""avp360: torna as leituras binarias do AvP1 independentes da ordem dos bytes.

Os arquivos do AvP (RIF, FFL, etc.) sao little-endian, e o codigo da Rebellion le os numeros direto do buffer com
`*(int *)p`, `*((int *)(p + 8))`, `*(ChunkVectorInt *)p` e afins. No Xbox 360 (big-endian) isso le lixo. Este script
reescreve cada LEITURA desse formato para `AVP_LE(tipo, expr)` (include/avp1_endian.h), que copia os bytes para uma
variavel local e troca a ordem palavra por palavra. ESCRITAS (`*(int *)p = x`, usadas para salvar chunks) ficam como
estao, assim como `&*(T *)p`, `++`/`--` e casts de `char`.

Uso: endian_reads.py <raiz do NakedAVP> [--report]
Roda de novo sem mudar nada: o que ja virou AVP_LE nao casa mais o padrao.
"""
import re
import sys
from pathlib import Path

# Largura da palavra de cada tipo. Estruturas so de int/float: 4. Com double: 8. Tipos fora da lista: 4, e o
# relatorio os mostra para revisao.
WORD = {
    "char": 0, "unsigned char": 0, "signed char": 0, "uint8_t": 0, "int8_t": 0, "BYTE": 0,
    "short": 2, "unsigned short": 2, "int16_t": 2, "uint16_t": 2, "WORD": 2,
    "double": 8, "unaligned_f64": 8, "ChunkVector": 8,
}
KNOWN_4 = {
    "int", "unsigned int", "long", "unsigned long", "int32", "int32_t", "uint32_t", "unsigned", "float",
    "unaligned_s32", "DWORD", "ObjectID", "ChunkVectorInt", "ChunkVectorFloat", "ChunkUV", "ChunkUV_List",
    "ChunkQuat", "ChunkMatrix", "VECTORCH", "TrackRequestTarget", "SwitchTarget", "WaypointLink",
    "Timed_Sound_Description", "RequestTarget", "Placed_Object_Light_Data", "Object_Animation_Frame",
    "MissionMessage", "ExtLift", "ChunkTrackSection", "ChunkPathPoint", "AltObjectLocation", "GlobalBMPFlags",
}

CAST = re.compile(r"\(\s*((?:const\s+)?(?:unsigned\s+|signed\s+)?[A-Za-z_]\w*)\s*\*\s*\)")
# So o codigo que interpreta dados de arquivo. objsetup/projload/chnkload/texio/kzsort fazem o mesmo tipo de cast em
# estruturas da memoria (nativas), e trocar os bytes ali estragaria tudo.
DIRS = ("src/platform/win95", "src/game/platform/win32")
PARSERS = re.compile(r"(chnk|chunk)[^/]*\.cpp$|/(animobs|bmpnames|enumsch|hierplace|zsp|ffread|huffman)\.cpp$", re.I)
EXCLUDE = re.compile(r"/chnkload\.cpp$", re.I)
KEYWORDS = {"return", "case", "else", "do"}
# Leituras que tem que continuar nativas: comparam os 8 caracteres do nome do chunk como dois int com a tabela que
# Chunk_With_Children::Register montou da mesma forma, a partir de strings na memoria.
NATIVE = {
    "chunk.cpp": ("test.chunk_id_1", "test.parent_id_1"),
}


def skip_ws(s, i):
    while i < len(s) and s[i] in " \t\r\n":
        i += 1
    return i


def match_paren(s, i):
    """s[i] == '(' -> indice depois do ')' correspondente."""
    depth = 0
    while i < len(s):
        c = s[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        elif c in "\"'":
            q = c
            i += 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
        i += 1
    return -1


def postfix_operand(s, i):
    """Identificador com sufixos ([...], ->x, .x, (args)); devolve o fim."""
    m = re.compile(r"[A-Za-z_]\w*").match(s, i)
    if not m:
        return -1
    i = m.end()
    while True:
        j = skip_ws(s, i)
        if j < len(s) and s[j] == "[":
            depth, k = 0, j
            while k < len(s):
                if s[k] == "[":
                    depth += 1
                elif s[k] == "]":
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            i = k + 1
        elif s.startswith("->", j) or (j < len(s) and s[j] == "." and not s[j + 1:j + 2].isdigit()):
            j += 2 if s[j] == "-" else 1
            m = re.compile(r"\s*[A-Za-z_]\w*").match(s, j)
            if not m:
                return i
            i = m.end()
        elif j < len(s) and s[j] == "(":
            i = match_paren(s, j)
        else:
            return i


def is_write(s, end, start):
    j = skip_ws(s, end)
    # (*(int *)p) = x
    while j < len(s) and s[j] == ")":
        j = skip_ws(s, j + 1)
    rest = s[j:j + 3]
    if rest.startswith("==") or rest.startswith("=>"):
        return False
    if rest.startswith("="):
        return True
    if re.match(r"(\+|-|\*|/|%|&|\||\^|<<|>>)=", rest) or rest.startswith("++") or rest.startswith("--"):
        return True
    k = start - 1
    while k >= 0 and s[k] in " \t":
        k -= 1
    if k >= 0 and s[k] == "&" and (k == 0 or s[k - 1] != "&"):
        return True  # &*(T*)p
    if k >= 1 and s[k - 1:k + 1] in ("++", "--"):
        return True
    return False


def norm_type(t):
    return re.sub(r"\s+", " ", t.replace("const", "")).strip()


def transform(src, unknown):
    out, i, n = [], 0, 0
    while True:
        star = src.find("*", i)
        if star < 0:
            break
        j = skip_ws(src, star + 1)
        outer = False
        if j < len(src) and src[j] == "(":
            k = skip_ws(src, j + 1)
            if k < len(src) and src[k] == "(" and CAST.match(src, k):
                outer, cast_at = True, k
            else:
                cast_at = j
        else:
            cast_at = j
        m = CAST.match(src, cast_at)
        before = src[:star].rstrip()
        prev = before[-1:]
        word_before = re.search(r"([A-Za-z_]\w*)$", before)
        if word_before and word_before.group(1) in KEYWORDS:
            prev = "("
        if not m or prev.isalnum() or prev in ")]_" or src[star - 1:star] == "/" or src[star + 1:star + 2] == "/":
            out.append(src[i:star + 1])
            i = star + 1
            continue
        t = norm_type(m.group(1))
        op_start = skip_ws(src, m.end())
        if outer:
            close = match_paren(src, j)
            if close < 0:
                out.append(src[i:star + 1]); i = star + 1; continue
            operand = src[op_start:close - 1]
            end = close
        else:
            if op_start < len(src) and src[op_start] == "(":
                op_end = match_paren(src, op_start)
            else:
                op_end = postfix_operand(src, op_start)
            if op_end < 0:
                out.append(src[i:star + 1]); i = star + 1; continue
            operand = src[op_start:op_end]
            end = op_end
        word = WORD.get(t, 4)
        if word == 0 or t == "void" or is_write(src, end, star) or not operand.strip():
            out.append(src[i:star + 1]); i = star + 1; continue
        if t not in WORD and t not in KNOWN_4:
            unknown[t] = unknown.get(t, 0) + 1
        out.append(src[i:star])
        out.append("AVP_LE(%s, %d, (%s))" % (m.group(1).replace("const ", "").strip(), word, operand.strip()))
        i = end
        n += 1
    out.append(src[i:])
    return "".join(out), n


def main():
    root = Path(sys.argv[1])
    report = "--report" in sys.argv
    total, unknown, changed = 0, {}, []
    seen = set()
    for d in DIRS:
        for f in sorted((root / d).rglob("*")):
            p = f.as_posix()
            if f in seen or not PARSERS.search(p) or EXCLUDE.search(p):
                continue
            seen.add(f)
            src = f.read_text(encoding="latin-1")
            keep = NATIVE.get(f.name.lower(), ())
            lines = src.split("\n")
            native = [k for k, l in enumerate(lines) if any(m in l for m in keep)]
            new, n = transform(src, unknown)
            if native:
                new_lines = new.split("\n")
                for k in native:
                    new_lines[k] = lines[k]
                    n -= 1
                new = "\n".join(new_lines)
            if n:
                if '#include "avp1_endian.h"' not in new:
                    new = '#include "avp1_endian.h"\n' + new
                f.write_text(new, encoding="latin-1")
                changed.append((n, f.relative_to(root)))
                total += n
    if report:
        for n, f in sorted(changed, reverse=True):
            print("%5d %s" % (n, f))
    print("endian_reads: %d leituras em %d arquivos" % (total, len(changed)))
    if unknown:
        print("tipos sem largura conhecida (tratados como palavras de 4 bytes):")
        for t, c in sorted(unknown.items(), key=lambda x: -x[1]):
            print("  %4d %s" % (c, t))


if __name__ == "__main__":
    main()
