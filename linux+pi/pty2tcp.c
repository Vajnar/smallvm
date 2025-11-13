
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

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

void transferData(int from_fd, int to_fd) {
	char buf[1024];
	int received, sent;

	received = read(from_fd, buf, sizeof(buf));
	if (received > 0) {
		sent = 0;
		while(sent < received) {
			int written = write(to_fd, &buf[sent], received - sent);
			if (written < 0) written = 0;
			sent += written;
		}
	}
}
void dataLoop() {
	printf("TCP socket: %d\r\nPTY fd: %d\r\n", tcp_socket, pty);
	while(1) {
		transferData(pty, tcp_socket);
		transferData(tcp_socket, pty);
		usleep(10000);
	}
}

void setupTcpConnection(void) {
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	const int bool_true = 1;

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr("192.168.1.106");
	saddr.sin_port = htons(9876);

	int ret = connect(tcp_socket, &saddr, sizeof(saddr));
	if (ret < 0) {
		perror(NULL);
		exit(-1);
	}
//	ret = setsockopt(tcp_socket, SOL_SOCKET, SO_KEEPALIVE, &bool_true, sizeof(bool_true));
//	ret = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &bool_true, sizeof(bool_true));
	int flags = fcntl(tcp_socket, F_GETFL, 0);
	ret = fcntl(tcp_socket, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
	signal(SIGINT, exit);
	atexit(exitGracefully);
	setupTcpConnection();
	openPseudoTerminal();
	dataLoop();
}
