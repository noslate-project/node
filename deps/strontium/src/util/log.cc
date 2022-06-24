#include <util/log.hpp>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

static const char* level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char* level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};

void log_log(int level, const char* file, int line, const char* fmt, ...) {
  time_t t = time(nullptr);
  auto time = localtime(&t);  // NOLINT

  char buf[16];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", time)] = '\0';

  printf("%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
         buf,
         level_colors[level],
         level_strings[level],
         file,
         line);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}
