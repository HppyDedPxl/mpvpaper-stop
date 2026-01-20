#ifndef MPVPAPER_STOP_OP_MODE_DEFAULT_H
#define MPVPAPER_STOP_OP_MODE_DEFAULT_H
#include "context.h"

/*
 * Runs the default operation mode
 * Pauses the global mpv socket (can be changed with -p argument) if
 * any window is currently open within Hyprland. Not multi-monitor aware.
 */
void op_mode_default(context_t* context) ;

#endif //MPVPAPER_STOP_OP_MODE_DEFAULT_H