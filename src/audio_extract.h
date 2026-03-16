#ifndef AUDIO_EXTRACT_H
#define AUDIO_EXTRACT_H

#include <stdbool.h>

bool audio_extract_to_wav(const char* video_path, const char* output_wav_path, volatile bool* cancel_flag);

#endif