/*
 * libnotux — network API (kernel net is not yet functional)
 */

#include <notux/libc.h>
#include <notux/syscalls.h>
#include "nx.h"

nx_sock_t nx_socket(int family, int type) {
    return (nx_sock_t)nx_syscall(SYS_SOCKET, family, type, 0);
}

int nx_connect(nx_sock_t s, uint32_t ip, uint16_t port) {
    return (int)nx_syscall(SYS_CONNECT, s, ip, port);
}

int nx_bind(nx_sock_t s, uint32_t ip, uint16_t port) {
    return (int)nx_syscall(SYS_BIND, s, ip, port);
}

int nx_listen(nx_sock_t s, int backlog) {
    return (int)nx_syscall(SYS_LISTEN, s, backlog, 0);
}

nx_sock_t nx_accept(nx_sock_t s, uint32_t *client_ip, uint16_t *client_port) {
    return (nx_sock_t)nx_syscall(SYS_ACCEPT, s,
                                 (long)(uintptr_t)client_ip,
                                 (long)(uintptr_t)client_port);
}

int64_t nx_send(nx_sock_t s, const void *buf, size_t n) {
    return nx_syscall(SYS_SEND, s, (long)(uintptr_t)buf, (long)n);
}

int64_t nx_recv(nx_sock_t s, void *buf, size_t n) {
    return nx_syscall(SYS_RECV, s, (long)(uintptr_t)buf, (long)n);
}

void nx_closesock(nx_sock_t s) {
    nx_syscall(SYS_SOCKCLOSE, s, 0, 0);
}

uint32_t nx_resolve(const char *hostname) {
    return (uint32_t)nx_syscall(SYS_RESOLVE, (long)(uintptr_t)hostname, 0, 0);
}

int nx_ping(uint32_t ip) {
    return (int)nx_syscall(SYS_PING, ip, 0, 0);
}