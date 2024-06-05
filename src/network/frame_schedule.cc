#include "frame_schedule.h"

namespace tsndgm
{

    FrameSchedule::FrameSchedule(nlohmann::json j)
    {
        std::vector<FrameTransmission> transmissions;
        transmissions.reserve(j["transmissions"].size());
        for (const auto &js : j["transmissions"])
        {
            transmissions.emplace_back(js);
        }
        *this = FrameSchedule(j["frame_number"], transmissions);
    }

} // namespace tsndgm
