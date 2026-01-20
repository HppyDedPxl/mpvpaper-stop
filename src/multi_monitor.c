#include "../include/multi_monitor.h"
#include "cJSON.h"
#include "constants.h"
#include "logging.h"
#include "sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


monitor_config_t mon_config_fromJSON(cJSON* json){
    monitor_config_t config;
    config.socket_path = "";

    cJSON* js_socket = cJSON_GetObjectItemCaseSensitive(json, "mpvsocket");
    if(!js_socket)
    {
        log_err("error: json provided to mon_config_fromJSON is invalid. Object initialization failed.");
        return config;
    }
    config.socket_path = strdup(cJSON_GetStringValue(js_socket));
    return config;
}

bool query_monitors(context_t* context, monitor_t* buffer, int* monitors_queried) {
    
    // Prepare monitor configuration 
    bool has_mmc = strcmp(context->multi_monitor_config_path , "") != 0;
    int configured_monitors = -1;
    cJSON* json_monitor_conf = NULL;
    // find amount of monitors configured so we know how much space to allocate
    if(has_mmc){
        FILE* conf_ptr = fopen(context->multi_monitor_config_path,"r");

        if(conf_ptr != NULL){
            fseek(conf_ptr,0,SEEK_END);
            long fsize = ftell(conf_ptr);
            fseek(conf_ptr,0,SEEK_SET);

            char* config_buffer = malloc(fsize + 1);
            fread(config_buffer,fsize,1,conf_ptr);
            config_buffer[fsize] = 0; // make zero terminate

            json_monitor_conf = cJSON_Parse(config_buffer);
            cJSON* json_monitor_array = cJSON_GetObjectItemCaseSensitive(json_monitor_conf,"monitors");
            configured_monitors = cJSON_GetArraySize(json_monitor_array);
            free(config_buffer);
            fclose(conf_ptr);
        }

        if(context->monitor_count > -1 && context->monitor_count != configured_monitors){
            log_err("error: global provided monitor-count does not match the configured amount of monitors in the config.json. Consider not passing a global monitor count together with a json config.\n");
            cJSON_free(json_monitor_conf);
            return false;
        }
    }

    char* hyprctl_mons_str = open_and_send_to_socket(QUERY_HYPRLAND_SOCKET_MONITORS_DATA, context->hyprland_socket_path);
    if (!hyprctl_mons_str) {
        log_err("error: failed to query monitors data\n %s\n", hyprctl_mons_str);
        cJSON_free(json_monitor_conf);
        return false;
    }

    cJSON* json_hyprctl_mons = cJSON_Parse(hyprctl_mons_str);
  
    if (!json_hyprctl_mons) {
        log_err("error: failed to parse JSON\n %s\n", hyprctl_mons_str);
        cJSON_free(json_monitor_conf);
        return false;
    }

    free(hyprctl_mons_str);

    // check size discrepancies and make sure we do not overflow
    int monitor_count = cJSON_GetArraySize(json_hyprctl_mons);
    if(has_mmc){
        *monitors_queried = configured_monitors;
        if(monitor_count != configured_monitors){
            log_out("warn: Found %d configured monitors but hyprctl only returned %d monitors. Check the configuration. Weird behaviour can occur.", configured_monitors,monitor_count);
            *monitors_queried = monitor_count < configured_monitors ? monitor_count : configured_monitors;
        }
    }
    else{
        *monitors_queried = monitor_count;
        if(context->monitor_count != monitor_count){
            log_out("warn: Tried to query %d monitors but hyprctl returned %d monitors. Please check the configuration. Weird behaviour can occur",context->monitor_count, monitor_count);
            *monitors_queried = context->monitor_count < monitor_count ? context->monitor_count : monitor_count;
        }
    }

    log_out("monitors_queried=%i\n",*monitors_queried);
  
    for (int i = 0; i < *monitors_queried; i++){

        cJSON* monitor = cJSON_GetArrayItem(json_hyprctl_mons, i);
        if(!monitor){
            log_err("error: failed to get monitor at index %d", i);
            cJSON_free(json_monitor_conf);
            return false;
        }

        // Get the monitor id
        cJSON* json_monitor_id = cJSON_GetObjectItemCaseSensitive(monitor,"id");
        int active_monitor_id = cJSON_IsNumber(json_monitor_id) ? json_monitor_id->valueint : -1;
        buffer[i].id = active_monitor_id;
        log_out("Setting monitor id: %d\n", buffer[i].id);

        // Get the monitor name
        cJSON* json_monitor_name = cJSON_GetObjectItemCaseSensitive(monitor,"name");
        char* active_monitor_name = cJSON_IsString(json_monitor_name) ? cJSON_GetStringValue(json_monitor_name) : "invalid";
        buffer[i].name = strdup(active_monitor_name);
        log_out("Setting monitor name: %s\n", buffer[i].name);

        cJSON* active_ws = cJSON_GetObjectItemCaseSensitive(monitor,"activeWorkspace");
        if(!active_ws){
            log_err("error: failed to get activeWorkspace at monitor index %d", i);
            cJSON_free(json_monitor_conf);
            return false;
        }
        // Get the active workspace id
        cJSON* json_active_ws_id = cJSON_GetObjectItemCaseSensitive(active_ws,"id");
        int active_ws_id = cJSON_IsNumber(json_active_ws_id) ? json_active_ws_id->valueint : -1;
        buffer[i].active_ws_id = active_ws_id;
        log_out("Setting monitor wsid: %d\n", buffer[i].active_ws_id);

        // find and save configuration for this monitor
        // populate data
        if(has_mmc && json_monitor_conf != NULL){  
            cJSON* json_monitor_array = cJSON_GetObjectItemCaseSensitive(json_monitor_conf,"monitors");
            for(size_t j = 0; j < configured_monitors; j++){
                cJSON* json_monitor = cJSON_GetArrayItem(json_monitor_array, j);
                char* monitor_name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json_monitor,"name"));
                // find the correct config for this monitor
                if(strcmp(monitor_name,buffer[i].name) == 0){
                    buffer[i].config = mon_config_fromJSON(json_monitor); 
                } 
            }
        }
        // We do not have a multi-monitor-config file. Assume the provided global socket for all monitors.
        else
        {
            buffer[i].config.socket_path = context->mpvpaper_socket_path;
        }
    }

    // We don't need the data anymore, we can reuse the space
    cJSON_Delete(json_hyprctl_mons);
    cJSON_free(json_monitor_conf);
    return true;
}

bool query_workspaces_from_monitors(context_t* context, monitor_t* monitors, int monitor_count, workspace_t* buffer) {
    // get amount of windows on each active workspace
    char* json_str = open_and_send_to_socket(QUERY_HYPRLAND_SOCKET_WORKSPACES_DATA, context->hyprland_socket_path);

    if (!json_str) {
        log_err("error: failed to query workspaces data\n");
        return false;
    }

    cJSON* json = cJSON_Parse(json_str);

     if (!json) {
        log_err("error: failed to parse JSON\n");
        return false;
    }

    int ws_count = cJSON_GetArraySize(json);
    log_out("Query workspaces for %d monitors\n",monitor_count);
    if(ws_count < monitor_count){
        log_err("error: Tried to query workspaces for %d monitors but hyprctl returned only %d workspaces. Please check the configuration.",monitor_count,ws_count);
        monitor_count = ws_count;
    }
    log_out("Found %d workspaces.\n", ws_count );

    // for each workspace that we determined as active, go through the workspaces and
    // read how many windows are active on it and write it back
    for (int i = 0; i < monitor_count; i++){
        int cur_monitor_ws = monitors[i].active_ws_id;
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
                buffer[i].windows = win_count;
                buffer[i].workspace_id = ws_id;
                buffer[i].monitor_id = monitors[i].id;
                buffer[i].monitor_name = strdup(monitors[i].name);
                log_out( "Found %d active windows on active workspace %d on monitor: %s\n",win_count,cur_monitor_ws, buffer[i].monitor_name);
                break; // we can break out and go on to the next monitor
            }
        }
    }
    // free json data again
    cJSON_Delete(json);
    free(json_str);
    return true;
}

bool query_monitors_and_workspaces(context_t* context, monitor_t* mon_buffer, int* monitors_queried, workspace_t** ws_buffer) {

    int result = -1;
    if(query_monitors(context,mon_buffer,monitors_queried)){
        *ws_buffer = calloc(*monitors_queried,sizeof(workspace_t));
        if(query_workspaces_from_monitors(context, mon_buffer, *monitors_queried, *ws_buffer))
        {
            for (size_t i = 0; i < *monitors_queried; i++) {
                log_out("Checking idx: %d data: {%d,%d,%s,%d}\n",i,(*ws_buffer)[i].monitor_id,(*ws_buffer)[i].workspace_id,(*ws_buffer)[i].monitor_name,(*ws_buffer)[i].windows);
                if(result == -1 || (*ws_buffer)[i].windows < result ){
                    result = (*ws_buffer)[i].windows;
                }
            }
        }
    }
    return result;
}

