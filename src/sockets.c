
#include "../include/sockets.h"
#include "../include/constants.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

bool init_sockets(context_t* context) {
    if(context->mpvpaper_socket_fd == -1)
        context->mpvpaper_socket_fd = initialize_socket(context->mpvpaper_socket_path);
    if(context->hyprland_socket_fd == -1)
        context->hyprland_socket_fd = initialize_socket(context->hyprland_socket_path);
    return (context->mpvpaper_socket_fd != -1 && context->hyprland_socket_fd != -1);
}

void close_sockets(context_t* context) {
    close(context->mpvpaper_socket_fd);
    close(context->hyprland_socket_fd);
    context->mpvpaper_socket_fd = -1;
    context->hyprland_socket_fd = -1;
}


int initialize_socket(const char* socket_path) {
    fprintf(stdout,"TryInitSocket\n");
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("error: socket error");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("error: connection to socket error");
        close(sockfd);
        return -1;
    }
    fprintf(stdout,"Socket Init successfull %s %d\n", socket_path, sockfd);
    return sockfd;
}


char* send_to_socket(const char* command, int* socket_fd, const char* socket_path, const bool reconnect) {

    if (reconnect) {
        close(*socket_fd);
        *socket_fd = initialize_socket(socket_path);
    }
    char buffer [SIZE_16KB] = {0};
    char* response = NULL;

    if (write(*socket_fd, command, strlen(command)) < 0) {
        perror("error: write to socket error");
        close(*socket_fd);

        return NULL;
    }
    const ssize_t n = read(*socket_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        response = strdup(buffer);
    }

    return response;
}

