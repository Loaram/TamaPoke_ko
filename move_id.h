#pragma once
#include <stdint.h>

// Existing releases stored move numbers in one byte and reached the 255-entry
// ceiling.  Natural learnsets across National Dex 1..1025 need more than twice
// that many entries, so every live, banked, battle and wire move ID uses this
// shared 16-bit type.  The old numeric IDs remain unchanged and migrate upward.
typedef uint16_t MoveId;
