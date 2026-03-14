#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdbool.h>

bool file_has_video_extension(const char* filename);
const char* file_get_filename_from_path(const char* path);

#endif