#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

typedef struct {
    bool verbose;
    bool fork_process;
    char* mpvpaper_socket_path;
    int mpvpaper_socket_fd;
    char* hyprland_socket_path;
    int hyprland_socket_fd;
    int socket_wait_time;
    int polling_period;
    bool multi_monitor_aware;
    bool retry_on_socket_error;
    int monitor_count;
    char* multi_monitor_config_path;
} context_t;

void init_context(context_t* config);

#endif
