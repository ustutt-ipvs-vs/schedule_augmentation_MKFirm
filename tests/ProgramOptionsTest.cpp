#include <IO/programOptions.h>
#include <gtest/gtest.h>

#include "CLI/Error.hpp"

TEST(ProgramOptionsTest, CorrectLoading) {
  std::vector<std::string> arguments = {
      "-t", "topology.txt",          "-s", "tt_streams.txt",
      "-e", "emergency_streams.txt", "-z", "schedule.txt"};

  const auto options = io::ProgramOptions(arguments);

  EXPECT_EQ(options.getTopologyPath(), "topology.txt");
  EXPECT_EQ(options.getTimeTriggeredStreamsPath(), "tt_streams.txt");
  EXPECT_EQ(options.getEmergencyStreams(), "emergency_streams.txt");
  EXPECT_EQ(options.getSchedulePath(), "schedule.txt");
}

TEST(ProgramOptionsTest, MissingArguments) {
  std::vector<std::string> arguments = {"-s", "tt_streams.txt",
                                        "-e", "emergency_streams.txt",
                                        "-z", "schedule.txt"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);

  arguments = {"-t", "topology.txt", "-e", "emergency_streams.txt",
               "-z", "schedule.txt"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);

  arguments = {"-t", "topology.txt", "-s", "tt_streams.txt",
               "-z", "schedule.txt"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);

  arguments = {"-t", "topology.txt",         "-s", "tt_streams.txt",
               "-e", "emergency_streams.txt"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);
}

TEST(ProgramOptionsTest, HelpArgument) {
  std::vector<std::string> arguments = {"-h"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);
}

TEST(ProgramOptionsTest, WrongArgument) {
  std::vector<std::string> arguments = {"-t", "topology.txt",         "-s", "tt_streams.txt",
               "-u", "emergency_streams.txt"};
  ASSERT_THROW(io::ProgramOptions{arguments}, std::runtime_error);
}
