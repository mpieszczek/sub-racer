#include "video_player.h"
#include <mpv/client.h>
#include <mpv/render.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct VideoPlayer {
    mpv_handle* mpv;
    mpv_render_context* mpv_gl;
    
    Texture2D texture;
    unsigned char* pixelBuffer;
    int width;
    int height;
    double duration;
    bool playing;
    bool loaded;
    bool videoReady;
};

VideoPlayer* vp_create(void) {
    printf("[VideoPlayer] Creating...\n");
    
    VideoPlayer* vp = calloc(1, sizeof(VideoPlayer));
    if (!vp) return NULL;
    
    vp->mpv = mpv_create();
    if (!vp->mpv) {
        free(vp);
        return NULL;
    }
    
    mpv_set_option_string(vp->mpv, "hwdec", "auto");
    mpv_set_option_string(vp->mpv, "keep-open", "yes");
    mpv_set_option_string(vp->mpv, "gapless-audio", "yes");
    
    if (mpv_initialize(vp->mpv) < 0) {
        mpv_terminate_destroy(vp->mpv);
        free(vp);
        return NULL;
    }
    
    mpv_set_option_string(vp->mpv, "vo", "libmpv");
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, "sw"},
    };
    
    if (mpv_render_context_create(&vp->mpv_gl, vp->mpv, params) < 0) {
        mpv_terminate_destroy(vp->mpv);
        free(vp);
        return NULL;
    }
    
    printf("[VideoPlayer] Created with libmpv vo\n");
    return vp;
}

void vp_destroy(VideoPlayer* vp) {
    if (!vp) return;
    
    if (vp->pixelBuffer) {
        free(vp->pixelBuffer);
        vp->pixelBuffer = NULL;
    }
    
    if (vp->texture.id != 0) {
        UnloadTexture(vp->texture);
        vp->texture.id = 0;
    }
    
    if (vp->mpv_gl) {
        mpv_render_context_free(vp->mpv_gl);
        vp->mpv_gl = NULL;
    }
    
    if (vp->mpv) {
        mpv_terminate_destroy(vp->mpv);
        vp->mpv = NULL;
    }
    
    free(vp);
    printf("[VideoPlayer] Destroyed\n");
}

bool vp_load(VideoPlayer* vp, const char* filename) {
    if (!vp || !filename) return false;
    
    vp->loaded = false;
    vp->videoReady = false;
    vp->playing = false;
    
    if (vp->texture.id != 0) {
        UnloadTexture(vp->texture);
        vp->texture.id = 0;
    }
    
    if (vp->pixelBuffer) {
        free(vp->pixelBuffer);
        vp->pixelBuffer = NULL;
    }
    
    vp->width = 0;
    vp->height = 0;
    vp->duration = 0.0;
    
    const char* cmd[] = { "loadfile", filename, NULL };
    if (mpv_command(vp->mpv, cmd) < 0) {
        printf("[VideoPlayer] Failed to load: %s\n", filename);
        return false;
    }
    
    vp->loaded = true;
    printf("[VideoPlayer] Loaded: %s\n", filename);
    return true;
}

static void update_video_info(VideoPlayer* vp) {
    int64_t w = 0, h = 0;
    int64_t dw = 0, dh = 0;
    double dur = 0.0;
    
    mpv_get_property(vp->mpv, "width", MPV_FORMAT_INT64, &w);
    mpv_get_property(vp->mpv, "height", MPV_FORMAT_INT64, &h);
    mpv_get_property(vp->mpv, "video-params/dw", MPV_FORMAT_INT64, &dw);
    mpv_get_property(vp->mpv, "video-params/dh", MPV_FORMAT_INT64, &dh);
    mpv_get_property(vp->mpv, "duration", MPV_FORMAT_DOUBLE, &dur);
    
    int videoW = (dw > 0) ? (int)dw : (int)w;
    int videoH = (dh > 0) ? (int)dh : (int)h;
    
    if (videoW > 0 && videoH > 0 && (!vp->videoReady || vp->width != videoW || vp->height != videoH)) {
        printf("[VideoPlayer] Video info: coded %dx%d, display %dx%d, duration %.2f\n", 
               (int)w, (int)h, videoW, videoH, dur);
        
        vp->width = videoW;
        vp->height = videoH;
        vp->duration = dur;
        
        int stride = vp->width * 4;
        vp->pixelBuffer = (unsigned char*)malloc(vp->height * stride);
        
        Image img = {
            .data = vp->pixelBuffer,
            .width = vp->width,
            .height = vp->height,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
            .mipmaps = 1
        };
        
        vp->texture = LoadTextureFromImage(img);
        SetTextureFilter(vp->texture, TEXTURE_FILTER_BILINEAR);
        
        vp->videoReady = true;
        printf("[VideoPlayer] Texture created: %dx%d\n", vp->width, vp->height);
    }
}

void vp_update(VideoPlayer* vp) {
    if (!vp || !vp->loaded || !vp->mpv) return;
    
    while (1) {
        mpv_event* event = mpv_wait_event(vp->mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        
        switch (event->event_id) {
            case MPV_EVENT_FILE_LOADED:
                printf("[VideoPlayer] File loaded\n");
                break;
                
            case MPV_EVENT_VIDEO_RECONFIG:
                update_video_info(vp);
                break;
                
            case MPV_EVENT_AUDIO_RECONFIG:
                break;
                
            case MPV_EVENT_END_FILE:
                vp->playing = false;
                break;
                
            case MPV_EVENT_PLAYBACK_RESTART:
                break;
                
            default:
                break;
        }
    }
    
    if (vp->videoReady && vp->mpv_gl && vp->pixelBuffer) {
        int stride = vp->width * 4;
        
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, "sw"},
            {MPV_RENDER_PARAM_SW_SIZE, &(int[2]){vp->width, vp->height}},
            {MPV_RENDER_PARAM_SW_FORMAT, "rgba"},
            {MPV_RENDER_PARAM_SW_STRIDE, &stride},
            {MPV_RENDER_PARAM_SW_POINTER, vp->pixelBuffer},
        };
        
        mpv_render_context_render(vp->mpv_gl, params);
        UpdateTexture(vp->texture, vp->pixelBuffer);
    }
}

void vp_render(VideoPlayer* vp, Rectangle dest) {
    if (!vp || !vp->videoReady || vp->texture.id == 0) return;
    
    float scaleX = dest.width / (float)vp->width;
    float scaleY = dest.height / (float)vp->height;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    
    float newWidth = (float)vp->width * scale;
    float newHeight = (float)vp->height * scale;
    
    Rectangle src = { 0, 0, (float)vp->width, (float)vp->height };
    Rectangle dst = { 
        dest.x + (dest.width - newWidth) / 2, 
        dest.y + (dest.height - newHeight) / 2, 
        newWidth, 
        newHeight 
    };
    
    DrawTexturePro(vp->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

Rectangle vp_get_video_rect(VideoPlayer* vp, Rectangle dest) {
    if (!vp || vp->width <= 0 || vp->height <= 0) return dest;
    
    float scaleX = dest.width / (float)vp->width;
    float scaleY = dest.height / (float)vp->height;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    
    float newWidth = (float)vp->width * scale;
    float newHeight = (float)vp->height * scale;
    
    return (Rectangle){
        dest.x + (dest.width - newWidth) / 2,
        dest.y + (dest.height - newHeight) / 2,
        newWidth,
        newHeight
    };
}

void vp_play(VideoPlayer* vp) {
    if (!vp || !vp->loaded) return;
    mpv_set_property_string(vp->mpv, "pause", "no");
    vp->playing = true;
}

void vp_pause(VideoPlayer* vp) {
    if (!vp || !vp->loaded) return;
    mpv_set_property_string(vp->mpv, "pause", "yes");
    vp->playing = false;
}

void vp_stop(VideoPlayer* vp) {
    if (!vp || !vp->loaded) return;
    const char* cmd[] = { "stop", NULL };
    mpv_command(vp->mpv, cmd);
    vp->playing = false;
}

void vp_unload(VideoPlayer* vp) {
    if (!vp) return;
    
    if (vp->mpv) {
        const char* cmd[] = { "stop", NULL };
        mpv_command(vp->mpv, cmd);
    }
    
    if (vp->texture.id != 0) {
        UnloadTexture(vp->texture);
        vp->texture.id = 0;
    }
    
    if (vp->pixelBuffer) {
        free(vp->pixelBuffer);
        vp->pixelBuffer = NULL;
    }
    
    vp->width = 0;
    vp->height = 0;
    vp->duration = 0.0;
    vp->playing = false;
    vp->loaded = false;
    vp->videoReady = false;
    
    printf("[VideoPlayer] Unloaded\n");
}

void vp_seek(VideoPlayer* vp, double timeSec) {
    if (!vp || !vp->loaded) return;
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%f", timeSec);
    const char* cmd_vec[] = { "seek", cmd, "absolute", NULL };
    mpv_command(vp->mpv, cmd_vec);
}

bool vp_is_playing(VideoPlayer* vp) {
    if (!vp) return false;
    
    if (vp->mpv) {
        int paused = 1;
        if (mpv_get_property(vp->mpv, "pause", MPV_FORMAT_FLAG, &paused) >= 0) {
            vp->playing = !paused;
        }
    }
    
    return vp->playing;
}

bool vp_is_loaded(VideoPlayer* vp) {
    if (!vp) return false;
    return vp->loaded;
}

double vp_get_time(VideoPlayer* vp) {
    if (!vp || !vp->loaded) return 0.0;
    
    double time = 0.0;
    mpv_get_property(vp->mpv, "time-pos", MPV_FORMAT_DOUBLE, &time);
    return time;
}

double vp_get_duration(VideoPlayer* vp) {
    if (!vp) return 0.0;
    return vp->duration;
}

int vp_get_width(VideoPlayer* vp) {
    if (!vp) return 0;
    return vp->width;
}

int vp_get_height(VideoPlayer* vp) {
    if (!vp) return 0;
    return vp->height;
}

void vp_refresh_subtitles(VideoPlayer* vp, const char* path) {
    if (!vp || !vp->mpv) return;
    const char* cmd1[] = { "sub-remove", NULL };
    mpv_command(vp->mpv, cmd1);
    if (path) {
        const char* cmd2[] = { "sub-add", path, NULL };
        mpv_command(vp->mpv, cmd2);
    }
}

double vp_get_fps(VideoPlayer* vp) {
    if (!vp || !vp->mpv) return 30.0;
    double fps = 30.0;
    if (mpv_get_property(vp->mpv, "fps", MPV_FORMAT_DOUBLE, &fps) >= 0 && fps > 0) {
        return fps;
    }
    return 30.0;
}
