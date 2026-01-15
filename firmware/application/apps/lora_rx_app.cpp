/*
 * Copyright (C) 2024 PortaPack Mayhem Contributors
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

#include "lora_rx_app.hpp"

#include "audio.hpp"
#include "baseband_api.hpp"
#include "portapack.hpp"
#include "rtc_time.hpp"
#include "string_format.hpp"

using namespace portapack;

namespace ui {

static std::string format_node_id(uint32_t id) {
    if (id == 0xFFFFFFFF) {
        return "BCAST";
    }
    return "!" + to_string_hex(id, 8);
}

void LoRaPacketEntry::update_summary(const uint8_t* payload, size_t length, meshcore::MeshProtocol proto) {
    if (length == 0) {
        std::strncpy(summary.data(), "[empty]", summary.size() - 1);
        return;
    }

    if (proto == meshcore::MeshProtocol::MESHTASTIC) {
        if (length >= 4) {
            bool is_printable = true;
            size_t check_len = std::min(length, size_t(16));
            for (size_t i = 0; i < check_len; i++) {
                if (payload[i] < 0x20 || payload[i] > 0x7E) {
                    is_printable = false;
                    break;
                }
            }
            if (is_printable) {
                size_t copy_len = std::min(length, summary.size() - 1);
                std::memcpy(summary.data(), payload, copy_len);
                summary[copy_len] = '\0';
                return;
            }
        }
        std::strncpy(summary.data(), "[Meshtastic]", summary.size() - 1);
    } else if (proto == meshcore::MeshProtocol::MESHCORE) {
        std::strncpy(summary.data(), "[MeshCore]", summary.size() - 1);
    } else {
        std::string hex_preview;
        for (size_t i = 0; i < std::min(length, size_t(8)); i++) {
            hex_preview += to_string_hex(payload[i], 2);
        }
        if (length > 8) {
            hex_preview += "...";
        }
        std::strncpy(summary.data(), hex_preview.c_str(), summary.size() - 1);
    }
    summary[summary.size() - 1] = '\0';
}

LoRaRxView::LoRaRxView(NavigationView& nav)
    : nav_(nav) {
    add_children({
        &labels_,
        &field_frequency_,
        &field_sf_,
        &field_bw_,
        &field_region_,
        &rssi_,
        &text_stats_,
        &check_log_,
    });

    recent_view_.set_parent_rect({0, 3 * 16, screen_width, 12 * 16});
    add_child(&recent_view_);

    field_frequency_.set_value(receiver_model.target_frequency());
    field_frequency_.on_change = [this](rf::Frequency f) {
        this->on_frequency_changed(f);
    };
    field_frequency_.set_step(25000);

    field_sf_.set_by_value(selected_sf_);
    field_sf_.on_change = [this](size_t idx, int32_t val) {
        this->on_sf_changed(idx, val);
    };

    field_bw_.set_by_value(selected_bw_);
    field_bw_.on_change = [this](size_t idx, int32_t val) {
        this->on_bw_changed(idx, val);
    };

    field_region_.set_by_value(selected_region_);
    field_region_.on_change = [this](size_t idx, int32_t val) {
        this->on_region_changed(idx, val);
    };

    check_log_.set_value(logging_enabled_);
    check_log_.on_select = [this](Checkbox&, bool v) {
        logging_enabled_ = v;
    };

    if (logging_enabled_) {
        logger_ = std::make_unique<LogFile>();
        logger_->open_for_append("LOGS/LORA.TXT");
    }

    configure_receiver();
}

LoRaRxView::~LoRaRxView() {
    audio::output::stop();
    receiver_model.disable();
    baseband::shutdown();
}

void LoRaRxView::focus() {
    field_frequency_.focus();
}

void LoRaRxView::configure_receiver() {
    receiver_model.set_target_frequency(field_frequency_.value());
    receiver_model.set_sampling_rate(4000000);
    receiver_model.set_baseband_bandwidth(1750000);
    receiver_model.enable();

    LoRaRxConfigureMessage message{
        static_cast<uint32_t>(field_frequency_.value()),
        static_cast<uint8_t>(field_sf_.selected_index_value()),
        static_cast<uint8_t>(field_bw_.selected_index_value()),
        1,
        0x2B,
        false};
    shared_memory.baseband_queue.push(message);
}

void LoRaRxView::on_frequency_changed(rf::Frequency f) {
    receiver_model.set_target_frequency(f);
    configure_receiver();
}

void LoRaRxView::on_sf_changed(size_t, int32_t value) {
    selected_sf_ = static_cast<uint8_t>(value);
    configure_receiver();
}

void LoRaRxView::on_bw_changed(size_t, int32_t value) {
    selected_bw_ = static_cast<uint8_t>(value);
    configure_receiver();
}

void LoRaRxView::on_region_changed(size_t, int32_t value) {
    selected_region_ = static_cast<uint8_t>(value);

    rf::Frequency new_freq = 906875000;
    switch (value) {
        case 1:
            new_freq = 906875000;
            break;
        case 2:
            new_freq = 433175000;
            break;
        case 3:
            new_freq = 869462500;
            break;
        case 5:
            new_freq = 920000000;
            break;
        case 6:
            new_freq = 916800000;
            break;
    }
    field_frequency_.set_value(new_freq);
}

void LoRaRxView::on_packet(const LoRaPacketMessage* message) {
    if (!message) return;

    packets_received_++;

    LoRaPacketEntry entry;
    entry.timestamp = rtc_time::now().value();
    entry.sf = static_cast<lora::SpreadingFactor>(message->spreading_factor);
    entry.rssi = message->rssi;
    entry.snr = message->snr;
    entry.payload_length = message->payload_length;

    entry.protocol = meshcore::MeshProtocol::UNKNOWN;
    if ((message->sync_word & 0xFF) == 0x2B) {
        entry.protocol = meshcore::MeshProtocol::MESHTASTIC;
        meshtastic_count_++;
    } else if ((message->sync_word & 0xFF) == 0xAB) {
        entry.protocol = meshcore::MeshProtocol::MESHCORE;
        meshcore_count_++;
    } else {
        other_count_++;
    }

    if (message->payload_length >= 8) {
        entry.from_node = (message->payload[0] << 24) |
                          (message->payload[1] << 16) |
                          (message->payload[2] << 8) |
                          message->payload[3];
    } else {
        entry.from_node = 0;
    }

    entry.update_summary(message->payload.data(), message->payload_length, entry.protocol);
    recent_packets_.on_packet(entry);

    if (logging_enabled_ && logger_) {
        log_packet(entry, message->payload.data(), message->payload_length);
    }

    update_stats();
}

void LoRaRxView::log_packet(const LoRaPacketEntry& entry, const uint8_t* payload, size_t length) {
    if (!logger_) return;

    std::string log_line;

    auto now = rtc_time::now();
    log_line += to_string_datetime(now);
    log_line += ",";
    log_line += meshcore::protocol_to_string(entry.protocol);
    log_line += ",";
    log_line += format_node_id(entry.from_node);
    log_line += ",";
    log_line += "SF" + to_string_dec_uint(static_cast<uint8_t>(entry.sf));
    log_line += ",";
    log_line += to_string_dec_int(entry.rssi) + "dBm,";
    log_line += to_string_dec_int(entry.snr) + "dB,";

    for (size_t i = 0; i < length; i++) {
        log_line += to_string_hex(payload[i], 2);
    }

    logger_->write_entry(log_line);
}

void LoRaRxView::update_stats() {
    std::string stats = "Pkts:" + to_string_dec_uint(packets_received_);
    stats += " M:" + to_string_dec_uint(meshtastic_count_);
    stats += " C:" + to_string_dec_uint(meshcore_count_);
    text_stats_.set(stats);
}

LoRaPacketDetailView::LoRaPacketDetailView(NavigationView& nav, const LoRaPacketEntry& entry)
    : entry_(entry) {
    add_children({
        &labels_,
        &text_from_,
        &text_protocol_,
        &text_sf_bw_,
        &text_rssi_,
        &text_length_,
        &text_payload_1_,
        &text_payload_2_,
        &text_payload_3_,
        &text_payload_4_,
        &button_close_,
    });

    text_from_.set(format_node_id(entry.from_node));
    text_protocol_.set(meshcore::protocol_to_string(entry.protocol));
    text_sf_bw_.set("SF" + to_string_dec_uint(static_cast<uint8_t>(entry.sf)));
    text_rssi_.set(to_string_dec_int(entry.rssi) + "dBm / " + to_string_dec_int(entry.snr) + "dB");
    text_length_.set(to_string_dec_uint(entry.payload_length) + " bytes");

    button_close_.on_select = [&nav](Button&) {
        nav.pop();
    };
}

void LoRaPacketDetailView::focus() {
    button_close_.focus();
}

MeshChatView::MeshChatView(NavigationView& nav)
    : nav_(nav) {
    add_children({
        &text_msg_1_,
        &text_msg_2_,
        &text_msg_3_,
        &text_msg_4_,
        &text_msg_5_,
        &text_msg_6_,
        &text_msg_7_,
        &text_msg_8_,
        &text_msg_9_,
        &text_msg_10_,
        &button_back_,
        &button_clear_,
    });

    button_back_.on_select = [&nav](Button&) {
        nav.pop();
    };

    button_clear_.on_select = [this](Button&) {
        message_count_ = 0;
        scroll_offset_ = 0;
        refresh_display();
    };

    refresh_display();
}

MeshChatView::~MeshChatView() {
}

void MeshChatView::focus() {
    button_back_.focus();
}

void MeshChatView::add_message(uint32_t from, const char* text, meshcore::MeshProtocol proto) {
    if (message_count_ >= MAX_MESSAGES) {
        for (size_t i = 0; i < MAX_MESSAGES - 1; i++) {
            messages_[i] = messages_[i + 1];
        }
        message_count_ = MAX_MESSAGES - 1;
    }

    auto& msg = messages_[message_count_++];
    msg.from_node = from;
    msg.timestamp = rtc_time::now().value();
    msg.protocol = proto;
    std::strncpy(msg.text.data(), text, msg.text.size() - 1);
    msg.text[msg.text.size() - 1] = '\0';

    refresh_display();
}

void MeshChatView::refresh_display() {
    Text* lines[] = {
        &text_msg_1_,
        &text_msg_2_,
        &text_msg_3_,
        &text_msg_4_,
        &text_msg_5_,
        &text_msg_6_,
        &text_msg_7_,
        &text_msg_8_,
        &text_msg_9_,
        &text_msg_10_};

    for (size_t i = 0; i < 10; i++) {
        size_t msg_idx = scroll_offset_ + i;
        if (msg_idx < message_count_) {
            auto& msg = messages_[msg_idx];
            std::string line = format_node_id(msg.from_node).substr(0, 6) + ": ";
            line += std::string(msg.text.data()).substr(0, 22);
            lines[i]->set(line);
        } else {
            lines[i]->set("");
        }
    }
}

}  // namespace ui
