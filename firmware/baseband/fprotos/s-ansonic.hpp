
#ifndef __FPROTO_ANSONIC_H__
#define __FPROTO_ANSONIC_H__

#include "subghzdbase.hpp"

typedef enum : uint8_t {
    AnsonicDecoderStepReset = 0,
    AnsonicDecoderStepCheckPreambule,
    AnsonicDecoderStepSaveDuration,
    AnsonicDecoderStepCheckDuration,
} AnsonicDecoderStep;

class FProtoSubGhzDAnsonic : public FProtoSubGhzDBase {
   public:
    FProtoSubGhzDAnsonic() {
        sensorType = FPS_ANSONIC;
        te_short = 300;
        te_long = 600;
        te_delta = 100;
        min_count_bit_for_found = 12;
    }

    void feed(bool level, uint32_t duration) {
        switch (parser_step) {
            case AnsonicDecoderStepReset:
                if ((!level) && (DURATION_DIFF(duration, te_short * 24) < te_delta * 24)) {
                    // Found sync
                    parser_step = AnsonicDecoderStepCheckPreambule;
                    decode_data = 0;
                    decode_count_bit = 0;
                }
                break;

            case AnsonicDecoderStepCheckPreambule:
                if (level) {
                    if (DURATION_DIFF(duration, te_short) < te_delta) {
                        parser_step = AnsonicDecoderStepSaveDuration;
                    } else {
                        parser_step = AnsonicDecoderStepReset;
                    }
                } else {
                    parser_step = AnsonicDecoderStepReset;
                }
                break;

            case AnsonicDecoderStepSaveDuration:
                if (!level) {
                    te_last = duration;
                    parser_step = AnsonicDecoderStepCheckDuration;
                } else {
                    parser_step = AnsonicDecoderStepReset;
                }
                break;

            case AnsonicDecoderStepCheckDuration:
                if (level) {
                    if (DURATION_DIFF(te_last, te_short) < te_delta &&
                        DURATION_DIFF(duration, te_long) < te_delta * 2) {
                        subghz_protocol_blocks_add_bit(0);
                        parser_step = AnsonicDecoderStepSaveDuration;
                    } else if (
                        DURATION_DIFF(te_last, te_long) < te_delta * 2 &&
                        DURATION_DIFF(duration, te_short) < te_delta) {
                        subghz_protocol_blocks_add_bit(1);
                        parser_step = AnsonicDecoderStepSaveDuration;
                    } else {
                        parser_step = AnsonicDecoderStepReset;
                    }

                    if (decode_count_bit == min_count_bit_for_found) {
                        data_count_bit = decode_count_bit;
                        if (callback) callback(this);
                        decode_data = 0;
                        decode_count_bit = 0;
                        parser_step = AnsonicDecoderStepReset;
                    }
                } else {
                    parser_step = AnsonicDecoderStepReset;
                }
                break;
        }
    }
};

#endif
