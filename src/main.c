#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#include "../include/constants.h"
#include "../include/sockets.h"
#include "../include/context.h"
#include "../include/logging.h"

#include <cjson/cJSON.h>

void print_help(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose                 Enables verbose output\n");
    printf("  -f, --fork                    Forks the process\n");
    printf("  -p, --socket-path PATH        Path to the mpvpaper socket (default: /tmp/mpvsocket)\n");
    printf("  -w, --socket-wait-time TIME   Wait time for the socket in milliseconds (default: 5000)\n");
    printf("  -m --multi-monitor-aware      Enables Multi Monitor mode. If any active workspace has no windows open, enable playback.\n");
    printf("  -t, --period TIME             Polling period in milliseconds (default: 1000)\n");
    printf("  -h, --help                    Shows this help message\n");
}

void wait_for_socket(const char *socket_path, const context_t* context) {
    int elapsed = 0;
    while (elapsed < context->socket_wait_time * 1000) {
        const int interval = 100000;
        if (access(socket_path, F_OK) == 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Socket %s is available", socket_path);
            log_out(msg);

            return;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "Socket %s not available, sleeping...", socket_path);
        log_out(msg);


        usleep(interval);
        elapsed += interval;
    }

    log_err("error: socket %s not available after waiting %d ms\n", socket_path, context->socket_wait_time);
    exit(EXIT_FAILURE);
}

char* send_to_mpv_socket(const char* command, context_t* context) {
    return send_to_socket(command, &context->mpvpaper_socket_fd, context->mpvpaper_socket_path, false);
}

char* send_to_hyprland_socket(const char* command, context_t* context) {
    return send_to_socket(command, &context->hyprland_socket_fd, context->hyprland_socket_path, true);
}

int query_windows(context_t* context) {
    char* json_str = send_to_hyprland_socket(QUERY_HYPRLAND_SOCKET_ACTIVE_WORKSPACE, context);
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

// todo:<AK> cleanup and split up big function into logical chunks
// -> "get all active workspaces on all monitors"
// -> "get amount of active windows on active monitors"
// -> "get lowest int"
int query_monitor_data(context_t* context) {
    char* json_str = send_to_hyprland_socket(QUERY_HYPRLAND_SOCKET_MONITORS_DATA, context);
    if (!json_str) {
        log_err("error: failed to query monitors data\n %s\n", json_str);
        return -1;
    }

    cJSON* json = cJSON_Parse(json_str);
  
    if (!json) {
        log_err("error: failed to parse JSON\n %s\n", json_str);
        return -1;
    }
      free(json_str);

    int monitor_count = cJSON_GetArraySize(json);
    log_out("Found %d monitors.\n", monitor_count );

    int arr_active_workspace_ids[monitor_count];

    // Collect the workspaces that are active on each monitor and save them into an array
    for (int i = 0; i < monitor_count; i++){
        cJSON* monitor = cJSON_GetArrayItem(json, i);
        if(!monitor){
            log_err("error: failed to get monitor at index %d", i);
            return -1;
        }
        cJSON* active_ws = cJSON_GetObjectItemCaseSensitive(monitor,"activeWorkspace");
        if(!active_ws){
            log_err("error: failed to get activeWorkspace at monitor index %d", i);
            return -1;
        }
        cJSON* json_active_ws_id = cJSON_GetObjectItemCaseSensitive(active_ws,"id");
        int active_ws_id = cJSON_IsNumber(json_active_ws_id) ? json_active_ws_id->valueint : -1;
        arr_active_workspace_ids[i] = active_ws_id;
    }

    // We dont need the data anymore, we can reuse the space
    cJSON_Delete(json);

    // get amount of windows on each active workspace
    json_str = send_to_hyprland_socket(QUERY_HYPRLAND_SOCKET_WORKSPACES_DATA, context);
    if (!json_str) {
        log_err("error: failed to query workspaces data\n");
        return -1;
    }

    json = cJSON_Parse(json_str);
    free(json_str);

     if (!json) {
        log_err("error: failed to parse JSON\n");
        return -1;
    }

    int ws_count = cJSON_GetArraySize(json);
    log_out("Found %d workspaces.\n", ws_count );

    int arr_active_windows_on_monitors[monitor_count];

    // for each workspace that we determined as active, go through the workspaces and
    // read how many windows are active on it and write it back
     for (int i = 0; i < monitor_count; i++){
        int cur_monitor_ws = arr_active_workspace_ids[i];
        
        for(int j = 0; j < ws_count; j++){
            cJSON* ws_json = cJSON_GetArrayItem(json,j);
            if(!ws_json){
                log_err("error: failed to get workspace at idx %d\n",j);
                return -1;
            }
            cJSON* js_ws_id = cJSON_GetObjectItemCaseSensitive(ws_json,"id");
            int ws_id = cJSON_IsNumber(js_ws_id) ? js_ws_id->valueint : -1;

            if(ws_id == cur_monitor_ws){
                cJSON* js_win_count = cJSON_GetObjectItemCaseSensitive(ws_json,"windows");
                int win_count = cJSON_IsNumber(js_win_count) ? js_win_count->valueint : 0;
                arr_active_windows_on_monitors[i] = win_count;
                log_out( "Found %d active windows on active workspace %d\n",win_count,cur_monitor_ws);
                break; // we can break out and go on to the next monitor
            }
        }
     }

     // free json data again
    cJSON_Delete(json);

    int min_windows = 1000;
    for (int i = 0; i < monitor_count; i++){
        if (arr_active_windows_on_monitors[i] < min_windows){
            min_windows = arr_active_windows_on_monitors[i];
        }
    }
    log_out("Minimum amount of windows on any active workspace: %d\n", min_windows);
    return min_windows;
}


bool query_pause_status(context_t* context) {
    char* json_str = send_to_mpv_socket(QUERY_MPVPAPER_SOCKET_PAUSE_PROPERTY, context);

    if (!json_str) {
        log_out("error: failed to query pause status\n");
        return false;
    }

    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        log_err("error: failed to parse JSON\n");
        return false;
    }

    cJSON* json_data = cJSON_GetObjectItemCaseSensitive(json, "data");
    bool paused = cJSON_IsBool(json_data) ? cJSON_IsTrue(json_data) : false;

    cJSON_Delete(json);

    return paused;
}

void resume_mpv(context_t* context) {
    log_out("Resuming");
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_RESUME, context);

    if (response) free(response);
}

void pause_mpv(context_t* context) {
    log_out("Pausing");
    char* response = send_to_mpv_socket(SET_MPVPAPER_SOCKET_PAUSE, context);
    if (response) free(response);
}

void update_mpv_state(context_t* context) {

	static int last_windows = -1;
	static bool last_paused = false;
    int windows = 0;

    if(context->multi_monitor_aware == true)
        windows = query_monitor_data(context);
    else
        windows = query_windows(context);

    if (windows < 0) return;

    bool is_paused = query_pause_status(context);

	if(windows == last_windows && is_paused == last_paused) return;

	last_windows = windows;
	last_paused = is_paused;

    char message[64];
    snprintf(message, sizeof(message), "{windows: %d, paused: %d}", windows, is_paused);
    log_out(message);

    if (windows == 0 && is_paused) {
        resume_mpv(context);
    } else if (windows > 0 && !is_paused) {
        pause_mpv(context);
    }

}

void fork_if(const bool flag) {
    if (!flag) return;

    int pid = fork();
    if (pid < 0) {
        perror("error: fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        perror("error: setsid failed");
        exit(EXIT_FAILURE);
    }
}

void validate_period(int period) {
    if (period <= 0) {
        log_err("error: period must be greater than 0\n");
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char **argv) {
    int opt;
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"fork", no_argument, NULL, 'f'},
        {"retry-on-socket-error", no_argument, NULL, 'r'},
        {"multi-monitor-aware", no_argument, NULL, 'm'},
        {"socket-path", required_argument, NULL, 'p'},
        {"period", required_argument, NULL, 't'},
        {"socket-wait-time", required_argument, NULL, 'w'},
        {0, 0, 0, 0}
    };

    context_t context;
    init_context(&context);

    while ((opt = getopt_long(argc, argv, "hvfmrp:t:w:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'f':
                context.fork_process = true;
                break;
            case 'v':
                context.verbose = true;
                set_logger_verbose(context.verbose);
                break;
            case 'p':
                context.mpvpaper_socket_path = optarg;
                break;
            case 't':
                context.polling_period = atoi(optarg);
                break;
            case 'w':
                context.socket_wait_time = atoi(optarg);
                break;
            case 'm':
                context.multi_monitor_aware = true;
                break;
            case 'r':
                context.retry_on_socket_error = true;
                break;
            default:
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    validate_period(context.polling_period);
    wait_for_socket(context.mpvpaper_socket_path, &context);
    fork_if(context.fork_process);


    log_out("Starting monitoring loop");
    bool sockets_connected = false;
    while (1) {
        // Initialize Sockets for this run (We do not keep sockets connected because if either Hyprland or Mpvpaper crash/close it will kill our process too.)
        sockets_connected = init_sockets(&context);
        // if -r was passed and we should continously wait for valid socket connections
        // go into a reconnect loop
        if(context.retry_on_socket_error && !sockets_connected){
            while(!sockets_connected){
                // close any socket that might have opened successfully again. We dont need it to be open for the sleep.
                // we also want to avoid process hangs that can occur when using usleep() with open sockets
                close_sockets(&context);
                usleep(context.polling_period*1000);
                sockets_connected = init_sockets(&context);
            }
        }

        // At this point we should have a valid socket connection, if this is not the case exit out.
        if(!sockets_connected){
            perror("error: could not establish a socket connection. Terminating...");
            exit(EXIT_FAILURE);
        }

        // Update
        update_mpv_state(&context);

        // Close all sockets again and wait for the next poll. 
        close_sockets(&context);
        usleep(context.polling_period*1000);
    }
}