#pragma once
#include <stdint.h>
#include "dex.h"

// Stable PokeAPI form IDs, scoped by National Dex. Zero ALWAYS means the
// released base species. Unknown IDs are retained in saves, never renumbered.
using FormId = uint16_t;
enum FormClass : uint8_t { FC_MEGA, FC_REGIONAL, FC_PRIMAL, FC_FUSION, FC_MASK, FC_SPECIAL, FC_GIANT };
struct FormEntry {
  uint16_t dex, id;
  const char *key, *nameKo;
  uint8_t category, level, type1, type2;
  uint8_t hp, atk, def, spe, spa, spd;
  bool shinyArt;
};
extern const FormEntry FORM_TBL[];
extern const uint16_t FORM_COUNT;
extern const uint16_t FORM_DATA_TAG;
const FormEntry *formFind(int16_t dex, FormId id);
uint8_t formCount(int16_t dex);
const FormEntry *formAt(int16_t dex, uint8_t index);
bool formEligible(int16_t dex, FormId id, uint16_t level);
DexEntry formDex(int16_t dex, FormId id);
const char *formClassName(uint8_t category, bool korean);
void formSpriteName(char *out, unsigned cap, int16_t dex, FormId id, bool shiny);
