#include "whisper_wrapper.h"
#include "project.h"
#include <whisper.h>
#include <raylib.h>
#include <samplerate.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG_MODE 0

static ResampleQuality resample_quality = RESAMPLE_BEST;

// Abort callback for whisper - returns true when should abort
static bool whisper_abort_callback(void* user_data) {
    if (!user_data) return false;
    volatile bool* cancel_flag = (volatile bool*)user_data;
    return *cancel_flag;  // Return true to abort
}

typedef struct {
    int16_t* data;
    int n_samples;
    int sample_rate;
    int n_channels;
} WavData;

static WavData* read_wav_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("[Whisper] Cannot open WAV file: %s\n", filename);
        return NULL;
    }
    
    char riff[4];
    if (fread(riff, 1, 4, f) != 4 || strncmp(riff, "RIFF", 4) != 0) {
        printf("[Whisper] Invalid WAV file (no RIFF)\n");
        fclose(f);
        return NULL;
    }
    
    int32_t file_size;
    if (fread(&file_size, 4, 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    char wave[4];
    if (fread(wave, 1, 4, f) != 4 || strncmp(wave, "WAVE", 4) != 0) {
        printf("[Whisper] Invalid WAV file (no WAVE)\n");
        fclose(f);
        return NULL;
    }
    
    int32_t sample_rate = 16000;
    int16_t n_channels = 1;
    int16_t bits_per_sample = 16;
    int32_t data_size = 0;
    bool found_data = false;
    bool found_fmt = false;
    
    while (!found_data) {
        char chunk_id[4];
        int32_t chunk_size;
        
        if (fread(chunk_id, 1,4, f) != 4) break;
        if (fread(&chunk_size, 4, 1, f) != 1) break;
        
        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            int16_t audio_format;
            int32_t byte_rate;
            int16_t block_align;
            
            fread(&audio_format, 2, 1, f);
            fread(&n_channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            fread(&byte_rate, 4, 1, f);
            fread(&block_align, 2, 1, f);
            fread(&bits_per_sample, 2, 1, f);
            
            if (chunk_size > 16) {
                fseek(f, chunk_size - 16, SEEK_CUR);
            }
            found_fmt = true;
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            found_data = true;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }
    
    if (!found_fmt || !found_data) {
        printf("[Whisper] Invalid WAV file (missing fmt or data)\n");
        fclose(f);
        return NULL;
    }
    
    printf("[Whisper] WAV format: sample_rate=%d, channels=%d, bits_per_sample=%d\n",
           sample_rate, n_channels, bits_per_sample);
    
    int bytes_per_sample = bits_per_sample / 8;
    int n_samples = data_size / bytes_per_sample / n_channels;
    
    int16_t* samples = malloc(data_size);
    if (!samples) {
        fclose(f);
        return NULL;
    }
    
    if (fread(samples, 1, data_size, f) != (size_t)data_size) {
        free(samples);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    
    WavData* wav = malloc(sizeof(WavData));
    if (!wav) {
        free(samples);
        return NULL;
    }
    
    wav->data = samples;
    wav->n_samples = n_samples;
    wav->sample_rate = sample_rate;
    wav->n_channels = n_channels;
    
    return wav;
}

static void free_wav_data(WavData* wav) {
    if (wav) {
        if (wav->data) free(wav->data);
        free(wav);
    }
}

#if DEBUG_MODE
static bool debug_save_wav_int16(const float* samples, int n_samples, int channels, 
                                 int sample_rate, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        printf("[Whisper DEBUG] Failed to open file for writing: %s\n", filename);
        return false;
    }
    
    // Konwersja float → int16
    int16_t* int_samples = malloc(n_samples * channels * sizeof(int16_t));
    if (!int_samples) {
        fclose(f);
        return false;
    }
    
    for (int i = 0; i < n_samples * channels; i++) {
        float val = samples[i];
        // Clamp do [-1.0, 1.0]
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        int_samples[i] = (int16_t)(val * 32767.0f);
    }
    
    // WAV Header for int16 PCM
    int32_t data_size = n_samples * channels * sizeof(int16_t);
    int32_t file_size = 36 + data_size;
    int16_t audio_format = 1; // PCM
    int16_t num_channels = channels;
    int32_t sr = sample_rate;
    int16_t bits_per_sample = 16;
    int32_t byte_rate = sr * num_channels * bits_per_sample / 8;
    int16_t block_align = num_channels * bits_per_sample / 8;
    
    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    
    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    int32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    
    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(int_samples, sizeof(int16_t), n_samples * channels, f);
    
    free(int_samples);
    fclose(f);
    
    printf("[Whisper DEBUG] Saved (int16): %s (%d samples, %dHz, %dch)\n", 
           filename, n_samples, sample_rate, channels);
    
    return true;
}
#endif
static void normalize_audio(float* samples, int n_samples) {
    if (n_samples <= 0) return;
    
    // Znajdź peak absolute value
    float max_val = 0.0f;
    for (int i = 0; i < n_samples; i++) {
        float abs_val = fabsf(samples[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    
    printf("[Whisper] Peak before normalization: %.6f\n", max_val);
    
    // Normalizuj do 0.95 (zostaw 5% headroom)
    if (max_val > 0.0001f && max_val < 1.0f) {
        float gain = 0.95f / max_val;
        for (int i = 0; i < n_samples; i++) {
            samples[i] *= gain;
        }
        printf("[Whisper] Normalized with gain: %.4f\n", gain);
    } else if (max_val >= 1.0f) {
        printf("[Whisper] WARNING: Audio is clipping (peak=%.4f), normalizing\n", max_val);
        float gain = 0.95f / max_val;
        for (int i = 0; i < n_samples; i++) {
            samples[i] *= gain;
        }
    } else {
        printf("[Whisper] Audio too quiet (peak=%.6f), skipping normalization\n", max_val);
    }
}
#if DEBUG_MODE
static void debug_log_audio_stats(const float* samples, int n_samples) {
    if (n_samples <= 0) {
        printf("[Whisper DEBUG] No samples to analyze\n");
        return;
    }
    
    float min_val = samples[0];
    float max_val = samples[0];
    double sum = 0.0;
    int out_of_range = 0;
    
    for (int i = 0; i < n_samples; i++) {
        float val = samples[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        sum += val;
        
        if (val < -1.0f || val > 1.0f) {
            out_of_range++;
        }
    }
    
    float mean = (float)(sum / n_samples);
    
    printf("[Whisper DEBUG] Audio Statistics:\n");
    printf("[Whisper DEBUG]   Samples: %d\n", n_samples);
    printf("[Whisper DEBUG]   Min: %.6f, Max: %.6f\n", min_val, max_val);
    printf("[Whisper DEBUG]   Mean: %.6f\n", mean);
    printf("[Whisper DEBUG]   Out of range [-1.0, 1.0]: %d (%.2f%%)\n", 
           out_of_range, (100.0f * out_of_range) / n_samples);
    
    if (out_of_range > 0) {
        printf("[Whisper DEBUG] WARNING: Some samples are outside expected range!\n");
    }
}
#endif

// Internal resampling function - returns stereo/interleaved output
static float* resample_with_src_internal(float* input_float, int input_frames, int channels,
                                          int input_rate, int output_rate, 
                                          int* out_frames, int* out_channels) {
    if (!input_float || input_frames <= 0) return NULL;
    
    // Map quality setting to SRC converter
    int src_converter;
    switch (resample_quality) {
        case RESAMPLE_FASTEST: src_converter = SRC_LINEAR; break;
        case RESAMPLE_BEST: src_converter = SRC_SINC_BEST_QUALITY; break;
        case RESAMPLE_MEDIUM:
        default: src_converter = SRC_SINC_MEDIUM_QUALITY; break;
    }
    
    // Calculate output size (estimate + margin)
    double ratio = (double)output_rate / input_rate;
    int max_output_frames = (int)(input_frames * ratio) + 1024;
    
    float* output_float = malloc(max_output_frames * channels * sizeof(float));
    if (!output_float) return NULL;
    
    // Initialize libsamplerate
    int error;
    SRC_STATE* state = src_new(src_converter, channels, &error);
    if (!state) {
        printf("[Whisper] SRC init failed: %s\n", src_strerror(error));
        free(output_float);
        return NULL;
    }
    
    // Configure
    SRC_DATA src_data;
    src_data.data_in = input_float;
    src_data.input_frames = input_frames;
    src_data.data_out = output_float;
    src_data.output_frames = max_output_frames;
    src_data.src_ratio = ratio;
    src_data.end_of_input = 1;
    
    // Process
    error = src_process(state, &src_data);
    if (error) {
        printf("[Whisper] SRC process failed: %s\n", src_strerror(error));
        src_delete(state);
        free(output_float);
        return NULL;
    }
    
    src_delete(state);
    
    int output_frames = src_data.output_frames_gen;
    *out_frames = output_frames;
    *out_channels = channels;
    
    printf("[Whisper] SRC: %d frames @ %dHz → %d frames @ %dHz (ratio=%.4f)\n",
           input_frames, input_rate, output_frames, output_rate, ratio);
    
    return output_float;
}

WhisperWrapper* whisper_wrapper_create(void) {
    WhisperWrapper* ww = calloc(1, sizeof(WhisperWrapper));
    if (!ww) return NULL;
    ww->loaded = false;
    return ww;
}

void whisper_wrapper_free(WhisperWrapper* ww) {
    if (!ww) return;
    if (ww->ctx) {
        whisper_free(ww->ctx);
        ww->ctx = NULL;
    }
    free(ww);
}

void whisper_set_resample_quality(ResampleQuality quality) {
    if (quality >= RESAMPLE_FASTEST && quality <= RESAMPLE_BEST) {
        resample_quality = quality;
        const char* quality_name;
        switch (quality) {
            case RESAMPLE_FASTEST: quality_name = "FASTEST (linear)"; break;
            case RESAMPLE_BEST: quality_name = "BEST (sinc)"; break;
            case RESAMPLE_MEDIUM:
            default: quality_name = "MEDIUM (sinc)"; break;
        }
        printf("[Whisper] Resample quality set to: %s\n", quality_name);
    }
}

bool whisper_wrapper_load_model(WhisperWrapper* ww, const char* model_path) {
    if (!ww || !model_path) return false;
    
    if (ww->ctx) {
        whisper_free(ww->ctx);
        ww->ctx = NULL;
        ww->loaded = false;
    }
    
    struct whisper_context_params cparams = whisper_context_default_params();
    ww->ctx = whisper_init_from_file_with_params(model_path, cparams);
    if (!ww->ctx) {
        printf("[Whisper] Failed to load model from: %s\n", model_path);
        return false;
    }
    
    strncpy(ww->model_path, model_path, sizeof(ww->model_path) - 1);
    ww->loaded = true;
    printf("[Whisper] Model loaded: %s\n", model_path);
    return true;
}

bool whisper_wrapper_load_default_model(WhisperWrapper* ww) {
    if (!ww) return false;
    
    char model_path[512];
    const char* app_dir = GetApplicationDirectory();
    
    snprintf(model_path, sizeof(model_path), "%s/%s", app_dir, WHISPER_MODEL_NAME);
    printf("[Whisper] Looking for model at: %s\n", model_path);
    
    FILE* f = fopen(model_path, "rb");
    if (f) {
        fclose(f);
        return whisper_wrapper_load_model(ww, model_path);
    }
    
    snprintf(model_path, sizeof(model_path), "./%s", WHISPER_MODEL_NAME);
    f = fopen(model_path, "rb");
    if (f) {
        fclose(f);
        return whisper_wrapper_load_model(ww, model_path);
    }
    
    snprintf(model_path, sizeof(model_path), "%s", WHISPER_MODEL_NAME);
    f = fopen(model_path, "rb");
    if (f) {
        fclose(f);
        return whisper_wrapper_load_model(ww, model_path);
    }
    
    printf("[Whisper] Model not found. Searched:\n");
    printf("  - %s/%s\n", app_dir, WHISPER_MODEL_NAME);
    printf("  - ./%s\n", WHISPER_MODEL_NAME);
    printf("  - %s\n", WHISPER_MODEL_NAME);
    
    return false;
}

TranscriptionResult* whisper_wrapper_transcribe(WhisperWrapper* ww,
                                                  const char* wav_path,
                                                  int max_segment_length,
                                                  volatile bool* cancel_flag) {
    if (!ww || !wav_path || !ww->loaded) {
        TranscriptionResult* result = calloc(1, sizeof(TranscriptionResult));
        if (result) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message), 
                     "Whisper not initialized");
        }
        return result;
    }
    
    TranscriptionResult* result = calloc(1, sizeof(TranscriptionResult));
    if (!result) return NULL;
    
    WavData* wav = read_wav_file(wav_path);
    if (!wav) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Cannot read WAV file: %s", wav_path);
        return result;
    }
    
    int input_frames = wav->n_samples;
    int input_channels = wav->n_channels;
    int input_rate = wav->sample_rate;
    
    printf("[Whisper] Loaded WAV: frames=%d, channels=%d, rate=%d\n",
           input_frames, input_channels, input_rate);
    
    // Step 1: Convert int16 to float
    float* input_float = malloc(input_frames * input_channels * sizeof(float));
    if (!input_float) {
        free_wav_data(wav);
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed");
        return result;
    }
    
    for (int i = 0; i < input_frames * input_channels; i++) {
        input_float[i] = (float)wav->data[i] / 32768.0f;
    }
    
#if DEBUG_MODE
    const char* projects_dir = project_get_projects_dir();
    double timestamp = GetTime() * 1000;
    char debug_path[512];
    
    // DEBUG 1: After int16->float conversion, before resampling
    snprintf(debug_path, sizeof(debug_path), "%s/debug_01_original_%.0f.wav", 
             projects_dir, timestamp);
    debug_save_wav_int16(input_float, input_frames, input_channels, input_rate, debug_path);
#endif
    
    // Step 2: Resample with libsamplerate
    int resampled_frames = 0;
    int resampled_channels = 0;
    float* resampled = resample_with_src_internal(input_float, input_frames, input_channels, 
                                                   input_rate, 16000, &resampled_frames, &resampled_channels);
    free(input_float);
    free_wav_data(wav);
    
    if (!resampled || resampled_frames <= 0) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to resample audio");
        return result;
    }
    
#if DEBUG_MODE
    // DEBUG 2: After resampling, still stereo
    snprintf(debug_path, sizeof(debug_path), "%s/debug_02_after_resample_%.0f.wav", 
             projects_dir, timestamp);
    debug_save_wav_int16(resampled, resampled_frames, resampled_channels, 16000, debug_path);
#endif
    
    // Step 3: Convert to mono
    float* mono_samples = NULL;
    if (resampled_channels > 1) {
        mono_samples = malloc(resampled_frames * sizeof(float));
        if (!mono_samples) {
            free(resampled);
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Memory allocation failed");
            return result;
        }
        
        for (int i = 0; i < resampled_frames; i++) {
            float sum = 0;
            for (int ch = 0; ch < resampled_channels; ch++) {
                sum += resampled[i * resampled_channels + ch];
            }
            mono_samples[i] = sum / resampled_channels;
        }
        free(resampled);
    } else {
        mono_samples = resampled;
    }
    
#if DEBUG_MODE
    // DEBUG 3: After mono conversion
    snprintf(debug_path, sizeof(debug_path), "%s/debug_03_after_mono_%.0f.wav", 
             projects_dir, timestamp);
    debug_save_wav_int16(mono_samples, resampled_frames, 1, 16000, debug_path);
#endif
    
    // Step 4: Normalize
    normalize_audio(mono_samples, resampled_frames);
    
#if DEBUG_MODE
    // DEBUG 4: After normalization
    snprintf(debug_path, sizeof(debug_path), "%s/debug_04_after_normalize_%.0f.wav", 
             projects_dir, timestamp);
    debug_save_wav_int16(mono_samples, resampled_frames, 1, 16000, debug_path);
    
    // DEBUG 5: Final (copy of 04 for backward compatibility)
    snprintf(debug_path, sizeof(debug_path), "%s/debug_05_final_%.0f.wav", 
             projects_dir, timestamp);
    debug_save_wav_int16(mono_samples, resampled_frames, 1, 16000, debug_path);
    
    // Log statistics
    debug_log_audio_stats(mono_samples, resampled_frames);
#endif
    
    int n_samples = resampled_frames;
    float* samples = mono_samples;
    
    if (cancel_flag && *cancel_flag) {
        free(samples);
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Cancelled");
        return result;
    }
    
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    // TODO: test WHISPER_SAMPLING_BEAM_SEARCH vs WHISPER_SAMPLING_GREEDY
    params.language = "auto";
    params.translate = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.max_len = max_segment_length > 0 ? max_segment_length : WHISPER_MAX_SEGMENT_LENGTH;
    params.token_timestamps = true;
    params.split_on_word = true;
    
    int ret = whisper_full(ww->ctx, params, samples, n_samples);
    free(samples);
    
    if (cancel_flag && *cancel_flag) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Cancelled");
        return result;
    }
    
    if (ret != 0) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Whisper transcription failed: %d", ret);
        return result;
    }
    
    const char* lang = whisper_lang_str(whisper_full_lang_id(ww->ctx));
    if (lang) {
        strncpy(result->detected_language, lang, sizeof(result->detected_language) - 1);
    }
    
    int n_segments = whisper_full_n_segments(ww->ctx);
    if (n_segments <= 0) {
        result->success = true;
        result->segment_count = 0;
        result->segments = NULL;
        return result;
    }
    
    result->segments = calloc(n_segments, sizeof(Subtitle));
    if (!result->segments) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed");
        return result;
    }
    
    result->segment_count = n_segments;
    
    for (int i = 0; i < n_segments; i++) {
        int64_t t0 = whisper_full_get_segment_t0(ww->ctx, i);
        int64_t t1 = whisper_full_get_segment_t1(ww->ctx, i);
        const char* text = whisper_full_get_segment_text(ww->ctx, i);
        
        result->segments[i].startTime = (double)t0 / 100.0;
        result->segments[i].endTime = (double)t1 / 100.0;
        result->segments[i].text = strdup(text ? text : "");
        
        if (!result->segments[i].text) {
            result->segments[i].text = strdup("");
        }
    }
    
    result->success = true;
    printf("[Whisper] Transcribed %d segments, language: %s\n", n_segments, lang);
    
    return result;
}

TranscriptionResult* whisper_wrapper_transcribe_with_callbacks(WhisperWrapper* ww,
                                                                  const char* wav_path,
                                                                  int max_segment_length,
                                                                  volatile bool* cancel_flag,
                                                                  whisper_progress_cb progress_callback,
                                                                  void* progress_user_data,
                                                                  whisper_segment_cb segment_callback,
                                                                  void* segment_user_data) {
    if (!ww || !wav_path || !ww->loaded) {
        TranscriptionResult* result = calloc(1, sizeof(TranscriptionResult));
        if (result) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message), 
                     "Whisper not initialized");
        }
        return result;
    }
    
    TranscriptionResult* result = calloc(1, sizeof(TranscriptionResult));
    if (!result) return NULL;
    
    WavData* wav = read_wav_file(wav_path);
    if (!wav) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Cannot read WAV file: %s", wav_path);
        return result;
    }
    
    int input_frames = wav->n_samples;
    int input_channels = wav->n_channels;
    int input_rate = wav->sample_rate;
    
    printf("[Whisper] Loaded WAV: frames=%d, channels=%d, rate=%d\n",
           input_frames, input_channels, input_rate);
    
    // Step 1: Convert int16 to float
    float* input_float = malloc(input_frames * input_channels * sizeof(float));
    if (!input_float) {
        free_wav_data(wav);
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed");
        return result;
    }
    
    for (int i = 0; i < input_frames * input_channels; i++) {
        input_float[i] = (float)wav->data[i] / 32768.0f;
    }
    
    // Step 2: Resample with libsamplerate
    int resampled_frames = 0;
    int resampled_channels = 0;
    float* resampled = resample_with_src_internal(input_float, input_frames, input_channels, 
                                                   input_rate, 16000, &resampled_frames, &resampled_channels);
    free(input_float);
    free_wav_data(wav);
    
    if (!resampled || resampled_frames <= 0) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to resample audio");
        return result;
    }
    
    // Step 3: Convert to mono
    float* mono_samples = NULL;
    if (resampled_channels > 1) {
        mono_samples = malloc(resampled_frames * sizeof(float));
        if (!mono_samples) {
            free(resampled);
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Memory allocation failed");
            return result;
        }
        
        for (int i = 0; i < resampled_frames; i++) {
            float sum = 0;
            for (int ch = 0; ch < resampled_channels; ch++) {
                sum += resampled[i * resampled_channels + ch];
            }
            mono_samples[i] = sum / resampled_channels;
        }
        free(resampled);
    } else {
        mono_samples = resampled;
    }
    
    // Step 4: Normalize
    normalize_audio(mono_samples, resampled_frames);
    
    int n_samples = resampled_frames;
    float* samples = mono_samples;
    
    if (cancel_flag && *cancel_flag) {
        free(samples);
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Cancelled");
        return result;
    }
    
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = "auto";
    params.translate = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.max_len = max_segment_length > 0 ? max_segment_length : WHISPER_MAX_SEGMENT_LENGTH;
    params.token_timestamps = true;
    params.split_on_word = true;
    
    // Set callbacks
    if (progress_callback) {
        params.progress_callback = progress_callback;
        params.progress_callback_user_data = progress_user_data;
    }
    
    if (segment_callback) {
        params.new_segment_callback = segment_callback;
        params.new_segment_callback_user_data = segment_user_data;
    }
    
    // Set abort callback for cancel support
    if (cancel_flag) {
        params.abort_callback = whisper_abort_callback;
        params.abort_callback_user_data = (void*)cancel_flag;
    }
    
    int ret = whisper_full(ww->ctx, params, samples, n_samples);
    free(samples);
    
    if (cancel_flag && *cancel_flag) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Cancelled");
        return result;
    }
    
    if (ret != 0) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Whisper transcription failed: %d", ret);
        return result;
    }
    
    const char* lang = whisper_lang_str(whisper_full_lang_id(ww->ctx));
    if (lang) {
        strncpy(result->detected_language, lang, sizeof(result->detected_language) - 1);
    }
    
    int n_segments = whisper_full_n_segments(ww->ctx);
    if (n_segments <= 0) {
        result->success = true;
        result->segment_count = 0;
        result->segments = NULL;
        return result;
    }
    
    result->segments = calloc(n_segments, sizeof(Subtitle));
    if (!result->segments) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed");
        return result;
    }
    
    result->segment_count = n_segments;
    
    for (int i = 0; i < n_segments; i++) {
        int64_t t0 = whisper_full_get_segment_t0(ww->ctx, i);
        int64_t t1 = whisper_full_get_segment_t1(ww->ctx, i);
        const char* text = whisper_full_get_segment_text(ww->ctx, i);
        
        result->segments[i].startTime = (double)t0 / 100.0;
        result->segments[i].endTime = (double)t1 / 100.0;
        result->segments[i].text = strdup(text ? text : "");
        
        if (!result->segments[i].text) {
            result->segments[i].text = strdup("");
        }
    }
    
    result->success = true;
    printf("[Whisper] Transcribed %d segments, language: %s\n", n_segments, lang);
    
    return result;
}

void whisper_wrapper_free_result(TranscriptionResult* result) {
    if (!result) return;
    
    if (result->segments) {
        for (int i = 0; i < result->segment_count; i++) {
            if (result->segments[i].text) {
                free(result->segments[i].text);
            }
        }
        free(result->segments);
    }
    
    free(result);
}