#include "inputLoader.h"

#include "../util/constants.h"
#include <fstream>
#include <iostream>
#include <src/network/message_stream.h>

auto io::load_emergency_traffic(const FilePath &in) -> std::vector<tsndgm::EmergencyStream> {
  try {
    std::ifstream i(in);
    check_file_loading(i, in);

    nlohmann::json j = nlohmann::json::parse(i);

    std::vector<tsndgm::EmergencyStream> streams;
    streams.reserve(j.size());
    for (const auto &js : j) {
      streams.emplace_back(tsndgm::createEmergencyStream(js));
    }

    return streams;
  } catch (nlohmann::json::parse_error &e) {
    std::cout << "Error parsing json file: " << in.string() << "\n";
    std::cout << e.what() << std::endl;
    std::exit(error_codes::JSON_PARSING_FAILED);
  }
}

auto io::load_time_triggered_traffic(const FilePath &in)
    -> std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> {
  try {
    std::ifstream i(in);
    check_file_loading(i, in);
    nlohmann::json j = nlohmann::json::parse(i);

    std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> streams;
    streams.reserve(j.size());
    for (const auto &js : j) {
      auto stream = tsndgm::createMessageStream(js);
      streams[stream.id] = std::move(stream);
    }

    return streams;
  } catch (nlohmann::json::parse_error &e) {
    std::cout << "Error parsing json file: " << in.string() << "\n";
    std::cout << e.what() << std::endl;
    std::exit(error_codes::JSON_PARSING_FAILED);
  }
}

auto io::load_schedule(const std::filesystem::path &in) -> std::vector<tsndgm::StreamSchedule> {
  try {
    std::ifstream i(in);
    check_file_loading(i, in);
    nlohmann::json j = nlohmann::json::parse(i);

    std::vector<tsndgm::StreamSchedule> scheduled_streams;
    scheduled_streams.reserve(j.size());
    for (const auto &js : j) {
      scheduled_streams.emplace_back(tsndgm::createStreamSchedule(js));
    }
    return scheduled_streams;
  } catch (nlohmann::json::parse_error &e) {
    std::cout << "Error parsing json file: " << in.string() << "\n";
    std::cout << e.what() << std::endl;
    std::exit(error_codes::JSON_PARSING_FAILED);
  }
}

auto io::set_routes(const std::vector<tsndgm::StreamSchedule> &schedules,
                    std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> &streams) -> void {
  for (const auto &current_stream : schedules) {
    streams.at(current_stream.stream_id).route.route = build_route(current_stream);
  }
}
auto io::load_topology(const FilePath &in) -> tsndgm::NetworkTopology {
  try {
    std::ifstream i(in);
    check_file_loading(i, in);
    nlohmann::json j = nlohmann::json::parse(i);

    tsndgm::NetworkTopology topology;

    for (auto n : j["nodes"]) {
      tsndgm::NetworkDeviceProperty device(n["id"], n["processing_delay_ns"], n["name"], n["queues_per_port"]);
      topology.add_device(device);
    }

    for (auto l : j["links"]) {
      tsndgm::Edge edge(l["source"], l["target"]);
      const auto link = tsndgm::DataLinkProperty{l["id"], l["name"], tsndgm::mbps_to_DataRate(l["link_speed_mbps"]),
                                                 l["propagation_delay_ns"]};
      topology.add_data_link({edge, link});
    }

    return topology;
  } catch (nlohmann::json::parse_error &e) {
    std::cout << "Error parsing json file: " << in.string() << "\n";
    std::cout << e.what() << std::endl;
    std::exit(error_codes::JSON_PARSING_FAILED);
  }
}

auto io::check_file_loading(const std::ifstream &i, const FilePath &in) -> void {
  if (not i.good()) {
    std::cout << "Error opening file: " << in.string() << "\n";
    std::exit(error_codes::FILE_NOT_FOUND);
  }
}
