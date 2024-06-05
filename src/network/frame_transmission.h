#pragma once
#include <filesystem>
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



    private:
        
    };

} // namespace tsndgm

