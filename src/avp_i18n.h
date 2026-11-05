// avp360: Internacionalizacao (Portugues PT-BR, Espanol, English) para AvP 1 (3dc) e AvP 2 (LithTech)
#ifndef AVP_I18N_H
#define AVP_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

// Retorna string traduzida (PT-BR ou Espanol) para o TEXTSTRING_ID do AvP 1, ou NULL para usar o Language.txt original (English)
const char *avp1_i18n_get(int textstring_id);

// Retorna string traduzida (PT-BR ou Espanol) para o IDS_* do AvP 2 (ClientRes.h), ou NULL para usar o ClientRes.rc original (English)
const char *avp2_i18n_get(int ids_id);

#ifdef __cplusplus
}
#endif

#endif // AVP_I18N_H
