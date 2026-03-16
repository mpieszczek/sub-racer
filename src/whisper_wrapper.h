#ifndef WHISPER_WRAPPER_H
#define WHISPER_WRAPPER_H

#include "subtitle.h"
#include <stdbool.h>

#define WHISPER_MODEL_NAME "ggml-base.bin"
#define WHISPER_MAX_SEGMENT_LENGTH 40

typedef enum {
    RESAMPLE_FASTEST = 0,  // SRC_LINEAR - minimal CPU
    RESAMPLE_MEDIUM = 1,   // SRC_SINC_MEDIUM - balanced (default)
    RESAMPLE_BEST = 2      // SRC_SINC_BEST - highest quality
} ResampleQuality;

typedef struct whisper_context whisper_context;

typedef struct {
    int segment_count;
    Subtitle* segments;
    char detected_language[16];
    bool success;
    char error_message[256];
} TranscriptionResult;

typedef struct {
    whisper_context* ctx;
    char model_path[512];
    bool loaded;
} WhisperWrapper;

WhisperWrapper* whisper_wrapper_create(void);
void whisper_wrapper_free(WhisperWrapper* ww);

void whisper_set_resample_quality(ResampleQuality quality);

bool whisper_wrapper_load_model(WhisperWrapper* ww, const char* model_path);
bool whisper_wrapper_load_default_model(WhisperWrapper* ww);

TranscriptionResult* whisper_wrapper_transcribe(WhisperWrapper* ww,
                                                  const char* wav_path,
                                                  int max_segment_length,
                                                  volatile bool* cancel_flag);

void whisper_wrapper_free_result(TranscriptionResult* result);

#endif