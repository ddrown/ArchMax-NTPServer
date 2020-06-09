#ifndef PTP_H
#define PTP_H

#define PTP_INCREMENT_DEFAULT 4223155695

struct timestamp {
  uint32_t seconds;
  uint32_t subseconds; // in 1s/2^31 units
};
extern volatile uint8_t ptp_pending;

void ptp_init();
void ptp_status();
void ptp_counters();
void ptp_timestamp(struct timestamp *now);
uint64_t ptp_ns_diff(const struct timestamp *start, const struct timestamp *end);
void ptp_set_step(uint8_t step);
void ptp_set_freq_div(int32_t div);
void ptp_update_s(int32_t sec);
void ptp_update_subs(int32_t subs);
uint64_t ptp_now();
uint32_t ptp_seconds();
void ptp_set_target();

#endif // PTP_H
