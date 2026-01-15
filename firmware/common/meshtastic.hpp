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

#ifndef __MESHTASTIC_HPP__
#define __MESHTASTIC_HPP__

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>

namespace meshtastic {

// Meshtastic LoRa Settings (default North America)
constexpr uint32_t DEFAULT_FREQUENCY = 906875000;  // US Channel 0
constexpr uint8_t DEFAULT_SF = 11;
constexpr uint32_t DEFAULT_BW = 250000;

// Maximum message sizes
constexpr size_t MAX_MESSAGE_LENGTH = 237;
constexpr size_t MAX_NODE_ID_LENGTH = 12;

// Meshtastic Port Numbers (what type of data)
enum class PortNum : uint8_t {
    UNKNOWN = 0,
    TEXT_MESSAGE = 1,      // Plain text messages
    REMOTE_HARDWARE = 2,   // GPIO control
    POSITION = 3,          // GPS position
    NODEINFO = 4,          // Node information
    ROUTING = 5,           // Mesh routing
    ADMIN = 6,             // Admin messages
    TELEMETRY = 7,         // Device/environment telemetry
    REPLY = 8,             // ACK/Reply
    IP_TUNNEL = 9,
    PAXCOUNTER = 10,
    STORE_FORWARD = 11,
    RANGE_TEST = 12,
    DETECTION_SENSOR = 13,
    AUDIO = 14,
    TRACEROUTE = 15,
    NEIGHBORINFO = 16,
    ATAK_FORWARDER = 17,
    MAP_REPORT = 18,
    SIMULATOR = 19,
};

inline const char* port_to_string(PortNum port) {
    switch (port) {
        case PortNum::TEXT_MESSAGE:
            return "TEXT";
        case PortNum::POSITION:
            return "POS";
        case PortNum::NODEINFO:
            return "NODE";
        case PortNum::TELEMETRY:
            return "TELEM";
        case PortNum::ROUTING:
            return "ROUTE";
        case PortNum::TRACEROUTE:
            return "TRACE";
        case PortNum::NEIGHBORINFO:
            return "NEIGH";
        default:
            return "UNK";
    }
}

// Meshtastic Packet Header Structure
// Note: Meshtastic uses Protocol Buffers, but we parse the key fields
struct __attribute__((packed)) MeshPacketHeader {
    uint32_t from;       // Sender node ID
    uint32_t to;         // Destination (0xFFFFFFFF = broadcast)
    uint32_t id;         // Packet ID
    uint8_t flags;       // Flags including hop count
    uint8_t channel;     // Channel hash
    // Followed by encrypted/protobuf payload
};

// Extracted fields from a Meshtastic packet
struct MeshPacket {
    // Header fields
    uint32_t from_node;
    uint32_t to_node;
    uint32_t packet_id;
    uint8_t hop_limit;
    uint8_t hop_start;
    uint8_t channel_hash;
    bool want_ack;
    bool via_mqtt;

    // Payload info
    PortNum port;
    std::array<uint8_t, MAX_MESSAGE_LENGTH> payload;
    size_t payload_length;

    // Decoded content (depending on port)
    struct TextMessage {
        std::array<char, MAX_MESSAGE_LENGTH> text;
        size_t length;
    } text;

    struct Position {
        int32_t latitude_i;   // degrees * 1e7
        int32_t longitude_i;  // degrees * 1e7
        int32_t altitude;     // meters
        uint32_t time;        // Unix timestamp
        uint8_t precision_bits;

        float latitude() const { return latitude_i / 1e7f; }
        float longitude() const { return longitude_i / 1e7f; }
    } position;

    struct NodeInfo {
        std::array<char, MAX_NODE_ID_LENGTH> user_id;
        std::array<char, 40> long_name;
        std::array<char, 8> short_name;
        uint8_t hw_model;
    } node_info;

    struct Telemetry {
        uint32_t time;
        float battery_level;      // 0-100%
        float voltage;            // Volts
        float temperature;        // Celsius
        float humidity;           // 0-100%
        float pressure;           // hPa
    } telemetry;

    // Reception metadata
    int16_t rssi;
    int8_t snr;
    uint32_t rx_time;

    // Utility
    bool is_broadcast() const { return to_node == 0xFFFFFFFF; }
    bool is_encrypted() const { return (payload_length > 0) && (port == PortNum::UNKNOWN); }
};

// Channel presets (frequency plans)
struct ChannelPreset {
    const char* name;
    uint32_t frequency;
    uint8_t spreading_factor;
    uint32_t bandwidth;
    uint8_t coding_rate;
};

// US Frequency Hopping Channels (Meshtastic default)
constexpr std::array<uint32_t, 8> US_CHANNELS = {
    906875000,  // Channel 0 (default)
    908125000,  // Channel 1
    909375000,  // Channel 2
    910625000,  // Channel 3
    911875000,  // Channel 4
    913125000,  // Channel 5
    914375000,  // Channel 6
    915625000,  // Channel 7
};

// EU Channels
constexpr std::array<uint32_t, 4> EU_CHANNELS = {
    869462500,  // Channel 0
    869587500,
    869712500,
    869837500,
};

// Meshtastic Region Presets
enum class Region : uint8_t {
    UNSET = 0,
    US = 1,
    EU_433 = 2,
    EU_868 = 3,
    CN = 4,
    JP = 5,
    ANZ = 6,
    KR = 7,
    TW = 8,
    RU = 9,
    IN = 10,
    NZ_865 = 11,
    TH = 12,
    LORA_24 = 13,
    UA_433 = 14,
    UA_868 = 15,
    MY_433 = 16,
    MY_919 = 17,
    SG_923 = 18,
};

// Simple decoder for Meshtastic packets
class MeshtasticDecoder {
   public:
    // Attempt to decode a LoRa payload as Meshtastic
    // Returns true if successful
    bool decode(const uint8_t* data, size_t length, MeshPacket& packet);

    // Decode text message from payload
    bool decode_text(const uint8_t* data, size_t length, MeshPacket::TextMessage& text);

    // Decode position from payload
    bool decode_position(const uint8_t* data, size_t length, MeshPacket::Position& pos);

    // Decode node info from payload
    bool decode_nodeinfo(const uint8_t* data, size_t length, MeshPacket::NodeInfo& info);

    // Decode telemetry from payload
    bool decode_telemetry(const uint8_t* data, size_t length, MeshPacket::Telemetry& telem);

    // Format node ID as hex string (e.g., "!a1b2c3d4")
    static void format_node_id(uint32_t id, char* buffer, size_t buflen);

   private:
    // Simple protobuf varint decoder
    uint64_t decode_varint(const uint8_t*& data, const uint8_t* end);

    // Skip unknown protobuf field
    void skip_field(const uint8_t*& data, const uint8_t* end, uint8_t wire_type);
};

}  // namespace meshtastic

#endif /* __MESHTASTIC_HPP__ */
