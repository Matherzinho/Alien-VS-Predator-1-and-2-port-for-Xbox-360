# avp360: Aliens versus Predator (1999) & Aliens versus Predator 2 (2001) para Xbox 360 (xsdk).
# Port feito por Matherzin.
# Este CMake e o do menu (AvP360.xex); os jogos sao avp1.xex e avp2.xex (platform/xbox360/, montados pelo build.sh).
cmake_minimum_required(VERSION 3.25)
project(avp360 C CXX)

add_executable(AvP360
	src/avp_main.c
	src/avp_i18n.c
	src/avp_audio.c
	src/xbox360_compat.c
)

target_link_libraries(AvP360 PRIVATE m)

set_target_properties(AvP360 PROPERTIES
	C_STANDARD 17
	C_EXTENSIONS ON
)

target_include_directories(AvP360 PRIVATE . src)

target_compile_definitions(AvP360 PRIVATE AVP_XBOX360 _XBOX)
target_compile_options(AvP360 PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off -w)

# Gera AvP360_shaders.h com blit_vert, blit_frag, blit_sharp_frag, blit_cubic_frag e blit_scale3x_frag.
xsdk_add_shaders(AvP360
	src/blit.vert
	src/blit.frag
	src/blit_sharp.frag
	src/blit_cubic.frag
	src/blit_scale3x.frag
)

xsdk_add_xex(AvP360 NAME "Aliens vs. Predator 1 & 2")
