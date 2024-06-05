#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "frame_schedule.h"


namespace tsndgm
{
    class StreamSchedule
    {
    public:
        int stream_id;
        int pcp;
        std::vector<FrameSchedule> frames;


        StreamSchedule(int stream_id, int pcp, std::vector<FrameSchedule> frames) :
            stream_id(stream_id), pcp(pcp), frames(frames) {}


        StreamSchedule(nlohmann::json j);



    private:
        
    };


    std::vector<StreamSchedule> load_schedule(const std::filesystem::path &in);
} // namespace tsndgm
