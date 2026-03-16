#include "app_state.h"
#include "project.h"
#include <string.h>
#include <stdlib.h>

void app_state_init(AppState* state) {
    memset(state, 0, sizeof(AppState));
    state->vp = NULL;
    sublist_init(&state->subtitles);
    state->subtitles.selectedIndex = -1;
    state->logoLoaded = false;
    state->showProjectPanel = true;
    state->projectList = NULL;
    state->projectListCount = 0;
    state->subtitleListScroll = 0;
    state->projectListScroll = 0;
    state->isDragging = false;
    state->wasPlayingBeforeDrag = false;
    state->startEditing = false;
    state->endEditing = false;
    state->textEditing = false;
    state->isTextEditing = false;
    state->showExportMessage = false;
    state->exportSuccess = false;
    state->showOverwriteConfirm = false;
    state->showTranscribeConfirm = false;
    state->showTranscribeProgress = false;
    state->showTranscribeComplete = false;
    state->transcribeCancel = false;
    state->transcribeProgress = 0;
    state->transcribeResultCount = 0;
    state->whisper = whisper_wrapper_create();
    state->timelinePanelHeight = 100;
}

void app_state_calculate_timeline_height(AppState* state, Font font) {
    int fontHeight = (font.texture.id != 0) ? font.baseSize : FONT_SIZE;
    int sliderHeight = fontHeight;
    int timeLineHeight = fontHeight;
    int helpLineHeight = fontHeight;
    int spacing = 10;
    int padding = 15;
    
    state->timelinePanelHeight = sliderHeight + timeLineHeight + helpLineHeight + 
                                  spacing * 2 + padding;
}

void app_state_free(AppState* state) {
    if (state->vp) {
        vp_destroy(state->vp);
        state->vp = NULL;
    }
    sublist_free(&state->subtitles);
    if (state->logoLoaded && state->logoTexture.id != 0) {
        UnloadTexture(state->logoTexture);
        state->logoLoaded = false;
    }
    if (state->appFont.texture.id != 0) {
        UnloadFont(state->appFont);
    }
    if (state->projectList) {
        project_list_free(state->projectList, state->projectListCount);
        state->projectList = NULL;
        state->projectListCount = 0;
    }
    if (state->whisper) {
        whisper_wrapper_free(state->whisper);
        state->whisper = NULL;
    }
}