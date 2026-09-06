#pragma once
#include "forms.h"
#include "moves.h"
#include "form_moves_data.h"
#include <string.h>
uint8_t tmLevelFor(const MoveEntry &m);
inline const FormLearnIndex *formLearnIndex(int16_t dex,FormId form) {
  if(!formFind(dex,form))return nullptr;
  for(const auto &r:FORM_LEARN_INDEX)if(r.dex==dex && r.form==form)return &r;
  return nullptr;
}
inline uint8_t formLearnCount(int16_t dex,FormId form) {
  auto r=formLearnIndex(dex,form);return r?r->count:learnCount(dex);
}
inline uint8_t formLearnLevel(int16_t dex,FormId form,uint8_t i) {
  auto r=formLearnIndex(dex,form);return r?(i<r->count?FORM_LEARN_ROWS[r->offset+i].level:255):learnLevel(dex,i);
}
inline MoveId formLearnMove(int16_t dex,FormId form,uint8_t i) {
  auto r=formLearnIndex(dex,form);return r?(i<r->count?FORM_LEARN_ROWS[r->offset+i].move:0):learnMove(dex,i);
}
inline uint8_t formLearnableList(int16_t dex,FormId form,uint8_t level,MoveId *out,uint8_t cap) {
  if(dex<1 || dex>DEX_COUNT)return 0;
  uint8_t n=0;
  auto add=[&](MoveId mv){if(!mv || mv>=MOVE_COUNT)return;for(int j=0;j<n;j++)if(out[j]==mv)return;if(n<cap)out[n++]=mv;};
  if(!formLearnIndex(dex,form))for(int i=0;i<evolutionMoveCount(dex);i++)add(evolutionMove(dex,i));
  for(int i=0;i<formLearnCount(dex,form);i++) {
    MoveId mv=formLearnMove(dex,form,i);if(!mv || mv>=MOVE_COUNT)continue;
    uint8_t at=formLearnLevel(dex,form,i);if((at?at:tmLevelFor(MOVE_TBL[mv]))<=level)add(mv);
  }
  return n;
}
// Keep selected slots in the save. Only strict form signatures become unusable
// outside their own form; ordinary previously learned moves are not deleted.
inline bool formMoveUsable(int16_t dex,FormId form,MoveId move) {
  const auto *f=formFind(dex,form);const char *key=f?f->key:"";
  if(dex==479) {
    const MoveId sig[]={MV_OVERHEAT,MV_HYDRO_PUMP,MV_BLIZZARD,MV_AIR_SLASH,MV_LEAF_STORM};
    const char *keys[]={"rotom-heat","rotom-wash","rotom-frost","rotom-fan","rotom-mow"};
    for(int i=0;i<5;i++)if(move==sig[i])return !strcmp(key,keys[i]);
  }
  if(dex==892 && (move==MV_WICKED_BLOW || move==MV_SURGING_STRIKES))return (move==MV_SURGING_STRIKES)==(!strcmp(key,"urshifu-rapid-strike"));
  if(dex==646 && (move==MV_FREEZE_SHOCK || move==MV_FUSION_BOLT))return !strcmp(key,"kyurem-black");
  if(dex==646 && (move==MV_ICE_BURN || move==MV_FUSION_FLARE))return !strcmp(key,"kyurem-white");
  if(move==MV_BEHEMOTH_BLADE)return dex==888 && !strcmp(key,"zacian-crowned");
  if(move==MV_BEHEMOTH_BASH)return dex==889 && !strcmp(key,"zamazenta-crowned");
  if(move==MV_HYPERSPACE_FURY)return dex==720 && !strcmp(key,"hoopa-unbound");
  return true;
}
