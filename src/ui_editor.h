#ifndef UI_EDITOR_H
#define UI_EDITOR_H

#include "app_state.h"
#include <stdbool.h>

#define UI_BG_COLOR      ((Color){ 15, 56, 15, 255 })
#define UI_BG_COLOR_HEX   0x0f380fff
#define UI_FG_COLOR      ((Color){ 48, 98, 48, 255 })
#define UI_FG_COLOR_HEX   0x306230ff
#define UI_HL_COLOR       ((Color){ 139, 172, 15, 255 })
#define UI_HL_COLOR_HEX   0x8bac0fff

void ui_render_project_panel(AppState* state);
void ui_render_editor(AppState* state);
bool ui_handle_file_drop(AppState* state);

#endif