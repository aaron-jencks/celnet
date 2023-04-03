#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../celnet.h"

size_t determine_char_slength(uint8_t i) {
    if (i >= 100) return 3;
    if (i >= 10) return 2;
    return 1;
}

size_t determine_short_slength(uint16_t i) {
    if (i >= 10000) return 5;
    if (i >= 1000) return 4;
    if (i >= 100) return 3;
    if (i >= 10) return 2;
    return 1;
}

char* display_ip(struct sockaddr_in* addr, socklen_t addrlen) {
    uint32_t ip = ntohl(addr->sin_addr.s_addr);
    uint8_t b1 = (uint8_t)(ip >> 24), b2 = (uint8_t)(ip >> 16), b3 = (uint8_t)(ip >> 8), b4 = (uint8_t)ip;
    uint16_t port = ntohs(addr->sin_port);
    size_t slen = 5;

    // Determine length of string
    slen += determine_char_slength(b1);
    slen += determine_char_slength(b2);
    slen += determine_char_slength(b3);
    slen += determine_char_slength(b4);
    slen += determine_short_slength(port);

    char* return_value = malloc(sizeof(char)*slen);

    return_value[--slen] = 0;
    snprintf(return_value, sizeof(char)*slen, "%hhu.%hhu.%hhu.%hhu:%hu", b1, b2, b3, b4, port);

    return return_value;
}

void echo_handler(int sock, sockaddr_t* addr, socklen_t addr_len, char* data, size_t data_len) {
    printf("%s\n", data);
    send(sock, (void*)data, data_len, 0);
}

typedef struct {
    pthread_t cthread;
    int fd;
    sockaddr_t* addr;
    socklen_t addrlen;
} disconnection_args_t;

void* client_disconnection_listener(void* args) {
    disconnection_args_t* dargs = (disconnection_args_t*)args;
    int dret = pthread_join(dargs->cthread, NULL);
    if (dret) {
        return NULL;
    }
    char* daddrstring = display_ip(dargs->addr, dargs->addrlen);
    printf("Client %s disconnected...\n", daddrstring);
    free(daddrstring);
    return NULL;
}

void client_cleanup(pthread_t cthread, int fd, sockaddr_t* addr, socklen_t addrlen) {
    char* daddrstring = display_ip(addr, addrlen);
    printf("Client %s connected...\n", daddrstring);
    free(daddrstring);

    disconnection_args_t* args = (disconnection_args_t*)malloc(sizeof(disconnection_args_t));
    args->cthread = cthread;
    args->fd = fd;
    args->addr = addr;
    args->addrlen = addrlen;

    pthread_t dthread;

    pthread_create(&dthread, NULL, client_disconnection_listener, (void*)args);
}

void main() {
    server_def_t definition = create_server_defaults();
    definition.address.sin_port = htons(23);
    definition.connection_handler = echo_handler;
    definition.thread_handler = client_cleanup;
    server_listen_and_serve(definition);
}