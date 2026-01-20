#ifndef MULTI_MONITOR_H
#define MULTI_MONITOR_H
#include "constants.h"
#include "context.h"
typedef struct {
    char* socket_path;
    // add more here
} monitor_config_t ;

typedef struct {
    int id;
    char* name;
    int active_ws_id;
    monitor_config_t config;
} monitor_t;

typedef struct {
    int workspace_id;
    int monitor_id;
    char* monitor_name;
    int windows;
} workspace_t;


/* Queries Monitor data 
* Arguments:
* in - context (Application Context)
* out - monitor_array - Pointer to where the function should write the monitor array to. 
* out - queried_monitors - the amount of monitors that were written back to the buffer.
* returns: true on success
*/
bool query_monitors(context_t* context, monitor_t* buffer, int* queried_monitors);

/* Queries workspace data 
* Arguments:
* in - context (Application Context)
* in - monitors - array of monitors to query workspace data from
* in - monitor_count - amount of monitors to query for and amount of workspaces that should be written back to the buffer
* out - buffer - Pointer to where the function should write the workspace array to (array size will be equal to monitor array size). 
* returns: true on success
*/
bool query_workspaces_from_monitors(context_t* context, monitor_t* monitors, int monitor_count, workspace_t* buffer);

/*
 * Queries monitors and workspaces together
 * in - context (Application Context)
 * out - pre-allocated buffer for the monitor array
 * out - pre-allocated buffer for the monitors_queried result amount
 * out - pre-allocated ptr to a not yet allocated buffer where workspace data will be written
 * returns: true on success
 * note: Memory Management needs to be done by the caller. Using free() clear all char* fields from entries in the mon_buffer array, as well as the ws_buffer array,
 * and mon_buffer and ptr_ws_buffer.
 * e.g.
 * // Cleanup allocations
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
 */
bool query_monitors_and_workspaces(context_t* context, monitor_t* mon_buffer, int* monitors_queried, workspace_t** ptr_ws_buffer);

#endif