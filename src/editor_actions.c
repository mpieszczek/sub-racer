#include "editor_actions.h"
#include "srt_io.h"
#include "project.h"
#include "time_utils.h"
#include "file_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void action_select_subtitle(AppState* state, int index) {
    if (!state || index < 0 || index >= state->subtitles.count) return;
    
    state->subtitles.selectedIndex = index;
    Subtitle* sub = &state->subtitles.items[index];
    
    vp_seek(state->vp, sub->startTime);
    strncpy(state->editText, sub->text ? sub->text : "", sizeof(state->editText) - 1);
    state->editText[sizeof(state->editText) - 1] = '\0';
    format_display_time(sub->startTime, state->editStartStr);
    format_display_time(sub->endTime, state->editEndStr);
}

void action_set_start_from_playback(AppState* state) {
    if (!state || !state->vp) return;
    double currentTime = vp_get_time(state->vp);
    format_display_time(currentTime, state->editStartStr);
    state->startEditing = false;
    state->endEditing = false;
    state->textEditing = false;
    state->isTextEditing = false;
}

void action_set_end_from_playback(AppState* state) {
    if (!state || !state->vp) return;
    double currentTime = vp_get_time(state->vp);
    format_display_time(currentTime, state->editEndStr);
    state->startEditing = false;
    state->endEditing = false;
    state->textEditing = false;
    state->isTextEditing = false;
}

void action_save_subtitle(AppState* state) {
    if (!state) return;
    
    Subtitle* sub = sublist_get(&state->subtitles, state->subtitles.selectedIndex);
    if (!sub) return;
    
    free(sub->text);
    sub->text = malloc(strlen(state->editText) + 1);
    strcpy(sub->text, state->editText);
    sub->startTime = parse_display_time(state->editStartStr);
    sub->endTime = parse_display_time(state->editEndStr);
    
    sublist_sort(&state->subtitles);
    srt_save_to_file(&state->subtitles, project_get_working_srt_path());
    vp_refresh_subtitles(state->vp, project_get_working_srt_path());
}

void action_delete_subtitle(AppState* state) {
    if (!state) return;
    
    sublist_remove(&state->subtitles, state->subtitles.selectedIndex);
    state->subtitles.selectedIndex = -1;
    srt_save_to_file(&state->subtitles, project_get_working_srt_path());
    vp_refresh_subtitles(state->vp, project_get_working_srt_path());
}

void action_add_subtitle(AppState* state) {
    if (!state || !state->vp) return;
    
    if (vp_is_playing(state->vp)) {
        vp_pause(state->vp);
    }
    
    double currentTime = vp_get_time(state->vp);
    sublist_add(&state->subtitles, currentTime, currentTime + 3.0, "New subtitle");
    sublist_sort(&state->subtitles);
    state->subtitles.selectedIndex = state->subtitles.count - 1;
    
    Subtitle* sub = sublist_get(&state->subtitles, state->subtitles.selectedIndex);
    if (sub) {
        strncpy(state->editText, sub->text, sizeof(state->editText) - 1);
        format_display_time(sub->startTime, state->editStartStr);
        format_display_time(sub->endTime, state->editEndStr);
    }
    
    srt_save_to_file(&state->subtitles, project_get_working_srt_path());
    vp_refresh_subtitles(state->vp, project_get_working_srt_path());
}

void action_sort_subtitles(AppState* state) {
    if (!state) return;
    sublist_sort(&state->subtitles);
}

bool action_load_video(AppState* state, const char* videoPath) {
    printf("[Editor] Loading video...\n");
    if (!state || !videoPath) return false;
    
    const char* filename = file_get_filename_from_path(videoPath);
    
    char projectName[256] = {0};
    strncpy(projectName, filename, sizeof(projectName) - 1);
    char* dot = strrchr(projectName, '.');
    if (dot) *dot = '\0';
    
    project_create(projectName, videoPath);
    
    printf("[Editor] Loading video: %s\n", videoPath);
    if (!vp_load(state->vp, videoPath)) {
        printf("[Editor] Failed to load video: %s\n", videoPath);
        return false;
    }
    
    printf("[Editor] Video loaded!\n");
    vp_play(state->vp);
    strncpy(state->currentVideoPath, videoPath, sizeof(state->currentVideoPath) - 1);
    state->currentVideoPath[sizeof(state->currentVideoPath) - 1] = '\0';
    
    const char* srtPath = project_get_source_srt_path();
    if (srtPath) {
        FILE* testF = fopen(srtPath, "r");
        if (testF) {
            fclose(testF);
            sublist_clear(&state->subtitles);
            srt_load_from_file(&state->subtitles, srtPath);
        }
        srt_save_to_file(&state->subtitles, project_get_working_srt_path());
        vp_refresh_subtitles(state->vp, project_get_working_srt_path());
    }
    
    state->showProjectPanel = false;
    return true;
}

void action_goto_project_panel(AppState* state) {
    if (!state || !state->vp) return;
    
    vp_unload(state->vp);
    state->currentVideoPath[0] = '\0';
    sublist_clear(&state->subtitles);
    state->subtitles.selectedIndex = -1;
    state->showProjectPanel = true;
}