#include "histogram.h"

namespace tsndgm {

DelayHistogram::DelayHistogram(nlohmann::json hist_data)
    : size(0), histogram({}) {
  Delay lower_bound = 0;
  for (auto bin : hist_data) {
    if (!histogram.empty()) {
      histogram[lower_bound] = bin["count"].template get<Delay>();
      size += histogram[lower_bound];
    }
    if (!bin["upper_bound"].is_null()) {
      std::string ub = bin["upper_bound"].template get<std::string>();
      std::string delay_str = ub.substr(0, ub.find(" "));
      lower_bound = static_cast<Delay>(stod(delay_str) * 1e6);
      histogram[lower_bound] = 0;
    }
  }
}

RTI DelayHistogram::compute_rti(double reliability, RTIPolicy policy) {
  if (policy == minimize_interval) {
    RTI rti(std::numeric_limits<Delay>::max());
    for (auto min_it = histogram.begin(); min_it != histogram.end(); ++min_it) {
      Count c = 0;
      auto max_it = min_it;
      for (; max_it != histogram.end(); ++max_it) {
        if (c >= size * reliability) {
          if (max_it->first - min_it->first < rti.d_max() - rti.d_min())
            rti = {max_it->first, min_it->first};
          break;
        }
        c += max_it->second;
      }
    }
    return rti;
  } else if (policy == minimize_dmax) {
    Count c = 0;
    Delay min = histogram.begin()->first;
    for (auto &bin : histogram) {
      if (c >= size * reliability)
        return RTI(bin.first, min);
      c += bin.second;
    }
  }

  throw std::logic_error("invalid RTIPolicy");
}

} // namespace tsndgm
