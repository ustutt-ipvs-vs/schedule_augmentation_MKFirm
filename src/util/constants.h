#pragma once

#include "typedefs.h"

namespace error_codes {

constexpr unsigned int FILE_NOT_FOUND = 2;
constexpr unsigned int JSON_PARSING_FAILED = 3;

} // namespace error_codes

namespace tsndgm {
constexpr Tick EPSILON = 1;

constexpr FrameSize INTER_FRAME_GAP_BYTES = 12;
constexpr double INTER_FRAME_GAP_INCREASE_FACTOR = (64.0 + INTER_FRAME_GAP_BYTES) / 64.0;
} // namespace tsndgm
