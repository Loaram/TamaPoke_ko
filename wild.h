#pragma once
#include <Arduino.h>

#define WILD_ENERGY_COST 30
#define WILD_LEVEL_SPREAD 5
#define WILD_SHINY_SCALE 100
#define WILD_SHINY_IV_FLOOR 20

enum WildResult : uint8_t {
  WILD_RESULT_NONE, WILD_RESULT_PARTY, WILD_RESULT_BOX,
  WILD_RESULT_ESCAPED, WILD_RESULT_LOST, WILD_RESULT_STORAGE_ERROR
};

// Encounter mix inherited from the Explore fork: legendary 1%, rare 7%,
// evolved 22%, common 70%.
uint8_t wildTierForRoll(uint8_t roll);
int16_t wildPickSpecies(uint8_t region, uint8_t tier, uint32_t roll);
uint8_t wildLevelMin(uint8_t playerLevel, bool hard);
uint8_t wildLevelMax(uint8_t playerLevel, bool hard);
bool wildShinyForRoll(uint32_t roll);
bool wildShinyNow();
void wildApplyShiny(bool shiny, uint8_t &ivAtk, uint8_t &ivDef,
                    uint8_t &ivSpe, uint8_t &ivHp);

// Generation III capture math at fixed 10% HP and no status.
// Every 100 collectible registered species adds 0.2 to the ball multiplier.
// Catch rate 3 uses 2.5% multiplied by the same bonus, not the shake formula.
uint8_t wildCatchRateForDex(int16_t dex);
uint16_t wildCaptureBallTenths(uint16_t registered);
uint16_t wildCaptureRarePermille(uint16_t registered);
uint32_t wildCaptureShakeThreshold(uint8_t catchRate, uint16_t registered = 0);
bool wildCaptureCheck(uint8_t catchRate, uint16_t rareRoll,
                      const uint16_t shakeRolls[4], uint16_t registered = 0);
bool wildCaptureNow(uint8_t catchRate, uint16_t registered = 0);
