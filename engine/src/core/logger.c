#include "logger.h"
#include "defines.h"
// TODO: temporary
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Kernel/mach/boolean.h>

KAPI b8 initialize_logging() {
    return TRUE;
};

void shutdown_logging() {
};

KAPI void log_output(log_level level, const char* message, ...) {
    const char* level_strings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};
    // b8 is_error = level < 2;

    // technically inposes a 32k character limit on a single log entry , but..
    // don't do that
    char out_message[32000];
    memset(out_message, 0, sizeof(out_message));

    // format original message.
    // Note: Oddly enough, MS's headers overrides the C/Clang va_list type with a "typedef char* va_list" in some
    //  cases, and as a result throws a strange error here, the workaround for now is to just use __builtin_va_list
    //  which is the type GCC/Clang's va_start expects.
    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(out_message, 3200, message, arg_ptr);
    va_end(arg_ptr);

    char out_message2[32000];
    sprintf(out_message2, "%s%s\n", level_strings[level], out_message);

    // TODO: platform specific output.
    printf("%s", out_message2);
};
