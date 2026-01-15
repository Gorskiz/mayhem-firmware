
#ifndef __FPROTO_ELRO_H__
#define __FPROTO_ELRO_H__

#include "subghzdbase.hpp"

typedef enum : uint8_t {
    ELRODecoderStepReset = 0,
    ELRODecoderStepSaveDuration,
    ELRODecoderStepCheckDuration,
} ELRODecoderStep;

class FProtoSubGhzDELRO : public FProtoSubGhzDBase {
   public:
    FProtoSubGhzDELRO() {
        sensorType = FPS_ELRO;
        te_short = 320;
        te_long = 960;
        te_delta = 150;
        min_count_bit_for_found = 12;
    }

    void feed(bool level, uint32_t duration) {
        switch (parser_step) {
            case ELRODecoderStepReset:
                if ((!level) && (DURATION_DIFF(duration, te_short * 37) < te_delta * 37)) {
                    // Found sync
                    parser_step = ELRODecoderStepSaveDuration;
                    decode_data = 0;
                    decode_count_bit = 0;
                }
                break;

            case ELRODecoderStepSaveDuration:
                if (level) {
                    te_last = duration;
                    parser_step = ELRODecoderStepCheckDuration;
                } else {
                    parser_step = ELRODecoderStepReset;
                }
                break;

            case ELRODecoderStepCheckDuration:
                if (!level) {
                    if ((DURATION_DIFF(te_last, te_short) < te_delta) &&
                        (DURATION_DIFF(duration, te_long) < te_delta * 3)) {
                        subghz_protocol_blocks_add_bit(0);
                        parser_step = ELRODecoderStepSaveDuration;
                    } else if (
                        (DURATION_DIFF(te_last, te_long) < te_delta * 3) &&
                        (DURATION_DIFF(duration, te_short) < te_delta)) {
                        subghz_protocol_blocks_add_bit(1);
                        parser_step = ELRODecoderStepSaveDuration;
                    } else if (duration >= ((uint32_t)te_short * 10)) {
                        if (decode_count_bit == min_count_bit_for_found) {
                            data_count_bit = decode_count_bit;
                            if (callback) callback(this);
                        }
                        decode_data = 0;
                        decode_count_bit = 0;
                        parser_step = ELRODecoderStepReset;
                    } else {
                        parser_step = ELRODecoderStepReset;
                    }
                } else {
                    parser_step = ELRODecoderStepReset;
                }
                break;
        }
    }
};

#endif
