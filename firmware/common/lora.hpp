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

#ifndef __LORA_HPP__
#define __LORA_HPP__

#include <cstdint>
#include <cstddef>
#include <array>

namespace lora {

// LoRa Regional Frequencies (MHz)
constexpr uint32_t FREQ_US_915 = 915000000;
constexpr uint32_t FREQ_EU_868 = 868000000;
constexpr uint32_t FREQ_AS_433 = 433000000;
constexpr uint32_t FREQ_AS_920 = 920000000;

// LoRa Bandwidths (Hz)
enum class Bandwidth : uint8_t {
    BW_125K = 0,
    BW_250K = 1,
    BW_500K = 2,
};

constexpr uint32_t bandwidth_hz(Bandwidth bw) {
    switch (bw) {
        case Bandwidth::BW_125K:
            return 125000;
        case Bandwidth::BW_250K:
            return 250000;
        case Bandwidth::BW_500K:
            return 500000;
        default:
            return 125000;
    }
}

// Spreading Factors (SF7=fastest, SF12=longest range)
enum class SpreadingFactor : uint8_t {
    SF7 = 7,
    SF8 = 8,
    SF9 = 9,
    SF10 = 10,
    SF11 = 11,
    SF12 = 12,
};

// Samples per symbol = 2^SF
constexpr uint16_t samples_per_symbol(SpreadingFactor sf) {
    return 1 << static_cast<uint8_t>(sf);
}

// Coding Rates (FEC)
enum class CodingRate : uint8_t {
    CR_4_5 = 1,
    CR_4_6 = 2,
    CR_4_7 = 3,
    CR_4_8 = 4,
};

// Sync Words (identify network type)
constexpr uint16_t SYNC_WORD_LORAWAN_PUBLIC = 0x3444;
constexpr uint16_t SYNC_WORD_LORAWAN_PRIVATE = 0x1424;
constexpr uint16_t SYNC_WORD_MESHTASTIC = 0x2B;
constexpr uint16_t SYNC_WORD_LEGACY = 0x12;

// Preamble
constexpr uint8_t PREAMBLE_MIN_SYMBOLS = 6;
constexpr uint8_t PREAMBLE_DEFAULT_SYMBOLS = 8;
constexpr uint16_t PREAMBLE_MAX_SYMBOLS = 65535;

// LoRa Packet Header modes
enum class HeaderMode : uint8_t {
    EXPLICIT = 0,
    IMPLICIT = 1,
};

// Maximum payload sizes
constexpr size_t MAX_PAYLOAD_LENGTH = 255;

// LoRa PHY packet structure
struct __attribute__((packed)) LoRaPacketHeader {
    uint8_t payload_length;
    uint8_t coding_rate : 3;
    uint8_t crc_enabled : 1;
    uint8_t reserved : 4;
};

// Decoded LoRa packet
struct LoRaPacket {
    uint32_t frequency;
    SpreadingFactor sf;
    Bandwidth bw;
    int16_t rssi;
    int8_t snr;
    uint32_t timestamp;
    uint16_t sync_word;
    HeaderMode header_mode;
    uint8_t payload_length;
    CodingRate coding_rate;
    bool crc_valid;
    std::array<uint8_t, MAX_PAYLOAD_LENGTH> payload;

    bool is_meshtastic() const {
        return (sync_word & 0xFF) == SYNC_WORD_MESHTASTIC;
    }

    bool is_lorawan() const {
        return sync_word == SYNC_WORD_LORAWAN_PUBLIC ||
               sync_word == SYNC_WORD_LORAWAN_PRIVATE;
    }
};

// Receiver state machine
enum class RxState : uint8_t {
    IDLE,
    PREAMBLE_DETECT,
    SYNC_DETECT,
    HEADER_DECODE,
    PAYLOAD_DECODE,
    CRC_CHECK,
    PACKET_READY,
};

// LoRa configuration for baseband processor
struct LoRaConfig {
    uint32_t frequency;
    SpreadingFactor sf;
    Bandwidth bw;
    CodingRate cr;
    uint16_t sync_word;
    HeaderMode header_mode;
    uint8_t preamble_length;
    bool crc_enabled;
    bool low_data_rate_optimize;
};

// Gray coding lookup (used in LoRa symbol encoding)
inline uint16_t gray_encode(uint16_t value) {
    return value ^ (value >> 1);
}

inline uint16_t gray_decode(uint16_t gray) {
    gray ^= (gray >> 8);
    gray ^= (gray >> 4);
    gray ^= (gray >> 2);
    gray ^= (gray >> 1);
    return gray;
}

// Calculate symbol duration in microseconds
inline uint32_t symbol_duration_us(SpreadingFactor sf, Bandwidth bw) {
    return (1000000ULL * samples_per_symbol(sf)) / bandwidth_hz(bw);
}

// Calculate air time for a packet (approximate, in milliseconds)
inline uint32_t calculate_airtime_ms(
    SpreadingFactor sf,
    Bandwidth bw,
    uint8_t preamble_len,
    uint8_t payload_len,
    CodingRate cr,
    bool explicit_header,
    bool crc_enabled) {
    uint32_t t_sym = symbol_duration_us(sf, bw);
    uint32_t t_preamble = (preamble_len + 4) * t_sym + t_sym / 4;

    uint8_t sf_val = static_cast<uint8_t>(sf);
    int payload_symbols = 8;
    int bits = 8 * payload_len - 4 * sf_val + 28;
    if (!crc_enabled) bits -= 16;
    if (explicit_header) bits += 20;
    if (bits > 0) {
        payload_symbols += ((bits + 4 * (sf_val - 2)) / (4 * (sf_val - 2))) *
                           (static_cast<uint8_t>(cr) + 4);
    }

    uint32_t t_payload = payload_symbols * t_sym;
    return (t_preamble + t_payload) / 1000;
}

}  // namespace lora

#endif /* __LORA_HPP__ */
