#include "../include/utils.h"

#include <cJSON.h>

#include "../include/constants.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "context.h"
#include "logging.h"
#include "sockets.h"

void create_temp_dir() {
	if (mkdir(TEMP_DIR, 0755) == -1) {
        if (errno != EEXIST) {
            perror("error: failed to create TEMP_DIR");
            exit(EXIT_FAILURE);
        }
    }
}

char* get_hyprctl_socket_path() {
    char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir) {
        fprintf(stderr, "error: XDG_RUNTIME_DIR is not set\n");
        exit(EXIT_FAILURE);
    }

    char* hyprland_instance_signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!hyprland_instance_signature) {
        fprintf(stderr, "error: HYPRLAND_INSTANCE_SIGNATURE is not set\n");
        exit(EXIT_FAILURE);
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", xdg_runtime_dir, hyprland_instance_signature);
    
	bool xdg_access_failed = access(path, F_OK) != 0; 
	if(!xdg_access_failed) return strdup(path); 

	fprintf(stderr, "error: hyprland socket at %s not found, fallbacking to /tmp/hypr/\n", path);
	
	snprintf(path, sizeof(path), "/tmp/hypr/%s/.socket.sock", hyprland_instance_signature);
	bool tmp_access_failed = access(path, F_OK) != 0;
    if (tmp_access_failed) {
        fprintf(stderr, "error: hyprland socket path %s does not exist\n", path);
        exit(EXIT_FAILURE);
    }

    return strdup(path);
}

void resume_mpv_on_socket(context_t* context, const char* socket_path) {
    log_out("Resuming socket: %s",socket_path);
    char* response = open_and_send_to_socket(SET_MPVPAPER_SOCKET_RESUME, socket_path);

    if (response) free(response);
}

void pause_mpv_on_socket(context_t* context, const char* socket_path) {
    log_out("Pausing");
    char* response = open_and_send_to_socket(SET_MPVPAPER_SOCKET_PAUSE, socket_path);
    if (response) free(response);
}

bool mpv_socket_is_paused(context_t* context, const char* socket_path) {
    char* json_str = open_and_send_to_socket(QUERY_MPVPAPER_SOCKET_PAUSE_PROPERTY, socket_path);

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