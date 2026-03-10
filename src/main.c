/*******************************************************************************************
*   Sub-Racer - Video Subtitle Editor
*******************************************************************************************/

#include "raylib.h"
#include "video_player.h"
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

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
    
    SetTargetFPS(60);
    
    VideoPlayer* vp = vp_create();
    if (!vp) {
        printf("Failed to create video player\n");
        CloseWindow();
        return 1;
    }
    
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
        }
    }
    
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
        
        if (IsKeyPressed(KEY_SPACE) && vp_is_loaded(vp)) {
            if (vp_is_playing(vp)) {
                vp_pause(vp);
            } else {
                vp_play(vp);
            }
        }
        
        if (IsKeyPressed(KEY_LEFT)) {
            double t = vp_get_time(vp);
            vp_seek(vp, t - 5.0);
        }
        
        if (IsKeyPressed(KEY_RIGHT)) {
            double t = vp_get_time(vp);
            vp_seek(vp, t + 5.0);
        }
        
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        if (vp_is_loaded(vp)) {
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int w = vp_get_width(vp);
            int h = vp_get_height(vp);
            
            if (w > 0 && h > 0) {
                float margin = 0.1f;
                Rectangle videoDest = { 
                    0, 0, 
                    (float)screenW, (float)(screenH * (1.0f - margin)) 
                };
                vp_render(vp, videoDest);
                
                DrawRectangleLinesEx(videoDest, 2.0f, RED);
                
                DrawRectangle(0, (int)(screenH * (1.0f - margin)), screenW, (int)(screenH * margin), Fade(GREEN, 0.1f));
                
                double time = vp_get_time(vp);
                double dur = vp_get_duration(vp);
                
                DrawText(TextFormat("Time: %.1f / %.1f", time, dur), 10, (int)(screenH * (1.0f - margin)) + 10, 20, WHITE);
                DrawText("SPACE: Play/Pause | LEFT/RIGHT: Seek 5s | Drag file to open", 10, (int)(screenH * (1.0f - margin)) + 40, 20, YELLOW);
            } else {
                int screenW = GetScreenWidth();
                int screenH = GetScreenHeight();
                DrawText("Loading video...", screenW/2 - 60, screenH/2, 20, YELLOW);
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
        EndDrawing();
    }
    
    vp_destroy(vp);
    CloseWindow();
    
    return 0;
}
