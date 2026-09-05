#pragma once
#include <Arduino.h>

#define WILD_ENERGY_COST 30
#define WILD_LEVEL_SPREAD 5
#define WILD_SHINY_SCALE 4096
#define WILD_SHINY_IV_FLOOR 20

// Encounter mix inherited from the Explore fork: legendary 1%, rare 7%,
// evolved 22%, common 70%.
uint8_t wildTierForRoll(uint8_t roll);
int16_t wildPickSpecies(uint8_t region, uint8_t tier, uint32_t roll);
uint8_t wildLevelMin(uint8_t playerLevel, bool hard);
uint8_t wildLevelMax(uint8_t playerLevel, bool hard);
bool wildShinyForRoll(uint32_t roll);
void wildApplyShiny(bool shiny, uint8_t &ivAtk, uint8_t &ivDef,
                    uint8_t &ivSpe, uint8_t &ivHp);

// Generation III Pokeball capture math at a fixed 10% HP and no status.
// Species whose canonical catch rate is 3 use the beta balance override: 2.5%.
uint8_t wildCatchRateForDex(int16_t dex);
uint32_t wildCaptureShakeThreshold(uint8_t catchRate);
bool wildCaptureCheck(uint8_t catchRate, uint16_t rareRoll,
                      const uint16_t shakeRolls[4]);
bool wildCaptureNow(uint8_t catchRate);
