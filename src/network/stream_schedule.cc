#include "stream_schedule.h"

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
