# avp1.xex: Aliens versus Predator (1999 Gold) no Xbox 360, a partir do NakedAVP (fonte da Rebellion, SDL3).
# O fonte do NakedAVP nao fica no repositorio: build.sh baixa o commit fixo e aplica patches/ (AVP1_SRC).
cmake_minimum_required(VERSION 3.25)
project(avp1_360 C CXX)

set(AVP1_SRC "" CACHE PATH "Checkout do NakedAVP com os patches aplicados")
set(SDL3_XENON "$ENV{HOME}/SDL-xenon" CACHE PATH "SDL-xenon (SDL3 para xsdk)")
if(NOT EXISTS "${AVP1_SRC}/src/frontend/main.c")
	message(FATAL_ERROR "AVP1_SRC nao aponta para o NakedAVP: ${AVP1_SRC}")
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/sources.txt" upstream REGEX "^src/")
# O renderizador OpenGL e o FMV (FFmpeg) sao trocados pelos nossos (xenos_gl.c, fmv_stub.c).
list(FILTER upstream EXCLUDE REGEX "^src/(renderer/oglfunc|renderer/postprocess|media/video/movie|media/audio/cdplayer)")
list(TRANSFORM upstream PREPEND "${AVP1_SRC}/")

add_executable(avp1
	${upstream}
	${CMAKE_CURRENT_SOURCE_DIR}/xenos_gl.c
	${CMAKE_CURRENT_SOURCE_DIR}/avp1_entry.c
	${CMAKE_CURRENT_SOURCE_DIR}/fmv_stub.c
	${CMAKE_CURRENT_SOURCE_DIR}/avp1_log.c
	${CMAKE_CURRENT_SOURCE_DIR}/xbox_posix.c
)

file(GLOB_RECURSE upstream_dirs LIST_DIRECTORIES true "${AVP1_SRC}/src/*")
list(FILTER upstream_dirs INCLUDE REGEX "/src(/[^.]+)?$")
# dirent.h e windows.h do Win32 nao servem aqui.
list(FILTER upstream_dirs EXCLUDE REGEX "/src/platform/win32$")
target_include_directories(avp1 BEFORE PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${AVP1_SRC}/src ${AVP1_SRC}/src/game ${AVP1_SRC}/include ${AVP1_SRC}/include/nakedavp
	${SDL3_XENON}/include
)
foreach(d IN LISTS upstream_dirs)
	if(IS_DIRECTORY "${d}")
		target_include_directories(avp1 PRIVATE "${d}")
	endif()
endforeach()

target_link_libraries(avp1 PRIVATE ${SDL3_XENON}/build-xsdk/libSDL3.a m)
# x86 tem char com sinal; o PowerPC nao. O jogo depende do comportamento do PC.
target_compile_definitions(avp1 PRIVATE AVP_XBOX360=1 AVP_NO_FMV=1)
target_compile_options(avp1 PRIVATE -O2 -fsigned-char -fno-strict-aliasing -fwrapv -w
	-include ${CMAKE_CURRENT_SOURCE_DIR}/include/avp1_xbox_prefix.h
	$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)

xsdk_add_shaders(avp1 world.vert world.frag)

xsdk_add_xex(avp1 NAME "Aliens versus Predator")
