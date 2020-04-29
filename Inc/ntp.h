#ifndef NTP_H
#define NTP_H

void ntp_init();
void ntp_poll(uint8_t dest);
void ntp_poll_set(uint8_t active);

#endif // NTP_H
