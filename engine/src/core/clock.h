#pragma once
#include "defines.h"

typedef struct platform_state platform_state;

typedef struct clock {
    f64 start_time;
    f64 elapsed;
} clock;

// Updates the provided clock. Should be called just before checking elapsed time.
// Has no effect on non-started clocks.
void clock_update(clock* clock);

// Starts the provided clock. Resets elapsed time.
void clock_start(clock* clock);

// Stops the provided clock. Does not reset elapsed time.
void clock_stop(clock* clock);

// Sets the platform state for the clock system to use
void clock_set_platform_state(platform_state* plat_state);

// Gets the absolute time from the platform
f64 clock_get_absolute_time(platform_state* plat_state);
