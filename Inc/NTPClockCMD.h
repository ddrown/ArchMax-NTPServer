#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void setntp();
void getntp();
void ntpoffset();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern NTPClock localClock;
#endif
