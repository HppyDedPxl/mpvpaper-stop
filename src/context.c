#include "../include/context.h"
#include "../include/utils.h"
#include "../include/constants.h"

void init_context(context_t* context) {
    context->verbose = false;
    context->fork_process = false;
    context->mpvpaper_socket_path = DEFAULT_MPVPAPER_SOCKET_PATH;
    context->hyprland_socket_path = get_hyprctl_socket_path();
    context->socket_wait_time = DEFAULT_MPVPAPER_SOCKET_WAIT_TIME;
    context->polling_period = DEFAULT_PERIOD;
    context->multi_monitor_aware = false;
    context->retry_on_socket_error = false;
    context->monitor_count = 1;
    context->multi_monitor_config_path = "";
 
}