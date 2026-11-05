# avp360 — estado real

## Arquitetura (desde 04/11/2026)

Tres executaveis lado a lado:

| Arquivo | O que e |
|---|---|
| `AvP360.xex` / `default.xex` | Menu "The Double Feature" (`assets/menu_art.jpg` -> `avp_menu.bin`), opcoes, abre o jogo escolhido |
| `avp1.xex` | **Aliens versus Predator Gold de verdade**: o motor da Rebellion via NakedAVP (SDL3), com renderizador Xenos |
| `avp2.xex` | AvP2: LithTech 2.2 do AVP2 Reconstructed + `proj/AVP2` (linka e gera o .xex; ainda sem video/entrada/som) |

Cada jogo e um .xex proprio: os dois motores nao disputam simbolos nem memoria, e sair de um volta ao menu
(`xsdk_launch`).

## AvP 1 — `platform/xbox360/avp1/`

- `build.sh` baixa o NakedAVP no commit de `patches/base-commit.txt`, roda `tools/endian_reads.py` e aplica
  `patches/avp1.patch`.
- `xenos_gl.c`: o subconjunto de OpenGL 1.x que o renderizador usa (~40 funcoes) sobre `xsdk_gpu`, com
  `world.vert`/`world.frag` (MODULATE, REPLACE, COMBINE, teste de alfa), anel de vertices de 3 quadros, 1280x720.
- Big-endian: `endian_reads.py` troca ~610 leituras `*(T*)p` dos parsers de RIF/FFL por `AVP_LE()`; o patch cobre o
  resto a mao (Huffman dos RIF comprimidos, `CHUNK_EXTRACT`, `MediaRead` dos BMP, cabecalho FFL, campos de bits do
  `AVP_Environment_Settings`, ordem dos canais das texturas de 32 bits).
- Controle: analogico esquerdo anda/desvia, direito mira (mouselook), RT atira, LT tiro alternativo, A pula,
  B agacha, X usa, Y proxima arma, LB visao, RB sinalizador/camuflagem, Start pausa. Nos menus: direcional + A/B.
- Logs no console: `avp1_log.txt` (cache: ou game:), `avp1_stderr.txt` e `avp1_stdout.txt` em game:.
- Falta: musica dos CDs (`music\Track N.wav`), FMVs (Bink), conferir com o console os pontos que so o teste mostra.

## AvP 2 — `platform/xbox360/avp2/` (em andamento)

**`avp2.xex` linka com o renderizador (0 simbolos indefinidos, 10 MB) e o menu ja o abre** (`src/avp_main.c`, como o
avp1.xex). Ainda nao foi testado no console nem no Xenia.

### Som (`xbox/mss_xbox.cpp`, `xbox/xaudio.c`)

As 89 funcoes do Miles (`AIL_*`) que o `soundmgr` usa, sobre um mixer de software estereo a 48 kHz no thread de audio
do xsdk (`xaudio.c`: quadros de 256 amostras). Amostras 2D (PCM 8/16 bits mono/estereo, ou um WAV inteiro com IMA
ADPCM decodificado ao carregar), amostras 3D (ganho pela distancia como no Miles: cheio ate a minima, minima/distancia
depois, mudo alem da maxima; pan pela orientacao do ouvinte; obstrucao/oclusao abafam), streams (o WAV lido inteiro do
handle que o `soundmgr` passa no nome) e `AIL_decompress_ADPCM` (devolve um WAV PCM, como o Miles). Interpolacao
linear, loops por bloco, posicao por ms/bytes, volume mestre. O provedor 3D anuncia os seis nomes que o `soundmgr`
conhece, todos o mesmo: qualquer configuracao do PC serve. Os cabecalhos WAV (`sound/wave.cpp`) sao lidos em
little-endian; as amostras sao lidas byte a byte.

Ainda nao: MP3 (`AIL_decompress_ASI` falha e o som fica de fora), reverb/EAX, filtros, musica DirectMusic, videos
Bink.

### Ordem dos bytes dos arquivos

Os arquivos do LithTech sao little-endian (x86). O ponto unico de conversao e o `iltstream.h` do SDK: no Xbox,
`STREAM_READ`/`STREAM_WRITE`, `>>`/`<<` e `LTStream_Read`/`Write` trocam os bytes de escalares e de arrays de
escalares; `LTVector` e `LTRotation` tem conversao propria. A escrita troca tambem (saves e configuracoes continuam
little-endian, como no PC). Uma auditoria (`build-work/audit.sh`: compila com `-DAVP360_STREAM_AUDIT`, que torna erro
ler qualquer struct inteiro por um stream) confirma que os unicos structs lidos inteiros sao os tres convertidos
campo a campo logo depois: `DtxHeader`, `SectionHeader` e `REZ_PCXHDR`.

Leituras fora dos streams, convertidas onde acontecem (`AVP_XBOX360`):

| Arquivo | O que |
|---|---|
| `lithshared/rezmgr/rezmgr.cpp` | cabecalho do `.rez` e os DWORDs dos blocos de diretorio (`REZ_DWORD`) |
| `shared/dtxmgr.cpp` | cabecalho, secoes e pixels de 16/32 bits das texturas `.dtx` |
| `world/de_mainworld.cpp` | poligonos das folhas, referencias de animacao de luz, tabela de blocos de colisao (`PBlockNode`), propriedades das luzes |
| `server/s_object.cpp` | dados das propriedades dos objetos do mundo (`avp360_FixPropData`: palavras de 4 bytes) |
| `model/model_load.cpp` | quadros-chave das animacoes (`NodeKeyFrame`) |
| `kernel/sys/win/load_pcx.cpp` | cabecalho PCX (fonte do console) |
| `shared/pixelformat.cpp` e a copia do `d3dren` | blocos DXT quando o motor os descomprime |
| `d3dren/unk/100323f9.cpp` | texels dos lightmaps animados (RLE de `uint32`) |
| `d3d7/d3d7_xenos.cpp` | DXT sobe para a GPU com as palavras de 16 bits trocadas (o Xenos le DXT big-endian) |

Ainda little-endian sem conversao: demos gravadas (`client/demomgr.cpp`).

### Controle (`xbox/input_xbox.cpp`, `xbox/xpad.c`)

O controle aciona as acoes do jogo pelo nome (as do `AddAction` do `autoexec.cfg`, com os codigos padrao do AvP2 de
reserva), sem passar pelos binds de teclado do PC. O analogico direito vira os eixos do mouse (`Axis1`/`Axis2`):
~150 graus/s de giro e 90 graus/s de inclinacao, curva quadratica. Nos menus (o jogo desliga a entrada na pausa e
antes de um mapa: `!CClientMgr::m_bInputState`) o controle vira teclas para o `OnKeyDown`: direcional/analogico =
setas com repeticao, A = Enter, B = Esc. Start e sempre Esc.

| Controle | Acao | Controle | Acao |
|---|---|---|---|
| Analogico esquerdo | andar / desviar | Analogico direito | mirar |
| RT | atirar (`Fire`) | LT | tiro alternativo (`AltFire`) |
| A | pular | B | agachar |
| X | usar (`Activate`) | Y | proxima arma |
| RB | recarregar | LB | proxima visao |
| Direcional cima | arma anterior | Direcional baixo | lanterna/sinalizador (`TorchSelect`) |
| Direcional esquerda | MediComp | Direcional direita | energia (`EnergySift`, Predador) |
| L3 | andar nas paredes (Alien) | R3 | zoom |
| Back | placar | Start | menu |

Ainda falta: a tela de "controles personalizados" do jogo lista teclado/mouse vazios (`GetDeviceObjects`/
`GetDeviceBindings` devolvem nulo) e nao muda o mapeamento do controle; vibracao.

### Renderizador (`d3d7/`)

O `d3d.ren` do proprio AvP2 (`NOLF-Reconstructed-master/src/d3dren`, que casa com o `RenderStruct` 3421 do jogo)
compila como esta, como um modulo isolado (`avp2_module(d3dren ...)`, exportando `RenderDLLSetup`,
`GetSupportedModes`, `FreeModeList`). O Direct3D 7 que ele chama e nosso:

| Arquivo | O que faz |
|---|---|
| `include/dx/ddraw.h`, `include/d3d.h`, `include/d3dTypes.h` | tipos, constantes (valores do SDK) e interfaces do DirectDraw 7 / Direct3D 7 que o `d3d.ren` usa, como classes C++ abstratas |
| `d3d7/d3d7_xenos.cpp` | as interfaces: superficies em memoria (texturas sobem para a GPU quando mudam: RGB 16/32 bits viram A8R8G8B8, DXT1/3/5 vao como estao), cadeias de mipmap e de flip, Blt/BltFast (para a tela viram desenhos), Clear, estados de render e de estagio, vertices TL e o caminho TnL (transformado na CPU) |
| `d3d7/xdev.c` | o Xenos: anel de vertices de 3 quadros, texturas, estados do xsdk_gpu, apresentacao em 1280x720 (C puro: os cabecalhos do xsdk nao convivem com o `windows.h` do shim) |
| `d3d7/tl.vert`, `d3d7/tl.frag` | vertices de tela -> clip (com `rhw`, perspectiva correta), nevoa linear/de vertice; os dois estagios de textura do D3D7 como pesos em constantes (o xsc nao aceita chamadas de funcao nem `discard` ao lado de outros `if`) |
| `d3d7/d3dren_globals.cpp` | os 251 globais que o decomp so declara (no projeto original vem da religacao com o binario), gerado por `tools/d3dren_globals.py`; os 28 escalares com valor inicial (padroes de variaveis de console) foram lidos do `d3d.ren` original |

Mudancas no fonte do decomp (guardadas por `AVP_XBOX360`): `TLRGB` na ordem a,r,g,b (a cor e escrita ora por campo,
ora como `uint32` 0xAARRGGBB), os 3 `__asm` x87 em C, as verificacoes de offset do `DrawPolyMgr` desligadas (ponteiro
para funcao-membro tem 8 bytes no GCC), o motor usa o `d3d.ren` linkado em vez de procurar `*.ren`, e o modo de video
e sempre 1280x720.

Limitacoes conhecidas: `Lock` do alvo de render devolve uma copia que nao volta para a tela, e ler a tela (Blt dela
para uma superficie) da preto; afeta capturas de tela e efeitos que leem a tela. Sombras projetadas que copiam a tela
para uma textura ficam vazias. O `Clear` e o descarte de faces seguem o D3D, mas ainda precisam ser conferidos na tela.

| Parte | Origem |
|---|---|
| Motor LithTech 2.2 | `NOLF-Reconstructed-master/src` (AVP2 Reconstructed), sem `d3dren`; da camada Windows ficam `render`, `stringmgr`, `bindmgr`, `musicdriver`, `version_resource`, `lt_cursor_impl` (compilam sobre o shim) |
| Codigo do jogo | `proj/AVP2` (listas dos `.dsp`: `client_sources.txt`, `object_sources.txt`) |
| lithshared | `nolfsrc-master/.../lithshared` adaptado (`lithshared/`: ButeMgr e CLTGUIFont do AvP2, stubs online) |
| Camada Xbox | `xbox/`: `client_xbox.cpp` (main e laco do `client.cpp`), `win32_xbox.cpp` (API Win32), `engine_globals.cpp` (globais e funcoes que o decomp so declara), `text_mgr_xbox.cpp`, `input_xbox.cpp` + `xpad.c` (controle), `mss_xbox.cpp` + `xaudio.c` (som) |

Como as DLLs viram um .xex: cshell.dll e object.lto tinham cada uma sua copia de `Shared/` e do lithshared (542
definicoes duplicadas num link unico). `avp2_module()` no `CMakeLists.txt` faz de cada uma um objeto `ld -r` em que so
as exportacoes ficam globais, com prefixo (`cshell_GetClientShellFunctions`...); o `LoadLibrary` do shim reconhece
`cshell.dll`/`object.lto` pelo nome e o `GetProcAddress` usa uma tabela. Outras DLLs (`ltmsg.dll`, `cres.dll`) sao
lidas do disco so pelos recursos: `LoadString`/`FindResource` leem a secao `.rsrc` do PE original, entao os textos
do jogo vem dos arquivos do proprio jogador.

Correcoes de VC6 no fonte (validas tambem no Windows, guardadas por `AVP_XBOX360` quando mudam comportamento):
`typename`, `this->`, `friend` de templates, escopo de `for` (`tools/fix_for_scope.py`), `##,` em macros, includes
com `\`, ponteiros de funcao com argumento padrao (`ltfnptr_defaults.h`), WON fora do build.

Falta, em ordem:
1. Testar o renderizador (Xenia ou console): cores, descarte de faces, nevoa, DXT (a ordem dos bytes dos blocos).
2. Testar o controle no jogo (sentido da mira, velocidade, menus).
3. Conferir no console a carga dos arquivos (o primeiro mapa e o teste: mundo, objetos, modelos, texturas, lightmaps).
4. Testar o som (volumes, posicao 3D, lado esquerdo/direito). Depois: MP3, musica DirectMusic, videos Bink.
5. Texto via `HLTFONT` (`text_mgr_xbox.cpp`): so mede; o AvP2 usa quase sempre as fontes em bitmap do lithshared.
6. Rede: so o driver local (um jogador).

## O que nao era real

As versoes anteriores de `src/avp1_bridge.c` e `src/avp2_bridge.c` eram raycasters numa grade 16x16 que so liam o
cabecalho dos arquivos do jogo. As duas pontes foram removidas: o menu so escolhe o jogo e abre `avp1.xex` ou `avp2.xex`.
