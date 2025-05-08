#include "logger.h"
#include "config.h"
#include <stdarg.h>
#include <stdio.h>

void info(const char *format, ...) {
    if (!config.verbose) return;

    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:info: ", config.program);
    vfprintf(stderr, format, args);
    va_end(args);
}

void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:error: ", config.program);
    vfprintf(stderr, format, args);
    va_end(args);
}