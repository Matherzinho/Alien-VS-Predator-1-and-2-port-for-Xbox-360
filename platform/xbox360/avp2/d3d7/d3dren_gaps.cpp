// avp360: o que o d3d.ren do AVP2 Reconstructed deixa para a religacao com o binario (alem dos globais gerados em
// d3dren_globals.cpp).
#include "d3dren_globals_includes.h"
#include "d3dren/d3d_texture.h"

// A versao de d3d_SetTexture fora de linha (0x100079e4): os objetos que definem D3DREN_SETTEXTURE_EXTERN a chamam.
// A inline do cabecalho e a mesma funcao; tomar o endereco dela faz o GCC emitir a copia com o nome externo.
int (*volatile g_avp360_SetTextureCopy)(SharedTexture *, uint32, uint32) = d3d_SetTexture;

// O gancho de nevoa por vertice vazio (0x1002cc80): o VC6 juntou-o com as outras funcoes vazias, entao o decomp o
// declara com duas assinaturas; esta e a do gancho.
void __fastcall d3d_NullPreFrameCallback(LTVector *, uint32 *) {}
