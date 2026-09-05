// Joy from the ball game, that leaving early still banks it, and that the three
// trainers raise BOND in proportion to the session.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <chrono>
#include <thread>
#include <cstdio>
uint32_t g_seed=0xC0FFEE; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop();
extern Pet pet;
extern bool gameOpen;
extern uint8_t gameScore;
extern uint8_t gameGain;
void startGame(); void leaveGame();
int main(){
  setvbuf(stdout, nullptr, _IONBF, 0);
  // This test only needs the real Pet and game-controller paths. Avoid a full
  // display loop so a renderer delay cannot mask a training regression.
  nvs().clear();
  pet.begin();
  pet.dbgHatchAs(25,false);

  printf("direct playResult:\n");
  for (int sc : {0, 3, 8, 20}) {
    pet.joy = 40;
    pet.playResult((uint8_t)sc);
    printf("  score %2d: joy 40 -> %u\n", sc, pet.joy);
  }
  // and the path that actually matters: start a game, score, leave early
  pet.joy = 40;
  pet.ivDef = 31;
  pet.trDef = 0;
  startGame();
  gameScore = 9;              // as if nine rallies had landed
  leaveGame();
  printf("\nleave early with score 9: joy 40 -> %u, DEF +%u, gameOpen=%d, record=%u\n",
         pet.joy, gameGain, (int)gameOpen, pet.gameHi);
  int bad = pet.joy > 40 ? 0 : 1;
  if (gameGain != 4) { printf("FAIL: result screen gain was not captured\n"); bad = 1; }

  // --- the ball game is DEFENCE's trainer now
  {
    Pet p; p.begin(); p.dbgHatchAs(25, false);
    p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 31;
    p.trDef = 0; p.energy = 100;
    uint8_t gained = p.playResult(40);
    printf("\nball game trains DEF: trDef 0 -> %u (reported +%u), energy 100 -> %u\n",
           p.trDef, gained, p.energy);
    if (gained != 18 || p.trDef != 18) {
      printf("FAIL: the ball game did not report its full defence gain\n"); bad = 1;
    }
    if (p.energy != 100 - DEF_TRAIN_ENERGY_COST) {
      printf("FAIL: defence training did not use the fixed energy cost\n"); bad = 1;
    }
  }

  // A high score earns more DEF but must not cost more energy than a low score.
  {
    Pet low, high;
    low.begin(); high.begin();
    low.dbgHatchAs(25, false); high.dbgHatchAs(25, false);
    low.energy = high.energy = 100;
    low.playResult(2); high.playResult(40);
    printf("fixed DEF energy: score 2 -> %u, score 40 -> %u\n", low.energy, high.energy);
    if (low.energy != high.energy || low.energy != 100 - DEF_TRAIN_ENERGY_COST) {
      printf("FAIL: defence energy still varies with score\n"); bad = 1;
    }
  }

  // --- every trainer bonds, and a bigger session bonds more
  {
    const char *names[3] = { "bag (ATK)", "reaction (SPE)", "ball (DEF)" };
    for (int t = 0; t < 3; t++) {
      uint8_t small = 0, big = 0;
      {
        Pet p; p.begin(); p.dbgHatchAs(25, false);
        p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 31;
        p.bond = 0;   // bondToday is private and starts at 0 on a fresh Pet
        if (t == 0) p.trainStrength(4); else if (t == 1) p.trainSpeed(2); else p.playResult(2);
        small = p.bond;
      }
      {
        Pet p; p.begin(); p.dbgHatchAs(25, false);
        p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 31;
        p.bond = 0;   // bondToday is private and starts at 0 on a fresh Pet
        if (t == 0) p.trainStrength(80); else if (t == 1) p.trainSpeed(40); else p.playResult(40);
        big = p.bond;
      }
      printf("  %-15s bond: small session +%u, full session +%u\n", names[t], small, big);
      if (!small) { printf("FAIL: %s gave no bond at all\n", names[t]); bad = 1; }
      if (big <= small) { printf("FAIL: %s does not bond more for a bigger session\n", names[t]); bad = 1; }
    }
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad;
}
