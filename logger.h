#ifndef _LOGGER_H_
#define _LOGGER_H_

#define EXTRA_DEBUG 6
#define DEBUG 5
#define VERBOSE 4
#define INFO 3
#define WARN 2
#define ERROR 1

extern int loglevel;

void log(int level, const char *message, ...);

#endif

