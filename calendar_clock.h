#pragma once
#include <stdint.h>

// Care/quota/individual snapshot dates share one logical calendar. Only its
// offset changes when a save moves between devices with different RTC dates.
inline uint32_t careCalendarDay(uint32_t epoch, int32_t offset) {
  if (!epoch) return 0;
  int64_t day = (int64_t)(epoch / 86400UL) + offset;
  return day < 1 || day > UINT32_MAX / 86400UL ? 0 : (uint32_t)day;
}

// A stopped/reset RTC must not replace a newer saved time with January 1.
inline uint32_t recoverRtcEpoch(uint32_t rtc, uint32_t saved) {
  return rtc >= saved && rtc ? rtc : (saved ? saved : 1767225600UL);
}
