#ifndef TIME_UTILS_H
#define TIME_UTILS_H

void format_srt_time(double seconds, char* output);
void format_display_time(double seconds, char* output);
void format_display_time_short(double seconds, char* output);
double parse_srt_time(const char* timeStr);
double parse_display_time(const char* timeStr);

#endif
