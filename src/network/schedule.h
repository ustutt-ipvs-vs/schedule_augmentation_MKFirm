#pragma once

#include "../util/typedefs.h"

namespace tsndgm
{
    struct FrameTransmission
    {
        LinkId link_id;
        std::string link_name;
        DeviceId source;
        DeviceId target;
        Tick start;
        Tick end;


        [[nodiscard]] auto toString() const -> std::string
        {
            return "\n|--link_id: " + std::to_string(link_id) + ", link_name: " + link_name +
                ", source: " + std::to_string(source) + ", target: " + std::to_string(target) +
                ", start: " + std::to_string(start) + ", end: " + std::to_string(end);
        }
    };

    struct FrameSchedule
    {
        unsigned int frame_number;
        std::vector<FrameTransmission> transmissions;

        [[nodiscard]] auto toString() const -> std::string
        {
            std::string output = "\n|-ScheduledFrame number: " + std::to_string(frame_number);
            for (const auto &t : transmissions)
            {
                output += t.toString();
            }
            return output;
        }
    };


    struct StreamSchedule
    {
        StreamID stream_id;
        int pcp;
        std::vector<FrameSchedule> frames;

        [[nodiscard]] auto toString() const -> std::string
        {
            std::string output = "ScheduledStream id: " + std::to_string(stream_id) + ", PCP: " + std::to_string(pcp);
            for (const auto &f : frames)
            {
                output += f.toString();
            }
            return output;
        }
    };

    inline auto createFrameTransmission(const nlohmann::json &j) -> FrameTransmission
    {
        return FrameTransmission{.link_id = j["link_id"],
                                 .link_name = j["link_name"],
                                 .source = j["source"],
                                 .target = j["target"],
                                 .start = j["start"],
                                 .end = j["end"]};
    }

    inline auto createFrameSchedule(nlohmann::json j) -> FrameSchedule
    {
        std::vector<FrameTransmission> transmissions;
        transmissions.reserve(j["transmissions"].size());
        for (const auto &js : j["transmissions"])
        {
            transmissions.emplace_back(createFrameTransmission(js));
        }
        return FrameSchedule{
            .frame_number = j["frame_number"],
            .transmissions = std::move(transmissions),
        };
    }

    inline auto createStreamSchedule(const nlohmann::json &j) -> StreamSchedule
    {
        std::vector<FrameSchedule> frames;
        frames.reserve(j["frames"].size());
        for (const auto &js : j["frames"])
        {
            frames.emplace_back(createFrameSchedule(js));
        }

        return StreamSchedule{
            .stream_id = j["stream_id"],
            .pcp = j["pcp"],
            .frames = std::move(frames),
        };
    }
} // namespace tsndgm
