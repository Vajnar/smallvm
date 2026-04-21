/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// Code for transferring data between IDE MicroBlocks (using
// pseudo-terminal) and smallVM (connected via TCP socket)

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

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
#include <poll.h>

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

void transferData() {
	const int nfds = 2;
	char buf[2][1024];
	int recv[2] = {0,0};
	int sent[2] = {0,0};
	struct pollfd fds[2];
	struct timespec timeout;
	timeout.tv_sec = 30;
	timeout.tv_nsec = 0;

	memset(fds, 0, sizeof(fds));
	for (int i = 0; i < 2; i++) {
		fds[i].fd = (i == 0) ? pty : tcp_socket;
		fds[i].events = POLLIN;
	}

	int ret = ppoll(fds, nfds, &timeout, NULL);
	if (ret > 0) {
		for (int i = 0; i < 2; i++) {
			if (fds[i].revents & POLLIN) {
				int readd = read(fds[i].fd, buf[i], sizeof(buf[0]));
				if (readd < 0) {
					perror("read()");
				} else if (readd) {
					recv[i] = readd;
					printf("%d = read(%d, %p, %ld)\n", readd, fds[i].fd, buf[i], sizeof(buf[0]));
				}
			}
		}
	}

	while (recv[0] > 0 || recv[1] > 0) {
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;
		memset(fds, 0, sizeof(fds));
		for (int i = 0; i < 2; i++) {
			if (recv[i] > 0) {
				fds[i].fd = (i == 0) ? tcp_socket : pty;
			} else {
				fds[i].fd = -1;
			}
			fds[i].events = POLLOUT;
		}

		ret = ppoll(fds, nfds, &timeout, NULL);
		if (ret > 0) {
			for (int i = 0; i < 2; i++) {
				if (fds[i].revents & POLLOUT) {
					int written = write(fds[i].fd, &buf[i][sent[i]], recv[i]);
					printf("%d = write(%d, %p, %d)\n\n", written, fds[i].fd, &buf[i][sent[i]], recv[i]);
					if (written < 0) {
						written = 0;
						perror("written(): ");
					}
					sent[i] += written;
					recv[i] -= written;
				}
			}
		}
	}
}

void dataLoop() {
	printf("TCP socket: %d\r\nPTY fd: %d\r\n", tcp_socket, pty);
	while(1) {
		transferData();
	}
}

void setupTcpConnection(void) {
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
//	const int bool_true = 1;

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
//	ret = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &bool_true, sizeof(bool_true));
//	int flags = fcntl(tcp_socket, F_GETFL, 0);
//	ret = fcntl(tcp_socket, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
	signal(SIGINT, exit);
	atexit(exitGracefully);
	setupTcpConnection();
	openPseudoTerminal();
	dataLoop();

	return 0;
}
