#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

static char exeDir[512] = { 0 };
static char projectsDir[512] = { 0 };
static Project currentProject = { 0 };
static bool hasCurrentProject = false;

static void get_executable_dir(char* path, int maxLen) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, path, maxLen);
    path[maxLen - 1] = '\0';
    char* d = strrchr(path, '\\');
    if (d) *d = '\0';
#else
    char tmp[512];
    readlink("/proc/self/exe", tmp, sizeof(tmp));
    char* d = strrchr(tmp, '/');
    if (d) *d = '\0';
    strncpy(path, tmp, maxLen - 1);
    path[maxLen - 1] = '\0';
#endif
}

static bool dir_exists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static bool file_exists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
#endif
}

static int make_dir(const char* path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0755);
#endif
}

void project_init(void) {
    get_executable_dir(exeDir, sizeof(exeDir));
    
    snprintf(projectsDir, sizeof(projectsDir), "%s/projects", exeDir);
    
    if (!dir_exists(projectsDir)) {
        make_dir(projectsDir);
    }
    
    printf("[Project] Initialized: exeDir=%s, projectsDir=%s\n", exeDir, projectsDir);
}

const char* project_get_exe_dir(void) {
    return exeDir;
}

const char* project_get_projects_dir(void) {
    return projectsDir;
}

const char* project_get_working_srt_path(void) {
    if (!hasCurrentProject) {
        return NULL;
    }
    return currentProject.projectPath;
}

const char* project_get_source_srt_path(void) {
    if (!hasCurrentProject) {
        return NULL;
    }
    return currentProject.sourceSrtPath;
}

const char* project_get_video_path(void) {
    if (!hasCurrentProject) {
        return NULL;
    }
    return currentProject.videoPath;
}

const char* project_get_name(void) {
    if (!hasCurrentProject) {
        return NULL;
    }
    return currentProject.name;
}

static bool save_project_file(const char* name, const char* videoPath, const char* workingSrtFile) {
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%s/%s.subracer", projectsDir, name);
    
    FILE* f = fopen(filePath, "w");
    if (!f) return false;
    
    fprintf(f, "video_path=%s\n", videoPath);
    fprintf(f, "working_srt=%s\n", workingSrtFile);
    
    fclose(f);
    return true;
}

static bool load_project_file(const char* name, char* videoPathOut, char* workingSrtFileOut) {
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%s/%s.subracer", projectsDir, name);
    
    FILE* f = fopen(filePath, "r");
    if (!f) return false;
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "video_path=", 11) == 0) {
            strncpy(videoPathOut, line + 11, 511);
            videoPathOut[511] = '\0';
            char* p = strchr(videoPathOut, '\n');
            if (p) *p = '\0';
        }
        else if (strncmp(line, "working_srt=", 12) == 0) {
            strncpy(workingSrtFileOut, line + 12, 255);
            workingSrtFileOut[255] = '\0';
            char* p = strchr(workingSrtFileOut, '\n');
            if (p) *p = '\0';
        }
    }
    
    fclose(f);
    return true;
}

static void get_srt_file_path_from_video_path(const char* videoPath, char* result, int maxLen) {
    // Copy the full path first
    strncpy(result, videoPath, maxLen - 1);
    result[maxLen - 1] = '\0';
    
    // Find the last dot to replace the extension
    char* dot = strrchr(result, '.');
    if (dot) {
        strcpy(dot, ".srt");
    } else {
        // No extension found, just append .srt
        strncat(result, ".srt", maxLen - strlen(result) - 1);
    }
}

static void generate_unique_name(const char* baseName, const char* ext, char* result, int maxLen) {
    char testPath[512];
    
    snprintf(result, maxLen, "%s", baseName);
    snprintf(testPath, sizeof(testPath), "%s/%s%s", projectsDir, result, ext);
    
    if (!file_exists(testPath)) {
        return;
    }
    
    int counter = 1;
    while (1) {
        snprintf(result, maxLen, "%s_%d", baseName, counter);
        snprintf(testPath, sizeof(testPath), "%s/%s%s", projectsDir, result, ext);
        
        if (!file_exists(testPath)) {
            return;
        }
        
        counter++;
    }
}

bool project_create(const char* name, const char* videoPath) {
    char uniqueName[256];
    generate_unique_name(name, ".subracer", uniqueName, sizeof(uniqueName));
    
    char workingSrtFile[256];
    char srtBase[256];
    snprintf(srtBase, sizeof(srtBase), "%s_working.srt", uniqueName);
    generate_unique_name(srtBase, ".srt", workingSrtFile, sizeof(workingSrtFile));

    char sourceSrtPath[512];
    get_srt_file_path_from_video_path(videoPath, sourceSrtPath, sizeof(sourceSrtPath));

    
    if (!save_project_file(uniqueName, videoPath, workingSrtFile)) {
        printf("[Project] Failed to create project file\n");
        return false;
    }
    
    strncpy(currentProject.name, uniqueName, sizeof(currentProject.name) - 1);
    currentProject.name[sizeof(currentProject.name) - 1] = '\0';
    
    strncpy(currentProject.videoPath, videoPath, sizeof(currentProject.videoPath) - 1);
    currentProject.videoPath[sizeof(currentProject.videoPath) - 1] = '\0';
    
    strncpy(currentProject.workingSrtFile, workingSrtFile, sizeof(currentProject.workingSrtFile) - 1);
    currentProject.workingSrtFile[sizeof(currentProject.workingSrtFile) - 1] = '\0';

    strncpy(currentProject.sourceSrtPath, sourceSrtPath, sizeof(currentProject.sourceSrtPath) - 1);
    currentProject.sourceSrtPath[sizeof(currentProject.sourceSrtPath) - 1] = '\0';
    
    snprintf(currentProject.projectPath, sizeof(currentProject.projectPath), 
             "%s/%s", projectsDir, workingSrtFile);
    
    hasCurrentProject = true;
    
    printf("[Project] Created: name=%s, video=%s, srt=%s\n", 
           uniqueName, videoPath, workingSrtFile);
    
    return true;
}

bool project_load(const char* name) {
    char videoPath[512];
    char workingSrtFile[256];
    
    if (!load_project_file(name, videoPath, workingSrtFile)) {
        printf("[Project] Failed to load project: %s\n", name);
        return false;
    }
    
    if (!file_exists(videoPath)) {
        printf("[Project] Video file not found: %s\n", videoPath);
        return false;
    }

    char sourceSrtPath[512];
    get_srt_file_path_from_video_path(videoPath, sourceSrtPath, sizeof(sourceSrtPath));

    
    strncpy(currentProject.name, name, sizeof(currentProject.name) - 1);
    currentProject.name[sizeof(currentProject.name) - 1] = '\0';
    
    strncpy(currentProject.videoPath, videoPath, sizeof(currentProject.videoPath) - 1);
    currentProject.videoPath[sizeof(currentProject.videoPath) - 1] = '\0';
    
    strncpy(currentProject.workingSrtFile, workingSrtFile, sizeof(currentProject.workingSrtFile) - 1);
    currentProject.workingSrtFile[sizeof(currentProject.workingSrtFile) - 1] = '\0';

    strncpy(currentProject.sourceSrtPath, sourceSrtPath, sizeof(currentProject.sourceSrtPath) - 1);
    currentProject.sourceSrtPath[sizeof(currentProject.sourceSrtPath) - 1] = '\0';
    
    snprintf(currentProject.projectPath, sizeof(currentProject.projectPath), 
             "%s/%s", projectsDir, workingSrtFile);
    
    hasCurrentProject = true;
    
    printf("[Project] Loaded: name=%s, video=%s, srt=%s\n", 
           name, videoPath, workingSrtFile);
    
    return true;
}

void project_save_current(void) {
    if (!hasCurrentProject) return;
    
    save_project_file(currentProject.name, currentProject.videoPath, currentProject.workingSrtFile);
}

bool project_has_current(void) {
    return hasCurrentProject;
}

char** project_list(int* count) {
#ifdef _WIN32
    WIN32_FIND_DATA data;
    HANDLE hFind;
    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s/*.subracer", projectsDir);
    
    int capacity = 16;
    int size = 0;
    char** list = malloc(capacity * sizeof(char*));
    
    typedef struct {
        char name[256];
        FILETIME mtime;
    } ProjectEntry;
    
    ProjectEntry* entries = malloc(capacity * sizeof(ProjectEntry));
    
    hFind = FindFirstFileA(searchPath, &data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            int len = strlen(data.cFileName);
            if (len > 9) {
                if (size >= capacity) {
                    capacity *= 2;
                    list = realloc(list, capacity * sizeof(char*));
                    entries = realloc(entries, capacity * sizeof(ProjectEntry));
                }
                
                char name[256];
                strncpy(name, data.cFileName, len - 9);
                name[len - 9] = '\0';
                
                strncpy(entries[size].name, name, 255);
                entries[size].name[255] = '\0';
                entries[size].mtime = data.ftLastWriteTime;
                
                list[size] = malloc(strlen(name) + 1);
                strcpy(list[size], name);
                size++;
            }
        } while (FindNextFileA(hFind, &data));
        FindClose(hFind);
    }
    
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            int cmp = CompareFileTime(&entries[j].mtime, &entries[i].mtime);
            if (cmp > 0) {
                ProjectEntry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
                
                char* tmp = list[i];
                list[i] = list[j];
                list[j] = tmp;
            }
        }
    }
    
    free(entries);
    
    *count = size;
    return list;
#else
    DIR* dir = opendir(projectsDir);
    if (!dir) {
        *count = 0;
        return NULL;
    }
    
    int capacity = 16;
    int size = 0;
    char** list = malloc(capacity * sizeof(char*));
    
    typedef struct {
        char name[256];
        time_t mtime;
    } ProjectEntry;
    
    ProjectEntry* entries = malloc(capacity * sizeof(ProjectEntry));
    
    struct dirent* entry;
    struct stat st;
    while ((entry = readdir(dir)) != NULL) {
        int len = strlen(entry->d_name);
        if (len > 9 && strcmp(entry->d_name + len - 9, ".subracer") == 0) {
            if (size >= capacity) {
                capacity *= 2;
                list = realloc(list, capacity * sizeof(char*));
                entries = realloc(entries, capacity * sizeof(ProjectEntry));
            }
            
            char name[256];
            strncpy(name, entry->d_name, len - 9);
            name[len - 9] = '\0';
            
            strncpy(entries[size].name, name, 255);
            entries[size].name[255] = '\0';
            
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", projectsDir, entry->d_name);
            stat(fullPath, &st);
            entries[size].mtime = st.st_mtime;
            
            list[size] = malloc(strlen(name) + 1);
            strcpy(list[size], name);
            size++;
        }
    }
    
    closedir(dir);
    
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (entries[j].mtime > entries[i].mtime) {
                ProjectEntry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
                
                char* tmp = list[i];
                list[i] = list[j];
                list[j] = tmp;
            }
        }
    }
    
    free(entries);
    
    *count = size;
    return list;
#endif
}

void project_list_free(char** list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

bool project_delete(const char* name) {
    char subracerPath[512];
    snprintf(subracerPath, sizeof(subracerPath), "%s/%s.subracer", projectsDir, name);
    
    char srtPath[512];
    snprintf(srtPath, sizeof(srtPath), "%s/%s_working.srt", projectsDir, name);
    
    remove(srtPath);
    remove(subracerPath);
    
    return true;
}
