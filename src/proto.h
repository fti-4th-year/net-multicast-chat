#pragma once

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PROTO_LEAVE   0x00
#define PROTO_INFORM  0x01
#define PROTO_ACCEPT  0x02
#define PROTO_ENTER   0x03
#define PROTO_SEND    0x04

void proto_enter(char *buffer, int *len) {
	buffer[0] = PROTO_ENTER;
	buffer[1] = 0;
	*len = 2;
}

void proto_enter_addr(char *buffer, int *len, struct sockaddr_in *addr) {
	int ip = addr->sin_addr.s_addr;
	short port = addr->sin_port;
	buffer[0] = PROTO_ENTER;
	buffer[1] = 1;
	bcopy(&port, buffer + 2, 2);
	bcopy(&ip, buffer + 4, 4);
	*len = 8;
}

void proto_get_addr(char *buffer, int len, struct sockaddr_in *addr) {
	int ip;
	short port;
	if(buffer[1]) {
		bcopy(buffer + 2, &port, 2);
		bcopy(buffer + 4, &ip, 4);
		addr->sin_addr.s_addr = ip;
		addr->sin_port = port;
	}
}

void proto_leave(char *buffer, int *len) {
	buffer[0] = PROTO_LEAVE;
	buffer[1] = 0;
	*len = 2;
}

void proto_inform(char *buffer, int *len, struct sockaddr_in *addr) {
	int ip = addr->sin_addr.s_addr;
	short port = addr->sin_port;
	buffer[0] = PROTO_INFORM;
	buffer[1] = 1;
	bcopy(&port, buffer + 2, 2);
	bcopy(&ip, buffer + 4, 4);
	*len = 8;
}

void proto_accept(char *buffer, int *len) {
	buffer[0] = PROTO_ACCEPT;
	buffer[1] = 0;
	*len = 2;
}
