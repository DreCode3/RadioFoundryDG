// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The user interface task (docs/ux/, docs/spec/ui.md)
#ifndef UI_H
#define UI_H

#include "ui_app.h"

void ui_start(void);
// The screen shown now, for the debug link's view: "home", "quick", "hub", "find", "diag", "hotspot", ... (any task)
const char *ui_screen_name(void);
// What is on screen (screen, depth, focus, banner, lock, Find query), as of the UI task's last pass (any task)
// The UI task's critical section (taskENTER_CRITICAL): another task copies what the UI task writes as it draws
// (ui.focus_label) inside it
void ui_crit_enter(void);
void ui_crit_exit(void);
void ui_get_status(ui_app_status_t *st);
// The backlight level last set, percent (any task)
uint8_t ui_backlight(void);

#endif
