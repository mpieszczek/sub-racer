#include "raylib.h"
#include "raygui.h"
#include "app_state.h"
#include "ui_editor.h"
#include "editor_actions.h"
#include "video_player.h"
#include "project.h"
#include "file_utils.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Sub-Racer - Video Player");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    GuiLoadStyleDefault();
    GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_SIZE);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, UI_FG_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, UI_BG_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, UI_HL_COLOR_HEX);
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, UI_HL_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, UI_FG_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, UI_HL_COLOR_HEX);

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, UI_FG_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, UI_HL_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, UI_FG_COLOR_HEX);
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, UI_FG_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, UI_BG_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, UI_FG_COLOR_HEX);
    
    SetTargetFPS(60);
    
    project_init();
    
    AppState state;
    app_state_init(&state);
    
    state.vp = vp_create();
    if (!state.vp) {
        printf("Failed to create video player\n");
        CloseWindow();
        return 1;
    }
    
    const char* exeDir = project_get_exe_dir();
    char fontPath[512];
    snprintf(fontPath, sizeof(fontPath), "%s/resources/Jersey_10/Jersey10-Regular.ttf", exeDir);
    state.appFont = LoadFontEx(fontPath, FONT_SIZE, NULL, 65535);
    if (state.appFont.texture.id == 0) {
        printf("[Font] Failed to load Jersey 10, using default\n");
    } else {
        printf("[Font] Loaded Jersey 10 successfully\n");
        GuiSetFont(state.appFont);
    }
    app_state_calculate_timeline_height(&state, state.appFont);
    
    char logoPath[512];
    snprintf(logoPath, sizeof(logoPath), "%s/resources/logo.png", exeDir);
    Image logoImg = LoadImage(logoPath);
    if (logoImg.data != NULL) {
        state.logoTexture = LoadTextureFromImage(logoImg);
        UnloadImage(logoImg);
        state.logoLoaded = true;
        
        Image icon = LoadImage(logoPath);
        if (icon.data != NULL) {
            printf("[Icon] Loaded icon successfully\n");
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }
    
    const char* videoFile = NULL;
    if (argc > 1) {
        printf("[Main] Movie path passed as parameter\n");
        videoFile = argv[1];
    }
    
    if (videoFile && file_has_video_extension(videoFile)) {
        action_load_video(&state, videoFile);
    }
    
    while (!WindowShouldClose()) {
        if (state.showProjectPanel) {
            ui_render_project_panel(&state);
            ui_handle_file_drop(&state);
            continue;
        }
        
        ui_handle_file_drop(&state);
        
        vp_update(state.vp);
        
        if (IsKeyPressed(KEY_SPACE) && vp_is_loaded(state.vp) && !state.isTextEditing) {
            if (vp_is_playing(state.vp)) {
                vp_pause(state.vp);
            } else {
                vp_play(state.vp);
            }
        }
        
        if (IsKeyPressed(KEY_LEFT) && !state.isTextEditing) {
            double t = vp_get_time(state.vp);
            vp_seek(state.vp, t - 5.0);
        }
        
        if (IsKeyPressed(KEY_RIGHT) && !state.isTextEditing) {
            double t = vp_get_time(state.vp);
            vp_seek(state.vp, t + 5.0);
        }
        
        if ((IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_PERIOD)) && !state.isTextEditing && vp_is_loaded(state.vp)) {
            double fps = vp_get_fps(state.vp);
            if (fps <= 0) fps = 30.0;
            double frameTime = 1.0 / fps;
            double t = vp_get_time(state.vp);
            if (IsKeyPressed(KEY_COMMA)) {
                vp_seek(state.vp, t - frameTime);
            } else {
                vp_seek(state.vp, t + frameTime);
            }
        }
        
        if (IsKeyPressed(KEY_F11) && !state.isTextEditing) {
            if (!IsWindowFullscreen()) {
                int monitor = GetCurrentMonitor();
                SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            }
            ToggleFullscreen();
        }
        
        ui_render_editor(&state);
        
        int screenW = GetScreenWidth();
        DrawFPS(screenW - 80, 10);
    }
    
    app_state_free(&state);
    CloseWindow();
    
    return 0;
}