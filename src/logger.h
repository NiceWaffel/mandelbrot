#ifndef _LOGGER_H_
#define _LOGGER_H_

typedef enum {
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    VERBOSE = 4,
    DEBUG = 5
} LogLevel;

void log(LogLevel level, const char *message, ...);

void setLogLevel(LogLevel level);

#endif

