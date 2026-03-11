#ifndef PROJECT_H
#define PROJECT_H

#include <stdbool.h>

typedef struct {
    char name[256];
    char videoPath[512];
    char workingSrtFile[256];
    char projectPath[512];
} Project;

void project_init(void);
const char* project_get_exe_dir(void);
const char* project_get_projects_dir(void);
const char* project_get_working_srt_path(void);
const char* project_get_video_path(void);
const char* project_get_name(void);

bool project_create(const char* name, const char* videoPath);
bool project_load(const char* name);
void project_save_current(void);
bool project_has_current(void);

char** project_list(int* count);
void project_list_free(char** list, int count);

bool project_delete(const char* name);

#endif
