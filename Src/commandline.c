#include "main.h"
#include "commandline.h"
#include "uart.h"
#include "ethernetif.h"
#include "ping.h"
#include "ptp.h"

#include <string.h>
#include <stdlib.h>

static void print_help() {
  write_uart_s(
  "commands:\n"
  "link - show state at link up\n"
  "phy - show PHY state\n"
  "blink - blink LED\n"
  "ping [ip] - ping ip\n"
  "ptp - ptp status\n"
  "count - ptp counters\n"
  "help - print help\n"
  );
}

static void link_status() {
  write_uart_s("link: ");
 
  if(heth.Init.DuplexMode == ETH_MODE_FULLDUPLEX) {
    write_uart_s("Full ");
  } else if(heth.Init.DuplexMode == ETH_MODE_HALFDUPLEX) {
    write_uart_s("Half ");
  } else {
    write_uart_s("???? ");
  }

  if(heth.Init.Speed == ETH_SPEED_10M) {
    write_uart_s("10M\n");
  } else if(heth.Init.Speed == ETH_SPEED_100M) {
    write_uart_s("100M\n");
  } else {
    write_uart_s("???\n");
  }
}

static void phy_status() {
  uint32_t phyreg;

  if((HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &phyreg)) != HAL_OK) {
    write_uart_s("phy read failed\n");
    return;
  }

  if((phyreg & 0b1) == 0) {
    write_uart_s("link: no\n");
  } else {
    write_uart_s("link: yes\n");
  }

  if((phyreg & 0b10) == 0) {
    write_uart_s("speed: 100M\n");
  } else {
    write_uart_s("speed: 10M\n");
  }

  if((phyreg & 0b100) == 0) {
    write_uart_s("duplex: half\n");
  } else {
    write_uart_s("duplex: full\n");
  }

  if((phyreg & 0b1000) != 0) {
    write_uart_s("loopback: on\n");
  }

  if((phyreg & 0b10000) == 0) {
    write_uart_s("auto-neg: incomplete\n");
  }

  if((phyreg & 0b100000) != 0) {
    write_uart_s("jabber detected\n");
  }

  if((phyreg & 0b1000000) != 0) {
    write_uart_s("remote fault detected\n");
  }

  if((phyreg & 0b100000000) != 0) {
    write_uart_s("received code word page\n");
  }
}

static void run_command(char *cmdline) {
  if(strcmp("blink", cmdline) == 0) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
  } else if(strcmp("link", cmdline) == 0) {
    link_status();
  } else if(strcmp("phy", cmdline) == 0) {
    phy_status();
  } else if(strcmp("ptp", cmdline) == 0) {
    ptp_status();
  } else if(strcmp("count", cmdline) == 0) {
    ptp_counters();
  } else if(strncmp("ping ", cmdline, 5) == 0) {
    ping_send(cmdline+5);
  } else {
    print_help();
  }
}

void cmdline_prompt() {
  write_uart_s("> ");
}

static char cmdline[40];
static uint32_t cmd_len = 0;

void reprint_prompt() {
  cmdline_prompt();
  if(cmd_len > 0) {
    write_uart_s(cmdline);
  }
}

void cmdline_addchr(char c) {
  switch(c) {
    case '\r':
    case '\n':
      write_uart_s("\n");
      run_command(cmdline);
      cmdline_prompt();
      cmdline[0] = '\0';
      cmd_len = 0;
      break;
    case '\b':
    case '\x7F':
      if(cmd_len > 0) {
        write_uart_s("\x7F");
        cmd_len--;
        cmdline[cmd_len] = '\0';
      }
      break;
    default:
      if(cmd_len < sizeof(cmdline)-1) {
        write_uart_ch(c);
        cmdline[cmd_len] = c;
        cmdline[cmd_len+1] = '\0';
        cmd_len = cmd_len + 1;
      }
  }
}
