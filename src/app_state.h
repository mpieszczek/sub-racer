#ifndef APP_STATE_H
#define APP_STATE_H

#include "video_player.h"
#include "subtitle.h"
#include <raylib.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define SIDE_PANEL_WIDTH 320
#define FONT_SIZE 20
#define FONT_SPACING 2

typedef struct {
    VideoPlayer* vp;
    SubtitleList subtitles;
    
    Font appFont;
    Texture2D logoTexture;
    bool logoLoaded;
    
    char currentVideoPath[512];
    
    bool showProjectPanel;
    int projectListScroll;
    char** projectList;
    int projectListCount;
    
    int subtitleListScroll;
    bool isDragging;
    bool wasPlayingBeforeDrag;
    bool startEditing;
    bool endEditing;
    bool textEditing;
    bool isTextEditing;
    char editText[256];
    char editStartStr[32];
    char editEndStr[32];
    
    bool showExportMessage;
    char exportMessage[256];
    bool exportSuccess;
    bool showOverwriteConfirm;
    
} AppState;

void app_state_init(AppState* state);
void app_state_free(AppState* state);

#endif