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

#ifndef __PROC_LORARX_HPP__
#define __PROC_LORARX_HPP__

#include "baseband_processor.hpp"
#include "baseband_thread.hpp"
#include "rssi_thread.hpp"
#include "dsp_decimate.hpp"
#include "spectrum_collector.hpp"
#include "channel_decimator.hpp"

#include "lora.hpp"

#include <array>
#include <cstdint>
#include <memory>

// Forward declaration for FFT
class FFT;

class LoRaRxProcessor : public BasebandProcessor {
   public:
    LoRaRxProcessor();

    void execute(const buffer_c8_t& buffer) override;
    void on_message(const Message* const message) override;

   private:
    // Baseband configuration
    static constexpr size_t baseband_fs = 4000000;  // 4 MHz sample rate

    // Decimation chain: 4MHz -> 500kHz
    std::array<complex16_t, 512> dst_buffer{};
    dsp::decimate::FIRC8xR16x24FS4Decim8 decim_0{};
    dsp::decimate::FIRC16xR16x32Decim8 decim_1{};

    // LoRa configuration
    lora::LoRaConfig config_{};
    bool configured_{false};

    // Receiver state
    lora::RxState rx_state_{lora::RxState::IDLE};

    // Chirp dechirping
    static constexpr size_t MAX_FFT_SIZE = 1024;  // Up to SF10 at 500kHz
    std::array<complex16_t, MAX_FFT_SIZE> ref_downchirp_{};
    std::array<complex16_t, MAX_FFT_SIZE> dechirped_buffer_{};
    std::array<uint32_t, MAX_FFT_SIZE> fft_magnitude_{};

    // Symbol accumulator
    size_t symbol_samples_{0};
    size_t samples_per_symbol_{0};
    size_t sample_index_{0};

    // Preamble detection
    uint8_t preamble_count_{0};
    uint16_t last_symbol_{0};
    static constexpr uint8_t MIN_PREAMBLE_CHIRPS = 4;

    // Sync word detection
    uint16_t sync_symbols_[2]{0, 0};
    uint8_t sync_index_{0};

    // Packet assembly
    static constexpr size_t MAX_SYMBOLS = 512;
    std::array<uint16_t, MAX_SYMBOLS> symbol_buffer_{};
    size_t symbol_count_{0};

    // Header info (for explicit mode)
    uint8_t payload_length_{0};
    uint8_t coding_rate_{0};
    bool has_crc_{false};

    // Payload accumulation
    static constexpr size_t MAX_PAYLOAD = 255;
    std::array<uint8_t, MAX_PAYLOAD> payload_buffer_{};
    size_t payload_index_{0};

    // Signal quality
    int16_t rssi_db_{-120};
    int8_t snr_db_{0};

    // Processing methods
    void configure(const LoRaRxConfigureMessage& message);
    void generate_reference_chirp();
    void dechirp_samples(const complex16_t* samples, size_t count);
    uint16_t detect_symbol_fft();
    void process_symbol(uint16_t symbol);
    void handle_preamble_state(uint16_t symbol);
    void handle_sync_detect_state(uint16_t symbol);
    void handle_header_decode_state(uint16_t symbol);
    void handle_payload_decode_state(uint16_t symbol);
    void decode_header();
    void decode_payload();
    void send_packet();
    void reset_receiver();

    // FEC decoding
    void deinterleave(const uint16_t* symbols, size_t count, uint8_t* output);
    void hamming_decode(uint8_t* data, size_t length, uint8_t cr);
    bool verify_crc16(const uint8_t* data, size_t length, uint16_t expected);

    // Utility
    int32_t estimate_frequency_offset(uint16_t symbol);
    void update_signal_quality(uint32_t peak_magnitude, uint32_t noise_floor);

    // Channel stats
    void feed_channel_stats(const buffer_c16_t& channel);

    // Threads
    BasebandThread baseband_thread_{baseband_fs, this, baseband::Direction::Receive};
    RSSIThread rssi_thread_{};
};

// LoRa RX Configuration Message
class LoRaRxConfigureMessage : public Message {
   public:
    constexpr LoRaRxConfigureMessage(
        uint32_t frequency,
        lora::SpreadingFactor sf,
        lora::Bandwidth bw,
        lora::CodingRate cr,
        uint16_t sync_word,
        bool implicit_header)
        : Message{ID::LoRaRxConfigure},
          frequency_(frequency),
          sf_(sf),
          bw_(bw),
          cr_(cr),
          sync_word_(sync_word),
          implicit_header_(implicit_header) {}

    uint32_t frequency() const { return frequency_; }
    lora::SpreadingFactor sf() const { return sf_; }
    lora::Bandwidth bw() const { return bw_; }
    lora::CodingRate cr() const { return cr_; }
    uint16_t sync_word() const { return sync_word_; }
    bool implicit_header() const { return implicit_header_; }

   private:
    uint32_t frequency_;
    lora::SpreadingFactor sf_;
    lora::Bandwidth bw_;
    lora::CodingRate cr_;
    uint16_t sync_word_;
    bool implicit_header_;
};

// LoRa Packet Received Message (M4 -> M0)
class LoRaPacketMessage : public Message {
   public:
    static constexpr size_t MAX_PAYLOAD = 255;

    LoRaPacketMessage()
        : Message{ID::LoRaPacket} {}

    uint32_t frequency{0};
    lora::SpreadingFactor sf{lora::SpreadingFactor::SF7};
    lora::Bandwidth bw{lora::Bandwidth::BW_125K};
    uint16_t sync_word{0};
    int16_t rssi{-120};
    int8_t snr{0};
    uint8_t payload_length{0};
    std::array<uint8_t, MAX_PAYLOAD> payload{};
    bool crc_valid{false};
};

#endif /* __PROC_LORARX_HPP__ */
