/*******************************************************************************************
 *   Sub-Racer - Video Subtitle Editor
 *******************************************************************************************/

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "video_player.h"
#include "subtitle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define SIDE_PANEL_WIDTH 300

static char currentVideoPath[512] = { 0 };
static bool showExportMessage = false;
static char exportMessage[256] = { 0 };
static bool exportSuccess = false;
static char workingSrtPath[512] = "./sub_racer_working.srt";

static void format_srt_time(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    int millis = (int)((seconds - (int)seconds) * 1000);
    snprintf(output, 16, "%02d:%02d:%02d,%03d", hours, minutes, secs, millis);
}

static double parse_srt_time(const char* timeStr) {
    int hours = 0, minutes = 0, seconds = 0, millis = 0;
    sscanf(timeStr, "%d:%d:%d,%d", &hours, &minutes, &seconds, &millis);
    return hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
}

static bool get_srt_path(const char* videoPath, char* srtPathOut, size_t outSize) {
    if (!videoPath || !srtPathOut) return false;
    
    strncpy(srtPathOut, videoPath, outSize - 1);
    srtPathOut[outSize - 1] = '\0';
    
    char* dot = strrchr(srtPathOut, '.');
    if (dot) {
        strcpy(dot, ".srt");
    } else {
        return false;
    }
    
    FILE* f = fopen(srtPathOut, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static void sort_subtitles(SubtitleList* list) {
    if (!list || list->count <= 1) return;
    
    for (int i = 1; i < list->count; i++) {
        Subtitle key = list->items[i];
        int j = i - 1;
        
        while (j >= 0 && list->items[j].startTime > key.startTime) {
            list->items[j + 1] = list->items[j];
            j--;
        }
        list->items[j + 1] = key;
    }
}

static bool load_srt_to_subtitle_list(SubtitleList* list, const char* srtPath) {
    if (!list || !srtPath) return false;
    
    FILE* f = fopen(srtPath, "r");
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

static bool save_working_srt(SubtitleList* list) {
    if (!list) return false;
    
    FILE* f = fopen(workingSrtPath, "w");
    if (!f) {
        return false;
    }
    
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

static bool export_subtitles_to_srt(SubtitleList* list, const char* videoPath) {
    if (!list || list->count == 0) {
        snprintf(exportMessage, sizeof(exportMessage), "No subtitles to export");
        return false;
    }
    
    if (!videoPath || videoPath[0] == '\0') {
        snprintf(exportMessage, sizeof(exportMessage), "No video file loaded");
        return false;
    }
    
    // Build .srt path
    char srtPath[512];
    strncpy(srtPath, videoPath, sizeof(srtPath) - 1);
    srtPath[sizeof(srtPath) - 1] = '\0';
    
    char* dot = strrchr(srtPath, '.');
    if (dot) {
        strcpy(dot, ".srt");
    } else {
        strcat(srtPath, ".srt");
    }
    
    FILE* f = fopen(srtPath, "w");
    if (!f) {
        snprintf(exportMessage, sizeof(exportMessage), "Failed to create file: %s", srtPath);
        return false;
    }
    
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
    
    snprintf(exportMessage, sizeof(exportMessage), "Exported %d subtitles to:\n%s", list->count, srtPath);
    return true;
}

static const char* supportedExtensions[] = { ".mp4", ".avi", ".mkv", ".mov", ".webm" };

static bool has_video_extension(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    for (int i = 0; i < sizeof(supportedExtensions)/sizeof(supportedExtensions[0]); i++) {
        if (strcasecmp(ext, supportedExtensions[i]) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Sub-Racer - Video Player");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    GuiLoadStyleDefault();
    
    SetTargetFPS(60);
    
    VideoPlayer* vp = vp_create();
    if (!vp) {
        printf("Failed to create video player\n");
        CloseWindow();
        return 1;
    }
    
    SubtitleList subtitles;
    sublist_init(&subtitles);
    
    char editText[256] = { 0 };
    char editStartStr[32] = { 0 };
    char editEndStr[32] = { 0 };
    
    const char* videoFile = NULL;
    if (argc > 1) {
        videoFile = argv[1];
    }
    
    if (videoFile && has_video_extension(videoFile)) {
        printf("Loading video: %s\n", videoFile);
        if (!vp_load(vp, videoFile)) {
            printf("Failed to load video: %s\n", videoFile);
            videoFile = NULL;
        } else {
            printf("Video loaded!\n");
            vp_play(vp);
            strncpy(currentVideoPath, videoFile, sizeof(currentVideoPath) - 1);
            currentVideoPath[sizeof(currentVideoPath) - 1] = '\0';
            
            char srtPath[512];
            if (get_srt_path(videoFile, srtPath, sizeof(srtPath))) {
                printf("Loading SRT: %s\n", srtPath);
                sublist_clear(&subtitles);
                load_srt_to_subtitle_list(&subtitles, srtPath);
                save_working_srt(&subtitles);
                vp_refresh_subtitles(vp, workingSrtPath);
            }
        }
    }
    
    static float lastSliderValue = 0;
    static bool wasPlayingBeforeDrag = false;
    static bool isDragging = false;
    static bool startEditing = false;
    static bool endEditing = false;
    static bool textEditing = false;
    static bool isTextEditing = false;
    
    while (!WindowShouldClose()) {
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                for (unsigned int i = 0; i < droppedFiles.count; i++) {
                    const char* file = droppedFiles.paths[i];
                    if (has_video_extension(file)) {
                        printf("Loading dropped video: %s\n", file);
                        if (vp_load(vp, file)) {
                            videoFile = file;
                            vp_play(vp);
                            strncpy(currentVideoPath, file, sizeof(currentVideoPath) - 1);
                            currentVideoPath[sizeof(currentVideoPath) - 1] = '\0';
                            
                            char srtPath[512];
                            if (get_srt_path(file, srtPath, sizeof(srtPath))) {
                                printf("Loading SRT: %s\n", srtPath);
                                sublist_clear(&subtitles);
                                load_srt_to_subtitle_list(&subtitles, srtPath);
                                save_working_srt(&subtitles);
                                vp_refresh_subtitles(vp, workingSrtPath);
                            }
                            
                            printf("Video loaded successfully\n");
                        } else {
                            printf("Failed to load dropped video\n");
                        }
                        break;
                    }
                }
            }
            UnloadDroppedFiles(droppedFiles);
        }
        
        vp_update(vp);
        
        if (IsKeyPressed(KEY_SPACE) && vp_is_loaded(vp) && !isTextEditing) {
            if (vp_is_playing(vp)) {
                vp_pause(vp);
            } else {
                vp_play(vp);
            }
        }
        
        if (IsKeyPressed(KEY_LEFT) && !isTextEditing) {
            double t = vp_get_time(vp);
            vp_seek(vp, t - 5.0);
        }
        
        if (IsKeyPressed(KEY_RIGHT) && !isTextEditing) {
            double t = vp_get_time(vp);
            vp_seek(vp, t + 5.0);
        }
        
        if ((IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_PERIOD)) && !isTextEditing && vp_is_loaded(vp)) {
            double fps = vp_get_fps(vp);
            if (fps <= 0) fps = 30.0;
            double frameTime = 1.0 / fps;
            double t = vp_get_time(vp);
            if (IsKeyPressed(KEY_COMMA)) {
                vp_seek(vp, t - frameTime);
            } else {
                vp_seek(vp, t + frameTime);
            }
        }
        
        if (IsKeyPressed(KEY_F11) && !isTextEditing) {
            ToggleFullscreen();
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        if (vp_is_loaded(vp)) {
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int w = vp_get_width(vp);
            int h = vp_get_height(vp);
            int videoW = screenW - SIDE_PANEL_WIDTH;
            
            if (w > 0 && h > 0) {
                float margin = 0.1f;
                Rectangle videoDest = { 
                    0, 0, 
                    (float)videoW, (float)(screenH * (1.0f - margin)) 
                };
                vp_render(vp, videoDest);
                
                Rectangle videoRect = vp_get_video_rect(vp, videoDest);
                DrawRectangleLinesEx(videoRect, 2.0f, RED);
                #ifdef USE_RAYLIB_TEXT
                Subtitle* currentSub = sublist_get_at_time(&subtitles, vp_get_time(vp));
                if (currentSub && currentSub->text && currentSub->text[0] != '\0') {
                    Font font = GetFontDefault();
                    int fontSize = 24;
                    Vector2 textSize = MeasureTextEx(font, currentSub->text, fontSize, 2);
                    float textX = videoW / 2 - textSize.x / 2;
                    float textY = screenH * (1.0f - margin) - textSize.y - 20;
                    
                    int bgPadding = 10;
                    DrawRectangle((int)textX - bgPadding, (int)textY - bgPadding,
                                  (int)textSize.x + bgPadding * 2, (int)textSize.y + bgPadding * 2,
                                  (Color){0, 0, 0, 150});
                    DrawTextEx(font, currentSub->text, (Vector2){textX, textY}, fontSize, 2, WHITE);
                }
                #endif
                
                int panelY = (int)(screenH * (1.0f - margin));
                int panelH = (int)(screenH * margin);
                
                DrawRectangle(0, panelY, videoW, panelH, (Color){40, 40, 40, 255});
                
                double time = vp_get_time(vp);
                double dur = vp_get_duration(vp);
                
                float sliderMax = (float)dur;
                if (sliderMax <= 0) sliderMax = 1;
                
                float sliderValue = (float)time;
                
                Rectangle sliderRec = { 10, panelY + 10, videoW - 20, 20 };
                
                float prevSliderValue = sliderValue;
                GuiSlider(sliderRec, NULL, NULL, &sliderValue, 0, sliderMax);
                
                bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
                
                if (!isDragging && sliderValue != prevSliderValue && mouseDown) {
                    isDragging = true;
                    wasPlayingBeforeDrag = vp_is_playing(vp);
                    vp_pause(vp);
                }
                
                if (isDragging) {
                    if (!mouseDown) {
                        isDragging = false;
                        vp_seek(vp, sliderValue);
                        if (wasPlayingBeforeDrag) {
                            vp_play(vp);
                        }
                    } else if (sliderValue != time) {
                        vp_seek(vp, sliderValue);
                    }
                }
                
                char timeText[64];
                snprintf(timeText, sizeof(timeText), "%.1f / %.1f", time, dur);
                DrawText(timeText, 10, panelY + 40, 20, WHITE);
                
                Rectangle exportBtn = { videoW - 100, panelY + 40, 90, 20 };
                if (GuiButton(exportBtn, "Export")) {
                    exportSuccess = export_subtitles_to_srt(&subtitles, currentVideoPath);
                    showExportMessage = true;
                }
                
                char helpText[] = "SPACE: Play/Pause | LEFT/RIGHT: Seek";
                DrawText(helpText, 10, panelY + 70, 20, YELLOW);
            } else {
                int screenW = GetScreenWidth();
                int screenH = GetScreenHeight();
                DrawText("Loading video...", (screenW - SIDE_PANEL_WIDTH)/2 - 60, screenH/2, 20, YELLOW);
            }
            
            DrawRectangle(screenW - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, screenH, (Color){50, 50, 50, 255});
            
            Rectangle headerBtn = { screenW - SIDE_PANEL_WIDTH + 10, 10, SIDE_PANEL_WIDTH - 20, 30 };
            if (GuiButton(headerBtn, "+ Add")) {
                if (vp_is_playing(vp)) {
                    vp_pause(vp);
                }
                double currentTime = vp_get_time(vp);
                sublist_add(&subtitles, currentTime, currentTime + 3.0, "New subtitle");
                sort_subtitles(&subtitles);
                subtitles.selectedIndex = subtitles.count - 1;
                Subtitle* sub = sublist_get(&subtitles, subtitles.selectedIndex);
                if (sub) {
                    strncpy(editText, sub->text, sizeof(editText) - 1);
                    snprintf(editStartStr, sizeof(editStartStr), "%.1f", sub->startTime);
                    snprintf(editEndStr, sizeof(editEndStr), "%.1f", sub->endTime);
                }
                save_working_srt(&subtitles);
                vp_refresh_subtitles(vp, workingSrtPath);
            }
            
            int listY = 50;
            int listH = screenH - listY - 10;
            
            for (int i = 0; i < subtitles.count; i++) {
                Subtitle* sub = &subtitles.items[i];
                Rectangle itemRec = { screenW - SIDE_PANEL_WIDTH + 10, listY + i * 35, SIDE_PANEL_WIDTH - 20, 30 };
                
                Color itemBg = (i == subtitles.selectedIndex) ? (Color){80, 80, 120, 255} : (Color){60, 60, 60, 255};
                DrawRectangleRec(itemRec, itemBg);
                
                char timeStr[32];
                int startMin = (int)(sub->startTime / 60);
                int startSec = (int)((int)sub->startTime % 60);
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", startMin, startSec);
                
                DrawText(timeStr, itemRec.x + 5, itemRec.y + 7, 14, YELLOW);
                
                char displayText[32] = {0};
                if (sub->text) {
                    strncpy(displayText, sub->text, 20);
                    if (strlen(sub->text) > 20) strcat(displayText, "...");
                }
                DrawText(displayText, itemRec.x + 60, itemRec.y + 7, 14, WHITE);
                
                if (CheckCollisionPointRec(GetMousePosition(), itemRec) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    subtitles.selectedIndex = i;
                    vp_seek(vp, sub->startTime);
                    strncpy(editText, sub->text ? sub->text : "", sizeof(editText) - 1);
                    snprintf(editStartStr, sizeof(editStartStr), "%.1f", sub->startTime);
                    snprintf(editEndStr, sizeof(editEndStr), "%.1f", sub->endTime);
                }
            }
            
            if (subtitles.selectedIndex >= 0) {
                int editY = screenH - 180;
                Rectangle editPanel = { screenW - SIDE_PANEL_WIDTH + 10, editY, SIDE_PANEL_WIDTH - 20, 170 };
                DrawRectangleRec(editPanel, (Color){45, 45, 45, 255});
                
                DrawText("Start:", editPanel.x + 5, editPanel.y + 5, 14, GRAY);
                Rectangle startRec = { editPanel.x + 60, editPanel.y + 5, 70, 20 };
                if (GuiTextBox(startRec, editStartStr, 32, startEditing)) {
                    startEditing = true;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = true;
                }
                
                DrawText("End:", editPanel.x + 170, editPanel.y + 5, 14, GRAY);
                Rectangle endRec = { editPanel.x + 210, editPanel.y + 5, 70, 20 };
                if (GuiTextBox(endRec, editEndStr, 32, endEditing)) {
                    endEditing = true;
                    startEditing = false;
                    textEditing = false;
                    isTextEditing = true;
                }
                
                Rectangle startSetBtn = { editPanel.x + 60, editPanel.y + 28, 70, 15 };
                if (GuiButton(startSetBtn, ".")) {
                    double currentTime = vp_get_time(vp);
                    snprintf(editStartStr, sizeof(editStartStr), "%.1f", currentTime);
                    startEditing = false;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = false;
                }
                
                Rectangle endSetBtn = { editPanel.x + 210, editPanel.y + 28, 70, 15 };
                if (GuiButton(endSetBtn, ".")) {
                    double currentTime = vp_get_time(vp);
                    snprintf(editEndStr, sizeof(editEndStr), "%.1f", currentTime);
                    startEditing = false;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = false;
                }
                
                DrawText("Text:", editPanel.x + 5, editPanel.y + 35, 14, GRAY);
                Rectangle textRec = { editPanel.x + 5, editPanel.y + 55, editPanel.width - 10, 60 };
                if (GuiTextBox(textRec, editText, 256, textEditing)) {
                    textEditing = true;
                    startEditing = false;
                    endEditing = false;
                    isTextEditing = true;
                }
                
                if ((startEditing || endEditing || textEditing) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (!CheckCollisionPointRec(GetMousePosition(), startRec) &&
                        !CheckCollisionPointRec(GetMousePosition(), endRec) &&
                        !CheckCollisionPointRec(GetMousePosition(), textRec)) {
                            textEditing = false;
                            startEditing = false;
                            endEditing = false;
                            isTextEditing = false;
                    }
                }
                
                Rectangle saveBtn = { editPanel.x + 5, editPanel.y + 125, 80, 25 };
                if (GuiButton(saveBtn, "Save")) {
                    Subtitle* sub = sublist_get(&subtitles, subtitles.selectedIndex);
                    if (sub) {
                        free(sub->text);
                        sub->text = malloc(strlen(editText) + 1);
                        strcpy(sub->text, editText);
                        sub->startTime = atof(editStartStr);
                        sub->endTime = atof(editEndStr);
                    }
                    sort_subtitles(&subtitles);
                    save_working_srt(&subtitles);
                    vp_refresh_subtitles(vp, workingSrtPath);
                }
                
                Rectangle delBtn = { editPanel.x + editPanel.width - 45, editPanel.y + 125, 40, 20 };
                if (GuiButton(delBtn, "DEL")) {
                    sublist_remove(&subtitles, subtitles.selectedIndex);
                    subtitles.selectedIndex = -1;
                    save_working_srt(&subtitles);
                    vp_refresh_subtitles(vp, workingSrtPath);
                }
            }
        } else {
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int cx = screenW / 2;
            int cy = screenH / 2;
            DrawText("Drag and drop a video file here", cx - 160, cy - 20, 20, GRAY);
            DrawText("or run: sub-racer.exe <video.mp4>", cx - 170, cy + 15, 20, GRAY);
        }
        
        int screenW = GetScreenWidth();
        DrawFPS(screenW - 80, 10);
        if (showExportMessage) {
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            Rectangle msgBox = { 
                screenW/2 - 150, 
                screenH/2 - 60, 
                300, 120 
            };
            
            int result = GuiMessageBox(
                msgBox,
                exportSuccess ? "#191#Export Success" : "#198#Export Error",
                exportMessage,
                "OK"
            );
            
            if (result >= 0) {
                showExportMessage = false;
            }
        }
        EndDrawing();
    }
    
    vp_destroy(vp);
    sublist_free(&subtitles);
    CloseWindow();
    
    return 0;
}
