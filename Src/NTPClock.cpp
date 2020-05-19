#include <cstddef>

#include "NTPClock.h"
#include "platform-clock.h"

void NTPClock::setTime(uint32_t micros, uint32_t ntpTimestamp) {
  timeset_ = 1;
  lastMicros_ = micros;
  ntpTimestamp_.units[NTPCLOCK_SECONDS] = ntpTimestamp;
  ntpTimestamp_.units[NTPCLOCK_FRACTIONAL] = 0;
}

uint8_t NTPClock::getTime(uint32_t *ntpTimestamp, uint32_t *ntpFractional) {
  uint32_t now = COUNTERFUNC();

  return getTime(now, ntpTimestamp, ntpFractional);
}

// takes around 28us on an esp8266 80MHz
// takes around 3us on an stm407 168MHz
uint8_t NTPClock::getTime(uint32_t now, uint32_t *ntpTimestamp, uint32_t *ntpFractional) {
  if (!timeset_)
    return 0;

  temp_.whole = countToNTP(now);
  if(ntpTimestamp != NULL)
    *ntpTimestamp = temp_.units[NTPCLOCK_SECONDS];
  if(ntpFractional != NULL)
    *ntpFractional = temp_.units[NTPCLOCK_FRACTIONAL];

  return 1;
}

// returns + for local slower, - for local faster
// takes around 33us on an esp8266 80MHz
int64_t NTPClock::getOffset(uint32_t now, uint32_t ntpTimestamp, uint32_t ntpFractional) {
  uint32_t localS, localFS;
  if (getTime(now, &localS, &localFS) != 1) {
    return 0;
  }

  int32_t diffS = ntpTimestamp - localS; // assumption: clocks are within 2^31s
  int64_t diffFS = (int64_t)ntpFractional - localFS;
  diffFS += diffS * 4294967296LL;

  return diffFS;
}

uint64_t NTPClock::countToNTP(uint32_t count) {
  uint64_t ntpFracPassed = (count - lastMicros_) * 4294967296LL / (uint64_t)COUNTSPERSECOND;
  ntpFracPassed = ntpFracPassed * 1000000000LL / (1000000000LL - ppb_);
  return ntpTimestamp_.whole + ntpFracPassed;
}

// use + for local slower, - for local faster
void NTPClock::setPpb(uint32_t now, int32_t ppb) {
  // update clock using last ppb_ setting
  ntpTimestamp_.whole = countToNTP(now);
  lastMicros_ = now;

  if(ppb >= -2000000 && ppb <= 2000000) { // limited to +/-2000ppm
    ppb_ = ppb;
  }
}
