#include "subtitle.h"
#include <string.h>
#include <stdio.h>

void sublist_init(SubtitleList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->selectedIndex = -1;
}

void sublist_free(SubtitleList* list) {
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].text) {
            free(list->items[i].text);
        }
    }
    if (list->items) {
        free(list->items);
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->selectedIndex = -1;
}

void sublist_add(SubtitleList* list, double start, double end, const char* text) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, list->capacity * sizeof(Subtitle));
    }
    
    Subtitle* sub = &list->items[list->count];
    sub->startTime = start;
    sub->endTime = end;
    
    if (text) {
        sub->text = malloc(strlen(text) + 1);
        strcpy(sub->text, text);
    } else {
        sub->text = malloc(1);
        sub->text[0] = '\0';
    }
    
    list->count++;
}

void sublist_remove(SubtitleList* list, int index) {
    if (index < 0 || index >= list->count) return;
    
    if (list->items[index].text) {
        free(list->items[index].text);
    }
    
    for (int i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    
    list->count--;
    
    if (list->selectedIndex == index) {
        list->selectedIndex = -1;
    } else if (list->selectedIndex > index) {
        list->selectedIndex--;
    }
}

void sublist_clear(SubtitleList* list) {
    sublist_free(list);
}

Subtitle* sublist_get_at_time(SubtitleList* list, double time) {
    for (int i = 0; i < list->count; i++) {
        if (time >= list->items[i].startTime && time <= list->items[i].endTime) {
            return &list->items[i];
        }
    }
    return NULL;
}

Subtitle* sublist_get(SubtitleList* list, int index) {
    if (index < 0 || index >= list->count) return NULL;
    return &list->items[index];
}
