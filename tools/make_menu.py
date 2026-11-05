#!/usr/bin/env python3
# Gera avp_menu.bin para o menu grafico (1280x720 em bytes RGB brutos, 2.764.800 bytes).
# Modos de uso:
#   1) A partir de qualquer imagem (requer Pillow):
#      python3 tools/make_menu.py imagem.jpg build/avp_menu.bin
#   2) Geracao procedural sci-fi embutida (sem dependencias externas):
#      python3 tools/make_menu.py build/avp_menu.bin
# As areas destacadas de cada jogo seguem menu_cabinet em src/avp_main.c:
#   Esquerda (AvP 1): x de 44 a 606, y de 58 a 678
#   Direita  (AvP 2): x de 672 a 1238, y de 58 a 678
# A arte oficial do menu e assets/menu_art.jpg. Sem Pillow, use tools/make_menu.ps1 no Windows.

import os
import sys

W, H = 1280, 720

def generate_procedural_menu(out_path: str) -> None:
    buf = bytearray(W * H * 3)
    for y in range(H):
        row_off = y * W * 3
        scan = 0.88 if (y & 1) else 1.0
        for x in range(W):
            # Fundo sci-fi escuro com grade tatica Weyland-Yutani
            grid = 18 if ((x % 64 == 0) or (y % 64 == 0)) else 0
            r = int((8 + grid // 2) * scan)
            g = int((18 + grid) * scan)
            b = int((14 + grid // 2) * scan)

            # Card Esquerdo: Aliens versus Predator 1 (1999 Gold - Verde Bioluminescente Xenomorfo)
            if 120 <= x < 610 and 90 <= y < 680:
                border = (x < 126 or x >= 604 or y < 96 or y >= 674)
                if border:
                    r, g, b = 40, 235, 115
                else:
                    gy = (y - 90) / 590.0
                    r = int((10 + 18 * (1.0 - gy) + grid // 2) * scan)
                    g = int((42 + 65 * (1.0 - gy) + grid) * scan)
                    b = int((24 + 32 * (1.0 - gy) + grid // 2) * scan)

            # Card Direito: Aliens versus Predator 2 (2001 Monolith - Ambar/Laranja Predator & Marine)
            elif 670 <= x < 1160 and 90 <= y < 680:
                border = (x < 676 or x >= 1154 or y < 96 or y >= 674)
                if border:
                    r, g, b = 245, 155, 40
                else:
                    gy = (y - 90) / 590.0
                    r = int((48 + 70 * (1.0 - gy) + grid) * scan)
                    g = int((26 + 38 * (1.0 - gy) + grid // 2) * scan)
                    b = int((10 + 16 * (1.0 - gy)) * scan)

            idx = row_off + x * 3
            buf[idx]     = min(255, r)
            buf[idx + 1] = min(255, g)
            buf[idx + 2] = min(255, b)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(buf)
    print(f"{out_path}: {len(buf)} bytes (1280x720 RGB procedural)")

def main() -> None:
    if len(sys.argv) == 2:
        generate_procedural_menu(sys.argv[1])
    elif len(sys.argv) == 3:
        from PIL import Image
        src, out = sys.argv[1], sys.argv[2]
        data = Image.open(src).convert("RGB").resize((W, H), Image.LANCZOS).tobytes()
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        with open(out, "wb") as f:
            f.write(data)
        print(out, len(data), "bytes")
    else:
        raise SystemExit("Uso: tools/make_menu.py [imagem_entrada] <avp_menu.bin>")

if __name__ == "__main__":
    main()
