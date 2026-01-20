#include "op_mode_simple_multi_monitor.h"

#include <stdlib.h>

#include "multi_monitor.h"
#include "utils.h"

void op_mode_simple_multi_monitor(context_t* context) {
    monitor_t* mon_buffer = calloc( 32,sizeof(monitor_t)); // Potentially a bit overkill but makes sure up to 32 monitors are supported.
    workspace_t** ptr_ws_buffer = calloc(1,sizeof(workspace_t*));
    int* monitors_queried = calloc(1,sizeof(int*));
    int result = -1;

    query_monitors_and_workspaces(context,mon_buffer,monitors_queried,ptr_ws_buffer);

    // Actual comparison operation todo: this could be an outside lambda call that takes in mon_buffer and ws_buffer
    for (int i = 0; i < (*monitors_queried); ++i) {
        if (result == -1 || (*ptr_ws_buffer)[i].windows < result) {
            result = (*ptr_ws_buffer)[i].windows;
        }
    }

    // save last windows statically so we can compare over multiple executions
    static int last_windows = -1;
    static bool last_paused = false;

    // Do we need to change the pause state? if not early out
    bool is_paused = mpv_socket_is_paused(context,context->mpvpaper_socket_path);
    if(result == last_windows && is_paused == last_paused) return;

    // cache new state
    last_windows = result;
    last_paused = is_paused;

    // apply new state
    if (result == 0 && is_paused) {
        resume_mpv_on_socket(context,context->mpvpaper_socket_path);
    } else if (result > 0 && !is_paused) {
        pause_mpv_on_socket(context,context->mpvpaper_socket_path);
    }

    // Cleanup allocations
    for (int i = 0; i < *monitors_queried; ++i) {
        if (mon_buffer != NULL)
            free(mon_buffer[i].name);
        if ((*ptr_ws_buffer) != NULL)
            free((*ptr_ws_buffer)[i].monitor_name);
    }
    free(*ptr_ws_buffer);
    free(ptr_ws_buffer);
    free(mon_buffer);
    free(monitors_queried);

}
