/*******************************************************************************************
 *   Sub-Racer - Video Subtitle Editor
 *******************************************************************************************/

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "video_player.h"
#include "subtitle.h"
#include "project.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define SIDE_PANEL_WIDTH 320

#define BACKGROUND_COLOR      CLITERAL(Color){ 15, 56, 15, 255 }
#define BACKGROUND_COLOR_HEX  0x0f380fff
#define FOREGROUND_COLOR      CLITERAL(Color){ 48, 98, 48, 255 }
#define FOREGROUND_COLOR_HEX  0x306230ff
#define HIGHLIGHT_COLOR       CLITERAL(Color){ 139, 172, 15, 255 }
#define HIGHLIGHT_COLOR_HEX   0x8bac0fff

static char currentVideoPath[512] = { 0 };
static bool showExportMessage = false;
static char exportMessage[256] = { 0 };
static bool exportSuccess = false;
static bool showProjectPanel = false;
static bool showNewProjectDialog = false;
static char newProjectName[256] = { 0 };
static int projectListCount = 0;
static char** projectList = NULL;

static void format_srt_time(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    int millis = (int)((seconds - (int)seconds) * 1000);
    snprintf(output, 16, "%02d:%02d:%02d,%03d", hours, minutes, secs, millis);
}

static void format_display_time(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    int millis = (int)((seconds - (int)seconds) * 1000);
    snprintf(output, 16, "%02d:%02d:%02d.%03d", hours, minutes, secs, millis);
}

static void format_display_time_short(double seconds, char* output) {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);
    snprintf(output, 16, "%02d:%02d:%02d", hours, minutes, secs);
}

static double parse_srt_time(const char* timeStr) {
    int hours = 0, minutes = 0, seconds = 0, millis = 0;
    sscanf(timeStr, "%d:%d:%d,%d", &hours, &minutes, &seconds, &millis);
    return hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
}

static double parse_display_time(const char* timeStr) {
    int hours = 0, minutes = 0, seconds = 0, millis = 0;
    sscanf(timeStr, "%d:%d:%d.%d", &hours, &minutes, &seconds, &millis);
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
    
    const char* srtPath = project_get_working_srt_path();
    if (!srtPath) return false;
    
    FILE* f = fopen(srtPath, "w");
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
    GuiSetStyle(DEFAULT, TEXT_SIZE, 15);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, FOREGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, BACKGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, HIGHLIGHT_COLOR_HEX);
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, HIGHLIGHT_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, FOREGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, HIGHLIGHT_COLOR_HEX);

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, FOREGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, HIGHLIGHT_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, FOREGROUND_COLOR_HEX);
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, FOREGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, BACKGROUND_COLOR_HEX);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, FOREGROUND_COLOR_HEX);
    
    SetTargetFPS(60);
    
    Texture2D logoTexture = { 0 };
    bool logoLoaded = false;
    
    project_init();
    
    const char* exeDir = project_get_exe_dir();
    char logoPath[512];
    snprintf(logoPath, sizeof(logoPath), "%s/resources/logo.png", exeDir);
    Image logoImg = LoadImage(logoPath);
    if (logoImg.data != NULL) {
        logoTexture = LoadTextureFromImage(logoImg);
        UnloadImage(logoImg);
        logoLoaded = true;
        
        Image icon = LoadImage(logoPath);
        if (icon.data != NULL) {
            SetWindowIcon(icon);
        }
    }
    
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
        const char* filename = strrchr(videoFile, '/');
        if (!filename) filename = strrchr(videoFile, '\\');
        if (!filename) filename = videoFile;
        else filename++;
        
        char projectName[256] = {0};
        strncpy(projectName, filename, sizeof(projectName) - 1);
        char* dot = strrchr(projectName, '.');
        if (dot) *dot = '\0';
        
        project_create(projectName, videoFile);
        
        printf("Loading video: %s\n", videoFile);
        if (!vp_load(vp, videoFile)) {
            printf("Failed to load video: %s\n", videoFile);
            videoFile = NULL;
        } else {
            printf("Video loaded!\n");
            vp_play(vp);
            strncpy(currentVideoPath, videoFile, sizeof(currentVideoPath) - 1);
            currentVideoPath[sizeof(currentVideoPath) - 1] = '\0';
            
            const char* srtPath = project_get_working_srt_path();
            if (srtPath) {
                FILE* testF = fopen(srtPath, "r");
                if (testF) {
                    fclose(testF);
                    sublist_clear(&subtitles);
                    load_srt_to_subtitle_list(&subtitles, srtPath);
                }
                save_working_srt(&subtitles);
                vp_refresh_subtitles(vp, project_get_working_srt_path());
            }
        }
    } else {
        // Bez argumentu - pokaż panel wyboru projektów
        showProjectPanel = true;
    }
    
    static float lastSliderValue = 0;
    static bool wasPlayingBeforeDrag = false;
    static bool isDragging = false;
    static bool startEditing = false;
    static bool endEditing = false;
    static bool textEditing = false;
    static bool isTextEditing = false;
    static int subtitleListScroll = 0;
    static int projectListScroll = 0;
    
    while (!WindowShouldClose()) {
        if (showProjectPanel) {
            BeginDrawing();
            ClearBackground(BACKGROUND_COLOR);
            
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int cx = screenW / 2;
            int cy = screenH / 2;
            
            if (logoLoaded) {
                float scale = 0.5f;
                float logoW = logoTexture.width * scale;
                float logoH = logoTexture.height * scale;
                Rectangle src = { 0, 0, (float)logoTexture.width, (float)logoTexture.height };
                Rectangle dst = { cx - logoW*5, cy - 120 - logoH*7, logoW*10, logoH*10 };
                DrawTexturePro(logoTexture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
            }
            
            DrawText("Drag and drop a video file here", cx - 160, cy - 60, 20, FOREGROUND_COLOR);
            DrawText("or run: sub-racer.exe <video.mp4>", cx - 170, cy - 30, 20, FOREGROUND_COLOR);
            
            if (projectList) {
                project_list_free(projectList, projectListCount);
            }
            projectList = project_list(&projectListCount);
            
            if (projectList && projectListCount > 0) {
                int listY = cy + 70;
                int listH = screenH - listY - 50;
                if (listH < 50) listH = 50;
                int itemHeight = 30;
                int visibleItems = listH / itemHeight;
                
                int wheelMove = GetMouseWheelMove();
                if (wheelMove != 0) {
                    int maxScroll = projectListCount - visibleItems;
                    if (maxScroll < 0) maxScroll = 0;
                    projectListScroll += wheelMove;
                    if (projectListScroll < 0) projectListScroll = 0;
                    if (projectListScroll > maxScroll) projectListScroll = maxScroll;
                }
                
                if (projectListCount <= visibleItems) {
                    projectListScroll = 0;
                } else if (projectListScroll > projectListCount - visibleItems) {
                    projectListScroll = projectListCount - visibleItems;
                }
                
                DrawText("Projects:", cx - 200, listY - 20, 20, HIGHLIGHT_COLOR);
                for (int i = projectListScroll; i < projectListCount && i < projectListScroll + visibleItems; i++) {
                    Rectangle projRec = { cx - 200, listY + (i - projectListScroll) * itemHeight, 400, 25 };
                    bool isHovered = CheckCollisionPointRec(GetMousePosition(), projRec);
                    DrawRectangleRec(projRec, FOREGROUND_COLOR);
                    if(isHovered) DrawRectangleLinesEx(projRec, 1, HIGHLIGHT_COLOR);
                    DrawText(projectList[i], projRec.x + 5, projRec.y + 5, 16, HIGHLIGHT_COLOR);
                    
                    if (CheckCollisionPointRec(GetMousePosition(), projRec) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (project_load(projectList[i])) {
                            if (vp_load(vp, project_get_video_path())) {
                                vp_play(vp);
                                strncpy(currentVideoPath, project_get_video_path(), sizeof(currentVideoPath) - 1);
                                showProjectPanel = false;
                                
                                const char* srtPath = project_get_working_srt_path();
                                if (srtPath) {
                                    FILE* testF = fopen(srtPath, "r");
                                    if (testF) {
                                        fclose(testF);
                                        sublist_clear(&subtitles);
                                        load_srt_to_subtitle_list(&subtitles, srtPath);
                                    }
                                    save_working_srt(&subtitles);
                                    vp_refresh_subtitles(vp, project_get_working_srt_path());
                                }
                            }
                        }
                    }
                }
            }
            
            if (IsFileDropped()) {
                FilePathList droppedFiles = LoadDroppedFiles();
                if (droppedFiles.count > 0) {
                    for (unsigned int i = 0; i < droppedFiles.count; i++) {
                        const char* file = droppedFiles.paths[i];
                        if (has_video_extension(file)) {
                            const char* filename = strrchr(file, '/');
                            if (!filename) filename = strrchr(file, '\\');
                            if (!filename) filename = file;
                            else filename++;
                            
                            char projectName[256] = {0};
                            strncpy(projectName, filename, sizeof(projectName) - 1);
                            char* dot = strrchr(projectName, '.');
                            if (dot) *dot = '\0';
                            
                            project_create(projectName, file);
                            
                            printf("Loading dropped video: %s\n", file);
                            if (vp_load(vp, file)) {
                                vp_play(vp);
                                strncpy(currentVideoPath, file, sizeof(currentVideoPath) - 1);
                                currentVideoPath[sizeof(currentVideoPath) - 1] = '\0';
                                showProjectPanel = false;
                                
                                const char* srtPath = project_get_working_srt_path();
                                if (srtPath) {
                                    save_working_srt(&subtitles);
                                    vp_refresh_subtitles(vp, project_get_working_srt_path());
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
            
            EndDrawing();
            continue;
        }
        
        if (showNewProjectDialog) {
            BeginDrawing();
            ClearBackground((Color){0, 0, 0, 100});
            
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            
            DrawRectangle((screenW - 400) / 2, (screenH - 200) / 2, 400, 200, (Color){50, 50, 50, 255});
            DrawText("Enter project name:", (screenW - 400) / 2 + 20, (screenH - 200) / 2 + 20, 20, WHITE);
            
            Rectangle nameRec = { (screenW - 350) / 2, (screenH - 200) / 2 + 50, 300, 25 };
            GuiTextBox(nameRec, newProjectName, 64, true);
            
            Rectangle createBtn = { (screenW - 150) / 2, (screenH - 200) / 2 + 100, 100, 30 };
            if (GuiButton(createBtn, "Drag video")) {
                showNewProjectDialog = false;
            }
            
            Rectangle cancelBtn = { (screenW + 50) / 2, (screenH - 200) / 2 + 100, 80, 30 };
            if (GuiButton(cancelBtn, "Cancel")) {
                showNewProjectDialog = false;
                newProjectName[0] = '\0';
            }
            
            EndDrawing();
            
            if (IsFileDropped()) {
                FilePathList droppedFiles = LoadDroppedFiles();
                if (droppedFiles.count > 0) {
                    for (unsigned int i = 0; i < droppedFiles.count; i++) {
                        const char* file = droppedFiles.paths[i];
                        if (has_video_extension(file)) {
                            if (newProjectName[0] == '\0') {
                                const char* filename = strrchr(file, '/');
                                if (!filename) filename = strrchr(file, '\\');
                                if (!filename) filename = file;
                                else filename++;
                                strncpy(newProjectName, filename, sizeof(newProjectName) - 1);
                                char* dot = strrchr(newProjectName, '.');
                                if (dot) *dot = '\0';
                            }
                            
                            project_create(newProjectName, file);
                            
                            if (vp_load(vp, file)) {
                                vp_play(vp);
                                strncpy(currentVideoPath, file, sizeof(currentVideoPath) - 1);
                                showProjectPanel = false;
                                showNewProjectDialog = false;
                                newProjectName[0] = '\0';
                                
                                save_working_srt(&subtitles);
                                vp_refresh_subtitles(vp, project_get_working_srt_path());
                            }
                            break;
                        }
                    }
                }
                UnloadDroppedFiles(droppedFiles);
            }
            
            continue;
        }
        
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                for (unsigned int i = 0; i < droppedFiles.count; i++) {
                    const char* file = droppedFiles.paths[i];
                    if (has_video_extension(file)) {
                        const char* filename = strrchr(file, '/');
                        if (!filename) filename = strrchr(file, '\\');
                        if (!filename) filename = file;
                        else filename++;
                        
                        char projectName[256] = {0};
                        strncpy(projectName, filename, sizeof(projectName) - 1);
                        char* dot = strrchr(projectName, '.');
                        if (dot) *dot = '\0';
                        
                        project_create(projectName, file);
                        
                        printf("Loading dropped video: %s\n", file);
                        if (vp_load(vp, file)) {
                            videoFile = file;
                            vp_play(vp);
                            strncpy(currentVideoPath, file, sizeof(currentVideoPath) - 1);
                            currentVideoPath[sizeof(currentVideoPath) - 1] = '\0';
                            
                            const char* srtPath = project_get_working_srt_path();
                            if (srtPath) {
                                save_working_srt(&subtitles);
                                vp_refresh_subtitles(vp, project_get_working_srt_path());
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
        ClearBackground(BACKGROUND_COLOR);
        
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
                DrawRectangleLinesEx(videoRect, 2.0f, HIGHLIGHT_COLOR);
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
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), videoDest)) {
                    if (vp_is_playing(vp)) {
                        vp_pause(vp);
                    } else {
                        vp_play(vp);
                    }
                }
                
                Rectangle backBtn = { 10, 10, 70, 30 };
                if (GuiButton(backBtn, "<- Back")) {
                    vp_unload(vp);
                    currentVideoPath[0] = '\0';
                    sublist_clear(&subtitles);
                    subtitles.selectedIndex = -1;
                    showProjectPanel = true;
                }
                
                int panelY = (int)(screenH - 100);
                int panelH = 100;
                
                DrawRectangle(0, panelY, videoW, panelH, BACKGROUND_COLOR);
                DrawRectangleLines(0, panelY, videoW, panelH, FOREGROUND_COLOR);
                
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
                char startTimeStr[32], endTimeStr[32];
                format_display_time(time, startTimeStr);
                format_display_time(dur, endTimeStr);
                snprintf(timeText, sizeof(timeText), "%s / %s", startTimeStr, endTimeStr);
                DrawText(timeText, 10, panelY + 40, 20, HIGHLIGHT_COLOR);
                
                Rectangle exportBtn = { videoW - 100, panelY + 40, 90, 20 };
                if (GuiButton(exportBtn, "Export")) {
                    exportSuccess = export_subtitles_to_srt(&subtitles, currentVideoPath);
                    showExportMessage = true;
                }
                
                char helpText[] = "SPACE: Play/Pause | LEFT/RIGHT: +/-5s | comma/dot: +/- frame";
                DrawText(helpText, 10, panelY + 70, 20, FOREGROUND_COLOR);
            } else {
                int screenW = GetScreenWidth();
                int screenH = GetScreenHeight();
                DrawText("Loading video...", (screenW - SIDE_PANEL_WIDTH)/2 - 60, screenH/2, 20, HIGHLIGHT_COLOR);
            }
            
            DrawRectangle(screenW - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, screenH, BACKGROUND_COLOR);
            DrawRectangleLines(screenW - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, screenH, FOREGROUND_COLOR);

            
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
                    format_display_time(sub->startTime, editStartStr);
                    format_display_time(sub->endTime, editEndStr);
                }
                save_working_srt(&subtitles);
                vp_refresh_subtitles(vp, project_get_working_srt_path());
            }
            
            int listY = 50;
            int listH = screenH - listY - 230;
            if (listH < 50) listH = 50;
            int itemHeight = 35;
            int visibleItems = listH / itemHeight;
            
            int wheelMove = GetMouseWheelMove();
            if (wheelMove != 0) {
                int maxScroll = subtitles.count - visibleItems;
                if (maxScroll < 0) maxScroll = 0;
                subtitleListScroll += wheelMove;
                if (subtitleListScroll < 0) subtitleListScroll = 0;
                if (subtitleListScroll > maxScroll) subtitleListScroll = maxScroll;
            }
            
            // Ensure scroll is valid when list changes
            if (subtitles.count <= visibleItems) {
                subtitleListScroll = 0;
            } else if (subtitleListScroll > subtitles.count - visibleItems) {
                subtitleListScroll = subtitles.count - visibleItems;
            }
            
            
            for (int i = subtitleListScroll; i < subtitles.count && i < subtitleListScroll + visibleItems + 1; i++) {
                Subtitle* sub = &subtitles.items[i];
                
                int itemY = listY + (i - subtitleListScroll) * itemHeight;
                
                // Skip if outside visible area
                if (itemY < listY || itemY >= listY + listH) continue;
                
                Rectangle itemRec = { screenW - SIDE_PANEL_WIDTH + 10, itemY, SIDE_PANEL_WIDTH - 20, 30 };
                
                Color itemBg = (i == subtitles.selectedIndex) ? HIGHLIGHT_COLOR : FOREGROUND_COLOR;
                DrawRectangleRec(itemRec, itemBg);
                if(CheckCollisionPointRec(GetMousePosition(), itemRec)) DrawRectangleLinesEx(itemRec,1, HIGHLIGHT_COLOR);
                char timeStr[32];
                format_display_time_short(sub->startTime, timeStr);

                DrawText(timeStr, itemRec.x + 5, itemRec.y + 7, 14, (i == subtitles.selectedIndex) ? BACKGROUND_COLOR : HIGHLIGHT_COLOR);
                
                char displayText[32] = {0};
                if (sub->text) {
                    strncpy(displayText, sub->text, 20);
                    if (strlen(sub->text) > 20) strcat(displayText, "...");
                }
                DrawText(displayText, itemRec.x + 60, itemRec.y + 7, 14, BACKGROUND_COLOR);
                
                if (CheckCollisionPointRec(GetMousePosition(), itemRec) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    subtitles.selectedIndex = i;
                    vp_seek(vp, sub->startTime);
                    strncpy(editText, sub->text ? sub->text : "", sizeof(editText) - 1);
                    format_display_time(sub->startTime, editStartStr);
                    format_display_time(sub->endTime, editEndStr);
                }
            }
            
            if (subtitles.selectedIndex >= 0) {
                int editY = screenH - 200;
                Rectangle editPanel = { screenW - SIDE_PANEL_WIDTH + 10, editY, SIDE_PANEL_WIDTH - 20, 190 };
                DrawRectangleRec(editPanel, BACKGROUND_COLOR);
                DrawRectangleLines(editPanel.x, editPanel.y, editPanel.width, editPanel.height, FOREGROUND_COLOR);
                
                DrawText("Start:", editPanel.x + 5, editPanel.y + 5, 14, FOREGROUND_COLOR);
                Rectangle startRec = { editPanel.x + 60, editPanel.y + 5, 85, 20 };
                if (GuiTextBox(startRec, editStartStr, 32, startEditing)) {
                    startEditing = true;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = true;
                }
                
                DrawText("End:", editPanel.x + 170, editPanel.y + 5, 14, FOREGROUND_COLOR);
                Rectangle endRec = { editPanel.x + 210, editPanel.y + 5, 85, 20 };
                if (GuiTextBox(endRec, editEndStr, 32, endEditing)) {
                    endEditing = true;
                    startEditing = false;
                    textEditing = false;
                    isTextEditing = true;
                }
                
                Rectangle startSetBtn = { editPanel.x + 60, editPanel.y + 28, 85, 15 };
                if (GuiButton(startSetBtn, "+")) {
                    double currentTime = vp_get_time(vp);
                    format_display_time(currentTime, editStartStr);
                    startEditing = false;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = false;
                }
                
                Rectangle endSetBtn = { editPanel.x + 210, editPanel.y + 28, 85, 15 };
                if (GuiButton(endSetBtn, "+")) {
                    double currentTime = vp_get_time(vp);
                    format_display_time(currentTime, editEndStr);
                    startEditing = false;
                    endEditing = false;
                    textEditing = false;
                    isTextEditing = false;
                }
                
                DrawText("Text:", editPanel.x + 5, editPanel.y + 55, 14, FOREGROUND_COLOR);
                Rectangle textRec = { editPanel.x + 5, editPanel.y + 75, editPanel.width - 10, 60 };
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
                
                Rectangle saveBtn = { editPanel.x + 5, editPanel.y + 145, 80, 25 };
                if (GuiButton(saveBtn, "Save")) {
                    Subtitle* sub = sublist_get(&subtitles, subtitles.selectedIndex);
                    if (sub) {
                        free(sub->text);
                        sub->text = malloc(strlen(editText) + 1);
                        strcpy(sub->text, editText);
                        sub->startTime = parse_display_time(editStartStr);
                        sub->endTime = parse_display_time(editEndStr);
                    }
                    sort_subtitles(&subtitles);
                    save_working_srt(&subtitles);
                    vp_refresh_subtitles(vp, project_get_working_srt_path());
                }
                
                Rectangle delBtn = { editPanel.x + editPanel.width - 90, editPanel.y + 145, 80, 25 };
                if (GuiButton(delBtn, "Delete")) {
                    sublist_remove(&subtitles, subtitles.selectedIndex);
                    subtitles.selectedIndex = -1;
                    save_working_srt(&subtitles);
                    vp_refresh_subtitles(vp, project_get_working_srt_path());
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
    if (logoLoaded) UnloadTexture(logoTexture);
    CloseWindow();
    
    return 0;
}
