#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "frame_transmission.h"


namespace tsndgm
{
    class FrameSchedule
    {
    public:
        unsigned int frame_number;
        std::vector<FrameTransmission> transmissions;


        FrameSchedule(unsigned int frame_number, std::vector<FrameTransmission> transmissions) :
            frame_number(frame_number), transmissions(transmissions) {}


        FrameSchedule(nlohmann::json j);



    private:
        
    };

} // namespace tsndgm

