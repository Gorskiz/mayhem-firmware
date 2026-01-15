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

#ifndef __MESHCORE_HPP__
#define __MESHCORE_HPP__

#include <cstdint>
#include <cstddef>
#include <array>

namespace meshcore {

// MeshCore LoRa Settings
// MeshCore uses similar frequencies to Meshtastic but different packet format
constexpr uint32_t DEFAULT_FREQUENCY_US = 906000000;
constexpr uint32_t DEFAULT_FREQUENCY_EU = 869525000;
constexpr uint8_t DEFAULT_SF = 10;
constexpr uint32_t DEFAULT_BW = 250000;

// MeshCore Sync Word (different from Meshtastic)
constexpr uint8_t SYNC_WORD = 0xAB;

// Maximum sizes
constexpr size_t MAX_PAYLOAD_LENGTH = 200;
constexpr size_t MAX_NAME_LENGTH = 32;

// MeshCore Message Types
enum class MessageType : uint8_t {
    TEXT = 0x01,           // Plain text chat message
    POSITION = 0x02,       // GPS position update
    STATUS = 0x03,         // Node status/heartbeat
    ACK = 0x04,            // Acknowledgment
    PING = 0x05,           // Ping request
    PONG = 0x06,           // Ping response
    ROUTING = 0x07,        // Routing table update
    CHANNEL_INFO = 0x08,   // Channel configuration
    FIRMWARE_UPDATE = 0x09,
    COMMAND = 0x0A,        // Remote command
    SENSOR_DATA = 0x0B,    // Sensor telemetry
    BROADCAST = 0x0C,      // Broadcast announcement
    DIRECT_MSG = 0x0D,     // Direct message
    GROUP_MSG = 0x0E,      // Group message
    FILE_TRANSFER = 0x0F,  // File chunk transfer
};

inline const char* message_type_to_string(MessageType type) {
    switch (type) {
        case MessageType::TEXT:
            return "TEXT";
        case MessageType::POSITION:
            return "POS";
        case MessageType::STATUS:
            return "STATUS";
        case MessageType::ACK:
            return "ACK";
        case MessageType::PING:
            return "PING";
        case MessageType::PONG:
            return "PONG";
        case MessageType::ROUTING:
            return "ROUTE";
        case MessageType::SENSOR_DATA:
            return "SENSOR";
        case MessageType::BROADCAST:
            return "BCAST";
        case MessageType::DIRECT_MSG:
            return "DM";
        case MessageType::GROUP_MSG:
            return "GROUP";
        default:
            return "UNK";
    }
}

// MeshCore Packet Header
// Simpler than Meshtastic's protobuf-based format
struct __attribute__((packed)) PacketHeader {
    uint8_t version : 4;     // Protocol version
    uint8_t flags : 4;       // Flags (encrypted, ack_req, etc.)
    uint32_t src_addr;       // Source node address
    uint32_t dst_addr;       // Destination (0xFFFFFFFF = broadcast)
    uint16_t packet_id;      // Unique packet ID
    uint8_t hop_count : 4;   // Current hop count
    uint8_t hop_limit : 4;   // Maximum hops
    uint8_t msg_type;        // MessageType enum
    uint8_t payload_len;     // Payload length
    // Followed by payload bytes
};

// Decoded MeshCore packet
struct CorePacket {
    // Header fields
    uint8_t version;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t packet_id;
    uint8_t hop_count;
    uint8_t hop_limit;
    MessageType msg_type;
    bool encrypted;
    bool ack_requested;

    // Payload
    std::array<uint8_t, MAX_PAYLOAD_LENGTH> payload;
    size_t payload_length;

    // Decoded content based on message type
    struct TextMessage {
        std::array<char, MAX_PAYLOAD_LENGTH> text;
        size_t length;
    } text;

    struct Position {
        float latitude;
        float longitude;
        int32_t altitude;     // meters
        uint8_t accuracy;     // meters
        uint16_t heading;     // degrees * 10
        uint16_t speed;       // cm/s
        uint32_t timestamp;
    } position;

    struct Status {
        uint8_t battery_percent;
        float voltage;
        int8_t temperature;   // Celsius
        uint32_t uptime;      // seconds
        uint8_t tx_power;     // dBm
        uint16_t rx_packets;
        uint16_t tx_packets;
    } status;

    struct SensorData {
        uint8_t sensor_type;
        float value;
        std::array<char, 16> unit;
    } sensor;

    // Reception metadata
    int16_t rssi;
    int8_t snr;
    uint32_t rx_time;
    uint32_t frequency;

    // Utility
    bool is_broadcast() const { return dst_addr == 0xFFFFFFFF; }
};

// Node information (learned from network)
struct NodeInfo {
    uint32_t address;
    std::array<char, MAX_NAME_LENGTH> name;
    uint32_t last_seen;       // Unix timestamp
    int16_t last_rssi;
    int8_t last_snr;
    uint8_t hop_distance;
    bool is_gateway;

    struct LastPosition {
        float latitude;
        float longitude;
        int32_t altitude;
        uint32_t time;
        bool valid;
    } position;
};

// MeshCore decoder
class MeshCoreDecoder {
   public:
    // Decode a LoRa payload as MeshCore packet
    bool decode(const uint8_t* data, size_t length, CorePacket& packet);

    // Decode specific message types
    bool decode_text(const uint8_t* data, size_t length, CorePacket::TextMessage& text);
    bool decode_position(const uint8_t* data, size_t length, CorePacket::Position& pos);
    bool decode_status(const uint8_t* data, size_t length, CorePacket::Status& status);
    bool decode_sensor(const uint8_t* data, size_t length, CorePacket::SensorData& sensor);

    // Format node address as hex string
    static void format_address(uint32_t addr, char* buffer, size_t buflen);

    // Check if packet is valid MeshCore format
    static bool is_meshcore_packet(const uint8_t* data, size_t length);

   private:
    // CRC-16 validation
    uint16_t calculate_crc16(const uint8_t* data, size_t length);
};

// Distinguish between Meshtastic and MeshCore
enum class MeshProtocol : uint8_t {
    UNKNOWN = 0,
    MESHTASTIC = 1,
    MESHCORE = 2,
    LORAWAN = 3,
    CUSTOM = 4,
};

// Auto-detect mesh protocol from packet
inline MeshProtocol detect_mesh_protocol(const uint8_t* data, size_t length, uint16_t sync_word) {
    if (length < 4) return MeshProtocol::UNKNOWN;

    // Check sync word first
    if ((sync_word & 0xFF) == 0x2B) {
        return MeshProtocol::MESHTASTIC;
    }

    if ((sync_word & 0xFF) == SYNC_WORD) {
        return MeshProtocol::MESHCORE;
    }

    if (sync_word == 0x3444 || sync_word == 0x1424) {
        return MeshProtocol::LORAWAN;
    }

    // Try to detect by header structure
    // MeshCore has specific version nibble
    if ((data[0] & 0x0F) <= 0x02) {  // Version 0, 1, or 2
        if (MeshCoreDecoder::is_meshcore_packet(data, length)) {
            return MeshProtocol::MESHCORE;
        }
    }

    return MeshProtocol::UNKNOWN;
}

inline const char* protocol_to_string(MeshProtocol proto) {
    switch (proto) {
        case MeshProtocol::MESHTASTIC:
            return "Meshtastic";
        case MeshProtocol::MESHCORE:
            return "MeshCore";
        case MeshProtocol::LORAWAN:
            return "LoRaWAN";
        case MeshProtocol::CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

}  // namespace meshcore

#endif /* __MESHCORE_HPP__ */
