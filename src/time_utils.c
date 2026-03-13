#include "time_utils.h"
#include <stdio.h>
#include <string.h>

void format_srt_time(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    int millis = (int)((seconds - (int)seconds) * 1000);
    snprintf(output, 16, "%02d:%02d:%02d,%03d", hours, minutes, secs, millis);
}

void format_display_time(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    int millis = (int)(((seconds - (int)seconds) * 1000) + 0.5);
    snprintf(output, 16, "%02d:%02d:%02d.%03d", hours, minutes, secs, millis);
}

void format_display_time_short(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    snprintf(output, 16, "%02d:%02d:%02d", hours, minutes, secs);
}

double parse_srt_time(const char* timeStr) {
    int hours = 0, minutes = 0, seconds = 0, millis = 0;
    sscanf(timeStr, "%d:%d:%d,%d", &hours, &minutes, &seconds, &millis);
    return hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
}

double parse_display_time(const char* timeStr) {
    int hours = 0, minutes = 0, seconds = 0, millis = 0;
    sscanf(timeStr, "%d:%d:%d.%d", &hours, &minutes, &seconds, &millis);
    return hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
}
