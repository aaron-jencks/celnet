# CelNet
A simple TelNet Server implemented in C.

## Installation
Copy `celnet.c` and `celnet.h` into your project. Make sure to link `pthread` to your project during compiling. (most likely `-lpthread`)

## Usage
You instantiate an server info struct using `create_server_defaults()`. This will set everything up to run on a random port on your local machine. Then optionally you can define handlers for the function pointers defined in the struct. Pass this server definition into `server_listen_and_serve(definition)` and voila, you're up and running.

**NOTE**: You do not have to `free` the data being passed into the handlers (neither the connection handler, nor the options handlers), the server will take care of that itself.

Here's a simple example:
```c
server_def_t definition = create_server_defaults();
server_listen_and_serve(definition);
```

Here's a real world example:
```c
void echo_handler(int sock, sockaddr_t* addr, socklen_t addr_len, char* data, size_t data_len) {
    printf("%s\n", data);
    send(sock, (void*)data, data_len, NULL);
}

server_def_t definition = create_server_defaults();
definition.connection_handler = echo_handler;
server_listen_and_serve(definition);
```

### Options
If you want to handle options for TelNet, then you need to implement option handlers. Aside from the reserved bytes for `WILL`, `WONT`, `DO`, `DONT` all other values are up for grabs. To use them, define an option handler using the `option_handler_t` format and then assign it to the corresponding option index in the `option_handlers` array.

Here's an example:

```c
size_t my_option_handler(int sock, char* data, size_t data_len, size_t data_start) {
    return 0;
}

option_handlers[34] = my_option_handler;

server_def_t definition = create_server_defaults();
server_listen_and_serve(definition);
```

### Other function pointers
There are two other function pointers that you can populate in the server definition struct, `options_callback` and `thread_handler`.

The options callback is for setting socket options (see `man 2 setsockopt`).

the thread handler is called whenever a new connection is made, if you don't plan on having this server running monolithically, then you'll want to use the thread handler function to intercept the `pthread_t` values as they're created so that you can peacefully join them or stop them later.
