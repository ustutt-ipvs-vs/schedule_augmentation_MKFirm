#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>


namespace tsndgm
{
    class FrameTransmission
    {
    public:
        std::string egress_port;
        unsigned int start;
        unsigned int end;


        FrameTransmission(std::string egress_port, unsigned int start, unsigned int end):
            egress_port(egress_port), start(start), end(end) {}


        FrameTransmission(nlohmann::json j) {
            *this = FrameTransmission(j["egress_port"], j["start"], j["end"]);
        };

        std::string toString() const;

    private:
        
    };


    class FrameSchedule
    {
    public:
        unsigned int frame_number;
        std::vector<FrameTransmission> transmissions;


        FrameSchedule(unsigned int frame_number, std::vector<FrameTransmission> transmissions) :
            frame_number(frame_number), transmissions(transmissions) {}


        FrameSchedule(nlohmann::json j);

        std::string toString() const;

    private:
        
    };


    class StreamSchedule
    {
    public:
        int stream_id;
        int pcp;
        std::vector<FrameSchedule> frames;


        StreamSchedule(int stream_id, int pcp, std::vector<FrameSchedule> frames) :
            stream_id(stream_id), pcp(pcp), frames(frames) {}


        StreamSchedule(nlohmann::json j);

        std::string toString() const;

    private:
        
    };

    std::vector<StreamSchedule> load_schedule(const std::filesystem::path &in);

} // namespace tsndgm
