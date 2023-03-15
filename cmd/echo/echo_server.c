#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../../celnet.h"

void echo_handler(int sock, sockaddr_t* addr, socklen_t addr_len, char* data, size_t data_len) {
    printf("%s\n", data);
    send(sock, (void*)data, data_len, 0);
}

void main() {
    server_def_t definition = create_server_defaults();
    definition.address.sin_port = 23;
    definition.connection_handler = echo_handler;
    server_listen_and_serve(definition);
}