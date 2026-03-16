#include "audio_extract.h"
#include "project.h"
#include <mpv/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

static bool validate_wav_header(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char header[12];
    size_t read = fread(header, 1, 12, f);
    fclose(f);
    if (read != 12) return false;
    return (strncmp(header, "RIFF", 4) == 0 && 
            strncmp(header + 8, "WAVE", 4) == 0);
}

bool audio_extract_to_wav(const char* video_path, const char* output_wav_path, volatile bool* cancel_flag) {
    if (!video_path || !output_wav_path) {
        printf("[AudioExtract] Invalid parameters\n");
        return false;
    }
    
    printf("[AudioExtract] Extracting audio from: %s\n", video_path);
    printf("[AudioExtract] Output: %s\n", output_wav_path);
    
    unlink(output_wav_path);
    
    mpv_handle* mpv = mpv_create();
    if (!mpv) {
        printf("[AudioExtract] Failed to create mpv handle\n");
        return false;
    }
    
    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "pcm");
    mpv_set_option_string(mpv, "ao-pcm-file", output_wav_path);
    mpv_set_option_string(mpv, "af", "format=format=s16");
    mpv_set_option_string(mpv, "video", "no");
    mpv_set_option_string(mpv, "audio-display", "no");
    mpv_set_option_string(mpv, "really-quiet", "yes");
    
    int ret = mpv_initialize(mpv);
    if (ret < 0) {
        printf("[AudioExtract] mpv_initialize failed: %d\n", ret);
        mpv_terminate_destroy(mpv);
        return false;
    }
    
    const char* load_cmd[] = { "loadfile", video_path, NULL };
    ret = mpv_command(mpv, load_cmd);
    if (ret < 0) {
        printf("[AudioExtract] loadfile failed: %s (code=%d)\n", mpv_error_string(ret), ret);
        mpv_terminate_destroy(mpv);
        return false;
    }
    
    printf("[AudioExtract] Processing...\n");
    
    bool completed = false;
    bool success = false;
    int loop_count = 0;
    const int max_loops = 30000;
    
    while (!completed && loop_count < max_loops) {
        if (cancel_flag && *cancel_flag) {
            printf("[AudioExtract] Cancelled by user\n");
            const char* stop_cmd[] = { "stop", NULL };
            mpv_command(mpv, stop_cmd);
            mpv_terminate_destroy(mpv);
            unlink(output_wav_path);
            return false;
        }
        
        mpv_event* event = mpv_wait_event(mpv, 0.1);
        
        printf("[AudioExtract] Event: %s (id=%d)\n", mpv_event_name(event->event_id), event->event_id);
        
        if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN) {
            completed = true;
            if (event->event_id == MPV_EVENT_END_FILE) {
                mpv_event_end_file* end_file = (mpv_event_end_file*)event->data;
                if (end_file->reason == MPV_END_FILE_REASON_EOF) {
                    printf("[AudioExtract] Audio extraction completed successfully\n");
                    success = true;
                } else {
                    printf("[AudioExtract] End file reason: %d, error: %d\n", 
                           end_file->reason, end_file->error);
                }
            }
        }
        
        loop_count++;
    }
    
    if (loop_count >= max_loops) {
        printf("[AudioExtract] Timeout waiting for audio extraction\n");
    }
    
    mpv_terminate_destroy(mpv);
    
    if (success) {
        if (!validate_wav_header(output_wav_path)) {
            printf("[AudioExtract] Output file is not a valid WAV\n");
            success = false;
        }
    }
    
    if (success) {
        FILE* f = fopen(output_wav_path, "rb");
        if (f) {
            fseek(f,0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            if (size >0) {
                printf("[AudioExtract] Successfully extracted audio (%ld bytes)\n", size);
                return true;
            }
        }
        printf("[AudioExtract] Output file is empty or missing\n");
    }
    
    unlink(output_wav_path);
    return false;
}