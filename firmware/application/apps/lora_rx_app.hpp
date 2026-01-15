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

#ifndef __LORA_RX_APP_HPP__
#define __LORA_RX_APP_HPP__

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_freq_field.hpp"
#include "ui_rssi.hpp"
#include "ui_record_view.hpp"

#include "app_settings.hpp"
#include "radio_state.hpp"
#include "log_file.hpp"
#include "recent_entries.hpp"

#include "lora.hpp"
#include "meshtastic.hpp"
#include "meshcore.hpp"

namespace ui {

// Recent packet entry for display
struct LoRaPacketEntry {
    using Key = uint32_t;

    uint32_t timestamp;
    uint32_t from_node;
    meshcore::MeshProtocol protocol;
    lora::SpreadingFactor sf;
    int16_t rssi;
    int8_t snr;
    uint8_t payload_length;
    std::array<char, 64> summary;

    Key key() const { return timestamp; }

    void update_summary(const uint8_t* payload, size_t length, meshcore::MeshProtocol proto);
};

using LoRaRecentEntries = RecentEntries<LoRaPacketEntry>;

class LoRaRxView : public View {
   public:
    LoRaRxView(NavigationView& nav);
    ~LoRaRxView();

    void focus() override;
    std::string title() const override { return "LoRa RX"; }

   private:
    NavigationView& nav_;

    // Radio configuration
    RxRadioState radio_state_{
        906875000,
        500000,
        4000000,
        ReceiverModel::Mode::WidebandFMAudio};

    // App settings
    bool logging_enabled_{true};
    uint8_t selected_sf_{10};
    uint8_t selected_bw_{1};
    uint8_t selected_region_{1};

    app_settings::SettingsManager settings_{
        "rx_lora",
        app_settings::Mode::RX,
        {
            {"logging"sv, &logging_enabled_},
            {"sf"sv, &selected_sf_},
            {"bw"sv, &selected_bw_},
            {"region"sv, &selected_region_},
        }};

    // Logger
    std::unique_ptr<LogFile> logger_{};

    // Recent entries
    LoRaRecentEntries recent_packets_{};
    RecentEntriesColumns columns_{
        {{"Time", 5},
         {"From", 8},
         {"Proto", 5},
         {"RSSI", 4},
         {"Info", 8}}};
    RecentEntriesView<LoRaRecentEntries> recent_view_{columns_, recent_packets_};

    // Packet statistics
    uint32_t packets_received_{0};
    uint32_t meshtastic_count_{0};
    uint32_t meshcore_count_{0};
    uint32_t other_count_{0};

    // Event handlers
    void on_packet(const LoRaPacketMessage* message);
    void on_frequency_changed(rf::Frequency f);
    void on_sf_changed(size_t index, int32_t value);
    void on_bw_changed(size_t index, int32_t value);
    void on_region_changed(size_t index, int32_t value);

    void configure_receiver();
    void log_packet(const LoRaPacketEntry& entry, const uint8_t* payload, size_t length);
    void update_stats();

    // UI Elements
    Labels labels_{
        {{0 * 8, 0 * 16}, "Freq:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 1 * 16}, "SF:", Theme::getInstance()->fg_light->foreground},
        {{8 * 8, 1 * 16}, "BW:", Theme::getInstance()->fg_light->foreground},
        {{16 * 8, 1 * 16}, "Region:", Theme::getInstance()->fg_light->foreground},
    };

    RxFrequencyField field_frequency_{
        {5 * 8, 0 * 16},
        nav_};

    OptionsField field_sf_{
        {3 * 8, 1 * 16},
        4,
        {{"SF7", 7},
         {"SF8", 8},
         {"SF9", 9},
         {"SF10", 10},
         {"SF11", 11},
         {"SF12", 12}}};

    OptionsField field_bw_{
        {11 * 8, 1 * 16},
        4,
        {{"125k", 0},
         {"250k", 1},
         {"500k", 2}}};

    OptionsField field_region_{
        {23 * 8, 1 * 16},
        4,
        {{"US", 1},
         {"EU868", 3},
         {"EU433", 2},
         {"AU", 6},
         {"JP", 5}}};

    RSSI rssi_{
        {0 * 8, 2 * 16 + 4, 10 * 8, 8}};

    Text text_stats_{
        {11 * 8, 2 * 16, 19 * 8, 16},
        "Pkts: 0  M: 0  C: 0"};

    Checkbox check_log_{
        {22 * 8, 2 * 16},
        3,
        "Log",
        true};

    // Message handlers
    MessageHandlerRegistration message_handler_packet_{
        Message::ID::LoRaPacket,
        [this](Message* const p) {
            this->on_packet(static_cast<const LoRaPacketMessage*>(p));
        }};
};

// Detailed packet view
class LoRaPacketDetailView : public View {
   public:
    LoRaPacketDetailView(NavigationView& nav, const LoRaPacketEntry& entry);

    void focus() override;
    std::string title() const override { return "Packet Detail"; }

   private:
    const LoRaPacketEntry& entry_;

    Labels labels_{
        {{0 * 8, 0 * 16}, "From:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 1 * 16}, "Protocol:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 2 * 16}, "SF/BW:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 3 * 16}, "RSSI/SNR:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 4 * 16}, "Length:", Theme::getInstance()->fg_light->foreground},
        {{0 * 8, 6 * 16}, "Payload:", Theme::getInstance()->fg_light->foreground},
    };

    Text text_from_{{6 * 8, 0 * 16, 24 * 8, 16}, ""};
    Text text_protocol_{{10 * 8, 1 * 16, 20 * 8, 16}, ""};
    Text text_sf_bw_{{10 * 8, 2 * 16, 20 * 8, 16}, ""};
    Text text_rssi_{{10 * 8, 3 * 16, 20 * 8, 16}, ""};
    Text text_length_{{10 * 8, 4 * 16, 20 * 8, 16}, ""};

    Text text_payload_1_{{0 * 8, 7 * 16, 30 * 8, 16}, ""};
    Text text_payload_2_{{0 * 8, 8 * 16, 30 * 8, 16}, ""};
    Text text_payload_3_{{0 * 8, 9 * 16, 30 * 8, 16}, ""};
    Text text_payload_4_{{0 * 8, 10 * 16, 30 * 8, 16}, ""};

    Button button_close_{
        {10 * 8, 15 * 16, 10 * 8, 24},
        "Back"};
};

// Mesh chat view (for text messages)
class MeshChatView : public View {
   public:
    MeshChatView(NavigationView& nav);
    ~MeshChatView();

    void focus() override;
    std::string title() const override { return "Mesh Chat"; }

    void add_message(uint32_t from, const char* text, meshcore::MeshProtocol proto);

   private:
    NavigationView& nav_;

    static constexpr size_t MAX_MESSAGES = 20;

    struct ChatMessage {
        uint32_t from_node;
        uint32_t timestamp;
        meshcore::MeshProtocol protocol;
        std::array<char, 128> text;
    };

    std::array<ChatMessage, MAX_MESSAGES> messages_{};
    size_t message_count_{0};
    size_t scroll_offset_{0};

    void refresh_display();

    Text text_msg_1_{{0, 0 * 16, 30 * 8, 16}, ""};
    Text text_msg_2_{{0, 1 * 16, 30 * 8, 16}, ""};
    Text text_msg_3_{{0, 2 * 16, 30 * 8, 16}, ""};
    Text text_msg_4_{{0, 3 * 16, 30 * 8, 16}, ""};
    Text text_msg_5_{{0, 4 * 16, 30 * 8, 16}, ""};
    Text text_msg_6_{{0, 5 * 16, 30 * 8, 16}, ""};
    Text text_msg_7_{{0, 6 * 16, 30 * 8, 16}, ""};
    Text text_msg_8_{{0, 7 * 16, 30 * 8, 16}, ""};
    Text text_msg_9_{{0, 8 * 16, 30 * 8, 16}, ""};
    Text text_msg_10_{{0, 9 * 16, 30 * 8, 16}, ""};

    Button button_back_{
        {0 * 8, 15 * 16, 8 * 8, 24},
        "Back"};

    Button button_clear_{
        {22 * 8, 15 * 16, 8 * 8, 24},
        "Clear"};

    MessageHandlerRegistration message_handler_packet_{
        Message::ID::LoRaPacket,
        [this](Message* const p) {
            (void)p;
        }};
};

}  // namespace ui

#endif /* __LORA_RX_APP_HPP__ */
