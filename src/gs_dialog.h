/* Native file dialogs. Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <string>

bool gs_dialog_available();
bool gs_dialog_open(const char *title, const char *windows_filter, std::string &out);
bool gs_dialog_save(const char *title, const char *suggested, std::string &out);
