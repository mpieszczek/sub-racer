#include "ui_editor.h"
#include "editor_actions.h"
#include "srt_io.h"
#include "project.h"
#include "time_utils.h"
#include "file_utils.h"
#include "audio_extract.h"
#include "whisper_wrapper.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void render_video_area(AppState* state, int videoW, int screenH);
static void render_timeline_panel(AppState* state, int videoW, int panelY);
static void render_subtitle_list(AppState* state, int screenW, int screenH);
static void render_edit_form(AppState* state, int screenW, int screenH);
static void render_export_dialog(AppState* state, int screenW, int screenH);
static void render_transcribe_dialog(AppState* state, int screenW, int screenH);

void ui_render_project_panel(AppState* state) {
    if (!state) return;
    
    BeginDrawing();
    ClearBackground(UI_BG_COLOR);
    
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int cx = screenW / 2;
    int cy = screenH / 2;
    
    if (state->logoLoaded) {
        float scale = 0.5f;
        float logoW = state->logoTexture.width * scale;
        float logoH = state->logoTexture.height * scale;
        Rectangle src = { 0, 0, (float)state->logoTexture.width, (float)state->logoTexture.height };
        Rectangle dst = { cx - logoW*5, cy - 120 - logoH*7, logoW*10, logoH*10 };
        DrawTexturePro(state->logoTexture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }
    
    Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
    int tempTextWidth = MeasureTextEx(font, "Drag and drop a video file here", FONT_SIZE, FONT_SPACING).x;
    DrawTextEx(font, "Drag and drop a video file here", (Vector2){ cx - tempTextWidth/2, cy - 60 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
    tempTextWidth = MeasureTextEx(font, "or run: sub-racer.exe <video.mp4>", FONT_SIZE, FONT_SPACING).x;
    DrawTextEx(font, "or run: sub-racer.exe <video.mp4>", (Vector2){ cx - tempTextWidth/2, cy - 30 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
    
    if (state->projectList) {
        project_list_free(state->projectList, state->projectListCount);
    }
    state->projectList = project_list(&state->projectListCount);
    
    if (state->projectList && state->projectListCount > 0) {
        int listY = cy + 70;
        int listH = screenH - listY - 50;
        if (listH < 50) listH = 50;
        int itemHeight = 30;
        int visibleItems = listH / itemHeight;
        int scrollbarWidth = 10;
        
        int maxScroll = state->projectListCount - visibleItems;
        if (maxScroll < 0) maxScroll = 0;
        
        int wheelMove = GetMouseWheelMove();
        if (wheelMove != 0) {
            state->projectListScroll -= wheelMove;
            if (state->projectListScroll < 0) state->projectListScroll = 0;
            if (state->projectListScroll > maxScroll) state->projectListScroll = maxScroll;
        }
        
        if (state->projectListCount <= visibleItems) {
            state->projectListScroll = 0;
        } else if (state->projectListScroll > maxScroll) {
            state->projectListScroll = maxScroll;
        }
        
        DrawTextEx(font, "Projects:", (Vector2){ cx - 200, listY - 20 }, FONT_SIZE, FONT_SPACING, UI_HL_COLOR);
        
        int listWidth = 400 - scrollbarWidth - 5;
        for (int i = state->projectListScroll; i < state->projectListCount && i < state->projectListScroll + visibleItems; i++) {
            Rectangle projRec = { cx - 200, listY + (i - state->projectListScroll) * itemHeight, listWidth, 25 };
            bool isHovered = CheckCollisionPointRec(GetMousePosition(), projRec);
            DrawRectangleRec(projRec, UI_FG_COLOR);
            if (isHovered) DrawRectangleLinesEx(projRec, 1, UI_HL_COLOR);
            DrawTextEx(font, state->projectList[i], (Vector2){ projRec.x + 5, projRec.y + 5 }, FONT_SIZE, FONT_SPACING, UI_HL_COLOR);
            
            if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (project_load(state->projectList[i])) {
                    if (vp_load(state->vp, project_get_video_path())) {
                        vp_play(state->vp);
                        strncpy(state->currentVideoPath, project_get_video_path(), sizeof(state->currentVideoPath) - 1);
                        state->showProjectPanel = false;
                        
                        const char* srtPath = project_get_working_srt_path();
                        if (srtPath) {
                            FILE* testF = fopen(srtPath, "r");
                            if (testF) {
                                fclose(testF);
                                sublist_clear(&state->subtitles);
                                srt_load_from_file(&state->subtitles, srtPath);
                            }
                            srt_save_to_file(&state->subtitles, project_get_working_srt_path());
                            vp_refresh_subtitles(state->vp, project_get_working_srt_path());
                        }
                    }
                }
            }
        }
        
        if (maxScroll > 0) {
            Rectangle scrollRec = { cx + 205 - scrollbarWidth, listY, scrollbarWidth, listH };
            state->projectListScroll = GuiScrollBar(scrollRec, state->projectListScroll, 0, maxScroll);
            DrawRectangleLinesEx(scrollRec, 1, UI_FG_COLOR);
        }
    }
    
    EndDrawing();
}

void ui_render_editor(AppState* state) {
    if (!state || !state->vp) return;
    
    BeginDrawing();
    ClearBackground(UI_BG_COLOR);
    
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int videoW = screenW - SIDE_PANEL_WIDTH;
    int panelY = screenH - state->timelinePanelHeight;
    
    render_video_area(state, videoW, panelY);
    render_timeline_panel(state, videoW, panelY);
    render_subtitle_list(state, screenW, screenH);
    render_edit_form(state, screenW, screenH);
    render_export_dialog(state, screenW, screenH);
    render_transcribe_dialog(state, screenW, screenH);
    
    EndDrawing();
}

static void render_video_area(AppState* state, int videoW, int panelY) {
    if (!vp_is_loaded(state->vp)) {
        int screenW = GetScreenWidth();
        Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
        DrawTextEx(font, "Loading video...", (Vector2){ videoW/2 - 60, panelY/2 }, FONT_SIZE, FONT_SPACING, UI_HL_COLOR);
        return;
    }
    
    Rectangle videoDest = { 0, 0, (float)videoW, (float)panelY };
    vp_render(state->vp, videoDest);
    
    Rectangle videoRect = vp_get_video_rect(state->vp, videoDest);
    DrawRectangleLinesEx(videoRect, 2.0f, UI_HL_COLOR);
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), videoRect) && !state->showExportMessage && !state->showOverwriteConfirm && !state->showTranscribeConfirm && !state->showTranscribeProgress && !state->showTranscribeComplete) {
        if (vp_is_playing(state->vp)) {
            vp_pause(state->vp);
        } else {
            vp_play(state->vp);
        }
    }
    
    Rectangle backBtn = { 10, 10, 70,30};
    if (GuiButton(backBtn, "<- Back")) {
        action_goto_project_panel(state);
    }
}

static void render_timeline_panel(AppState* state, int videoW, int panelY) {
    int screenH = GetScreenHeight();
    int panelH = state->timelinePanelHeight;
    
    DrawRectangle(0, panelY, videoW, panelH, UI_BG_COLOR);
    DrawRectangleLines(0, panelY, videoW, panelH, UI_FG_COLOR);
    
    double time = vp_get_time(state->vp);
    double dur = vp_get_duration(state->vp);
    
    float sliderMax = (float)dur;
    if (sliderMax <= 0) sliderMax = 1;
    float sliderValue = (float)time;
    
    Rectangle sliderRec = { 10, panelY + 10, videoW - 20, 20 };
    
    float prevSliderValue = sliderValue;
    GuiSlider(sliderRec, NULL, NULL, &sliderValue, 0, sliderMax);
    
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    
    if (!state->isDragging && sliderValue != prevSliderValue && mouseDown) {
        state->isDragging = true;
        state->wasPlayingBeforeDrag = vp_is_playing(state->vp);
        vp_pause(state->vp);
    }
    
    if (state->isDragging) {
        if (!mouseDown) {
            state->isDragging = false;
            vp_seek(state->vp, sliderValue);
            if (state->wasPlayingBeforeDrag) {
                vp_play(state->vp);
            }
        } else if (sliderValue != time) {
            vp_seek(state->vp, sliderValue);
        }
    }
    
    char timeText[64];
    char startTimeStr[32], endTimeStr[32];
    format_display_time(time, startTimeStr);
    format_display_time(dur, endTimeStr);
    snprintf(timeText, sizeof(timeText), "%s / %s", startTimeStr, endTimeStr);
    
    Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
    DrawTextEx(font, timeText, (Vector2){ 10, panelY + 40 }, FONT_SIZE, FONT_SPACING, UI_HL_COLOR);
    
    Rectangle exportBtn = { videoW - 250, panelY + 40, 90, 20 };
    if (GuiButton(exportBtn, "Export")) {
        char srtPath[512];
        strncpy(srtPath, state->currentVideoPath, sizeof(srtPath) - 1);
        char* dot = strrchr(srtPath, '.');
        if (dot) strcpy(dot, ".srt");
        
        FILE* testF = fopen(srtPath, "r");
        if (testF) {
            fclose(testF);
            state->showOverwriteConfirm = true;
        } else {
            state->exportSuccess = srt_export_to_video_path(&state->subtitles, state->currentVideoPath, state->exportMessage, sizeof(state->exportMessage));
            state->showExportMessage = true;
        }
    }
    
    Rectangle transcribeBtn = { videoW - 150, panelY + 40, 140, 20 };
    if (GuiButton(transcribeBtn, "Auto-Transcribe")) {
        if (state->subtitles.count > 0) {
            state->showTranscribeConfirm = true;
        } else {
            transcribe_thread_init(&state->transcribeThread);
            strncpy(state->transcribeThread.video_path, state->currentVideoPath, 
                    sizeof(state->transcribeThread.video_path) - 1);
            state->transcribeThread.max_segment_length = WHISPER_MAX_SEGMENT_LENGTH;
            
            if (transcribe_thread_start(&state->transcribeThread)) {
                state->showTranscribeProgress = true;
                sublist_clear(&state->subtitles);
                state->subtitles.selectedIndex = -1;
            } else {
                snprintf(state->exportMessage, sizeof(state->exportMessage),
                         "Failed to start transcription thread");
                state->showTranscribeComplete = true;
            }
        }
    }
    
    char helpText[] = "SPACE: Play/Pause | LEFT/RIGHT: +/-5s | comma/dot: +/- frame";
    DrawTextEx(font, helpText, (Vector2){ 10, panelY + 70 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
}

static void render_subtitle_list(AppState* state, int screenW, int screenH) {
    Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
    
    DrawRectangle(screenW - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, screenH, UI_BG_COLOR);
    DrawRectangleLines(screenW - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, screenH, UI_FG_COLOR);
    
    Rectangle headerBtn = { screenW - SIDE_PANEL_WIDTH + 10, 10, SIDE_PANEL_WIDTH - 20, 30 };
    if (GuiButton(headerBtn, "+ Add")) {
        action_add_subtitle(state);
    }
    
    int listY = 50;
    int listH = screenH - listY - 230;
    if (listH < 50) listH = 50;
    int itemHeight = 35;
    int visibleItems = listH / itemHeight;
    int scrollbarWidth = 10;
    
    int maxScroll = state->subtitles.count - visibleItems;
    if (maxScroll < 0) maxScroll = 0;
    
    int wheelMove = GetMouseWheelMove();
    if (wheelMove != 0) {
        state->subtitleListScroll -= wheelMove;
        if (state->subtitleListScroll < 0) state->subtitleListScroll = 0;
        if (state->subtitleListScroll > maxScroll) state->subtitleListScroll = maxScroll;
    }
    
    if (state->subtitles.count <= visibleItems) {
        state->subtitleListScroll = 0;
    } else if (state->subtitleListScroll > maxScroll) {
        state->subtitleListScroll = maxScroll;
    }
    
    int itemWidth = SIDE_PANEL_WIDTH - 20 - scrollbarWidth - 5;
    for (int i = state->subtitleListScroll; i < state->subtitles.count && i < state->subtitleListScroll + visibleItems + 1; i++) {
        Subtitle* sub = &state->subtitles.items[i];
        
        int itemY = listY + (i - state->subtitleListScroll) * itemHeight;
        
        if (itemY < listY || itemY >= listY + listH) continue;
        
        Rectangle itemRec = { screenW - SIDE_PANEL_WIDTH + 10, itemY, itemWidth, 30 };
        
        Color itemBg = (i == state->subtitles.selectedIndex) ? UI_HL_COLOR : UI_FG_COLOR;
        DrawRectangleRec(itemRec, itemBg);
        if (CheckCollisionPointRec(GetMousePosition(), itemRec)) DrawRectangleLinesEx(itemRec, 1, UI_HL_COLOR);
        
        char timeStr[32];
        format_display_time_short(sub->startTime, timeStr);
        
        DrawTextEx(font, timeStr, (Vector2){ itemRec.x + 5, itemRec.y + 7 }, FONT_SIZE, FONT_SPACING, (i == state->subtitles.selectedIndex) ? UI_BG_COLOR : UI_HL_COLOR);
        
        char displayText[32] = {0};
        if (sub->text) {
            strncpy(displayText, sub->text, 20);
            if (strlen(sub->text) > 20) strcat(displayText, "...");
        }
        DrawTextEx(font, displayText, (Vector2){ itemRec.x + 80, itemRec.y + 7 }, FONT_SIZE, FONT_SPACING, UI_BG_COLOR);
        
        if (CheckCollisionPointRec(GetMousePosition(), itemRec) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            action_select_subtitle(state, i);
        }
    }
    
    if (maxScroll > 0) {
        Rectangle scrollRec = { screenW - scrollbarWidth - 5, listY, scrollbarWidth, listH };
        state->subtitleListScroll = GuiScrollBar(scrollRec, state->subtitleListScroll, 0, maxScroll);
        DrawRectangleLinesEx(scrollRec, 1, UI_FG_COLOR);
    }
}

static void render_edit_form(AppState* state, int screenW, int screenH) {
    if (state->subtitles.selectedIndex < 0) return;
    
    Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
    
    int editY = screenH - 200;
    Rectangle editPanel = { screenW - SIDE_PANEL_WIDTH + 10, editY, SIDE_PANEL_WIDTH - 20, 190 };
    DrawRectangleRec(editPanel, UI_BG_COLOR);
    DrawRectangleLines(editPanel.x, editPanel.y, editPanel.width, editPanel.height, UI_FG_COLOR);
    
    DrawTextEx(font, "Start:", (Vector2){ editPanel.x + 5, editPanel.y + 5 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
    Rectangle startRec = { editPanel.x + 60, editPanel.y + 5, 85, 20 };
    if (GuiTextBox(startRec, state->editStartStr, 32, state->startEditing)) {
        state->startEditing = true;
        state->endEditing = false;
        state->textEditing = false;
        state->isTextEditing = true;
    }
    
    DrawTextEx(font, "End:", (Vector2){ editPanel.x + 170, editPanel.y + 5 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
    Rectangle endRec = { editPanel.x + 210, editPanel.y + 5, 85, 20 };
    if (GuiTextBox(endRec, state->editEndStr, 32, state->endEditing)) {
        state->endEditing = true;
        state->startEditing = false;
        state->textEditing = false;
        state->isTextEditing = true;
    }
    
    Rectangle startSetBtn = { editPanel.x + 60, editPanel.y + 28, 85, 15 };
    if (GuiButton(startSetBtn, "+")) {
        action_set_start_from_playback(state);
    }
    
    Rectangle endSetBtn = { editPanel.x + 210, editPanel.y + 28, 85, 15 };
    if (GuiButton(endSetBtn, "+")) {
        action_set_end_from_playback(state);
    }
    
    DrawTextEx(font, "Text:", (Vector2){ editPanel.x + 5, editPanel.y + 55 }, FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
    Rectangle textRec = { editPanel.x + 5, editPanel.y + 75, editPanel.width - 10, 60 };
    if (GuiTextBox(textRec, state->editText, 256, state->textEditing)) {
        state->textEditing = true;
        state->startEditing = false;
        state->endEditing = false;
        state->isTextEditing = true;
    }
    
    if ((state->startEditing || state->endEditing || state->textEditing) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!CheckCollisionPointRec(GetMousePosition(), startRec) &&
            !CheckCollisionPointRec(GetMousePosition(), endRec) &&
            !CheckCollisionPointRec(GetMousePosition(), textRec)) {
            state->textEditing = false;
            state->startEditing = false;
            state->endEditing = false;
            state->isTextEditing = false;
        }
    }
    
    Rectangle saveBtn = { editPanel.x + 5, editPanel.y + 145, 80, 25 };
    if (GuiButton(saveBtn, "Save")) {
        action_save_subtitle(state);
    }
    
    Rectangle delBtn = { editPanel.x + editPanel.width - 90, editPanel.y + 145, 80, 25 };
    if (GuiButton(delBtn, "Delete")) {
        action_delete_subtitle(state);
    }
}

static void render_export_dialog(AppState* state, int screenW, int screenH) {
    if (state->showExportMessage) {
        Rectangle msgBox = { screenW/2 - 500, screenH/2 - 60, 1000, 120 };
        
        int result = GuiMessageBox(
            msgBox,
            state->exportSuccess ? "#191#Export Success" : "#198#Export Error",
            state->exportMessage,
            "OK"
        );
        
        if (result >= 0) {
            state->showExportMessage = false;
        }
    }
    
    if (state->showOverwriteConfirm) {
        Rectangle confirmBox = { screenW/2 - 150, screenH/2 - 60, 300, 140 };
        
        int result = GuiMessageBox(
            confirmBox,
            "#198#File Exists",
            "The .srt file already exists.\nIt will be overwritten.\nContinue?",
            "Continue;Abort"
        );
        
        if (result >= 0) {
            state->showOverwriteConfirm = false;
            if (result == 1) {
                state->exportSuccess = srt_export_to_video_path(&state->subtitles, state->currentVideoPath, state->exportMessage, sizeof(state->exportMessage));
                state->showExportMessage = true;
            }
        }
    }
}

bool ui_handle_file_drop(AppState* state) {
    if (!IsFileDropped()) return false;
    
    FilePathList droppedFiles = LoadDroppedFiles();
    bool handled = false;
    
    if (droppedFiles.count > 0) {
        for (unsigned int i = 0; i < droppedFiles.count; i++) {
            const char* file = droppedFiles.paths[i];
            if (file_has_video_extension(file)) {
                if (action_load_video(state, file)) {
                    handled = true;
                    break;
                }
            }
        }
    }
    
    UnloadDroppedFiles(droppedFiles);
    return handled;
}

static void render_transcribe_dialog(AppState* state, int screenW, int screenH) {
    Font font = state->appFont.texture.id != 0 ? state->appFont : GetFontDefault();
    
    // === CONFIRM DIALOG ===
    if (state->showTranscribeConfirm) {
        Rectangle confirmBox = { screenW/2 - 180, screenH/2 - 70, 360, 140 };
        
        int result = GuiMessageBox(
            confirmBox,
            "#198#Overwrite Subtitles?",
            "Existing subtitles will be replaced.\nContinue with transcription?",
            "Continue;Cancel"
        );
        
        if (result >= 0) {
            state->showTranscribeConfirm = false;
            if (result == 1) {
                transcribe_thread_init(&state->transcribeThread);
                strncpy(state->transcribeThread.video_path, state->currentVideoPath, 
                        sizeof(state->transcribeThread.video_path) - 1);
                state->transcribeThread.max_segment_length = WHISPER_MAX_SEGMENT_LENGTH;
                
                if (transcribe_thread_start(&state->transcribeThread)) {
                    state->showTranscribeProgress = true;
                    // Clear existing subtitles before transcription
                    sublist_clear(&state->subtitles);
                    state->subtitles.selectedIndex = -1;
                } else {
                    snprintf(state->exportMessage, sizeof(state->exportMessage),
                             "Failed to start transcription thread");
                    state->showTranscribeComplete = true;
                }
            }
        }
    }
    
    // === PROGRESS DIALOG ===
    if (state->showTranscribeProgress) {
        Rectangle progressBox = { screenW/2 - 200, screenH/2 - 80, 400, 160 };
        
        DrawRectangleRec(progressBox, UI_BG_COLOR);
        DrawRectangleLinesEx(progressBox, 2, UI_FG_COLOR);
        
        // Title
        DrawTextEx(font, "Transcribing...", 
                   (Vector2){ progressBox.x + 10, progressBox.y + 10 }, 
                   FONT_SIZE, FONT_SPACING, UI_HL_COLOR);
        
        // Language (if detected)
        if (state->transcribeThread.detected_language[0] != '\0') {
            char langText[64];
            snprintf(langText, sizeof(langText), "Detected: %s", 
                     state->transcribeThread.detected_language);
            DrawTextEx(font, langText, 
                       (Vector2){ progressBox.x + 10, progressBox.y + 35 }, 
                       FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
        }
        
        // Progress text
        char progressText[64];
        snprintf(progressText, sizeof(progressText), "Progress: %d%% (%d segments)", 
                 state->transcribeThread.progress, state->transcribeThread.segments_count);
        DrawTextEx(font, progressText, 
                   (Vector2){ progressBox.x + 10, progressBox.y + 60 }, 
                   FONT_SIZE, FONT_SPACING, UI_FG_COLOR);
        
        // Progress bar
        Rectangle progressBar = { progressBox.x + 10, progressBox.y + 90, 380, 25 };
        DrawRectangleRec(progressBar, UI_FG_COLOR);
        float progressFill = state->transcribeThread.progress / 100.0f;
        Rectangle filledPart = { progressBar.x, progressBar.y, progressBar.width * progressFill, progressBar.height };
        DrawRectangleRec(filledPart, UI_HL_COLOR);
        
        // Copy new segments to main list (real-time update)
        int prev_count = state->subtitles.count;
        transcribe_thread_copy_new_segments(&state->transcribeThread, &state->subtitles);
        
        // If new segments were added, save to file and refresh video
        if (state->subtitles.count > prev_count) {
            srt_save_to_file(&state->subtitles, project_get_working_srt_path());
            vp_refresh_subtitles(state->vp, project_get_working_srt_path());
        }
        
        // Cancel button
        Rectangle cancelBtn = { progressBox.x + 300, progressBox.y + 125, 90, 25 };
        if (GuiButton(cancelBtn, "Cancel")) {
            transcribe_thread_request_cancel(&state->transcribeThread);
            transcribe_thread_join(&state->transcribeThread);
            
            // Reload subtitles from working file
            const char* srtPath = project_get_working_srt_path();
            if (srtPath) {
                sublist_clear(&state->subtitles);
                srt_load_from_file(&state->subtitles, srtPath);
                vp_refresh_subtitles(state->vp, srtPath);
            }
            
            state->showTranscribeProgress = false;
        }
        
        // Check completion
        if (transcribe_thread_is_completed(&state->transcribeThread)) {
            transcribe_thread_join(&state->transcribeThread);
            
            // Skip dialog if cancelled (already handled in Cancel button)
            if (state->transcribeThread.was_cancelled) {
                state->showTranscribeProgress = false;
                state->transcribeThread.was_cancelled = false;
            } else if (transcribe_thread_is_success(&state->transcribeThread)) {
                strncpy(state->transcribeLanguage, state->transcribeThread.detected_language, 
                        sizeof(state->transcribeLanguage) - 1);
                state->transcribeResultCount = state->transcribeThread.segments_count;
                
                srt_save_to_file(&state->subtitles, project_get_working_srt_path());
                vp_refresh_subtitles(state->vp, project_get_working_srt_path());
                
                snprintf(state->exportMessage, sizeof(state->exportMessage),
                         "Transcription complete: %d subtitles", 
                         state->transcribeResultCount);
                state->exportSuccess = true;
                state->showTranscribeProgress = false;
                state->showTranscribeComplete = true;
            } else {
                snprintf(state->exportMessage, sizeof(state->exportMessage),
                         "%s", state->transcribeThread.error_message);
                state->exportSuccess = false;
                state->showTranscribeProgress = false;
                state->showTranscribeComplete = true;
            }
        }
    }
    
    // === COMPLETE DIALOG ===
    if (state->showTranscribeComplete) {
        Rectangle completeBox = { screenW/2 - 200, screenH/2 - 60, 400, 120 };
        
        int result = GuiMessageBox(
            completeBox,
            state->exportSuccess ? "#191#Complete" : "#198#Error",
            state->exportMessage,
            "OK"
        );
        
        if (result >= 0) {
            state->showTranscribeComplete = false;
        }
    }
}