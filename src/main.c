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
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        if (vp_is_loaded(vp)) {
            int w = vp_get_width(vp);
            int h = vp_get_height(vp);
            
            if (w > 0 && h > 0) {
                Rectangle dest = { 
                    0, 0, 
                    (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT 
                };
                vp_render(vp, dest);
                
                double time = vp_get_time(vp);
                double dur = vp_get_duration(vp);
                
                DrawText(TextFormat("Time: %.1f / %.1f", time, dur), 10, 10, 20, WHITE);
                DrawText("SPACE: Play/Pause | LEFT/RIGHT: Seek 5s | Drag file to open", 10, 40, 20, YELLOW);
            } else {
                DrawText("Loading video...", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2, 20, YELLOW);
            }
        } else {
            int cx = SCREEN_WIDTH / 2;
            int cy = SCREEN_HEIGHT / 2;
            DrawText("Drag and drop a video file here", cx - 160, cy - 20, 20, GRAY);
            DrawText("or run: sub-racer.exe <video.mp4>", cx - 170, cy + 15, 20, GRAY);
        }
        
        DrawFPS(SCREEN_WIDTH - 80, 10);
        EndDrawing();
    }
    
    vp_destroy(vp);
    CloseWindow();
    
    return 0;
}
