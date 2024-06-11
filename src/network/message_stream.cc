#include "message_stream.h"

namespace tsndgm
{

    nlohmann::json MessageStream::dump() const
    {
        nlohmann::json j = {
            {"route", {}},          {"name", name},  {"period", period}, {"frame_size", frame_size},
            {"deadline", deadline}, {"rti_map", {}},

        };

        for (const auto hop : route.route)
            j["route"].push_back(hop);

        return j;
    }
    auto createMessageStream(const nlohmann::json &j) -> MessageStream
    {
        return MessageStream{
            .id = j["id"],
            .name = j["name"],
            .period = j["cycle_time_ns"],
            .frame_size = j["frame_size_byte"],
            .deadline = j["deadline_ns"],
            .route = RouteWrapper(j["source"], j["target"]),
        };
    }

    void dump_streams(const std::vector<MessageStream> &streams, const std::filesystem::path &out)
    {
        nlohmann::json j = {};
        for (const MessageStream &stream : streams)
        {
            j.push_back(stream.dump());
        }

        std::ofstream o(out);
        o << std::setw(4) << j << std::endl;
    }

} // namespace tsndgm
