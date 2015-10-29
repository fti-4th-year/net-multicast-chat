#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CONN_ACTIVE 0x01
#define CONN_CLOSED 0x00

typedef struct {
	struct sockaddr_in addr;
	int status;
} conn_t;

#define CONN_LIST_MAX_SIZE 0x100
typedef struct {
	conn_t conns[CONN_LIST_MAX_SIZE];
	int size;
	pthread_mutex_t mutex;
} conn_list_t;

void conn_list_init(conn_list_t *cl) {
	int i;
	cl->size = 0;
	for(i = 0; i < CONN_LIST_MAX_SIZE; ++i) {
		cl->conns[i].status = CONN_CLOSED;
	}
	pthread_mutex_init(&cl->mutex, NULL);
}

void conn_list_destroy(conn_list_t *cl) {
	pthread_mutex_destroy(&cl->mutex);
}

int conn_list_add(conn_list_t *cl, struct sockaddr_in *addr) {
	int i;
	for(i = 0; i < CONN_LIST_MAX_SIZE; ++i) {
		if(cl->conns[i].status == CONN_CLOSED) {
			bcopy(addr, &cl->conns[i].addr, sizeof(struct sockaddr_in));
			cl->conns[i].status = CONN_ACTIVE;
			++cl->size;
			return 0;
		}
	}
	return 1;
}

int conn_list_remove(conn_list_t *cl, struct sockaddr_in *addr) {
	int i;
	for(i = 0; i < CONN_LIST_MAX_SIZE; ++i) {
		if(
		   cl->conns[i].status != CONN_CLOSED &&
		   cl->conns[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
		   cl->conns[i].addr.sin_port == addr->sin_port
		   ) {
			cl->conns[i].status = CONN_CLOSED;
			--cl->size;
			return 0;
		}
	}
	return 1;
}

void conn_list_lock(conn_list_t *cl) {
	pthread_mutex_lock(&cl->mutex);
}

void conn_list_unlock(conn_list_t *cl) {
	pthread_mutex_unlock(&cl->mutex);
}
