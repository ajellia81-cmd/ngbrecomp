#pragma once
#include <stdio.h>
#include <unistd.h>
// Silence native-library stdout/stderr too. This opens no diagnostic file;
// gameplay saves/settings still use their ordinary independent FILE streams.
static inline int ngb_ios_quiet_stdio(void) {
    if (!freopen("/dev/null", "w", stdout)) return 0;
    return dup2(fileno(stdout), STDERR_FILENO) >= 0;
}
