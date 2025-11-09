#!/bin/sh
# Build uBlocks for generic GNU/Linux (stripped-down version)
# Connect to it via pseudo terminal
#
# Prerequisites to run on 64-bit Linux (tested on Ubuntu 20.04):
#	sudo apt install gcc-multilib

gcc -m32 -std=c99 -Wall -Wno-unused-variable -Wno-unused-result -O3 \
	-D GNUBLOCKS \
	-D NUTTX \
	-I ../vm \
	nuttx-udp.c ../vm/*.c \
	-lm \
	-o vm_linux_stripped_udp_i386

gcc -std=c99 -Wall -O3 \
	pty2udp.c \
	-o pty2udp
