# LoRa Implementation Plan for Mayhem Firmware

## 🎯 Overview

LoRa (Long Range) uses **Chirp Spread Spectrum (CSS)** modulation, which is fundamentally different from the FSK/OOK protocols currently implemented. This document outlines how to add comprehensive LoRa support to PortaPack Mayhem.

---

## 📡 LoRa Technical Background

### Modulation: Chirp Spread Spectrum (CSS)
- **Upchirp**: Frequency sweeps from low to high
- **Downchirp**: Frequency sweeps from high to low
- **Symbol encoding**: Data is encoded as cyclic shifts of the base chirp
- **Spreading Factor (SF)**: SF7-SF12 (higher = longer range, slower data rate)

### Common Frequencies
| Region | Frequency Band |
|--------|----------------|
| US/Americas | 902-928 MHz |
| EU | 863-870 MHz |
| Asia | 433 MHz / 920-925 MHz |

### LoRa Parameters
- **Bandwidth**: 125kHz, 250kHz, 500kHz
- **Spreading Factor**: 7-12
- **Coding Rate**: 4/5 to 4/8
- **Preamble**: 8+ chirps

---

## 🏗️ Implementation Architecture

### Phase 1: LoRa RX (Sniffer) - Core DSP

```
┌─────────────────────────────────────────────────────────────┐
│                     HackRF SDR Input                        │
│                      (4 MHz sample)                          │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              Decimation Chain (proc_lorarx.cpp)             │
│                 4MHz → 500kHz baseband                       │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  Chirp Dechirping                            │
│        Multiply incoming signal with reference chirp         │
│             (converts CSS to frequency shift)                │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     FFT (256/512/1024)                       │
│              Find peak = decoded symbol value                │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│               Symbol to Bit Conversion                       │
│          Gray coding, deinterleaving, FEC decode             │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  Packet Assembly (M0)                        │
│         CRC check, protocol detection, UI display            │
└─────────────────────────────────────────────────────────────┘
```

### Phase 2: Mesh Protocol Decoders
- Parse Meshtastic packet format (protobuf-based)
- Parse MeshCore packet format (simpler binary format)
- Auto-detect protocol from sync word and header
- Extract: Node ID, Message Text, Position, Telemetry

### Supported Mesh Protocols
| Protocol | Sync Word | Format | Notes |
|----------|-----------|--------|-------|
| **Meshtastic** | 0x2B | Protobuf | Most popular, encrypted by default |
| **MeshCore** | 0xAB | Binary | Simpler format, growing community |
| **LoRaWAN** | 0x3444 | Standard | Enterprise/IoT |
- Display on UI with sender info

### Phase 3: LoRa TX (Replay/Transmit)
- Generate CSS chirps
- Encode data into symbols
- Apply FEC and interleaving

---

## 📁 Files to Create

### Baseband (M4 Processor)
```
firmware/baseband/
├── proc_lorarx.cpp          # Main LoRa RX processor
├── proc_lorarx.hpp          # Header
├── lora_demod.cpp           # CSS demodulation logic
├── lora_demod.hpp           # Chirp dechirping, FFT peak detection
├── lora_fec.cpp             # Forward Error Correction (Hamming)
├── lora_fec.hpp
└── lora_packet.hpp          # Packet structures
```

### Application (M0 Processor)
```
firmware/application/
├── apps/
│   ├── lora_rx_app.cpp      # LoRa Sniffer UI
│   ├── lora_rx_app.hpp
│   ├── ui_lora.cpp          # LoRa settings view
│   └── ui_lora.hpp
├── protocols/
│   ├── meshtastic.cpp       # Meshtastic protocol parser
│   └── meshtastic.hpp
└── external/
    └── lora_analyzer/       # Advanced analysis tool (external app)
```

### Common
```
firmware/common/
├── lora.hpp                 # LoRa constants, enums, structures
└── message_lora.hpp         # IPC messages for LoRa data
```

---

## 🔧 Key Implementation Details

### 1. Chirp Dechirping (Critical DSP)

```cpp
// lora_demod.cpp
class LoRaDemodulator {
public:
    // Generate reference downchirp for dechirping
    void generate_reference_chirp(uint8_t sf, uint32_t bw);
    
    // Dechirp incoming signal (multiply by conjugate of reference)
    // Result: CSS modulated signal becomes a pure tone at symbol frequency
    void dechirp(const buffer_c16_t& in, buffer_c16_t& out);
    
    // FFT to find peak bin = symbol value
    uint16_t detect_symbol(const buffer_c16_t& dechirped);
    
private:
    std::array<complex16_t, 1024> ref_chirp_;  // Reference downchirp
    uint8_t spreading_factor_;
    uint32_t bandwidth_;
};
```

### 2. Symbol Decoding

Each LoRa symbol encodes `SF` bits. For SF7, each symbol = 7 bits, 128 possible values.

```cpp
// Symbol value to bits (with Gray coding)
uint16_t gray_decode(uint16_t symbol, uint8_t sf) {
    symbol ^= (symbol >> 8);
    symbol ^= (symbol >> 4);
    symbol ^= (symbol >> 2);
    symbol ^= (symbol >> 1);
    return symbol & ((1 << sf) - 1);
}
```

### 3. LoRa Packet Structure

```cpp
// lora_packet.hpp
struct LoRaPacket {
    uint8_t preamble_count;      // Usually 8
    uint16_t sync_word;          // 0x12 for LoRaWAN, 0x34 for Meshtastic
    bool explicit_header;        // Header mode
    uint8_t payload_length;
    uint8_t coding_rate;         // 4/5 to 4/8
    bool crc_enabled;
    std::vector<uint8_t> payload;
    uint16_t crc;
};
```

### 4. Meshtastic Packet Parsing

```cpp
// meshtastic.hpp
struct MeshtasticPacket {
    uint32_t from_node;          // Sender node ID
    uint32_t to_node;            // Destination (0xFFFFFFFF = broadcast)
    uint8_t hop_limit;
    uint8_t port;                // TEXT_MESSAGE, POSITION, TELEMETRY, etc.
    std::string payload;         // Decoded payload
    
    // Position data (if port == POSITION)
    struct Position {
        float latitude;
        float longitude;
        int32_t altitude;
    } position;
};
```

---

## 🎮 "On-Brand Mayhem" Features

### 1. 📡 LoRa Sniffer
- Display all LoRa packets in range
- Show: Frequency, SF, BW, RSSI, Payload (hex/ASCII)
- Filter by sync word (LoRaWAN vs Meshtastic vs custom)

### 2. 💬 Meshtastic Chat Spy
- Real-time display of Meshtastic text messages
- Show sender node ID, message content
- Log to SD card

### 3. 🗺️ Node Mapper
- Extract GPS coordinates from Meshtastic packets
- Display on GeoMap view
- Track node movements over time

### 4. 🔁 LoRa Replay Attack
- Capture LoRa packets
- Replay at will
- Useful for testing mesh resilience

### 5. 📊 LoRa Spectrum View
- Visualize chirp patterns in waterfall
- Detect LoRa activity across bands
- Identify occupied channels

### 6. 🚨 Meshtastic Alert Injection (Advanced)
- Generate fake Meshtastic packets
- Send custom messages to mesh network
- Testing/red team purposes

---

## 📋 Implementation Roadmap

### Week 1: Foundation
- [ ] Create `lora.hpp` with constants and structures
- [ ] Create `proc_lorarx.cpp` skeleton
- [ ] Add to CMakeLists.txt and build system
- [ ] Basic decimation chain setup

### Week 2: CSS Demodulation
- [ ] Implement reference chirp generation
- [ ] Implement dechirping (complex multiply)
- [ ] Add FFT for symbol detection
- [ ] Test with generated LoRa signals

### Week 3: Packet Decoding
- [ ] Preamble detection
- [ ] Sync word detection
- [ ] Header decoding
- [ ] Payload extraction with FEC

### Week 4: Application UI
- [ ] Create `lora_rx_app.cpp` 
- [ ] Add to app menu
- [ ] Display decoded packets
- [ ] Settings for SF/BW/Frequency

### Week 5: Meshtastic Integration
- [ ] Parse Meshtastic packet format
- [ ] Extract text messages
- [ ] Extract position data
- [ ] GeoMap integration

### Week 6: Advanced Features
- [ ] LoRa TX capability
- [ ] Replay functionality
- [ ] Logging to SD card
- [ ] Multi-channel scanning

---

## ⚠️ Technical Challenges

### 1. FFT Performance
- LoRa demod requires FFT per symbol
- SF12 = 4096 samples per symbol at 125kHz
- Need efficient FFT implementation (existing in codebase)

### 2. Timing Synchronization  
- Must lock to preamble chirps
- Symbol timing recovery critical

### 3. Memory Constraints
- M4 has limited RAM
- FFT buffers need careful allocation

### 4. Multi-SF Detection
- Different SFs are orthogonal
- Initial version: single SF
- Future: SF enumeration

---

## 📚 References

1. [LoRa PHY Specification](https://lora-alliance.org/resource_hub/rp002-1-0-4-regional-parameters/)
2. [gr-lora (GNU Radio)](https://github.com/rpp0/gr-lora) - Reference implementation
3. [Meshtastic Protocol](https://meshtastic.org/docs/developers/firmware/portapack)
4. [LoRa Reverse Engineering Paper](https://pspace.ngi.eu/lora/)

---

## 🚀 Let's Build This!

Ready to start with **Week 1: Foundation**?

Priority order:
1. `firmware/common/lora.hpp` - Core constants and structures
2. `firmware/baseband/proc_lorarx.cpp` - M4 processor skeleton
3. Add to build system

