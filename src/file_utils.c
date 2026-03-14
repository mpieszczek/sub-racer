#include "file_utils.h"
#include <string.h>

static const char* supported_extensions[] = {".mp4", ".avi", ".mkv", ".mov", ".webm"};

bool file_has_video_extension(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    for (int i = 0; i < (int)(sizeof(supported_extensions) / sizeof(supported_extensions[0])); i++) {
#ifdef _WIN32
        if (_stricmp(ext, supported_extensions[i]) == 0) {
#else
        if (strcasecmp(ext, supported_extensions[i]) == 0) {
#endif
            return true;
        }
    }
    return false;
}

const char* file_get_filename_from_path(const char* path) {
    const char* filename = strrchr(path, '/');
    if (!filename) filename = strrchr(path, '\\');
    if (!filename) return path;
    return filename + 1;
}