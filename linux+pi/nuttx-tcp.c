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
#include <sys/select.h>
#include <errno.h>

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

static int tcp_conn_socket = -1; // pseudo terminal used for communication with the IDE
static fd_set fdSet;
static struct timeval tv;

int serialConnected() {
	return tcp_conn_socket > -1;
}

int recvBytes(uint8 *buf, int count) {
	int readCount = 0;

	FD_ZERO(&fdSet);
	FD_SET(tcp_conn_socket, &fdSet);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int ret = select(tcp_conn_socket+1, &fdSet, NULL, NULL, &tv);
	if (ret == -1) {
		perror("select()");
	} else if (ret) {
		readCount = read(tcp_conn_socket, buf, count);
		if (readCount < 0) {
			readCount = 0;
			perror("Error recvBytes");
		}
	}
	return readCount;
}

int sendBytes(uint8 *buf, int start, int end) {
	int writtenBytes = 0;

	FD_ZERO(&fdSet);
	FD_SET(tcp_conn_socket, &fdSet);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int ret = select(tcp_conn_socket+1, NULL, &fdSet, NULL, &tv);
	if (ret == -1) {
		perror("select()");
	} else if (ret) {
		writtenBytes = write(tcp_conn_socket, &buf[start], end - start);
		if (writtenBytes < 0) {
			writtenBytes = 0;
			perror("Error sendBytes");
		}
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
	close(tcp_conn_socket);
}

void segfault() {
	printf("-- VM crashed --\n");
	exitGracefully();
}

void setupTcpConnection(void) {
	const int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	const int bool_true = 1;
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &bool_true, sizeof(bool_true));

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(9876);

	bind(tcp_socket, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	listen(tcp_socket, 1);
	tcp_conn_socket = accept(tcp_socket, NULL, NULL);
//	setsockopt(tcp_conn_socket, SOL_SOCKET, SO_KEEPALIVE, &bool_true, sizeof(bool_true));
//	setsockopt(tcp_conn_socket, IPPROTO_TCP, TCP_NODELAY, &bool_true, sizeof(bool_true));
//	int flags = fcntl(tcp_conn_socket, F_GETFL, 0);
//	fcntl(tcp_conn_socket, F_SETFL, flags | O_NONBLOCK);
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
