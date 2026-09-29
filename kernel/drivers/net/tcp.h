#pragma once
#include <stdint.h>
#include <stddef.h>
int      tcp_socket(int family, int type);
int      tcp_connect(int sock, uint32_t ip, uint16_t port);
int      tcp_bind(int sock, uint32_t ip, uint16_t port);
int      tcp_listen(int sock, int backlog);
int      tcp_accept(int sock, uint32_t *client_ip, uint16_t *client_port);
int64_t  tcp_send(int sock, const void *buf, size_t n);
int64_t  tcp_recv(int sock, void *buf, size_t n);
void     tcp_close(int sock);
uint32_t dns_resolve(const char *hostname);
int      net_ping(uint32_t ip);
