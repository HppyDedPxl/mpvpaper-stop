#include "op_mode_default.h"

#include <cJSON.h>
#include <stdlib.h>

#include "constants.h"
#include "logging.h"
#include "sockets.h"
#include "utils.h"

int query_windows_on_active_ws(context_t* context) {
    char* json_str = open_and_send_to_socket(QUERY_HYPRLAND_SOCKET_ACTIVE_WORKSPACE, context->hyprland_socket_path);
    if (!json_str) {
        log_err("error: failed to query active workspace\n");
        return -1;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        log_err("error: failed to parse JSON\n");
        return -1;
    }

    cJSON* json_windows = cJSON_GetObjectItemCaseSensitive(json, "windows");
    int windows = cJSON_IsNumber(json_windows) ? json_windows->valueint : 0;

    cJSON_Delete(json);

    return windows;
}

void op_mode_default(context_t* context) {

    // save last windows statically so we can compare over multiple executions
    static int last_windows = -1;
    static bool last_paused = false;
    // find how many windows are currently open on the active workspace
    int windows = 0;
    windows = query_windows_on_active_ws(context);
    if (windows < 0) return;

    // Do we need to change the pause state? if not early out
    bool is_paused = mpv_socket_is_paused(context,context->mpvpaper_socket_path);
    if(windows == last_windows && is_paused == last_paused) return;

    // cache new state
    last_windows = windows;
    last_paused = is_paused;

    // apply new state
    if (windows == 0 && is_paused) {
        resume_mpv_on_socket(context,context->mpvpaper_socket_path);
    } else if (windows > 0 && !is_paused) {
        pause_mpv_on_socket(context,context->mpvpaper_socket_path);
    }
}
