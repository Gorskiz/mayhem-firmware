/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2024 PortaPack-Mayhem Contributors
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __UI_SD_FORMAT_H__
#define __UI_SD_FORMAT_H__

#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "string_format.hpp"
#include "ff.h"
#include "sd_card.hpp"

#include <cstdint>

using namespace ui;

namespace ui::external_app::sd_format {

class FormatSDView : public View {
   public:
    FormatSDView(NavigationView& nav);
    ~FormatSDView();
    void focus() override;

    std::string title() const override { return "Format SD"; };

   private:
    NavigationView& nav_;

    bool confirmed = false;
    bool format_in_progress = false;
    static Thread* thread;

    static msg_t static_fn(void* arg) {
        auto obj = static_cast<FormatSDView*>(arg);
        obj->run();
        return 0;
    }

    void run();
    void start_format();
    void update_status(const std::string& status);

    Labels labels{
        {{7 * 8, 4 * 16}, "Format SD Card", Theme::getInstance()->bg_darkest->foreground},
        {{2 * 8, 7 * 16}, "This will ERASE all data!", Theme::getInstance()->fg_red->foreground},
        {{2 * 8, 9 * 16}, "Format: FAT32", Theme::getInstance()->fg_light->foreground},
    };

    Text text_info{
        {7 * 8, 12 * 16, 16 * 8, 16},
        "Ready"};

    ProgressBar progress{
        {2 * 8, 14 * 16, 26 * 8, 24}};

    Button button_format{
        {2 * 8, 17 * 16, 12 * 8, 2 * 16},
        "Format"};

    Button button_cancel{
        {16 * 8, 17 * 16, 12 * 8, 2 * 16},
        "Cancel"};
};

}  // namespace ui::external_app::sd_format

#endif /*__UI_SD_FORMAT_H__*/
