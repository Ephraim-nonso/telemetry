cmake_minimum_required(VERSION 3.20)

add_executable(telemetry_tests
  test_main.cpp
  test_protocol.cpp
  test_collector.cpp
  ../src/net/protocol.cpp
  ../src/metrics/collector.cpp
)

target_include_directories(telemetry_tests PRIVATE ../include .)

add_test(NAME telemetry_tests COMMAND telemetry_tests)


