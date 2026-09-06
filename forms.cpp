#include "forms.h"
#include "forms_data.h"
#include <stdio.h>

const FormEntry *formFind(int16_t dex, FormId id) {
  if (!id) return nullptr;
  for (uint16_t i=0; i<FORM_COUNT; ++i)
    if (FORM_TBL[i].dex == dex && FORM_TBL[i].id == id) return &FORM_TBL[i];
  return nullptr;
}
uint8_t formCount(int16_t dex) {
  uint8_t n=0;
  for (uint16_t i=0; i<FORM_COUNT; ++i) if (FORM_TBL[i].dex==dex) ++n;
  return n;
}
const FormEntry *formAt(int16_t dex, uint8_t index) {
  for (uint16_t i=0; i<FORM_COUNT; ++i)
    if (FORM_TBL[i].dex==dex && index--==0) return &FORM_TBL[i];
  return nullptr;
}
bool formEligible(int16_t dex, FormId id, uint16_t level) {
  if (dex<1 || dex>DEX_COUNT) return false;
  if (!id) return true;
  const auto *f=formFind(dex,id);
  return f && level>=f->level;
}
DexEntry formDex(int16_t dex, FormId id) {
  DexEntry d=DEX_TBL[dex>=1 && dex<=DEX_COUNT ? dex : 0];
  const auto *f=formFind(dex,id);
  if (f) {
    d.type1=f->type1; d.type2=f->type2;
    d.bHp=f->hp; d.bAtk=f->atk; d.bDef=f->def;
    d.bSpe=f->spe; d.bSpA=f->spa; d.bSpD=f->spd;
  }
  return d;
}
const char *formClassName(uint8_t c, bool ko) {
  static const char *const K[]={"메가진화","지방의 모습","원시회귀","합체","가면","특수·타입","거대화"};
  static const char *const E[]={"Mega","Regional","Primal","Fusion","Mask","Special / type","Gigantamax"};
  return c<7 ? (ko?K[c]:E[c]) : "?";
}
void formSpriteName(char *out, unsigned cap, int16_t dex, FormId id, bool shiny) {
  // Retain unsupported IDs in saves, but never load an old excluded still
  // left on an SD card or in an upgraded app's extracted asset directory.
  if(id && !formFind(dex,id)) id=0;
  if (id) snprintf(out,cap,"p%s%03uf%05u.bin",shiny?"s":"",(unsigned)dex,(unsigned)id);
  else snprintf(out,cap,"p%s%03u.bin",shiny?"s":"",(unsigned)dex);
}
