#ifndef PTP_H
#define PTP_H

struct timestamp {
  uint32_t seconds;
  uint32_t subseconds; // in 1s/2^31 units
};

void ptp_init();
void ptp_status();
void ptp_counters();
void ptp_timestamp(struct timestamp *now);
uint64_t ptp_ns_diff(struct timestamp *start, struct timestamp *end);

#endif // PTP_H
