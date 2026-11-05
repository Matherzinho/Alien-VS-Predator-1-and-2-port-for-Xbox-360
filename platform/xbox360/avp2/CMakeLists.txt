# avp2.xex: Aliens versus Predator 2 no Xbox 360.
# Motor: LithTech 2.2 do AVP2 Reconstructed (NOLF-Reconstructed-master/src, sem o d3dren); jogo: proj/AVP2.
# Etapa atual: compilacao exploratoria do motor para PowerPC, para medir o que falta.
cmake_minimum_required(VERSION 3.25)
project(avp2_360 C CXX)

# As tres arvores de terceiros, com os patches de patches/ aplicados (build.sh as prepara em build-work/):
#   AVP2_ENGINE  AVP2 Reconstructed (patches/avp2-reconstructed.base + .patch)
#   AVP2_SOURCE  o codigo-fonte oficial do AvP2 v1.0.9.6 (proj/: AVP2 e LT2), do proprio usuario + patches/avp2-source.patch
#   NOLF_LITHSHARED  o lithshared do nolfsrc (patches/nolfsrc.base + nolfsrc-lithshared.patch)
set(AVP2_ENGINE "${CMAKE_CURRENT_SOURCE_DIR}/../../../NOLF-Reconstructed-master" CACHE PATH "AVP2 Reconstructed")
set(AVP2_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../proj" CACHE PATH "AvP2 Source v1.0.9.6 (pasta proj)")
set(NOLF_LITHSHARED "${CMAKE_CURRENT_SOURCE_DIR}/../../../nolfsrc-master/nolfsrc-master/src/lithshared" CACHE PATH "nolfsrc src/lithshared")
file(GLOB_RECURSE engine CONFIGURE_DEPENDS
	"${AVP2_ENGINE}/src/client/*.cpp" "${AVP2_ENGINE}/src/kernel/*.cpp" "${AVP2_ENGINE}/src/lithshared/*.cpp"
	"${AVP2_ENGINE}/src/model/*.cpp" "${AVP2_ENGINE}/src/server/*.cpp" "${AVP2_ENGINE}/src/shared/*.cpp"
	"${AVP2_ENGINE}/src/sound/*.cpp" "${AVP2_ENGINE}/src/world/*.cpp" "${AVP2_ENGINE}/src/*/*.c"
	"${AVP2_ENGINE}/src/sdk/ltquatbase.cpp")

# Camada Windows substituida por platform/xbox360/avp2/xbox/ (ou desligada: DirectInput, rede, DirectMusic, Bink).
# client/render/stringmgr/bindmgr/text_mgr/cursor/musicdriver/version compilam sobre o shim Win32.
list(FILTER engine EXCLUDE REGEX "/sys/win/(client|text_mgr|input|udpdriver|ltdirectmusic[a-z_]*|binkvideomgrimpl|smackvideomgrimpl)\\.cpp$")
list(FILTER engine EXCLUDE REGEX "/(exceptionhandler|mss_glue)\\.cpp$")

file(GLOB xbox CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xbox/*.cpp")
# CLTMessage_Stream (server/game_serialize.cpp) compartilha os metodos do LMessageImpl (shared/lmessage.cpp), que so
# usam ReadRawFL/WriteRaw e o helper; o decomp ainda nao tem a base comum. Gera-se a copia para a classe do stream.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/gen")
set(LMSG "${AVP2_ENGINE}/src/shared/lmessage.cpp")
execute_process(COMMAND sed -n -E
	-e "30,453{s/^(LTRESULT|char const\\*) LMessageImpl::/\\1 CLTMessage_Stream::/;s/= m_Unknown04;/= (LMessageHelper*)m_pHelper;/;p}"
	"${LMSG}" OUTPUT_FILE "${CMAKE_BINARY_DIR}/gen/lmessage_stream.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LMSG}")
add_library(lithtech STATIC ${engine} ${xbox})
set(LT2 "${AVP2_SOURCE}/LT2")
# O decomp inclui o SDK por caminhos relativos do ambiente original ("../../build/proj/LT2/...",
# "../../jupiter/dx9inc/..."). Uma arvore de links no diretorio de build faz esses caminhos existirem.
set(LTSHIM "${CMAKE_BINARY_DIR}/ltshim")
file(MAKE_DIRECTORY "${LTSHIM}/build" "${LTSHIM}/jupiter" "${LTSHIM}/a/b/c/d/e")
file(CREATE_LINK "${AVP2_SOURCE}" "${LTSHIM}/build/proj" SYMBOLIC)
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/include/dx" "${LTSHIM}/jupiter/dx9inc" SYMBOLIC)
target_include_directories(lithtech BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/xbox
	${CMAKE_BINARY_DIR}/gen
	${AVP2_ENGINE}/include
	${LT2}/sdk/inc ${LT2}/lithshared/incs ${LT2}/lithshared/stdlith ${LT2}/lithshared/rezmgr
	${LT2}/lithshared/controlfilemgr ${LT2}/lithshared/lith ${LT2}/lithshared/wonapi ${LTSHIM}/a/b ${LTSHIM}/a/b/c)
# O codigo foi escrito para WIN32 (o decomp compila com /DWIN32): segue esse caminho, com a API do include/windows.h.
target_compile_definitions(lithtech PRIVATE AVP_XBOX360=1 NDEBUG WIN32 _WIN32 _M_PPC=1)
target_compile_options(lithtech PRIVATE -O2 -fsigned-char -fno-strict-aliasing -fwrapv -w
	-include ${CMAKE_CURRENT_SOURCE_DIR}/include/avp2_xbox_prefix.h
	$<$<COMPILE_LANGUAGE:CXX>:-fpermissive -fms-extensions -std=gnu++11 -Wno-narrowing>)

# d3d.ren: o renderizador do AvP2 (AVP2 Reconstructed, src/d3dren), escrito para Direct3D 7. Compila como estava; o
# Direct3D 7 que ele chama e xbox/d3d7_xenos.cpp sobre o xsdk_gpu (include/ddraw.h, include/d3d.h). Como no PC e uma
# DLL a parte, com copias proprias de partes do motor: vira um modulo isolado (avp2_module abaixo).
file(GLOB_RECURSE d3dren CONFIGURE_DEPENDS "${AVP2_ENGINE}/src/d3dren/*.cpp")
# d3dren_globals.cpp: os globais que o decomp so declara (gerado por tools/d3dren_globals.py; ver o cabecalho dele).
add_library(avp2_d3dren STATIC ${d3dren} ${CMAKE_CURRENT_SOURCE_DIR}/d3d7/d3d7_xenos.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/d3d7/d3dren_globals.cpp ${CMAKE_CURRENT_SOURCE_DIR}/d3d7/d3dren_gaps.cpp)
target_include_directories(avp2_d3dren BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/gen
	${AVP2_ENGINE}/include
	${LT2}/sdk/inc ${LT2}/lithshared/incs ${LT2}/lithshared/stdlith ${LTSHIM}/a/b ${LTSHIM}/a/b/c ${LTSHIM}/a/b/c/d/e)
target_compile_definitions(avp2_d3dren PRIVATE AVP_XBOX360=1 NDEBUG WIN32 _WIN32 _M_PPC=1)
target_compile_options(avp2_d3dren PRIVATE -O2 -fsigned-char -fno-strict-aliasing -fwrapv -w
	-include ${CMAKE_CURRENT_SOURCE_DIR}/include/avp2_xbox_prefix.h
	$<$<COMPILE_LANGUAGE:CXX>:-fpermissive -fms-extensions -std=gnu++11 -Wno-narrowing>)
target_include_directories(avp2_d3dren PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/d3d7)

# Codigo do jogo (no PC: cshell.dll e object.lto dentro do AVP2DLL.REZ). Aqui entra estatico no avp2.xex.
set(GAME "${AVP2_SOURCE}/AVP2")
# As listas vem dos .dsp originais (tools/dsp_sources.py): a pasta tem arquivos velhos que o projeto nao compila.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/client_sources.txt" game_client)
list(TRANSFORM game_client PREPEND "${GAME}/ClientShellDLL/")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/object_sources.txt" game_object)
list(TRANSFORM game_object PREPEND "${GAME}/ObjectDLL/")
foreach(part client object)
	add_library(avp2_${part} STATIC ${game_${part}})
	target_include_directories(avp2_${part} BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
		${GAME}/Shared ${GAME}/ClientRes ${GAME}/ServerRes ${GAME}/ClientShellDLL ${GAME}/ObjectDLL
		${LT2}/sdk/inc ${LT2}/sdk/inc/compat ${LT2}/lithshared/incs ${LT2}/lithshared/stdlith ${LT2}/lithshared/butemgr
		${LT2}/lithshared/lith ${LT2}/lithshared/regmgr ${LT2}/lithshared/cryptmgr ${LT2}/lithshared/lithfontmgr
		${LT2}/lithshared/ltguimgr ${LT2}/lithshared/dibmgr ${LT2}/lithshared/mfcstub ${LTSHIM}/a/b ${LTSHIM}/a/b/c)
	# As mesmas definicoes dos .dsp originais (Release/Final do AvP2).
	target_compile_definitions(avp2_${part} PRIVATE AVP_XBOX360=1 NDEBUG WIN32 _WIN32 _WINDOWS _M_PPC=1
		_AVP2BUILD LT15_COMPAT NO_PRAGMA_LIBS _FINAL)
	target_compile_options(avp2_${part} PRIVATE -O2 -fsigned-char -fno-strict-aliasing -fwrapv -w
		-include ${CMAKE_CURRENT_SOURCE_DIR}/include/avp2_xbox_prefix.h
		$<$<COMPILE_LANGUAGE:CXX>:-fpermissive -fms-extensions -std=gnu++11 -Wno-narrowing>)
endforeach()
target_compile_definitions(avp2_client PRIVATE _CLIENTBUILD)
target_compile_definitions(avp2_object PRIVATE _OBJECTBUILD)

# lithshared: o release do AvP2 so traz cabecalhos e .lib de x86. O fonte vem do NOLF (mesma geracao do LithTech).
set(NOLF_LS "${NOLF_LITHSHARED}")
file(GLOB lithshared CONFIGURE_DEPENDS
	"${NOLF_LS}/ButeMgr/arange.cpp" "${NOLF_LS}/ButeMgr/avector.cpp"
	"${NOLF_LS}/CryptMgr/*.cpp" "${NOLF_LS}/lithfontmgr/*.cpp" "${NOLF_LS}/ltguimgr/*.cpp"
	"${NOLF_LS}/MFCStub/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/lithshared/*.cpp")
# O ButeMgr do AvP2 (lithshared/butemgr_avp2.cpp) usa o scanner do NOLF: as linhas dele sao extraidas aqui.
set(NOLF_BUTE "${NOLF_LS}/ButeMgr/butemgr.cpp")
execute_process(COMMAND sed -n "13,182p;290,328p;346,415p" "${NOLF_BUTE}"
	OUTPUT_FILE "${CMAKE_BINARY_DIR}/gen/butemgr_nolf_scanner.inc" RESULT_VARIABLE bute_rc)
if(NOT bute_rc EQUAL 0)
	message(FATAL_ERROR "nao consegui extrair o scanner de ${NOLF_BUTE}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NOLF_BUTE}")
# lithfontmgr: o AvP2 trocou LITHFONTSAVEDATA::szPrevString (char*, comparado por ponteiro) por sPrevString
# (CString, comparado por conteudo). Copias geradas do NOLF com essa troca.
file(GLOB nolf_fonts "${NOLF_LS}/lithfontmgr/*.cpp")
foreach(f IN LISTS nolf_fonts)
	get_filename_component(n "${f}" NAME)
	execute_process(COMMAND sed -E
		-e "s/\\.szPrevString = LTNULL;/.sPrevString.Empty();/"
		-e "s/!m_lfSD->szPrevString \\|\\| m_lfSD->szPrevString != szString/m_lfSD->sPrevString.IsEmpty() || m_lfSD->sPrevString.Compare(szString) != 0/"
		-e "s/szPrevString/sPrevString/g"
		-e "1i #include \"mfcstub.h\"\\n#include \"iltclient.h\""
		"${f}" OUTPUT_FILE "${CMAKE_BINARY_DIR}/gen/${n}")
	list(APPEND lithshared "${CMAKE_BINARY_DIR}/gen/${n}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${f}")
endforeach()
list(FILTER lithshared EXCLUDE REGEX "/(lithfontmgr/[^/]+|ltguimgr/ltguifont)\\.cpp$")
add_library(avp2_lithshared STATIC ${lithshared})
# No fonte do NOLF uma troca global DWORD -> uint32 virou SDWORD em "Suint32"; o cabecalho do AvP2 usa DWORD/SDWORD.
file(GLOB mfcstub_src "${NOLF_LS}/MFCStub/*.cpp")
set_source_files_properties(${mfcstub_src} PROPERTIES COMPILE_DEFINITIONS "uint32=DWORD;Suint32=SDWORD;LTBOOL=BOOL")
get_target_property(game_inc avp2_client INCLUDE_DIRECTORIES)
target_include_directories(avp2_lithshared BEFORE PRIVATE ${CMAKE_BINARY_DIR}/gen ${game_inc})
target_compile_definitions(avp2_lithshared PRIVATE _NOMFC AVP_XBOX360=1 NDEBUG WIN32 _WIN32 _WINDOWS _M_PPC=1 _AVP2BUILD LT15_COMPAT NO_PRAGMA_LIBS _FINAL)
target_compile_options(avp2_lithshared PRIVATE -O2 -fsigned-char -fno-strict-aliasing -fwrapv -w
	-include ${CMAKE_CURRENT_SOURCE_DIR}/include/avp2_xbox_prefix.h
	$<$<COMPILE_LANGUAGE:CXX>:-fpermissive -fms-extensions -std=gnu++11 -Wno-narrowing>)

# No PC cshell.dll e object.lto sao DLLs: cada uma tem sua copia do codigo Shared/ e do lithshared, e o motor so as
# ve pelas exportacoes. Aqui cada uma vira um objeto relocavel (ld -r) onde so as exportacoes ficam globais, com o
# prefixo do modulo; o LoadLibrary/GetProcAddress do shim (xbox/win32_xbox.cpp) acha-as por uma tabela.
# --force-group-allocation desfaz os grupos COMDAT para que as instancias de template de um modulo nao sejam
# descartadas em favor das do motor.
function(avp2_module name lib)
	set(out "${CMAKE_BINARY_DIR}/${name}_module.o")
	set(undef "")
	set(keep "")
	set(redef "")
	foreach(sym IN LISTS ARGN)
		list(APPEND undef -u ${sym})
		list(APPEND keep --keep-global-symbol=${sym})
		list(APPEND redef --redefine-sym ${sym}=${name}_${sym})
	endforeach()
	add_custom_command(OUTPUT "${out}"
		COMMAND ${CMAKE_LINKER} -r --force-group-allocation ${undef} -o "${out}.r"
			--whole-archive $<TARGET_FILE:${lib}> --no-whole-archive $<TARGET_FILE:avp2_lithshared>
		COMMAND ${CMAKE_OBJCOPY} ${keep} "${out}.r" "${out}.l"
		COMMAND ${CMAKE_OBJCOPY} ${redef} "${out}.l" "${out}"
		DEPENDS ${lib} avp2_lithshared VERBATIM)
	set_source_files_properties("${out}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
	set(${name}_obj "${out}" PARENT_SCOPE)
endfunction()
avp2_module(cshell avp2_client GetClientShellFunctions GetClientShellVersion SetInstanceHandle)
avp2_module(object avp2_object GetServerShellFunctions GetServerShellVersion SetInstanceHandle ObjectDLLSetup)

avp2_module(d3dren avp2_d3dren RenderDLLSetup GetSupportedModes FreeModeList)

# xdev.c (o Xenos por baixo do Direct3D 7) usa os cabecalhos do xsdk, que nao convivem com o windows.h do shim: fica
# fora dos modulos, sem o prefixo, e o d3d.ren o acha pelos simbolos xdev_* globais.
add_executable(avp2 ${cshell_obj} ${object_obj} ${d3dren_obj} ${CMAKE_CURRENT_SOURCE_DIR}/d3d7/xdev.c
	${CMAKE_CURRENT_SOURCE_DIR}/xbox/xpad.c ${CMAKE_CURRENT_SOURCE_DIR}/xbox/xaudio.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/d3d7/xdev.c PROPERTIES COMPILE_OPTIONS "-O2")
xsdk_add_shaders(avp2 d3d7/tl.vert d3d7/tl.frag)
target_link_libraries(avp2 PRIVATE -Wl,--start-group lithtech avp2_lithshared -Wl,--end-group m)
xsdk_add_xex(avp2 NAME "Aliens vs Predator 2")
