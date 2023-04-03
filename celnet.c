#include "celnet.h"
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

const uint8_t IAC=255, WILL=251, WONT=252, DO=253, DONT=254;

/**
 * A mapping of option handlers,
 * if the option is available, 
 * then the pointer will be populated,
 * if you don't want to handle the option,
 * then leave the pointer as NULL
*/
option_handler_t option_handlers[256];

/**
 * Creates a server definition struct with all the fields set to the default
 * address: set to AF_INET, INADDR_ANY and 0 for the port
 * buffer_size: 1024
 * backlog: 1
 * initial_threadpool_size: 10
 * function pointers are initialized to NULL
*/
server_def_t create_server_defaults() {
    sockaddr_t address;
    // bzero(&address, sizeof(address))
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = 0;
    server_def_t result = {
        address, 1024, 1, 10,
        NULL, NULL
    };
    return result;
}

typedef struct {
    int connfd;
    sockaddr_t* addr;
    socklen_t* addr_len;
    size_t buffer_size;
    void (*user_connection_handler)(int, sockaddr_t*, socklen_t, char*, size_t);
} connection_handler_args_t;


/**
 * Handle incoming connections from the server
 * @param args Must be of type connection_handler_args_t*
 * @returns Nothing
*/
void* connection_handler(void* args) {
    connection_handler_args_t* cargs = (connection_handler_args_t*)args;

    // Sets up the buffers

    // Used for parsing telnet commands
    uint8_t* pbuffer = malloc(sizeof(uint8_t)*cargs->buffer_size);
    if (!pbuffer) return NULL;
    size_t pbuffer_count = 0;
    size_t pbuffer_size = cargs->buffer_size;

    // The actual receiving buffer
    uint8_t* rbuffer = malloc(sizeof(uint8_t)*cargs->buffer_size);
    if (!rbuffer) return NULL;

    for (;;) {
        IAC_LOOP_START:
        // Get some data
        ssize_t rbuff = recv(cargs->connfd, (void*)rbuffer, sizeof(uint8_t)*cargs->buffer_size, 0);
        if (rbuff < 0) break;
        if (!rbuff) continue;

        // handle if we were waiting for more data from the client
        // for a command
        size_t start_offset = 0;
        if (pbuffer_count) {
            uint8_t* interbuff = malloc(sizeof(uint8_t) * (pbuffer_count + rbuff));
            if (!interbuff) break;
            memcpy(interbuff, pbuffer, sizeof(uint8_t)*pbuffer_count);
            memcpy(interbuff+pbuffer_count, rbuffer, sizeof(uint8_t)*rbuff);
            size_t distance = option_handlers[interbuff[0]](cargs->connfd, interbuff, pbuffer_count+rbuff, 0);
            free(interbuff);
            if (distance < 0) {
                size_t new_size = pbuffer_count + rbuff;
                if (new_size > pbuffer_size) {
                    pbuffer_size = new_size + 10;
                    pbuffer = realloc(pbuffer, sizeof(uint8_t)*new_size);
                    if (!pbuffer) break;
                }
                memcpy(pbuffer+pbuffer_count, rbuffer, sizeof(uint8_t)*rbuff);
                pbuffer_count += rbuff;
                continue;
            }
            start_offset = distance - (pbuffer_count - 2);
        }

        // Search for IAC bytes
        size_t last_start = start_offset;
        for (ssize_t ri = start_offset; ri < rbuff; ri++) {
            if (rbuffer[ri] == IAC) {
                if (last_start < ri && cargs->user_connection_handler) {
                    size_t len = ri - last_start;
                    char* interbuff = malloc(sizeof(uint8_t) * len);
                    if (!interbuff) goto EXIT_HANDLE;
                    memcpy(interbuff, rbuffer+last_start, sizeof(uint8_t) * len);
                    cargs->user_connection_handler(cargs->connfd, cargs->addr, sizeof(cargs->addr), interbuff, len);
                    free(interbuff);
                }

                // Handle the command byte
                uint8_t response[3] = {IAC, 0, 0};
                int err;
                switch((uint8_t)rbuffer[++ri]) {
                    case 251: // WILL
                        uint8_t can = WONT;
                        if (option_handlers[rbuffer[ri+1]]) {
                            can = WILL;
                        }
                        response[1] = can;
                        response[2] = rbuffer[ri+1];
                        err = send(cargs->connfd, (void*)response, sizeof(uint8_t)*3, 0);
                        if (err < 0) goto EXIT_HANDLE;
                        last_start = ++ri + 1;
                        break;
                    case 253: // DO
                        if (option_handlers[rbuffer[ri+1]]) {
                            size_t skip_len = option_handlers[rbuffer[ri+1]](cargs->connfd, rbuffer, rbuff, ri+1);
                            if (skip_len < 0) {
                                size_t new_size = rbuff - (ri+1);
                                if (new_size > pbuffer_size) {
                                    pbuffer_size = new_size + 10;
                                    pbuffer = realloc(pbuffer, sizeof(uint8_t)*new_size);
                                    if (!pbuffer) goto EXIT_HANDLE;
                                }
                                memcpy(pbuffer, rbuffer, sizeof(uint8_t)*rbuff);
                                pbuffer_count = new_size;
                                goto IAC_LOOP_START;
                            }
                            ri += skip_len;
                            last_start = ri;
                            continue;
                        } else {
                            response[1] = DONT;
                            response[2] = rbuffer[ri+1];
                            err = send(cargs->connfd, (void*)response, sizeof(uint8_t)*3, 0);
                            if (err < 0) goto EXIT_HANDLE;
                            last_start = ++ri + 1;
                            break;
                        }
                    default:
                        if (option_handlers[rbuffer[ri]]) {
                            size_t skip_len = option_handlers[rbuffer[ri]](cargs->connfd, rbuffer, rbuff, ri);
                            if (skip_len < 0) {
                                size_t new_size = rbuff - (ri);
                                if (new_size > pbuffer_size) {
                                    pbuffer_size = new_size + 10;
                                    pbuffer = realloc(pbuffer, sizeof(uint8_t)*new_size);
                                    if (!pbuffer) goto EXIT_HANDLE;
                                }
                                memcpy(pbuffer, rbuffer, sizeof(uint8_t)*rbuff);
                                pbuffer_count = new_size;
                                goto IAC_LOOP_START;
                            }
                            ri += skip_len;
                            last_start = ri;
                            continue;
                        } else {
                            response[1] = DONT;
                            response[2] = rbuffer[ri];
                            err = send(cargs->connfd, (void*)response, sizeof(uint8_t)*3, 0);
                            if (err < 0) goto EXIT_HANDLE;
                            last_start = ++ri + 1;
                            break;
                        }
                }
            }
        }
        if (last_start < rbuff && cargs->user_connection_handler) {
            // There was data at the end that weren't part of a command
            size_t len = rbuff - last_start;
            char* interbuff = malloc(sizeof(uint8_t) * len);
            if (!interbuff) goto EXIT_HANDLE;
            memcpy(interbuff, rbuffer+last_start, sizeof(uint8_t) * len);
            cargs->user_connection_handler(cargs->connfd, cargs->addr, sizeof(cargs->addr), interbuff, len);
            free(interbuff);
        }
    }

    EXIT_HANDLE:
    printf("Leaving handler loop\n");
    if (pbuffer) free(pbuffer);
    if (rbuffer) free(rbuffer);
}



void server_listen_and_serve(server_def_t definition) {
    // Initialize the socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return; // There was an error
    if (definition.options_callback) definition.options_callback(sockfd);
    if (bind(sockfd, (struct sockaddr*)&definition.address, sizeof(definition.address))) {
        printf("Failed to bind to port\n");
        return;
    }
    if (listen(sockfd, definition.backlog)) {
        printf("Failed to start listening\n");
        return;
    }

    printf("Listening on: %d\n", ntohs(definition.address.sin_port));

    // Begin the loop
    for (;;) {
        struct sockaddr_in* caddr = malloc(sizeof(struct sockaddr_in));
        if (!caddr) return;
        socklen_t* caddr_len = malloc(sizeof(socklen_t));
        if (!caddr_len) return;
        int csock = accept(sockfd, (struct sockaddr*)caddr, caddr_len);
        if (csock < 0) {
            printf("Failed to accept socket\n");
            return;
        }

        connection_handler_args_t* cargs = malloc(sizeof(connection_handler_args_t));
        if (!cargs) return;
        *cargs = (connection_handler_args_t){csock, (struct sockaddr*)caddr, caddr_len, definition.buffer_size, definition.connection_handler};

        pthread_t cthread;
        int cret;

        cret = pthread_create(&cthread, NULL, connection_handler, (void*)cargs);
        if (cret) {
            printf("Failed to create pthread handler\n");
            errno = cret;
            return;
        }
        if(definition.thread_handler) definition.thread_handler(cthread, csock, (struct sockaddr*)caddr, caddr_len);
    }
}

