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
        unsigned int link_id;
        std::string link_name;
        unsigned int source;
        unsigned int target;
        unsigned int start;
        unsigned int end;


        FrameTransmission(unsigned int link_id, std::string link_name, unsigned int source, unsigned int target, unsigned int start, unsigned int end):
            link_id(link_id), link_name(link_name), source(source), target(target), start(start), end(end) {}


        FrameTransmission(nlohmann::json j) {
            *this = FrameTransmission(j["link_id"], j["link_name"], j["source"], j["target"], j["start"], j["end"]);
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
