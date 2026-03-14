#include "srt_io.h"
#include "time_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool srt_load_from_file(SubtitleList* list, const char* path) {
    if (!list || !path) return false;
    
    FILE* f = fopen(path, "r");
    if (!f) return false;
    
    char line[1024];
    int state = 0;
    double startTime = 0, endTime = 0;
    char textBuffer[4096] = {0};
    
    while (fgets(line, sizeof(line), f)) {
        if (state == 0) {
            state = 1;
        }
        else if (state == 1) {
            char startStr[32], endStr[32];
            if (sscanf(line, "%31s --> %31s", startStr, endStr) == 2) {
                startTime = parse_srt_time(startStr);
                endTime = parse_srt_time(endStr);
                textBuffer[0] = '\0';
                state = 2;
            }
        }
        else if (state == 2) {
            if (strcmp(line, "\n") == 0 || line[0] == '\n') {
                if (textBuffer[0] != '\0') {
                    size_t len = strlen(textBuffer);
                    if (len > 0 && textBuffer[len-1] == '\n') {
                        textBuffer[len-1] = '\0';
                    }
                    sublist_add(list, startTime, endTime, textBuffer);
                }
                state = 0;
            } else {
                strncat(textBuffer, line, sizeof(textBuffer) - strlen(textBuffer) - 1);
            }
        }
    }
    
    if (state == 2 && textBuffer[0] != '\0') {
        size_t len = strlen(textBuffer);
        if (len > 0 && textBuffer[len-1] == '\n') {
            textBuffer[len-1] = '\0';
        }
        sublist_add(list, startTime, endTime, textBuffer);
    }
    
    fclose(f);
    return list->count > 0;
}

bool srt_save_to_file(SubtitleList* list, const char* path) {
    if (!list || !path) return false;
    
    FILE* f = fopen(path, "w");
    if (!f) return false;
    
    for (int i = 0; i < list->count; i++) {
        Subtitle* sub = &list->items[i];
        
        char startStr[16];
        char endStr[16];
        format_srt_time(sub->startTime, startStr);
        format_srt_time(sub->endTime, endStr);
        
        fprintf(f, "%d\n", i + 1);
        fprintf(f, "%s --> %s\n", startStr, endStr);
        fprintf(f, "%s\n\n", sub->text ? sub->text : "");
    }
    
    fclose(f);
    return true;
}

bool srt_export_to_video_path(SubtitleList* list, const char* videoPath, char* messageOut, size_t messageSize) {
    if (!list || list->count == 0) {
        if (messageOut) snprintf(messageOut, messageSize, "No subtitles to export");
        return false;
    }
    
    if (!videoPath || videoPath[0] == '\0') {
        if (messageOut) snprintf(messageOut, messageSize, "No video file loaded");
        return false;
    }
    
    char srtPath[512];
    strncpy(srtPath, videoPath, sizeof(srtPath) - 1);
    srtPath[sizeof(srtPath) - 1] = '\0';
    
    char* dot = strrchr(srtPath, '.');
    if (dot) {
        strcpy(dot, ".srt");
    } else {
        strcat(srtPath, ".srt");
    }
    
    if (!srt_save_to_file(list, srtPath)) {
        if (messageOut) snprintf(messageOut, messageSize, "Failed to create file: %s", srtPath);
        return false;
    }
    
    if (messageOut) snprintf(messageOut, messageSize, "Exported %d subtitles to: %s", list->count, srtPath);
    return true;
}