#ifndef NTP_H
#define NTP_H

void ntp_init();
void ntp_send(const char *dest);
void ntp_poll();
void ntp_poll_set(uint8_t active);

#endif // NTP_H
