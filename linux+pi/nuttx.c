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

static int pty; // pseudo terminal used for communication with the IDE

int serialConnected() {
	return pty > -1;
}

static void makePtyFile() {
	FILE *file = fopen("/tmp/ublocksptyname", "w");
	if (file) {
		fprintf(file, "%s", (char*) ptsname(pty));
		fclose(file);
	}
}

static void exitGracefully() {
	remove("/tmp/ublocksptyname");
	exit(0);
}

static void openPseudoTerminal() {
	pty = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (-1 == pty) {
		perror("Error opening pseudo terminal\n");
		exit(-1);
	}

	struct termios settings;
	tcgetattr(pty, &settings);
	cfmakeraw(&settings);
	tcsetattr(pty, TCSANOW, &settings);

 	grantpt(pty);
 	unlockpt(pty);

	makePtyFile();
}

int recvBytes(uint8 *buf, int count) {
	int readCount = read(pty, buf, count);
	if (readCount < 0) readCount = 0;
	return readCount;
}

int canReadByte() {
	int bytesAvailable;
	ioctl(pty, FIONREAD, &bytesAvailable);
	return (bytesAvailable > 0);
}

int sendByte(char aByte) {
	return write(pty, &aByte, 1);
}

int sendBytes(uint8 *buf, int start, int end) {
	return write(pty, &buf[start], end - start);
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

void segfault() {
	printf("-- VM crashed --\n");
	exitGracefully();
}

// Linux Main

int main(int argc, char *argv[]) {
	signal(SIGSEGV, segfault);
	signal(SIGINT, exit);
	atexit(exitGracefully);
	openPseudoTerminal();
	printf(
		"Starting NuttX MicroBlocks... Connect on %s\n",
		(char*) ptsname(pty));
	initTimers();
	memInit(10000); // 10k words = 40k bytes
	primsInit();
	outputString("Welcome to uBlocks for NuttX!");
	restoreScripts();
	startAll();
	vmLoop();
	return 0;
}
