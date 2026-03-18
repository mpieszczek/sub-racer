#ifndef TRANSCRIBE_THREAD_H
#define TRANSCRIBE_THREAD_H

#include "subtitle.h"
#include <stdbool.h>
#include <pthread.h>

typedef struct {
    // === INPUT (ustawiane przed start) ===
    char video_path[512];
    int max_segment_length;
    
    // === OUTPUT - Subtitles (chronione mutexem) ===
    SubtitleList temp_subtitles;
    pthread_mutex_t subtitles_mutex;
    volatile int new_segments_count;
    
    // === STATUS (volatile - shared between threads) ===
    volatile int progress;
    volatile bool cancel_requested;
    volatile bool running;
    volatile bool completed;
    volatile bool success;
    volatile bool was_cancelled;
    
    // === RESULT INFO ===
    char error_message[256];
    char detected_language[16];
    int segments_count;
    
    // === INTERNAL ===
    pthread_t thread;
    void* whisper;
} TranscribeThread;

// === LIFECYCLE ===
void transcribe_thread_init(TranscribeThread* tt);
bool transcribe_thread_start(TranscribeThread* tt);
void transcribe_thread_join(TranscribeThread* tt);
void transcribe_thread_cleanup(TranscribeThread* tt);

// === CONTROL ===
void transcribe_thread_request_cancel(TranscribeThread* tt);

// === STATUS ===
bool transcribe_thread_is_running(TranscribeThread* tt);
bool transcribe_thread_is_completed(TranscribeThread* tt);
bool transcribe_thread_is_success(TranscribeThread* tt);

// === DATA TRANSFER ===
void transcribe_thread_copy_new_segments(TranscribeThread* tt, SubtitleList* dest);

#endif