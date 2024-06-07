#include "scheduleLoader.h"

namespace tsndgm
{
    StreamSchedule::StreamSchedule(nlohmann::json j)
    {
        std::vector<FrameSchedule> frames;
        frames.reserve(j["frames"].size());
        for (const auto &js : j["frames"])
        {
            frames.emplace_back(js);
        }
        *this = StreamSchedule(j["stream_id"], j["pcp"], frames);
    }

    std::string StreamSchedule::toString() const {
        std::string output = "ScheduledStream id: " + std::to_string(stream_id) + ", PCP: " + std::to_string(pcp);
        for(FrameSchedule i : frames){
            output += i.toString();
        }
        return output;
    }

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

    std::string FrameSchedule::toString() const {
        std::string output = "\n|-ScheduledFrame number: " + std::to_string(frame_number);
        for(FrameTransmission i : transmissions){
            output += i.toString();
        }
        return output;
    }

    std::string FrameTransmission::toString() const {
        return "\n|--link_id: " + std::to_string(link_id) + ", link_name: " + link_name + ", source: " + std::to_string(source) + ", target: " + std::to_string(target) + ", start: " + std::to_string(start) + ", end: " + std::to_string(end);
    }

    std::vector<StreamSchedule> load_schedule(const std::filesystem::path &in)
    {
        try
        {
            std::ifstream i(in);
            nlohmann::json j = nlohmann::json::parse(i);

            std::vector<StreamSchedule> scheduled_streams;
            scheduled_streams.reserve(j.size());
            for (const auto &js : j)
            {
                scheduled_streams.emplace_back(js);
            }
            return scheduled_streams;
        }
        catch (nlohmann::json::parse_error &e)
        {
            std::cout << "Error parsing json file: " << in.string() << "\n";
            std::cout << e.what() << std::endl;
            std::exit(3);
        }
    }
} // namespace tsndgm
