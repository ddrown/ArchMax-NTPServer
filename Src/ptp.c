#include "main.h"
#include "ptp.h"
#include "ethernetif.h"
#include "uart.h"

#define ETH_PTPTSSR_TSFFMAE_Pos                       (18U)
#define ETH_PTPTSSR_TSFFMAE_Msk                       (0x1UL << ETH_PTPTSSR_TSFFMAE_Pos)
#define ETH_PTPTSSR_TSFFMAE                           ETH_PTPTSSR_TSFFMAE_Msk  /* Time stamp PTP frame filtering MAC address enable */

// these aren't the right offsets in the headers
#undef ETH_PTPTSSR_TSTTR_Pos
#undef ETH_PTPTSSR_TSTTR_Msk
#undef ETH_PTPTSSR_TSTTR
#undef ETH_PTPTSSR_TSSO_Pos
#undef ETH_PTPTSSR_TSSO_Msk
#undef ETH_PTPTSSR_TSSO
#define ETH_PTPTSSR_TSTTR_Pos                         (1U)
#define ETH_PTPTSSR_TSTTR_Msk                         (0x1UL << ETH_PTPTSSR_TSTTR_Pos) /*!< 0x00000020 */
#define ETH_PTPTSSR_TSTTR                             ETH_PTPTSSR_TSTTR_Msk    /* Time stamp target time reached */
#define ETH_PTPTSSR_TSSO_Pos                          (0U)
#define ETH_PTPTSSR_TSSO_Msk                          (0x1UL << ETH_PTPTSSR_TSSO_Pos) /*!< 0x00000010 */
#define ETH_PTPTSSR_TSSO                              ETH_PTPTSSR_TSSO_Msk     /* Time stamp seconds overflow */

/*
 * the clock was running at s/cycle
 * 1/168000000            = 0.000000005952
 * PTPSSIR determines the increment in s/cycle
 * 13/(2^31)              = 0.000000006053
 * so it needs to be this % slow -- not enough precision
 * 6053/5952              = 1.016969086
 * PTPTSAR slows the clock down by a %
 * (2-1.016969086)*(2^32) = 4222085626
 *
 * calculating new updates/s
 * (4227995019*168000000) / (2^32) = 165380342
 * which ends up being s/s
 * (165380342*13)/(2^31)           = 1.001145898
 *
 * default setting
 * (4223155695*168000000) / (2^32) * 13/(2^31) = 1.000000000111
 *
 * running 1500ppm fast
 * (4229490428 *168000000) / (2^32) * 13/(2^31) = 1.001499999982
 * running 1500ppm slow
 * (4216820962 *168000000) / (2^32) * 13/(2^31) = .998500000239
 * 6334733 counts per 1500ppm
 */
void ptp_init() {
  heth.Instance->PTPTSCR |= ETH_PTPTSCR_TSE; // enable PTP system
  heth.Instance->PTPTSCR &= ~ETH_PTPTSSR_TSSSR_Msk; // use 2^31 subseconds
  heth.Instance->PTPTSAR = PTP_INCREMENT_DEFAULT;
  heth.Instance->PTPSSIR = 13;
  while(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSARU_Msk) { // wait for ARU to clear
    // TODO: timeout
  }
  heth.Instance->PTPTSCR |= ETH_PTPTSCR_TSARU | ETH_PTPTSCR_TSFCU; // set fine clock control & update addend

  heth.Instance->PTPTSHUR = 3762467957; // start at Mar 25 01:59:17 UTC 2019
  heth.Instance->PTPTSLUR = 0; // and subsec=0
  heth.Instance->PTPTSCR |= ETH_PTPTSCR_TSSTI; // init time

  heth.Instance->PTPTSCR |= ETH_PTPTSSR_TSSARFE; // Time stamp snapshot for all received frames

  heth.Instance->MACIMR |= ETH_MACIMR_TSTIM; // turn off timestamp trigger irq
}

void ptp_set_step(uint8_t step) {
  heth.Instance->PTPSSIR = step;
}

void ptp_set_freq_div(int32_t ppb) {
  // 6334733 counts per 1500000ppb
  int32_t newcount = ppb * 4.223155333333f;
  heth.Instance->PTPTSAR = newcount + PTP_INCREMENT_DEFAULT;
  while(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSARU_Msk) { // wait for ARU to clear
    // TODO: timeout
  }
  heth.Instance->PTPTSCR |= ETH_PTPTSCR_TSARU; // update addend
}

static void ptp_set_update_time() {
  while(heth.Instance->PTPTSCR & (ETH_PTPTSCR_TSSTI_Msk|ETH_PTPTSCR_TSSTU_Msk)) { // wait for previous time set/update to finish
    // TODO: timeout
  }
  heth.Instance->PTPTSCR |= ETH_PTPTSCR_TSSTU; // apply update
}

void ptp_update_s(int32_t sec) {
  if(sec < 0) {
    sec = sec * -1;
    heth.Instance->PTPTSLUR = ETH_PTPTSLUR_TSUPNS; // mark it as a negative offset
  } else {
    heth.Instance->PTPTSLUR = 0;
  }
  heth.Instance->PTPTSHUR = sec;
  ptp_set_update_time();
}

void ptp_update_subs(int32_t subs) {
  heth.Instance->PTPTSHUR = 0;
  if(subs < 0) {
    subs = subs * -1;
    heth.Instance->PTPTSLUR = ETH_PTPTSLUR_TSUPNS | (uint32_t)subs;
  } else {
    heth.Instance->PTPTSLUR = subs;
  }
  ptp_set_update_time();
}

void ptp_status() {
  write_uart_s("conf: ");
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSE) {
    write_uart_s("enable ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSFCU) {
    write_uart_s("fine ");
  } else {
    write_uart_s("coarse ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSSTI) {
    write_uart_s("init ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSSTU) {
    write_uart_s("update ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSITE) {
    write_uart_s("irq ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSARU) {
    write_uart_s("addend ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSARFE) {
    write_uart_s("all ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSSR) {
    write_uart_s("1bill ");
  } else {
    write_uart_s("31bit ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSPTPPSV2E) {
    write_uart_s("PTPv2 ");
  } else {
    write_uart_s("PTPv1 ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSPTPOEFE) {
    write_uart_s("PTPoE ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSIPV6FE) {
    write_uart_s("PTPo6 ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSIPV4FE) {
    write_uart_s("PTPo4 ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSEME) {
    write_uart_s("events ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSSMRME) {
    write_uart_s("master ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSCR_TSCNT) {
    write_uart_s("type=");
    write_uart_u((heth.Instance->PTPTSCR & ETH_PTPTSCR_TSCNT) >> ETH_PTPTSCR_TSCNT_Pos);
    write_uart_s(" ");
  }
  if(heth.Instance->PTPTSCR & ETH_PTPTSSR_TSFFMAE) {
    write_uart_s("mac");
  }
  write_uart_s("\n");

  write_uart_s("status: ");
  if(heth.Instance->PTPTSSR & ETH_PTPTSSR_TSTTR) {
    write_uart_s("target ");
  }
  if(heth.Instance->PTPTSSR & ETH_PTPTSSR_TSSO) {
    write_uart_s("overflow");
  }
  write_uart_s("\n");
}

void ptp_timestamp(struct timestamp *now) {
  now->seconds = heth.Instance->PTPTSHR;
  now->subseconds = heth.Instance->PTPTSLR;
}

uint64_t ptp_now() {
  struct timestamp now;

  now.seconds = heth.Instance->PTPTSHR;
  now.subseconds = heth.Instance->PTPTSLR;

  return (uint64_t)now.seconds << 32 | now.subseconds << 1;
}

#define SUBSECONDS_PER_SECOND 2147483648

// limited to around 18 seconds
// assumes timestamps are in 2^31 subsecond format
uint64_t ptp_ns_diff(const struct timestamp *start, const struct timestamp *end) {
  uint64_t ns = 0;

  // TODO: counter wraps
  if(end->seconds > start->seconds) {
    ns = end->seconds - start->seconds;
    ns *= SUBSECONDS_PER_SECOND;
  }

  ns += end->subseconds;
  ns -= start->subseconds;
  ns = ns * 1000000000 / SUBSECONDS_PER_SECOND;
  return ns;
}

volatile uint8_t ptp_pending = 0;
void ptp_set_target() {
  uint32_t seconds = 1;
  if(ptp_pending) {
    return;
  }

  if(heth.Instance->PTPTSLR > 2000000000) {
    seconds++; // allow at least 68 ms
  }
  seconds += heth.Instance->PTPTSHR;
  heth.Instance->PTPTTHR = seconds;
  heth.Instance->PTPTTLR = 0;
  ptp_pending = 1;
}

void ptp_counters() {
  struct timestamp now;

  ptp_timestamp(&now);

  write_uart_s("subs=");
  write_uart_u(heth.Instance->PTPSSIR);
  write_uart_s(" add=");
  write_uart_u(heth.Instance->PTPTSAR);
  write_uart_s("\ntime=");
  write_uart_u(now.seconds);
  write_uart_s(".");
  write_uart_u(now.subseconds);
  write_uart_s("\nirq=");
  write_uart_u(heth.Instance->PTPTTHR);
  write_uart_s(".");
  write_uart_u(heth.Instance->PTPTTLR);
  write_uart_s("\n");
}
