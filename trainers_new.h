#pragma once
// First-story teams; see docs/GYMS-GALAR-PALDEA.ko.md for sources/adaptations.
// Galar: 8 gyms, Bede's interruption, 3 Cup finals, Leon (not Elite Four).
// Leon uses the Grookey-player / Cinderace variant, independent of live pet.
static const Trainer TRAINERS_GALAR[TRAINER_COUNT] = {
  { "MILO", "TURFFIELD", T_GRASS, 2, {{829,19},{830,20}} },
  { "NESSA", "HULBURY", T_WATER, 3, {{118,22},{846,23},{834,24}} },
  { "KABU", "MOTOSTOKE", T_FIRE, 3, {{38,25},{59,25},{851,27}} },
  { "BEA", "STOW-ON-SIDE", T_FIGHTING, 4, {{237,34},{675,34},{865,35},{68,36}} },
  { "OPAL", "BALLONLEA", T_FAIRY, 4, {{110,36,10326},{303,36},{468,37},{869,38}} },
  { "GORDIE", "CIRCHESTER", T_ROCK, 4, {{689,40},{213,40},{874,41},{839,42}} },
  { "PIERS", "SPIKEMUTH", T_DARK, 4, {{560,44},{687,45},{435,45},{862,46}} },
  { "RAIHAN", "HAMMERLOCKE", T_DRAGON, 4, {{526,46},{330,47},{844,46},{884,48}} },
  { "BEDE", "CUP CHALLENGE", T_FAIRY, 4, {{303,51},{282,51},{78,52,10322},{858,53}} },
  { "NESSA", "CUP FINALS", T_WATER, 5, {{768,51},{279,51},{847,52},{119,52},{834,53}} },
  { "BEA", "CUP FINALS", T_FIGHTING, 5, {{701,52},{853,52},{865,53},{870,53},{68,54}} },
  { "RAIHAN", "CUP FINALS", T_DRAGON, 5, {{324,53},{706,54},{776,54},{330,54},{884,55}} },
  { "LEON", "CHAMPION", T_FIRE, 6, {{681,62},{887,62},{612,63},{537,64},{815,64},{6,65}} },
};
static const Trainer GALAR_SHIELD_GHOST =
  { "ALLISTER", "STOW-ON-SIDE", T_GHOST, 4, {{562,34,10338},{778,34},{864,35},{94,36}} };
static const Trainer GALAR_SHIELD_ICE =
  { "MELONY", "CIRCHESTER", T_ICE, 4, {{873,40},{555,40,10336},{875,41},{131,42}} };
static const Trainer GALAR_SHIELD_FINAL =
  { "ALLISTER", "CUP FINALS", T_GHOST, 5, {{477,52},{609,52},{864,53},{855,53},{94,54}} };

static const Trainer TRAINERS_PALDEA[TRAINER_COUNT] = {
  { "KATY", "CORTONDO", T_BUG, 3, {{919,14},{917,14},{216,15}} },
  { "BRASSIUS", "ARTAZON", T_GRASS, 3, {{548,16},{928,16},{185,17}} },
  { "IONO", "LEVINCIA", T_ELECTRIC, 4, {{940,23},{939,23},{404,23},{429,24}} },
  { "KOFU", "CASCARRAFA", T_WATER, 3, {{976,29},{961,29},{740,30}} },
  { "LARRY", "MEDALI", T_NORMAL, 3, {{775,35},{982,35},{398,36}} },
  { "RYME", "MONTENEVERA", T_GHOST, 4, {{354,41},{778,41},{972,41},{849,42}} },
  { "TULIP", "ALFORNADA", T_PSYCHIC, 4, {{981,44},{282,44},{956,44},{671,45}} },
  { "GRUSHA", "GLASEADO", T_ICE, 4, {{873,47},{614,47},{975,47},{334,48}} },
  { "RIKA", "ELITE 4", T_GROUND, 5, {{340,57},{323,57},{232,57},{51,57},{980,58}} },
  { "POPPY", "ELITE 4", T_STEEL, 5, {{879,58},{462,58},{437,58},{823,58},{959,59}} },
  { "LARRY", "ELITE 4", T_FLYING, 5, {{357,59},{741,59,0,T_ELECTRIC,T_FLYING},{334,59},{398,59},{973,60}} },
  { "HASSEL", "ELITE 4", T_DRAGON, 5, {{715,60},{612,60},{691,60},{841,60},{998,61}} },
  { "GEETA", "CHAMPION", T_ROCK, 6, {{956,61},{673,61},{976,61},{713,61},{983,61},{970,62}} },
};
