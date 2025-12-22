#include "clock.h"

#include "platform/platform.h"

static platform_state* g_platform_state = 0;

void clock_set_platform_state(platform_state* plat_state) {
    g_platform_state = plat_state;
}

void clock_update(clock* clock) {
    if (clock->start_time != 0) {
        clock->elapsed = platform_get_absolute_time(g_platform_state) - clock->start_time;
    }
}

void clock_start(clock* clock) {
    clock->start_time = platform_get_absolute_time(g_platform_state);
    clock->elapsed = 0;
}

void clock_stop(clock* clock) {
    clock->start_time = 0;
}

f64 clock_get_absolute_time(platform_state* plat_state) {
    return platform_get_absolute_time(plat_state);
}
