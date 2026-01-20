
#ifndef MPVPAPER_STOP_OP_MODE_ANY_MONITOR_FREE_H
#define MPVPAPER_STOP_OP_MODE_ANY_MONITOR_FREE_H
#include "context.h"

/*
 * Runs the simple multi-monitor operation mode
 * Pauses the global mpv socket (passed with the -p argument) if all
 * monitors have at least one window open on their active workspace.
 */
void op_mode_simple_multi_monitor(context_t* context);

#endif //MPVPAPER_STOP_OP_MODE_ANY_MONITOR_FREE_H