#!/bin/sh

arm-none-eabi-gdb -ex "target extended-remote localhost:3333" build/ArchMax.elf
