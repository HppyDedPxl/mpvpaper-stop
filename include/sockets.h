#ifndef SOCKETS_H
#define SOCKETS_H

#include <stdbool.h>
#include "context.h"

bool init_sockets(context_t* context);
void close_sockets(context_t* context);
int initialize_socket(const char* socket_path);
char* send_to_socket(const char* command, int* socket_fd, const char* socket_path);
char* open_and_send_to_socket(const char* command, const char* socket_path);

#endif