#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

LogLevel loglevel;

const char *getLevelName(int level) {
	switch(level) {
		case DEBUG: return "DEBUG";
		case VERBOSE: return "VERBOSE";
		case INFO: return "INFO";
		case WARN: return "WARN";
		case ERROR: return "ERROR";
		default: return "LOG";
	}
}

void mandelLog(LogLevel level, const char *message, ...) {
	if(level > loglevel)
		return;
	va_list va;
	va_start(va, message);
	if(level <= WARN) { // WARN or ERROR
		fprintf(stderr, "[%s] ", getLevelName(level));
		vfprintf(stderr, message, va);
	} else {
		fprintf(stdout, "[%s] ", getLevelName(level));
		vfprintf(stdout, message, va);
	}
	va_end(va);
}

void setLogLevel(LogLevel level) {
	loglevel = level;
}
