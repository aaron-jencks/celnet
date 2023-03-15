#ifndef CELNET_H
#define CELNET_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

extern const uint8_t IAC, WILL, WONT, DO, DONT;

typedef struct sockaddr_in sockaddr_t;

/**
 * Defines a handler for an option
 * the handler is given the connection socket,
 * the current received buffer
 * as well as the length of the buffer
 * and the starting index of the option in question
 * the starting index points immediately after the IAC byte
 * should return the number of bytes that corresponded to this option
 * so that they do not get reinterpreted.
 * Return -1 if there are not enough bytes left for processing
 * syntax is (conn, buff, len, start)
*/
typedef size_t (*option_handler_t)(int, char*, size_t, size_t);

/**
 * A mapping of option handlers,
 * if the option is available, 
 * then the pointer will be populated,
 * if you don't want to handle the option,
 * then leave the pointer as NULL
*/
extern option_handler_t option_handlers[256];

/**
 * Contains settings for the server
*/
typedef struct {
    sockaddr_t address;                                             // The address to bind the server to
    size_t buffer_size;                                             // the size of the tx/rx buffer to use
    int backlog;                                                    // defines the maximum number of pending connections are allowed before refusing
    size_t initial_threadpool_size;                                 // The initial threadpool size, this is not the max, just the start
    void (*options_callback)(int);                                  // A callback for setting socket options, called between `socket` and `bind`
    /**
     * Handles a connection once a telnet connection is established
     * You are given the socket fd, the remote address, the length of that address
     * and the character data read in from the socket, 
     * and the length of the character data read in.
    */
    void (*connection_handler)(int, sockaddr_t*, socklen_t, char*, size_t);
    void (*thread_handler)(pthread_t);                              // Will be called whenever a thread is launched successfully
} server_def_t;

/**
 * Creates a server definition struct with all the fields set to the default
 * address: set to AF_INET, INADDR_ANY and 0 for the port
 * buffer_size: 1024
 * backlog: 1
 * initial_threadpool_size: 10
 * function pointers are initialized to NULL
*/
server_def_t create_server_defaults();

void server_listen_and_serve(server_def_t definition);

#endif