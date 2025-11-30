/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// Code for transferring data between IDE MicroBlocks (using
// pseudo-terminal) and smallVM (connected via TCP socket)

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define DEBUG

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>

static int pty;
static int tcp_socket;

static void makePtyFile() {
	FILE *file = fopen("/tmp/ublocksptyname", "w");
	if (file) {
		fprintf(file, "%s", (char*) ptsname(pty));
		fclose(file);
	}
}

static void exitGracefully() {
	remove("/tmp/ublocksptyname");
	close(tcp_socket);
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

void transferData(int from_fd, int to_fd) {
	unsigned char buf[1500];

	int received = read(from_fd, buf, sizeof(buf));
	if (received > 0) {
		int sent = 0;
#ifdef DEBUG
		printf("transferData: read() of %d from %d\n", received, from_fd);
		print_hex_dump(buf, received);
#endif
		while(sent < received) {
			int written = write(to_fd, &buf[sent], received - sent);
			if (written < 0) written = 0;
#ifdef DEBUG
			else if (written > 0) {
				printf("transferData: write() of %d to %d\n", written, to_fd);
				print_hex_dump(&buf[sent], written);
			}
#endif
			sent += written;
		}
	}
}
void dataLoop() {
	printf("TCP socket: %d\r\nPTY fd: %d\r\n", tcp_socket, pty);
	while(1) {
		transferData(pty, tcp_socket);
		transferData(tcp_socket, pty);
		usleep(1000);
	}
}

void setupTcpConnection(void) {
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	const int bool_true = 1;

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	saddr.sin_port = htons(9876);

	int ret = connect(tcp_socket, &saddr, sizeof(saddr));
	if (ret < 0) {
		perror(NULL);
		exit(-1);
	}
//	ret = setsockopt(tcp_socket, SOL_SOCKET, SO_KEEPALIVE, &bool_true, sizeof(bool_true));
	ret = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &bool_true, sizeof(bool_true));
	int flags = fcntl(tcp_socket, F_GETFL, 0);
	ret = fcntl(tcp_socket, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
	signal(SIGINT, exit);
	atexit(exitGracefully);
	setupTcpConnection();
	openPseudoTerminal();
	dataLoop();

	return 0;
}
