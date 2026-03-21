/*
	OSGLUFB_Logging.c

	Logging module for Linux Framebuffer port.
	Writes all logs to both stderr and a log file.
*/

#include "OSGLUFB_Common.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

LOCALVAR FILE *log_file = NULL;

GLOBALOSGLUPROC log_init(const char *log_path)
{
	if (log_path == NULL) {
		log_path = "minivmac.log";
	}

	log_file = fopen(log_path, "a");
	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file: %s\n", log_path);
	}
}

GLOBALOSGLUPROC log_close(void)
{
	if (log_file != NULL) {
		fclose(log_file);
		log_file = NULL;
	}
}

GLOBALOSGLUPROC log_printf(const char *fmt, ...)
{

	va_list args;
	char buffer[4096];
	time_t now;
	struct tm *tm_info;
	char time_buf[64];

	/* Format timestamp */
	time(&now);
	tm_info = localtime(&now);
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

	/* Format message */
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	/* Write to stderr */
	//fprintf(stderr, "[%s] %s", time_buf, buffer);
	//fflush(stderr);

	/* Write to log file */
	if (log_file != NULL) {
		fprintf(log_file, "[%s] %s", time_buf, buffer);
		fflush(log_file);
	}
}