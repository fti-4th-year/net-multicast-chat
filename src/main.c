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

int done = 0;

void sighandler(int signo) {
	done = 1;
}

typedef struct {
	int sock;
} cookie_t;

void *recv_main(void *cookie) {
	char buffer[0x400];
	int len;
	int sock = ((cookie_t *) cookie)->sock;
	
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
		
		printf("%s:%d: %s\n", inet_ntoa(other.sin_addr), ntohs(other.sin_port), buffer);
	}
}

int main(int argc, char *argv[]) {
	int ret, status;
	int sock;
	struct sockaddr_in me, addr;
	struct ip_mreq req;
	pthread_t thread;
	char ttl = 1;
	int reuse = 1, loop = 1;
	
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	
	if(argc < 3) {
		printf("usage: %s <multicast-ip> <multicast-port>\n", argv[0]);
		goto quit;
	}
	
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock < 0) {
		perror("socket");
		ret = 1;
		goto quit;
	}
	
	status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if(status < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		ret = 8;
		goto close_socket;
	}
	
	status = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	if(status < 0) {
		perror("setsockopt(IP_MULTICAST_LOOP)");
		ret = 9;
		goto close_socket;
	}
	
	status = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &ttl, sizeof(ttl));
	if(status < 0) {
		perror("setsockopt(IP_MULTICAST_TTL)");
		ret = 3;
		goto close_socket;
	}
	
	bzero(&me, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_port = htons(atoi(argv[2]));
	me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	
	status = bind(sock, (struct sockaddr *) &me, sizeof(me));
	if(status < 0) {
		perror("bind");
		ret = 2;
		goto close_socket;
	}
	
	req.imr_multiaddr.s_addr = inet_addr(argv[1]);
	req.imr_interface.s_addr = htonl(INADDR_ANY);
	
	status = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &req, sizeof(req));
	if(status < 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		ret = 5;
		goto close_socket;
	}
	
	cookie_t cookie;
	cookie.sock = sock;
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
			goto drop;
		}
		if(!(pfd.revents & POLLIN))
			continue;
		
		len = read(1, buffer, sizeof(buffer));
		if(len < 0) {
			perror("read");
			ret = 6;
			goto drop;
		}
		if(len > 1) {
			len = sendto(sock, buffer, len - 1, 0, (struct sockaddr *) &addr, sizeof(addr));
			if(len < 0) {
				ret = 4;
				goto drop;
			}
		}
	}
	
drop:
	setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*) &req, sizeof(req));
join_thread:
	done = 1;
	pthread_join(thread, NULL);
close_socket:
	close(sock);
quit:
	return 0;
}
