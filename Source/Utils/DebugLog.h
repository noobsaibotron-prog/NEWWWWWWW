#pragma once

#if AIEQ_GUI_DEBUG
#include <cstdio>
#include <cstdarg>

// Debug log helper — writes to /tmp/aieq_debug.log so it works from hosted plugins (Ableton etc.)
inline void aieqDebugLog(const char* fmt, ...) {
    static FILE* f = nullptr;
    if (!f) { f = fopen("/tmp/aieq_debug.log", "w"); if (!f) return; }
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fflush(f);
}
#endif
