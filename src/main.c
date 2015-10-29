#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "conn.h"
#include "proto.h"

int done = 0;

void sighandler(int signo) {
	done = 1;
}

int broadcast(int sock, conn_list_t *cl, char *buffer, int blen) {
	int i, len;
	for(i = 0; i < CONN_LIST_MAX_SIZE; ++i) {
		if(cl->conns[i].status != CONN_ACTIVE)
			continue;
		len = sendto(sock, buffer, blen, 0, (struct sockaddr *) &cl->conns[i].addr, sizeof(cl->conns[i].addr));
		if(len < 0) {
			perror("sendto");
			return -1;
		}
	}
	return 0;
}

typedef struct {
	int sock;
	conn_list_t *conn_list;
} cookie_t;

void *recv_main(void *cookie) {
	char buffer[0x400];
	int len;
	int sock = ((cookie_t *) cookie)->sock;
	conn_list_t *cl =  ((cookie_t *) cookie)->conn_list;
	
	while(!done) {
		struct sockaddr_in other;
		int slen = sizeof(struct sockaddr_in);
		struct pollfd pfd;
		
		bzero(&other, sizeof(struct sockaddr_in *));
		pfd.fd = sock;
		pfd.events = POLLIN;
		poll(&pfd, 1, 0x100);
		if(pfd.revents & (POLLNVAL | POLLHUP)) {
			perror("poll(sock)");
			return NULL;
		}
		if(!(pfd.revents & POLLIN))
			continue;
		
		len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *) &other, &slen);
		buffer[len] = '\0';
		if(len < 0) {
			perror("recv");
			return NULL;
		}
		
		char key = buffer[0];
		switch(key) {
		case PROTO_SEND:
			printf("%s:%d: %s\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port), buffer + 1);
			break;
			
		case PROTO_ENTER:
			proto_get_addr(buffer, len, &other);
			proto_accept(buffer, &len);
			len = sendto(sock, buffer, len, 0, (struct sockaddr *) &other, sizeof(other));
			if(len < 0) {
				perror("sendto");
				return NULL;
			}
			proto_inform(buffer, &len, &other);
			conn_list_lock(cl);
			len = broadcast(sock, cl, buffer, len);
			conn_list_add(cl, &other);
			conn_list_unlock(cl);
			if(len < 0) {
				return NULL;
			}
			printf("%s:%d entered\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port));
			break;
			
		case PROTO_LEAVE:
			conn_list_lock(cl);
			conn_list_remove(cl, &other);
			conn_list_unlock(cl);
			printf("%s:%d left\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port));
			break;
			
		case PROTO_INFORM:
			proto_get_addr(buffer, len, &other);
			proto_accept(buffer, &len);
			len = sendto(sock, buffer, len, 0, (struct sockaddr *) &other, sizeof(other));
			if(len < 0) {
				perror("sendto");
				return NULL;
			}
			conn_list_lock(cl);
			conn_list_add(cl, &other);
			conn_list_unlock(cl);
			printf("%s:%d invited\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port));
			break;
			
		case PROTO_ACCEPT:
			conn_list_lock(cl);
			conn_list_add(cl, &other);
			conn_list_unlock(cl);
			printf("%s:%d accepted\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port));
			break;
			
		default:
			fprintf(stderr, "unknown message key %d\n", (int) key);
			break;
		}
	}
}

int main(int argc, char *argv[]) {
	int port;
	int ret, sock, status;
	struct sockaddr_in me;
	pthread_t thread;
	conn_list_t conn_list;
	char tbuf[8];
	int tlen = 8;
	
	conn_list_init(&conn_list);
	
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	
	if(argc < 2) {
		printf("usage: multicast <port> [<target-ip> <target-port>]\n");
		goto quit;
	}
	
	port = atoi(argv[1]);
	
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock < 0) {
		perror("socket");
		ret = 1;
		goto quit;
	}
	
	bzero(&me, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_port = htons(port);
	me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	status = bind(sock, (struct sockaddr *) &me, sizeof(me));
	if(status < 0) {
		perror("bind");
		ret = 2;
		goto close_socket;
	}
	
	if(argc >= 4) {
		struct sockaddr_in target;
		struct hostent *he;
		char buffer[2];
		int len = 2;
		
		bzero(&target, sizeof(target));
		target.sin_family = AF_INET;
		target.sin_port = htons(atoi(argv[3]));
		target.sin_addr.s_addr = htonl(INADDR_ANY);
		he = gethostbyname(argv[2]);
		if(he == NULL) {
			fprintf(stderr, "error resolving hostme '%s'\n", argv[2]);
			goto close_socket;
		}
		bcopy((char*) he->h_addr, (char*) &target.sin_addr.s_addr, he->h_length);
		
		proto_enter(buffer, &len);
		len = sendto(sock, buffer, len, 0, (struct sockaddr *) &target, sizeof(target));
		if(len < 0) {
			perror("sendto");
			ret = 8;
			goto close_socket;
		}
	}
	
	cookie_t cookie;
	cookie.sock = sock;
	cookie.conn_list = &conn_list;
	pthread_create(&thread, NULL, recv_main, &cookie);
	
	while(!done) {
		char buffer[0x400];
		int len;
		struct pollfd pfd;
		
		pfd.fd = 1;
		pfd.events = POLLIN;
		poll(&pfd, 1, 0x100);
		if(pfd.revents & (POLLNVAL | POLLHUP)) {
			perror("poll(1)");
			ret = 7;
			goto join_thread;
		}
		if(!(pfd.revents & POLLIN))
			continue;
		
		len = read(1, buffer + 1, sizeof(buffer) - 1);
		if(len < 0) {
			perror("read");
			ret = 6;
			goto join_thread;
		}
		if(len > 1) {
			buffer[0] = PROTO_SEND;
			conn_list_lock(&conn_list);
			len = broadcast(sock, &conn_list, buffer, len);
			conn_list_unlock(&conn_list);
			if(len < 0) {
				ret = 4;
				goto join_thread;
			}
		}
	}
	
join_thread:
	done = 1;
	proto_leave(tbuf, &tlen);
	broadcast(sock, &conn_list, tbuf, tlen);
	pthread_join(thread, NULL);
close_socket:
	close(sock);
quit:
	conn_list_destroy(&conn_list);
	return 0;
}
