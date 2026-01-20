#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/un.h>
#include "../include/sockets.h"
#include "../include/context.h"
#include "../include/logging.h"
#include "op_mode_default.h"
#include "op_mode_simple_multi_monitor.h"
#include "op_mode_full_multi_monitor.h"


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

void update_mpv_state(context_t* context) {

    if (context->multi_monitor_aware == true && strcmp(context->multi_monitor_config_path,"") != 0) {
        op_mode_full_multi_monitor(context);
    }
    else if(context->multi_monitor_aware == true){
         op_mode_simple_multi_monitor(context);
    }
    else {
        op_mode_default(context);
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
                context.monitor_count = -1;
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

    while (1) {
        update_mpv_state(&context);
        usleep(context.polling_period*1000);
    }
}