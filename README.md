Hardware: Seeed's Arch Max with STM32F407 + 100M ethernet

Software: built on STM32CubeMX + LwIP

It will poll the NTP servers listed in Src/ntp.c:dests every second and output to the debug serial port (USB J8)

There's a simple command line interface

Build system is make + arm-none-eabi-gcc

Programming is done via openocd
