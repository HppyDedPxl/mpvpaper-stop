#include "op_mode_full_multi_monitor.h"

#include <stdlib.h>

#include "multi_monitor.h"
#include "utils.h"

void op_mode_full_multi_monitor(context_t* context) {
    monitor_t* mon_buffer = calloc( 32,sizeof(monitor_t)); // Potentially a bit overkill but makes sure up to 32 monitors are supported.
    workspace_t** ptr_ws_buffer = calloc(1,sizeof(workspace_t*));
    int* monitors_queried = calloc(1,sizeof(int*));
    int result = -1;

    query_monitors_and_workspaces(context,mon_buffer,monitors_queried,ptr_ws_buffer);

    // cross reference the monitors and workspaces
    for (int i = 0; i < (*monitors_queried); ++i) {
       for (int j = 0; j < (*monitors_queried); ++j) {
           // find the active workspace for this monitor
           if (mon_buffer[i].active_ws_id == (*ptr_ws_buffer)[j].workspace_id) {
               bool is_paused = mpv_socket_is_paused(context, mon_buffer[i].config.socket_path);
               int windows = (*ptr_ws_buffer)[j].windows;
               if (is_paused && windows == 0) {
                   resume_mpv_on_socket(context,mon_buffer[i].config.socket_path);
               }
               else if (!is_paused && windows > 0) {
                   pause_mpv_on_socket(context,mon_buffer[i].config.socket_path);
               }
               break;
           }
       }
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
