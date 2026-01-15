
#ifndef __FPROTO_ECOWITT_H__
#define __FPROTO_ECOWITT_H__

#include "weatherbase.hpp"

typedef enum {
    EcowittDecoderStepReset = 0,
    EcowittDecoderStepFoundStartBit,
    EcowittDecoderStepSaveDuration,
    EcowittDecoderStepCheckDuration,
} EcowittDecoderStep;

class FProtoWeatherEcowitt : public FProtoWeatherBase {
   public:
    FProtoWeatherEcowitt() {
        sensorType = FPW_Ecowitt;
    }

    void feed(bool level, uint32_t duration) {
        switch (parser_step) {
            case EcowittDecoderStepReset:
                if ((level) && (DURATION_DIFF(duration, te_long * 2) < te_delta * 4)) {
                    // Found long sync pulse
                    parser_step = EcowittDecoderStepFoundStartBit;
                    decode_data = 0;
                    decode_count_bit = 0;
                }
                break;

            case EcowittDecoderStepFoundStartBit:
                if ((!level) && (DURATION_DIFF(duration, te_short) < te_delta)) {
                    // Found gap after sync
                    parser_step = EcowittDecoderStepSaveDuration;
                } else {
                    parser_step = EcowittDecoderStepReset;
                }
                break;

            case EcowittDecoderStepSaveDuration:
                if (level) {
                    te_last = duration;
                    parser_step = EcowittDecoderStepCheckDuration;
                } else {
                    parser_step = EcowittDecoderStepReset;
                }
                break;

            case EcowittDecoderStepCheckDuration:
                if (!level) {
                    if (DURATION_DIFF(te_last, te_short) < te_delta) {
                        if (DURATION_DIFF(duration, te_short) < te_delta) {
                            subghz_protocol_blocks_add_bit(0);
                            parser_step = EcowittDecoderStepSaveDuration;
                        } else if (DURATION_DIFF(duration, te_long) < te_delta * 2) {
                            subghz_protocol_blocks_add_bit(1);
                            parser_step = EcowittDecoderStepSaveDuration;
                        } else if (duration > te_long * 2) {
                            // End of packet
                            if ((decode_count_bit >= min_count_bit_for_found) &&
                                ws_protocol_ecowitt_check()) {
                                if (callback) callback(this);
                            }
                            parser_step = EcowittDecoderStepReset;
                        } else {
                            parser_step = EcowittDecoderStepReset;
                        }
                    } else {
                        parser_step = EcowittDecoderStepReset;
                    }
                } else {
                    parser_step = EcowittDecoderStepReset;
                }
                break;
        }
    }

   protected:
    uint32_t te_short = 500;
    uint32_t te_long = 1000;
    uint32_t te_delta = 200;
    uint32_t min_count_bit_for_found = 40;

    bool ws_protocol_ecowitt_check() {
        if (!decode_data) return false;

        // Ecowitt uses CRC-8 CCITT
        uint8_t msg[5];
        msg[0] = static_cast<uint8_t>(decode_data >> 32);
        msg[1] = static_cast<uint8_t>(decode_data >> 24);
        msg[2] = static_cast<uint8_t>(decode_data >> 16);
        msg[3] = static_cast<uint8_t>(decode_data >> 8);
        msg[4] = static_cast<uint8_t>(decode_data);

        uint8_t crc = 0;
        for (uint8_t i = 0; i < 4; i++) {
            crc = FProtoGeneral::subghz_protocol_blocks_crc8(msg, i + 1, 0x31, 0x00);
        }

        return (crc == msg[4]);
    }
};

#endif
