#ifndef MPVPAPER_STOP_OP_MODE_FULL_MULTI_MONITOR_H
#define MPVPAPER_STOP_OP_MODE_FULL_MULTI_MONITOR_H
#include "context.h"

/*
 * Runs the full multi-monitor operation mode
 * Requires a configuration file passed with the -j argument.
 * see the template.json for formatting. Will associate workspaces to monitors
 * and monitors to sockets.
 * Will pause sockets where the corresponding monitor has a window.
 * Will resume sockets where the corresponding monitor has no windows.
 */
void op_mode_full_multi_monitor(context_t* context);


#endif //MPVPAPER_STOP_OP_MODE_FULL_MULTI_MONITOR_H