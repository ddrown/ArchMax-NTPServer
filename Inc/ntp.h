#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void TXTimestampCallback(uint32_t TimeStampLow, uint32_t TimeStampHigh, uint32_t length, uint8_t *packet);
void ntp_init();
void expire_clients();

#ifdef __cplusplus
};

extern NTPClients clientList;
#endif
