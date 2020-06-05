Hardware: Seeed's Arch Max with STM32F407 + 100M ethernet

Software: built on STM32CubeMX + LwIP

There's a simple command line interface on the debug serial port (USB J8)

It expects a GPS to be connected to PD6 (uart RX) and PA3 (GPS PPS)

Build system is make + arm-none-eabi-gcc

Programming is done via openocd
