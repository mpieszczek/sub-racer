#include "transcribe_thread.h"
#include "audio_extract.h"
#include "project.h"
#include "whisper_wrapper.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <whisper.h>

static void* thread_func(void* arg);
static void progress_callback(struct whisper_context* ctx, struct whisper_state* state, int progress, void* user_data);
static void new_segment_callback(struct whisper_context* ctx, struct whisper_state* state, int n_new, void* user_data);
static bool abort_callback(void* user_data);

void transcribe_thread_init(TranscribeThread* tt) {
    memset(tt, 0, sizeof(TranscribeThread));
    
    tt->progress = 0;
    tt->cancel_requested = false;
    tt->running = false;
    tt->completed = false;
    tt->success = false;
    tt->was_cancelled = false;
    tt->max_segment_length = 40;
    tt->error_message[0] = '\0';
    tt->detected_language[0] = '\0';
    tt->segments_count = 0;
    tt->new_segments_count = 0;
    tt->whisper = NULL;
    
    pthread_mutex_init(&tt->subtitles_mutex, NULL);
    sublist_init(&tt->temp_subtitles);
}

bool transcribe_thread_start(TranscribeThread* tt) {
    if (tt->running) return false;
    
    tt->progress = 0;
    tt->cancel_requested = false;
    tt->completed = false;
    tt->success = false;
    tt->was_cancelled = false;
    tt->new_segments_count = 0;
    tt->segments_count = 0;
    tt->error_message[0] = '\0';
    tt->detected_language[0] = '\0';
    
    pthread_mutex_lock(&tt->subtitles_mutex);
    sublist_clear(&tt->temp_subtitles);
    pthread_mutex_unlock(&tt->subtitles_mutex);
    
    tt->running = true;
    
    int rc = pthread_create(&tt->thread, NULL, thread_func, tt);
    return (rc == 0);
}

void transcribe_thread_join(TranscribeThread* tt) {
    if (tt->running) {
        pthread_join(tt->thread, NULL);
    }
}

void transcribe_thread_cleanup(TranscribeThread* tt) {
    if (tt->running || tt->completed) {
        pthread_join(tt->thread, NULL);
    }
    pthread_mutex_destroy(&tt->subtitles_mutex);
    sublist_clear(&tt->temp_subtitles);
    
    if (tt->whisper) {
        whisper_wrapper_free(tt->whisper);
        tt->whisper = NULL;
    }
}

void transcribe_thread_request_cancel(TranscribeThread* tt) {
    tt->cancel_requested = true;
}

bool transcribe_thread_is_running(TranscribeThread* tt) {
    return tt->running;
}

bool transcribe_thread_is_completed(TranscribeThread* tt) {
    return tt->completed;
}

bool transcribe_thread_is_success(TranscribeThread* tt) {
    return tt->success;
}

void transcribe_thread_copy_new_segments(TranscribeThread* tt, SubtitleList* dest) {
    if (tt->new_segments_count <= 0) return;
    
    pthread_mutex_lock(&tt->subtitles_mutex);
    
    for (int i = dest->count; i < tt->temp_subtitles.count; i++) {
        Subtitle* src = &tt->temp_subtitles.items[i];
        sublist_add(dest, src->startTime, src->endTime, src->text);
    }
    
    tt->new_segments_count = 0;
    
    pthread_mutex_unlock(&tt->subtitles_mutex);
}

static void progress_callback(struct whisper_context* ctx, struct whisper_state* state, int progress, void* user_data) {
    (void)ctx;
    (void)state;
    TranscribeThread* tt = (TranscribeThread*)user_data;
    tt->progress = 10 + (progress * 80) / 100;
}

static void new_segment_callback(struct whisper_context* ctx, struct whisper_state* state, int n_new, void* user_data) {
    (void)state;
    TranscribeThread* tt = (TranscribeThread*)user_data;
    
    if (tt->cancel_requested) return;
    
    int n_segments = whisper_full_n_segments(ctx);
    int start = n_segments - n_new;
    
    pthread_mutex_lock(&tt->subtitles_mutex);
    
    for (int i = start; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        
        double start_time = (double)t0 / 100.0;
        double end_time = (double)t1 / 100.0;
        
        sublist_add(&tt->temp_subtitles, start_time, end_time, text);
        tt->new_segments_count++;
        tt->segments_count++;
    }
    
    pthread_mutex_unlock(&tt->subtitles_mutex);
}

static bool abort_callback(void* user_data) {
    TranscribeThread* tt = (TranscribeThread*)user_data;
    return tt->cancel_requested;  // Return true to abort
}

static void* thread_func(void* arg) {
    TranscribeThread* tt = (TranscribeThread*)arg;
    
    printf("[TranscribeThread] Starting transcription for: %s\n", tt->video_path);
    
    char temp_wav[512];
    snprintf(temp_wav, sizeof(temp_wav), "%s/temp_audio_%d.wav", 
             project_get_projects_dir(), (int)time(NULL));
    
    // === Faza 1: Audio extraction (0-10%) ===
    printf("[TranscribeThread] Phase 1: Audio extraction\n");
    tt->progress = 5;
    
    if (tt->cancel_requested) {
        printf("[TranscribeThread] Cancelled before audio extraction\n");
        goto cancel_cleanup;
    }
    
    if (!audio_extract_to_wav(tt->video_path, temp_wav, (bool*)&tt->cancel_requested)) {
        snprintf(tt->error_message, sizeof(tt->error_message), "Failed to extract audio from video");
        printf("[TranscribeThread] ERROR: %s\n", tt->error_message);
        goto error_cleanup;
    }
    
    if (tt->cancel_requested) {
        printf("[TranscribeThread] Cancelled after audio extraction\n");
        goto cancel_cleanup;
    }
    
    tt->progress = 10;
    printf("[TranscribeThread] Audio extraction complete\n");
    
    // === Faza 2: Transcription (10-90%) ===
    printf("[TranscribeThread] Phase 2: Transcription\n");
    
    tt->whisper = whisper_wrapper_create();
    if (!tt->whisper) {
        snprintf(tt->error_message, sizeof(tt->error_message), "Failed to initialize Whisper");
        printf("[TranscribeThread] ERROR: %s\n", tt->error_message);
        goto error_cleanup;
    }
    
    if (!whisper_wrapper_load_default_model(tt->whisper)) {
        snprintf(tt->error_message, sizeof(tt->error_message), "Failed to load Whisper model");
        printf("[TranscribeThread] ERROR: %s\n", tt->error_message);
        whisper_wrapper_free(tt->whisper);
        tt->whisper = NULL;
        goto error_cleanup;
    }
    
    TranscriptionResult* result = whisper_wrapper_transcribe_with_callbacks(
        tt->whisper,
        temp_wav,
        tt->max_segment_length,
        (bool*)&tt->cancel_requested,
        progress_callback,
        tt,
        new_segment_callback,
        tt
    );
    
    if (tt->cancel_requested) {
        printf("[TranscribeThread] Cancelled during transcription\n");
        if (result) whisper_wrapper_free_result(result);
        whisper_wrapper_free(tt->whisper);
        tt->whisper = NULL;
        remove(temp_wav);
        goto cancel_cleanup;
    }
    
    if (!result || !result->success) {
        snprintf(tt->error_message, sizeof(tt->error_message), 
                 result ? result->error_message : "Transcription failed");
        printf("[TranscribeThread] ERROR: %s\n", tt->error_message);
        if (result) whisper_wrapper_free_result(result);
        whisper_wrapper_free(tt->whisper);
        tt->whisper = NULL;
        goto error_cleanup;
    }
    
    strncpy(tt->detected_language, result->detected_language, sizeof(tt->detected_language) - 1);
    
    printf("[TranscribeThread] Transcription complete, detected language: %s, segments: %d\n", 
           tt->detected_language, tt->segments_count);
    
    whisper_wrapper_free_result(result);
    whisper_wrapper_free(tt->whisper);
    tt->whisper = NULL;
    
    // === Faza 3: Finalize (90-100%) ===
    printf("[TranscribeThread] Phase 3: Finalize\n");
    tt->progress = 100;
    
    remove(temp_wav);
    
    tt->success = true;
    tt->completed = true;
    tt->running = false;
    
    printf("[TranscribeThread] Completed successfully\n");
    return NULL;

cancel_cleanup:
    remove(temp_wav);
    tt->success = false;
    tt->was_cancelled = true;
    tt->completed = true;
    tt->running = false;
    printf("[TranscribeThread] Cancelled by user\n");
    return NULL;

error_cleanup:
    remove(temp_wav);
    tt->success = false;
    tt->completed = true;
    tt->running = false;
    return NULL;
}