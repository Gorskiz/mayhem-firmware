
#ifndef __FPROTO_FINEOFFSET_WH2_H__
#define __FPROTO_FINEOFFSET_WH2_H__

#include "weatherbase.hpp"

typedef enum {
    FineOffsetWH2DecoderStepReset = 0,
    FineOffsetWH2DecoderStepCheckPreambule,
    FineOffsetWH2DecoderStepSaveDuration,
    FineOffsetWH2DecoderStepCheckDuration,
} FineOffsetWH2DecoderStep;

class FProtoWeatherFineOffsetWH2 : public FProtoWeatherBase {
   public:
    FProtoWeatherFineOffsetWH2() {
        sensorType = FPW_FineOffsetWH2;
    }

    void feed(bool level, uint32_t duration) {
        switch (parser_step) {
            case FineOffsetWH2DecoderStepReset:
                if ((!level) && (DURATION_DIFF(duration, te_short * 4) < te_delta * 4)) {
                    // Found sync preamble
                    parser_step = FineOffsetWH2DecoderStepCheckPreambule;
                    header_count = 0;
                }
                break;

            case FineOffsetWH2DecoderStepCheckPreambule:
                if (level) {
                    if (DURATION_DIFF(duration, te_short * 4) < te_delta * 4) {
                        header_count++;
                    } else if (DURATION_DIFF(duration, te_short) < te_delta) {
                        // Start of data
                        if (header_count >= 4) {
                            parser_step = FineOffsetWH2DecoderStepSaveDuration;
                            decode_data = 0;
                            decode_count_bit = 0;
                        } else {
                            parser_step = FineOffsetWH2DecoderStepReset;
                        }
                    } else {
                        parser_step = FineOffsetWH2DecoderStepReset;
                    }
                } else {
                    if (DURATION_DIFF(duration, te_short * 4) < te_delta * 4) {
                        // Continue sync
                    } else {
                        parser_step = FineOffsetWH2DecoderStepReset;
                    }
                }
                break;

            case FineOffsetWH2DecoderStepSaveDuration:
                if (level) {
                    te_last = duration;
                    parser_step = FineOffsetWH2DecoderStepCheckDuration;
                } else {
                    parser_step = FineOffsetWH2DecoderStepReset;
                }
                break;

            case FineOffsetWH2DecoderStepCheckDuration:
                if (!level) {
                    if (DURATION_DIFF(te_last, te_short) < te_delta) {
                        if (DURATION_DIFF(duration, te_short) < te_delta) {
                            subghz_protocol_blocks_add_bit(0);
                            parser_step = FineOffsetWH2DecoderStepSaveDuration;
                        } else if (DURATION_DIFF(duration, te_long) < te_delta * 2) {
                            subghz_protocol_blocks_add_bit(1);
                            parser_step = FineOffsetWH2DecoderStepSaveDuration;
                        } else {
                            parser_step = FineOffsetWH2DecoderStepReset;
                        }
                    } else if (DURATION_DIFF(te_last, te_long) < te_delta * 2) {
                        if (DURATION_DIFF(duration, te_short) < te_delta) {
                            subghz_protocol_blocks_add_bit(1);
                            parser_step = FineOffsetWH2DecoderStepSaveDuration;
                        } else if (DURATION_DIFF(duration, te_long) < te_delta * 2) {
                            subghz_protocol_blocks_add_bit(0);
                            parser_step = FineOffsetWH2DecoderStepSaveDuration;
                        } else {
                            parser_step = FineOffsetWH2DecoderStepReset;
                        }
                    } else {
                        parser_step = FineOffsetWH2DecoderStepReset;
                    }

                    if (decode_count_bit == min_count_bit_for_found) {
                        if (ws_protocol_fineoffset_wh2_check()) {
                            if (callback) callback(this);
                        }
                        parser_step = FineOffsetWH2DecoderStepReset;
                    }
                } else {
                    parser_step = FineOffsetWH2DecoderStepReset;
                }
                break;
        }
    }

   protected:
    uint32_t te_short = 500;
    uint32_t te_long = 1000;
    uint32_t te_delta = 150;
    uint32_t min_count_bit_for_found = 48;

    bool ws_protocol_fineoffset_wh2_check() {
        if (!decode_data) return false;

        // Check CRC (sum of nibbles)
        uint8_t msg[] = {
            static_cast<uint8_t>(decode_data >> 40),
            static_cast<uint8_t>(decode_data >> 32),
            static_cast<uint8_t>(decode_data >> 24),
            static_cast<uint8_t>(decode_data >> 16),
            static_cast<uint8_t>(decode_data >> 8)};

        uint8_t crc = 0;
        for (uint8_t i = 0; i < 5; i++) {
            crc += (msg[i] >> 4) + (msg[i] & 0x0F);
        }

        return ((crc & 0x0F) == (decode_data & 0x0F));
    }
};

#endif
