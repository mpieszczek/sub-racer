#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <stdbool.h>
#include <raylib.h>

typedef struct VideoPlayer VideoPlayer;

VideoPlayer* vp_create(void);
void vp_destroy(VideoPlayer* vp);

bool vp_load(VideoPlayer* vp, const char* filename);
void vp_unload(VideoPlayer* vp);

void vp_update(VideoPlayer* vp);
void vp_render(VideoPlayer* vp, Rectangle dest);
Rectangle vp_get_video_rect(VideoPlayer* vp, Rectangle dest);

void vp_play(VideoPlayer* vp);
void vp_pause(VideoPlayer* vp);
void vp_stop(VideoPlayer* vp);
void vp_seek(VideoPlayer* vp, double timeSec);

bool vp_is_playing(VideoPlayer* vp);
bool vp_is_loaded(VideoPlayer* vp);
double vp_get_time(VideoPlayer* vp);
double vp_get_duration(VideoPlayer* vp);
int vp_get_width(VideoPlayer* vp);
int vp_get_height(VideoPlayer* vp);

void vp_reload_subtitles(VideoPlayer* vp);
void vp_add_subtitles(VideoPlayer* vp, const char* path);

#endif
