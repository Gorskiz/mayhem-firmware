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
    TEXT = 0x01,
    POSITION = 0x02,
    STATUS = 0x03,
    ACK = 0x04,
    PING = 0x05,
    PONG = 0x06,
    ROUTING = 0x07,
    CHANNEL_INFO = 0x08,
    FIRMWARE_UPDATE = 0x09,
    COMMAND = 0x0A,
    SENSOR_DATA = 0x0B,
    BROADCAST = 0x0C,
    DIRECT_MSG = 0x0D,
    GROUP_MSG = 0x0E,
    FILE_TRANSFER = 0x0F,
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

// MeshCore Packet Header (simpler binary format)
struct __attribute__((packed)) PacketHeader {
    uint8_t version : 4;
    uint8_t flags : 4;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t packet_id;
    uint8_t hop_count : 4;
    uint8_t hop_limit : 4;
    uint8_t msg_type;
    uint8_t payload_len;
};

// Decoded MeshCore packet
struct CorePacket {
    uint8_t version;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t packet_id;
    uint8_t hop_count;
    uint8_t hop_limit;
    MessageType msg_type;
    bool encrypted;
    bool ack_requested;

    std::array<uint8_t, MAX_PAYLOAD_LENGTH> payload;
    size_t payload_length;

    struct TextMessage {
        std::array<char, MAX_PAYLOAD_LENGTH> text;
        size_t length;
    } text;

    struct Position {
        float latitude;
        float longitude;
        int32_t altitude;
        uint8_t accuracy;
        uint16_t heading;
        uint16_t speed;
        uint32_t timestamp;
    } position;

    struct Status {
        uint8_t battery_percent;
        float voltage;
        int8_t temperature;
        uint32_t uptime;
        uint8_t tx_power;
        uint16_t rx_packets;
        uint16_t tx_packets;
    } status;

    struct SensorData {
        uint8_t sensor_type;
        float value;
        std::array<char, 16> unit;
    } sensor;

    int16_t rssi;
    int8_t snr;
    uint32_t rx_time;
    uint32_t frequency;

    bool is_broadcast() const { return dst_addr == 0xFFFFFFFF; }
};

// Node information (learned from network)
struct NodeInfo {
    uint32_t address;
    std::array<char, MAX_NAME_LENGTH> name;
    uint32_t last_seen;
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
    bool decode(const uint8_t* data, size_t length, CorePacket& packet);
    bool decode_text(const uint8_t* data, size_t length, CorePacket::TextMessage& text);
    bool decode_position(const uint8_t* data, size_t length, CorePacket::Position& pos);
    bool decode_status(const uint8_t* data, size_t length, CorePacket::Status& status);
    bool decode_sensor(const uint8_t* data, size_t length, CorePacket::SensorData& sensor);
    static void format_address(uint32_t addr, char* buffer, size_t buflen);
    static bool is_meshcore_packet(const uint8_t* data, size_t length);

   private:
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

    if ((sync_word & 0xFF) == 0x2B) {
        return MeshProtocol::MESHTASTIC;
    }

    if ((sync_word & 0xFF) == SYNC_WORD) {
        return MeshProtocol::MESHCORE;
    }

    if (sync_word == 0x3444 || sync_word == 0x1424) {
        return MeshProtocol::LORAWAN;
    }

    if ((data[0] & 0x0F) <= 0x02) {
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
