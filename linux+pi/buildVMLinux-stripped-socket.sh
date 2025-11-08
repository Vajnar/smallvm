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
	nuttx-socket.c ../vm/*.c \
	-lm \
	-o vm_linux_stripped_socket_i386

gcc -std=c99 -Wall -O3 \
	pty2socket.c \
	-o pty2socket
