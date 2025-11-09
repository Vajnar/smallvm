/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// linux.c - Microblocks for NuttX

// John Maloney, December 2017
// Bernat Romagosa, February 2018
// Martin Vajnar, November 2025

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

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
#include <arpa/inet.h>

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

static int udp_socket = -1; // pseudo terminal used for communication with the IDE

int serialConnected() {
	return udp_socket > -1;
}

int recvBytes(uint8 *buf, int count) {
	int readCount = read(udp_socket, buf, count);
	if (readCount < 0) readCount = 0;
	return readCount;
}

int canReadByte() {
	int bytesAvailable;
	ioctl(udp_socket, FIONREAD, &bytesAvailable);
	return (bytesAvailable > 0);
}

int sendByte(char aByte) {
	return write(udp_socket, &aByte, 1);
}

int sendBytes(uint8 *buf, int start, int end) {
	return write(udp_socket, &buf[start], end - start);
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
	close(udp_socket);
}

void segfault() {
	printf("-- VM crashed --\n");
	exitGracefully();
}

void setupTcpConnection(void) {
	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	saddr.sin_port = htons(9877);

	struct sockaddr_in daddr;
	memset(&daddr, 0, sizeof(struct sockaddr_in));
	daddr.sin_family = AF_INET;
	daddr.sin_addr.s_addr = htonl(INADDR_ANY);
	daddr.sin_port = htons(9876);

	bind(udp_socket, (struct sockaddr *)&daddr, sizeof(struct sockaddr_in));
	connect(udp_socket, &saddr, sizeof(saddr));
	int flags = fcntl(udp_socket, F_GETFL, 0);
	fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK);
}

// Linux Main

int main(int argc, char *argv[]) {
	signal(SIGSEGV, segfault);
	signal(SIGINT, exit);
	atexit(exitGracefully);
	setupTcpConnection();
	printf(
		"Starting NuttX MicroBlocks...\n");
	initTimers();
	memInit();
	primsInit();
	outputString("Welcome to uBlocks for NuttX!");
	restoreScripts();
	startAll();
	vmLoop();
	return 0;
}
