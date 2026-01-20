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
#include "../include/multi_monitor.h"

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
    printf("  -n, --multi-monitor-count     Sets the amount of monitors to be queried in multi monitor mode.\n");
    printf("  -j, --multi-monitor-config    Sets the path to the multi-monitor-config.json (overwrites multi-monitor-count).\n");
}

void wait_for_socket(const char *socket_path, const context_t* context) {
    int elapsed = 0;
    while (elapsed < context->socket_wait_time * 1000) {
        const int interval = 100000;
        if (access(socket_path, F_OK) == 0) {
            log_out("Socket %s is available\n", socket_path);
            return;
        }
        log_out("Socket %s not available, sleeping...\n", socket_path);
        usleep(interval);
        elapsed += interval;
    }

    log_err("error: socket %s not available after waiting %d ms\n", socket_path, context->socket_wait_time);
    exit(EXIT_FAILURE);
}

char* send_to_mpv_socket(const char* command, context_t* context) {
    return send_to_socket(command, &context->mpvpaper_socket_fd, context->mpvpaper_socket_path);
}

char* send_to_hyprland_socket(const char* command, context_t* context) {
    return send_to_socket(command, &context->hyprland_socket_fd, context->hyprland_socket_path);
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

int mode_multi_monitor_no_config(context_t* context) {

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
    return result;
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

    log_out("0");
    if(context->multi_monitor_aware == true){
         windows = mode_multi_monitor_no_config(context);
    }
    else
        windows = query_windows(context);

    if (windows < 0) return;

    bool is_paused = query_pause_status(context);

	if(windows == last_windows && is_paused == last_paused) return;

	last_windows = windows;
	last_paused = is_paused;

    log_out("{windows: %d, paused: %d}",windows,is_paused);

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
        set_perror("error: fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        set_perror("error: setsid failed");
        exit(EXIT_FAILURE);
    }
}

void validate_period(int period) {
    if (period <= 0) {
        set_perror("error: period must be greater than 0\n");
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
        {"multi-monitor-count", required_argument, NULL, 'n'},
        {"multi-monitor-config", required_argument, NULL, 'j'},
        {0, 0, 0, 0}
    };

    context_t context;
    init_context(&context);


    while ((opt = getopt_long(argc, argv, "hvfrmp:t:w:n:j:", long_options, NULL)) != -1) {
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
            case 'n':
                context.monitor_count = atoi(optarg);
                break;
            case 'j':
                context.multi_monitor_config_path = optarg;
                break;
            default:
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    validate_period(context.polling_period);

    if(context.socket_wait_time > 0){
        wait_for_socket(context.mpvpaper_socket_path, &context);
    }

    fork_if(context.fork_process);

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