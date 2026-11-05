#!/bin/bash
# avp360: compila o menu (AvP360.xex), o Aliens versus Predator 1 (avp1.xex) e o AvP 2 (avp2.xex) com xsdk.
# Nenhum dado ou asset proprietario dos jogos entra nos .xex.
#   XSDK_HOME  pasta contendo xsdk/, xsdk-gcc/ e SDL-xenon/ (padrao: $HOME)
#   WORK       pasta de trabalho (padrao: ./build-work)
#   JOBS       compilacoes em paralelo (padrao: nproc)
#   AVP2_SOURCE  a pasta proj do AvP2 Source Code v1.0.9.6 (padrao: ./proj)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
XSDK_HOME="${XSDK_HOME:-$HOME}"
WORK="${WORK:-$HERE/build-work}"
JOBS="${JOBS:-$(nproc)}"
AVP1="$HERE/platform/xbox360/avp1"
mkdir -p "$WORK" "$HERE/build"
export HOME="$XSDK_HOME"

# 1. NakedAVP (fonte do AvP Gold da Rebellion, porte SDL3) no commit fixo, so baixado uma vez.
read -r NAKED_URL NAKED_COMMIT < "$AVP1/patches/base-commit.txt"
if [ ! -d "$WORK/nakedavp-orig/.git" ]; then
	rm -rf "$WORK/nakedavp-orig"
	git clone --quiet "$NAKED_URL" "$WORK/nakedavp-orig"
fi
git -C "$WORK/nakedavp-orig" -c advice.detachedHead=false checkout --quiet "$NAKED_COMMIT"

# 2. Arvore de trabalho: original -> leituras little-endian (tools/endian_reads.py) -> patch do Xbox 360.
rm -rf "$WORK/nakedavp"
cp -r "$WORK/nakedavp-orig" "$WORK/nakedavp"
rm -rf "$WORK/nakedavp/.git"
python3 "$AVP1/tools/endian_reads.py" "$WORK/nakedavp"
patch -d "$WORK/nakedavp" -p1 --quiet --no-backup-if-mismatch < "$AVP1/patches/avp1.patch"

# 3. avp1.xex
cmake -S "$AVP1" -B "$WORK/avp1" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$XSDK_HOME/xsdk/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release \
	-DAVP1_SRC="$WORK/nakedavp" -DSDL3_XENON="$XSDK_HOME/SDL-xenon" > /dev/null
cmake --build "$WORK/avp1" -j "$JOBS"

# 4. avp2.xex: LithTech 2.2 (AVP2 Reconstructed) + jogo e SDK (codigo-fonte oficial do AvP2 v1.0.9.6, do usuario:
#    a licenca dele nao deixa redistribuir) + lithshared do NOLF (nolfsrc). Os dois repositorios publicos sao baixados
#    no commit fixo de patches/*.base; os tres recebem os patches de platform/xbox360/avp2/patches/.
AVP2="$HERE/platform/xbox360/avp2"
AVP2_SOURCE="${AVP2_SOURCE:-$HERE/proj}"	# a pasta proj do AvP2 Source (com AVP2/ e LT2/)
# O codigo do AvP2 foi escrito no Windows e mistura maiusculas e minusculas nos nomes (Stdafx.cpp / StdAfx.cpp): as
# arvores precisam ficar num disco sem essa diferenca, como o do Windows visto pelo WSL (/mnt/c/...).
AVP2_TREES="${AVP2_TREES:-$HERE/build-work}"

fetch() { # $1 arquivo .base, $2 destino
	local url commit
	read -r url commit < "$1"
	if [ ! -d "$2/.git" ]; then
		rm -rf "$2"
		git clone --quiet "$url" "$2"
	fi
	git -C "$2" -c advice.detachedHead=false checkout --quiet "$commit"
}

# Aplica um patch uma vez: pula se a arvore ja o tem.
apply_once() { # $1 pasta, $2 patch
	if patch -d "$1" -p1 -R --dry-run --binary --quiet < "$2" > /dev/null 2>&1; then
		return 0
	fi
	patch -d "$1" -p1 --binary --quiet --no-backup-if-mismatch < "$2"
}

if [ -d "$AVP2_SOURCE/AVP2" ] && [ -d "$AVP2_SOURCE/LT2" ]; then
	fetch "$AVP2/patches/avp2-reconstructed.base" "$AVP2_TREES/upstream/avp2-reconstructed"
	fetch "$AVP2/patches/nolfsrc.base" "$AVP2_TREES/upstream/nolfsrc"
	rm -rf "$AVP2_TREES/avp2-engine" "$AVP2_TREES/nolf-lithshared" "$AVP2_TREES/avp2-source"
	cp -r "$AVP2_TREES/upstream/avp2-reconstructed" "$AVP2_TREES/avp2-engine" && rm -rf "$AVP2_TREES/avp2-engine/.git"
	cp -r "$AVP2_TREES/upstream/nolfsrc/src/lithshared" "$AVP2_TREES/nolf-lithshared"
	cp -r "$AVP2_SOURCE" "$AVP2_TREES/avp2-source"
	apply_once "$AVP2_TREES/avp2-engine" "$AVP2/patches/avp2-reconstructed.patch"
	apply_once "$AVP2_TREES/nolf-lithshared" "$AVP2/patches/nolfsrc-lithshared.patch"
	[ -f "$AVP2/patches/avp2-source.patch" ] && apply_once "$AVP2_TREES/avp2-source" "$AVP2/patches/avp2-source.patch"

	cmake -S "$AVP2" -B "$WORK/avp2" -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE="$XSDK_HOME/xsdk/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release \
		-DAVP2_ENGINE="$AVP2_TREES/avp2-engine" -DAVP2_SOURCE="$AVP2_TREES/avp2-source" \
		-DNOLF_LITHSHARED="$AVP2_TREES/nolf-lithshared" > /dev/null
	cmake --build "$WORK/avp2" -j "$JOBS"
else
	echo "avp2: sem o codigo-fonte do AvP2 v1.0.9.6 em $AVP2_SOURCE (AVP2/ e LT2/): pulando o avp2.xex"
fi

# 5. AvP360.xex (menu)
cmake -S "$HERE" -B "$WORK/xbox" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$XSDK_HOME/xsdk/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$WORK/xbox" -j "$JOBS"

# 6. Saida: build/ e, se existir, a pasta de teste com os dados do jogo.
for dest in "$HERE/build" "$HERE/AvP360_Xbox360"; do
	[ -d "$dest" ] || continue
	cp "$WORK/xbox/AvP360.xex" "$dest/AvP360.xex"
	cp "$WORK/xbox/AvP360.xex" "$dest/default.xex"
	cp "$WORK/avp1/avp1.xex" "$dest/avp1.xex"
	[ -f "$WORK/avp2/avp2.xex" ] && cp "$WORK/avp2/avp2.xex" "$dest/avp2.xex"
done
ls -la "$HERE/build/"*.xex
