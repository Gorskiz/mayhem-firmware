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

#include "ui_sd_format.hpp"

using namespace ui;

namespace ui::external_app::sd_format {

Thread* FormatSDView::thread{nullptr};

FormatSDView::FormatSDView(NavigationView& nav)
    : nav_(nav) {
    add_children({&labels,
                  &text_info,
                  &progress,
                  &button_format,
                  &button_cancel});

    button_format.on_select = [this](Button&) {
        if (!format_in_progress) {
            nav_.push<ModalMessageView>(
                "Warning!",
                "Format SD card to FAT32?\nALL DATA WILL BE LOST!",
                YESNO,
                [this](bool choice) {
                    if (choice) {
                        confirmed = true;
                        start_format();
                    }
                });
        }
    };

    button_cancel.on_select = [this](Button&) {
        if (!format_in_progress) {
            nav_.pop();
        }
    };
}

FormatSDView::~FormatSDView() {
    if (thread)
        chThdTerminate(thread);
}

void FormatSDView::focus() {
    button_cancel.focus();
}

void FormatSDView::start_format() {
    if (format_in_progress)
        return;

    BlockDeviceInfo block_device_info;
    if (sdcGetInfo(&SDCD1, &block_device_info) != CH_SUCCESS) {
        text_info.set("SD card error!");
        return;
    }

    format_in_progress = true;
    button_format.set_focusable(false);
    button_cancel.set_focusable(false);
    text_info.set("Formatting...");
    progress.set_max(100);
    progress.set_value(10);

    thread = chThdCreateFromHeap(NULL, 4096, NORMALPRIO, FormatSDView::static_fn, this);
}

void FormatSDView::run() {
    // Work buffer for f_mkfs - needs to be at least 512 bytes
    auto work_buffer = std::make_unique<std::array<uint8_t, 4096>>();
    
    // Unmount before formatting
    f_mount(nullptr, reinterpret_cast<const TCHAR*>(_T("")), 0);
    
    progress.set_value(30);
    
    // Format as FAT32 with default cluster size
    // FM_FAT32 = 0x02, with automatic cluster size (0)
    FRESULT result = f_mkfs(
        reinterpret_cast<const TCHAR*>(_T("")),  // Path (root)
        FM_FAT32,                                  // FAT32 format
        0,                                         // Auto cluster size
        work_buffer->data(),                       // Work buffer
        work_buffer->size()                        // Work buffer size
    );
    
    progress.set_value(80);
    
    // Remount the SD card
    FRESULT mount_result = f_mount(&sd_card::fs, reinterpret_cast<const TCHAR*>(_T("")), 1);
    
    progress.set_value(100);
    format_in_progress = false;
    
    if (result == FR_OK && mount_result == FR_OK) {
        text_info.set("Format complete!");
    } else if (result != FR_OK) {
        text_info.set("Format failed!");
    } else {
        text_info.set("Mount failed!");
    }
    
    button_cancel.set_focusable(true);
    button_cancel.focus();
}

}  // namespace ui::external_app::sd_format
