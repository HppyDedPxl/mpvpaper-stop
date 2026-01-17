#include "../include/utils.h"
#include "../include/constants.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

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