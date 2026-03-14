#ifndef EDITOR_ACTIONS_H
#define EDITOR_ACTIONS_H

#include "app_state.h"
#include <stdbool.h>

void action_select_subtitle(AppState* state, int index);
void action_set_start_from_playback(AppState* state);
void action_set_end_from_playback(AppState* state);
void action_save_subtitle(AppState* state);
void action_delete_subtitle(AppState* state);
void action_add_subtitle(AppState* state);
void action_sort_subtitles(AppState* state);
bool action_load_video(AppState* state, const char* videoPath);
void action_goto_project_panel(AppState* state);

#endif