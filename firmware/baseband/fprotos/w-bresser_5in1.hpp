
#ifndef __FPROTO_BRESSER_5IN1_H__
#define __FPROTO_BRESSER_5IN1_H__

#include "weatherbase.hpp"

typedef enum {
    Bresser5in1DecoderStepReset = 0,
    Bresser5in1DecoderStepSaveDuration,
    Bresser5in1DecoderStepCheckDuration,
} Bresser5in1DecoderStep;

class FProtoWeatherBresser5in1 : public FProtoWeatherBase {
   public:
    FProtoWeatherBresser5in1() {
        sensorType = FPW_Bresser5in1;
    }

    void feed(bool level, uint32_t duration) {
        switch (parser_step) {
            case Bresser5in1DecoderStepReset:
                if ((!level) && (DURATION_DIFF(duration, te_short * 36) < te_delta * 36)) {
                    // Found long sync
                    parser_step = Bresser5in1DecoderStepSaveDuration;
                    decode_data = 0;
                    decode_count_bit = 0;
                }
                break;

            case Bresser5in1DecoderStepSaveDuration:
                if (level) {
                    te_last = duration;
                    parser_step = Bresser5in1DecoderStepCheckDuration;
                } else {
                    parser_step = Bresser5in1DecoderStepReset;
                }
                break;

            case Bresser5in1DecoderStepCheckDuration:
                if (!level) {
                    if (DURATION_DIFF(te_last, te_short) < te_delta) {
                        if (DURATION_DIFF(duration, te_long) < te_delta * 2) {
                            subghz_protocol_blocks_add_bit(0);
                            parser_step = Bresser5in1DecoderStepSaveDuration;
                        } else if (DURATION_DIFF(duration, te_short) < te_delta) {
                            subghz_protocol_blocks_add_bit(1);
                            parser_step = Bresser5in1DecoderStepSaveDuration;
                        } else if (duration > (te_short * 10)) {
                            // End of packet
                            if ((decode_count_bit >= min_count_bit_for_found) &&
                                ws_protocol_bresser_5in1_check()) {
                                if (callback) callback(this);
                            }
                            parser_step = Bresser5in1DecoderStepReset;
                        } else {
                            parser_step = Bresser5in1DecoderStepReset;
                        }
                    } else {
                        parser_step = Bresser5in1DecoderStepReset;
                    }

                    if (decode_count_bit == min_count_bit_for_found) {
                        if (ws_protocol_bresser_5in1_check()) {
                            if (callback) callback(this);
                        }
                        parser_step = Bresser5in1DecoderStepReset;
                    }
                } else {
                    parser_step = Bresser5in1DecoderStepReset;
                }
                break;
        }
    }

   protected:
    uint32_t te_short = 500;
    uint32_t te_long = 1000;
    uint32_t te_delta = 150;
    uint32_t min_count_bit_for_found = 40;

    bool ws_protocol_bresser_5in1_check() {
        if (!decode_data) return false;

        // Bresser 5-in-1 uses CRC-8 with polynomial 0x31
        uint8_t msg[5];
        msg[0] = static_cast<uint8_t>(decode_data >> 32);
        msg[1] = static_cast<uint8_t>(decode_data >> 24);
        msg[2] = static_cast<uint8_t>(decode_data >> 16);
        msg[3] = static_cast<uint8_t>(decode_data >> 8);
        msg[4] = static_cast<uint8_t>(decode_data);

        uint8_t crc = 0xFF;  // Bresser starts with 0xFF
        for (uint8_t i = 0; i < 4; i++) {
            crc ^= msg[i];
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x31;
                } else {
                    crc = crc << 1;
                }
            }
        }

        return (crc == msg[4]);
    }
};

#endif
