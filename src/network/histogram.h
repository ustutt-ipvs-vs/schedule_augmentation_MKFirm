#ifndef TSN_DGM_HISTOGRAM_H
#define TSN_DGM_HISTOGRAM_H

#include "message_stream.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace tsndgm {

typedef unsigned long Count;
typedef std::map<Delay, Count> Histogram;

enum RTIPolicy { minimize_interval, minimize_dmax };

struct DelayHistogram {
  DelayHistogram(std::filesystem::path hist_path)
      : DelayHistogram(std::ifstream(hist_path)){};
  DelayHistogram(std::ifstream hist_istream)
      : DelayHistogram(nlohmann::json::parse(hist_istream)){};
  DelayHistogram(nlohmann::json hist_data);

  RTI compute_rti(double reliability, RTIPolicy policy = minimize_interval);

  Histogram histogram;
  Count size;
};

} // namespace tsndgm

#endif // TSN_DGM_HISTOGRAM_H
