/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// nuttx.c - Microblocks for NuttX

// John Maloney, December 2017
// Bernat Romagosa, February 2018
// Martin Vajnar, November 2025

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define DEBUG
#define CONFIG_INTERPRETERS_SMALLVM_TCP 1
#define CONFIG_INTERPRETERS_SMALLVM_TCP_PORT 9876

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> // still needed?
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h> // still needed?
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <ctype.h>
//#include <nuttx/config.h>

#include "mem.h"
#include "interp.h"
#include "persist.h"

// Timing Functions

static int startSecs = 0;

static void initTimers() {
	struct timeval now;
	gettimeofday(&now, NULL);
	startSecs = now.tv_sec;
}

uint32 microsecs() {
	struct timeval now;
	gettimeofday(&now, NULL);

	return (1000000 * (now.tv_sec - startSecs)) + now.tv_usec;
}

uint32 millisecs() {
	struct timeval now;
	gettimeofday(&now, NULL);

	return (1000 * (now.tv_sec - startSecs)) + (now.tv_usec / 1000);
}

uint64 totalMicrosecs() {
        // Returns a 64-bit integer containing microseconds since start.
	struct timeval now;
	gettimeofday(&now, NULL);
	return (1000000 * (now.tv_sec - startSecs)) + now.tv_usec;
}

void delay(int ms) {
	clock_t start = millisecs();
	while (millisecs() < start + ms);
}


// Communication/System Functions

#ifdef DEBUG
static void print_hex_dump(const unsigned char *buffer, size_t length) {
    for (size_t i = 0; i < length; i += 16) {
        // Print the hex values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < length) {
                printf("%02X ", buffer[i + j]);
            } else {
                printf("   "); // Print spaces for missing bytes
            }
        }

        // Print the ASCII representation
        printf(" |");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < length) {
                printf("%c", isprint(buffer[i + j]) ? buffer[i + j] : '.');
            }
        }
        printf("|\n");
    }
}
#endif

static int fd = -1; // pseudo terminal used for communication with the IDE
static fd_set fdSet;
static struct timeval tv;

int serialConnected() {
	return fd > -1;
}

int recvBytes(uint8 *buf, int count) {
	int readCount = 0;

	FD_ZERO(&fdSet);
	FD_SET(fd, &fdSet);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int ret = select(fd+1, &fdSet, NULL, NULL, &tv);
	if (ret == -1) {
		perror("select()");
	} else if (ret) {
		readCount = read(fd, buf, count);
		if (readCount < 0) {
			readCount = 0;
			perror("Error recvBytes: ");
		}
#ifdef DEBUG
		else if (readCount > 0) {
			printf("recvBytes: buf = %p, readCount = %d, count = %d\n", buf, readCount, count);
			print_hex_dump(buf, readCount);
		}
#endif
	}
	return readCount;
}

int sendBytes(uint8 *buf, int start, int end) {
	int writtenBytes = 0;

	FD_ZERO(&fdSet);
	FD_SET(fd, &fdSet);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int ret = select(fd+1, NULL, &fdSet, NULL, &tv);
	if (ret == -1) {
		perror("select()");
	} else if (ret) {
		writtenBytes = write(fd, &buf[start], end - start);
		if (writtenBytes < 0) {
			writtenBytes = 0;
			perror("Error sendBytes no: ");
		}
#ifdef DEBUG
		else if (writtenBytes > 0) {
			printf("sendBytes: &buf[start] = %p, start = %d, end = %d, writtenBytes = %d, to write() = %d\n", &buf[start], start, end, writtenBytes, end - start);
			print_hex_dump(&buf[start], writtenBytes);
		}
#endif
	}
	return writtenBytes;
}

int ideConnected() {
	return serialConnected();
}

// System Functions

const char * boardType() {
	return "NuttX";
}

void primSetUserLED(OBJ *args) {
	printf("Turning LED: %s\r\n", trueObj == args[0] ? "on" : "off");
}

// Stubs

int useTFT = 0;

void turnOffInternalNeoPixels() { }
OBJ primMBDisplayOff(int argCount, OBJ *args) { return falseObj; }
OBJ primButtonA(OBJ *args) { return falseObj; }
OBJ primButtonB(OBJ *args) { return falseObj; }
void stopTone() { }
OBJ primI2cGet(OBJ *args) { return int2obj(0); }
OBJ primI2cSet(OBJ *args) { return int2obj(0); }
OBJ primSPISend(OBJ *args) { return int2obj(0); }
OBJ primSPIRecv(OBJ *args) { return int2obj(0); }
void updateMicrobitDisplay() { }
void resetRadio() { }
void BLE_setEnabled(int enableFlag) { }
void handleMicosecondClockWrap() { }

// Stubs for IO primitives

OBJ primAnalogPins(OBJ *args) { return int2obj(0); }
OBJ primDigitalPins(OBJ *args) { return int2obj(0); }
OBJ primAnalogRead(int argCount, OBJ *args) { return int2obj(0); }
void primAnalogWrite(OBJ *args) { }
OBJ primDigitalRead(int argCount, OBJ *args) { return int2obj(0); }
void primDigitalWrite(OBJ *args) { }
void primDigitalSet(int pinNum, int flag) { };

// Stubs for other functions not used on Linux

void processFileMessage(int msgType, int dataSize, char *data) {}
void resetServos() {}
void stopPWM() {}
void systemReset() {}
void turnOffPins() {}
void stopServos() {}

// Persistence support

int initCodeFile(uint8 *flash, int flashByteCount) { return 0; }
void writeCodeFile(uint8 *code, int byteCount) { }
void writeCodeFileWord(int word) { }
void clearCodeFile(int ignore) { }

// Debug

static void exitGracefully() {
	close(fd);
}

void segfault() {
	printf("-- VM crashed --\n");
	exitGracefully();
}

#ifdef CONFIG_INTERPRETERS_SMALLVM_SERIAL
void setupConnection(void) {
	fd = open(CONFIG_INTERPRETERS_SMALLVM_SERIAL_DEVICE,
		O_NOCTTY | O_NONBLOCK | O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("setupConnection: open()");
		exit(-1);
	}

	struct termios settings;
	memset(&settings, 0, sizeof(settings));

	tcgetattr(fd, &settings);
	cfmakeraw(&settings);
	cfsetispeed(&settings, B115200);
	cfsetospeed(&settings, B115200);
	settings.c_cc[VMIN] = 0;
	settings.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSANOW, &settings);
	tcflush(fd, TCIOFLUSH);
}
#endif

#ifdef CONFIG_INTERPRETERS_SMALLVM_TCP
void setupConnection(void) {
	const int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	const int bool_true = 1;
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &bool_true, sizeof(bool_true));

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(CONFIG_INTERPRETERS_SMALLVM_TCP_PORT);

	bind(tcp_socket, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	listen(tcp_socket, 1);
	fd = accept(tcp_socket, NULL, NULL);
//	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &bool_true, sizeof(bool_true));
//	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &bool_true, sizeof(bool_true));
//	int flags = fcntl(fd, F_GETFL, 0);
//	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

// NuttX Main

int main(int argc, char *argv[]) {
	signal(SIGSEGV, segfault);
	signal(SIGINT, exit);
	atexit(exitGracefully);
	setupConnection();
	printf("Starting NuttX MicroBlocks...\n");
	initTimers();
	memInit();
	primsInit();
	outputString("Welcome to uBlocks for NuttX!");
	restoreScripts();
	startAll();
	vmLoop();
	return 0;
}
