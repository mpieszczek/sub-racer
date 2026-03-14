#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    double startTime;
    double endTime;
    char* text;
} Subtitle;

typedef struct {
    Subtitle* items;
    int count;
    int capacity;
    int selectedIndex;
} SubtitleList;

void sublist_init(SubtitleList* list);
void sublist_free(SubtitleList* list);

void sublist_add(SubtitleList* list, double start, double end, const char* text);
void sublist_remove(SubtitleList* list, int index);
void sublist_clear(SubtitleList* list);
void sublist_sort(SubtitleList* list);

Subtitle* sublist_get_at_time(SubtitleList* list, double time);
Subtitle* sublist_get(SubtitleList* list, int index);

#endif
