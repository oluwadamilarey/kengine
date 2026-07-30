#include "logger.h"
#include "defines.h"
// TODO: temporary
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Kernel/mach/boolean.h>
#include "platform/filesystem.h"
#include "core/kmemory.h"

typedef struct logger_system_state {
    file_handle log_file_handle;
} logger_system_state;

static logger_system_state* state_ptr;
void append_to_log_file(const char* message) {
    if (state_ptr && state_ptr->log_file_handle.is_valid) {
        u64 length = string_length(message);
        u64 written = 0;
        if (!filesystem_write(&state_ptr->log_file_handle, length, message, &written)) {
            platform_console_write_error("ERROR writing to console.log", LOG_LEVEL_ERROR);
        }
    }
}

b8 initialize_logging(u64* memory_requirement, void* state) {
    *memory_requirement = sizeof(logger_system_state);
    if (state == 0) {
        return true;
    }
    state_ptr = state;

    if (!filesystem_open("console.log", FILE_MODE_WRITE, false, &state_ptr->log_file_handle)) {
        platform_console_write_error("ERROR: Unable to open console.log for writing.", LOG_LEVEL_ERROR);
        return false;
    }

    KFATAL("A test message: %f", 3.14f);
    KERROR("A test message: %f", 3.14f);
    KWARN("A test message: %f", 3.14f);
    KINFO("A test message: %f", 3.14f);
    KDEBUG("A test message: %f", 3.14f);
    KTRACE("A test message: %f", 3.14f);

    return true;
}

void shutdown_logging(void* state) {
    // TODO: cleanup logging/write queued entries.
    state_ptr = 0;
}

KAPI void log_output(log_level level, const char* message, ...) {
    const char* level_strings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};
    b8 is_error = level < LOG_LEVEL_WARN;

    // technically inposes a 32k character limit on a single log entry , but..
    // don't do that
    char out_message[32000];
    // memset(out_message, 0, sizeof(out_message));
    kzero_memory(out_message, sizeof(out_message));

    // format original message.
    // Note: Oddly enough, MS's headers overrides the C/Clang va_list type with a "typedef char* va_list" in some
    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(out_message, 3200, message, arg_ptr);
    va_end(arg_ptr);

    char out_message2[32000];
    sprintf(out_message2, "%s%s\n", level_strings[level], out_message);

    // TODO: platform specific output.
    if (is_error) {
        platform_console_write_error(out_message2, level);
    } else {
        platform_console_write(out_message2, level);
    }

    // queue a copy to be written to the log file
    append_to_log_file(out_message);
};

void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line) {
    log_output(LOG_LEVEL_FATAL, "Assertion Failure: %s, in file: %s, line: %d\n", expression, message, file, line);
};