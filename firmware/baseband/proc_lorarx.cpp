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

#include "proc_lorarx.hpp"
#include "portapack_shared_memory.hpp"
#include "event_m4.hpp"
#include "dsp_fft.hpp"
#include "sine_table_int8.hpp"

#include <cmath>
#include <algorithm>

LoRaRxProcessor::LoRaRxProcessor() {
    // Initialize with default configuration
    config_.sf = lora::SpreadingFactor::SF10;
    config_.bw = lora::Bandwidth::BW_250K;
    config_.sync_word = lora::SYNC_WORD_MESHTASTIC;
}

void LoRaRxProcessor::execute(const buffer_c8_t& buffer) {
    if (!configured_) return;

    // Decimation chain: 4MHz -> 500kHz baseband
    // Stage 0: 4MHz / 8 = 500kHz
    const auto decim_0_out = decim_0.execute(buffer, dst_buffer);

    feed_channel_stats(decim_0_out);

    // Process decimated samples for LoRa symbols
    for (size_t i = 0; i < decim_0_out.count; i++) {
        // Accumulate samples for one symbol period
        dechirped_buffer_[sample_index_] = decim_0_out.p[i];
        sample_index_++;

        // When we have enough samples for one symbol
        if (sample_index_ >= samples_per_symbol_) {
            // Dechirp the accumulated samples
            dechirp_samples(dechirped_buffer_.data(), samples_per_symbol_);

            // Perform FFT and find peak (symbol value)
            uint16_t symbol = detect_symbol_fft();

            // Process the detected symbol through state machine
            process_symbol(symbol);

            // Reset for next symbol
            sample_index_ = 0;
        }
    }
}

void LoRaRxProcessor::generate_reference_chirp() {
    // Generate a downchirp for dechirping (conjugate of upchirp)
    // LoRa uses upchirps for preamble, downchirp dechirping converts to fixed tone

    const size_t n_samples = samples_per_symbol_;
    const float bandwidth = static_cast<float>(lora::bandwidth_hz(config_.bw));
    const float sample_rate = 500000.0f;  // After decimation

    for (size_t i = 0; i < n_samples; i++) {
        // Downchirp: frequency decreases linearly from +BW/2 to -BW/2
        float t = static_cast<float>(i) / sample_rate;
        float freq = bandwidth / 2.0f - (bandwidth * i) / n_samples;
        float phase = 2.0f * M_PI * freq * t;

        // Generate complex sample (conjugate for dechirping)
        ref_downchirp_[i] = {
            static_cast<int16_t>(32767.0f * cosf(phase)),
            static_cast<int16_t>(-32767.0f * sinf(phase))  // Negative for conjugate
        };
    }
}

void LoRaRxProcessor::dechirp_samples(const complex16_t* samples, size_t count) {
    // Multiply input samples by reference downchirp (complex multiply)
    // This converts the CSS modulated signal into a tone at the symbol frequency

    for (size_t i = 0; i < count && i < MAX_FFT_SIZE; i++) {
        int32_t in_re = samples[i].real();
        int32_t in_im = samples[i].imag();
        int32_t ref_re = ref_downchirp_[i].real();
        int32_t ref_im = ref_downchirp_[i].imag();

        // Complex multiplication: (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
        int32_t out_re = (in_re * ref_re - in_im * ref_im) >> 15;
        int32_t out_im = (in_re * ref_im + in_im * ref_re) >> 15;

        dechirped_buffer_[i] = {
            static_cast<int16_t>(out_re),
            static_cast<int16_t>(out_im)};
    }
}

uint16_t LoRaRxProcessor::detect_symbol_fft() {
    // Perform FFT on dechirped samples
    // The peak bin corresponds to the symbol value

    // Zero-pad if necessary
    for (size_t i = samples_per_symbol_; i < MAX_FFT_SIZE; i++) {
        dechirped_buffer_[i] = {0, 0};
    }

    // In-place FFT (using existing FFT from codebase)
    // Note: This is a placeholder - actual FFT implementation needed
    // The existing codebase has FFT functions we can use

    // Calculate magnitude for each bin
    uint32_t max_magnitude = 0;
    uint16_t max_bin = 0;
    uint32_t noise_sum = 0;

    for (size_t i = 0; i < samples_per_symbol_; i++) {
        int32_t re = dechirped_buffer_[i].real();
        int32_t im = dechirped_buffer_[i].imag();
        uint32_t mag = static_cast<uint32_t>(re * re + im * im);
        fft_magnitude_[i] = mag;

        if (mag > max_magnitude) {
            max_magnitude = mag;
            max_bin = i;
        }
        noise_sum += mag;
    }

    // Update signal quality estimate
    uint32_t noise_floor = (noise_sum - max_magnitude) / (samples_per_symbol_ - 1);
    update_signal_quality(max_magnitude, noise_floor);

    // Apply Gray decoding to get actual symbol value
    return lora::gray_decode(max_bin, static_cast<uint8_t>(config_.sf));
}

void LoRaRxProcessor::process_symbol(uint16_t symbol) {
    switch (rx_state_) {
        case lora::RxState::IDLE:
        case lora::RxState::PREAMBLE_DETECT:
            handle_preamble_state(symbol);
            break;

        case lora::RxState::SYNC_DETECT:
            handle_sync_detect_state(symbol);
            break;

        case lora::RxState::HEADER_DECODE:
            handle_header_decode_state(symbol);
            break;

        case lora::RxState::PAYLOAD_DECODE:
            handle_payload_decode_state(symbol);
            break;

        case lora::RxState::PACKET_READY:
            send_packet();
            reset_receiver();
            break;

        default:
            reset_receiver();
            break;
    }
}

void LoRaRxProcessor::handle_preamble_state(uint16_t symbol) {
    // Preamble consists of unmodulated upchirps (symbol value ~0)
    // After dechirping, they appear as DC (bin 0)

    const uint16_t preamble_threshold = samples_per_symbol_ / 8;

    if (symbol < preamble_threshold || symbol > (samples_per_symbol_ - preamble_threshold)) {
        // Symbol is near 0 (considering wrap-around)
        preamble_count_++;

        if (preamble_count_ >= MIN_PREAMBLE_CHIRPS) {
            rx_state_ = lora::RxState::SYNC_DETECT;
            sync_index_ = 0;
        }
    } else {
        // Not a preamble symbol, reset
        if (preamble_count_ > 0) {
            preamble_count_--;
        }
    }

    last_symbol_ = symbol;
}

void LoRaRxProcessor::handle_sync_detect_state(uint16_t symbol) {
    // Sync word is encoded as two symbols with specific values
    // Store the symbols and check against expected sync word

    sync_symbols_[sync_index_++] = symbol;

    if (sync_index_ >= 2) {
        // Check if sync word matches
        // The sync word encoding depends on the SF
        uint16_t detected_sync = (sync_symbols_[0] << 4) | (sync_symbols_[1] >> 4);

        // For now, accept any sync word and record it
        config_.sync_word = detected_sync;

        if (config_.header_mode == lora::HeaderMode::EXPLICIT) {
            rx_state_ = lora::RxState::HEADER_DECODE;
            symbol_count_ = 0;
        } else {
            rx_state_ = lora::RxState::PAYLOAD_DECODE;
            symbol_count_ = 0;
        }
    }
}

void LoRaRxProcessor::handle_header_decode_state(uint16_t symbol) {
    // Explicit header contains: payload length, coding rate, CRC presence
    // Header is always transmitted at CR 4/8

    symbol_buffer_[symbol_count_++] = symbol;

    // Header is 8 symbols (for SF >= 7)
    if (symbol_count_ >= 8) {
        decode_header();
        rx_state_ = lora::RxState::PAYLOAD_DECODE;
        symbol_count_ = 0;
    }
}

void LoRaRxProcessor::handle_payload_decode_state(uint16_t symbol) {
    symbol_buffer_[symbol_count_++] = symbol;

    // Calculate expected symbol count for payload
    size_t payload_symbols = (payload_length_ * 8 + static_cast<uint8_t>(config_.sf) - 1) /
                             static_cast<uint8_t>(config_.sf);

    if (symbol_count_ >= payload_symbols || symbol_count_ >= MAX_SYMBOLS) {
        decode_payload();
        rx_state_ = lora::RxState::PACKET_READY;
    }
}

void LoRaRxProcessor::decode_header() {
    // Deinterleave and decode header symbols
    std::array<uint8_t, 16> header_bytes{};

    deinterleave(symbol_buffer_.data(), 8, header_bytes.data());
    hamming_decode(header_bytes.data(), 5, 4);  // Header uses CR 4/8

    // Extract header fields
    payload_length_ = header_bytes[0];
    coding_rate_ = (header_bytes[1] >> 1) & 0x07;
    has_crc_ = (header_bytes[1] & 0x01) != 0;

    // Sanity check
    if (payload_length_ > MAX_PAYLOAD) {
        payload_length_ = MAX_PAYLOAD;
    }
}

void LoRaRxProcessor::decode_payload() {
    // Deinterleave payload symbols
    std::array<uint8_t, MAX_PAYLOAD + 16> decoded{};

    deinterleave(symbol_buffer_.data(), symbol_count_, decoded.data());
    hamming_decode(decoded.data(), payload_length_, coding_rate_);

    // Copy to payload buffer
    for (size_t i = 0; i < payload_length_ && i < MAX_PAYLOAD; i++) {
        payload_buffer_[i] = decoded[i];
    }

    payload_index_ = payload_length_;
}

void LoRaRxProcessor::send_packet() {
    // Build and send packet message to M0
    LoRaPacketMessage message;

    message.frequency = config_.frequency;
    message.sf = config_.sf;
    message.bw = config_.bw;
    message.sync_word = config_.sync_word;
    message.rssi = rssi_db_;
    message.snr = snr_db_;
    message.payload_length = static_cast<uint8_t>(payload_index_);
    message.crc_valid = true;  // TODO: Implement CRC check

    for (size_t i = 0; i < payload_index_ && i < message.payload.size(); i++) {
        message.payload[i] = payload_buffer_[i];
    }

    shared_memory.application_queue.push(message);
}

void LoRaRxProcessor::reset_receiver() {
    rx_state_ = lora::RxState::IDLE;
    preamble_count_ = 0;
    sync_index_ = 0;
    symbol_count_ = 0;
    payload_index_ = 0;
    sample_index_ = 0;
}

void LoRaRxProcessor::deinterleave(const uint16_t* symbols, size_t count, uint8_t* output) {
    // LoRa uses diagonal interleaving
    // This is a simplified implementation

    const uint8_t sf = static_cast<uint8_t>(config_.sf);

    for (size_t i = 0; i < count && i < MAX_PAYLOAD; i++) {
        // Extract bits from symbol and place in output
        uint16_t sym = symbols[i];
        for (uint8_t bit = 0; bit < sf; bit++) {
            size_t out_byte = (i * sf + bit) / 8;
            uint8_t out_bit = (i * sf + bit) % 8;

            if ((sym >> bit) & 1) {
                output[out_byte] |= (1 << out_bit);
            }
        }
    }
}

void LoRaRxProcessor::hamming_decode(uint8_t* data, size_t length, uint8_t cr) {
    // Hamming FEC decoding
    // CR 4/5: 1 parity bit, can detect 1 error
    // CR 4/6: 2 parity bits, can correct 1 error
    // CR 4/7: 3 parity bits, can correct 1 error, detect 2
    // CR 4/8: 4 parity bits (full Hamming 8,4)

    // Simplified: just strip parity bits for now
    // Full implementation would do syndrome check and correction

    (void)data;
    (void)length;
    (void)cr;
    // TODO: Implement proper Hamming decoding
}

void LoRaRxProcessor::update_signal_quality(uint32_t peak_magnitude, uint32_t noise_floor) {
    // Estimate SNR from FFT peak vs noise floor
    if (noise_floor > 0) {
        float snr_linear = static_cast<float>(peak_magnitude) / noise_floor;
        snr_db_ = static_cast<int8_t>(10.0f * log10f(snr_linear));
    }

    // RSSI would come from RF frontend, placeholder for now
    rssi_db_ = -80;  // Placeholder
}

void LoRaRxProcessor::feed_channel_stats(const buffer_c16_t& channel) {
    // Calculate channel power for RSSI indication
    uint64_t power_sum = 0;

    for (size_t i = 0; i < channel.count; i++) {
        int32_t re = channel.p[i].real();
        int32_t im = channel.p[i].imag();
        power_sum += static_cast<uint64_t>(re * re + im * im);
    }

    // Convert to dBm (approximate)
    if (power_sum > 0 && channel.count > 0) {
        float avg_power = static_cast<float>(power_sum) / channel.count;
        rssi_db_ = static_cast<int16_t>(10.0f * log10f(avg_power) - 90.0f);
    }
}

void LoRaRxProcessor::configure(const LoRaRxConfigureMessage& message) {
    config_.frequency = message.frequency();
    config_.sf = message.sf();
    config_.bw = message.bw();
    config_.cr = message.cr();
    config_.sync_word = message.sync_word();
    config_.header_mode = message.implicit_header() ? lora::HeaderMode::IMPLICIT
                                                    : lora::HeaderMode::EXPLICIT;

    // Calculate samples per symbol at decimated rate (500kHz)
    // samples_per_symbol = 2^SF * (sample_rate / bandwidth)
    uint32_t bw_hz = lora::bandwidth_hz(config_.bw);
    samples_per_symbol_ = (lora::samples_per_symbol(config_.sf) * 500000) / bw_hz;

    // Limit to buffer size
    if (samples_per_symbol_ > MAX_FFT_SIZE) {
        samples_per_symbol_ = MAX_FFT_SIZE;
    }

    // Generate reference chirp for this configuration
    generate_reference_chirp();

    // Configure decimation filters
    // Using existing wideband filters from the codebase
    // decim_0.configure(taps_xxx);

    reset_receiver();
    configured_ = true;
}

void LoRaRxProcessor::on_message(const Message* const message) {
    if (message->id == Message::ID::LoRaRxConfigure) {
        configure(*reinterpret_cast<const LoRaRxConfigureMessage*>(message));
    }
}

int main() {
    EventDispatcher event_dispatcher{std::make_unique<LoRaRxProcessor>()};
    event_dispatcher.run();
    return 0;
}
