#ifndef SRT_IO_H
#define SRT_IO_H

#include "subtitle.h"
#include <stdbool.h>

bool srt_load_from_file(SubtitleList* list, const char* path);
bool srt_save_to_file(SubtitleList* list, const char* path);
bool srt_export_to_video_path(SubtitleList* list, const char* videoPath, char* messageOut, size_t messageSize);

#endif