#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

void set_logger_verbose(bool flag);
void log_out(const char * message , ...);
void log_err(const char* message, ...);
void set_perror(const char* message, ...);

#endif