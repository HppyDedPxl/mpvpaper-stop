#include "../include/logging.h"
#include "../include/constants.h"
#include <stdarg.h>
#include <stdio.h>

static bool VERBOSE_LOGGING = false;

void set_logger_verbose(bool flag){
    VERBOSE_LOGGING = flag;
}

void log_out(const char * message , ...){
    if(!VERBOSE_LOGGING) return;
    va_list args;
    va_start(args,message);

    vfprintf(stdout,message,args);

    va_end(args);
}
void log_err(const char* message, ...){
    va_list args;
    va_start(args,message);

    vfprintf(stderr,message,args);

    va_end(args);
}

void set_perror(const char *message, ...){
    va_list args;
    va_start(args,message);

    char formatted_message[SIZE_8KB];;
    snprintf(formatted_message, sizeof(formatted_message), message, args);
    log_err(formatted_message);
    perror(formatted_message);
    va_end(args);

}