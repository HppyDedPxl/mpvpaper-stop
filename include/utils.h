#ifndef UTILS_H
#define UTILS_H
#include "context.h"

char* get_hyprctl_socket_path();
void create_temp_dir();
void resume_mpv_on_socket(context_t* context, const char* socket_path);
void pause_mpv_on_socket(context_t* context, const char* socket_path);
bool mpv_socket_is_paused(context_t* context, const char* socket_path);

#endif