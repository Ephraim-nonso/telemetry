cmake_minimum_required(VERSION 3.20)
project(telemetryd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(telemetryd
  src/main.cpp
  src/net/tcp_server.cpp
  src/net/protocol.cpp
  src/metrics/collector.cpp
  src/metrics/default_sources.cpp
  src/metrics/simulated_metrics.cpp
  src/util/time.cpp
)

target_include_directories(telemetryd PRIVATE include)

if(UNIX AND NOT APPLE)
  target_sources(telemetryd PRIVATE src/metrics/linux_metrics.cpp)
endif()

if(APPLE)
  target_sources(telemetryd PRIVATE src/metrics/macos_metrics.cpp)
endif()

if(WIN32)
  target_sources(telemetryd PRIVATE src/metrics/windows_metrics.cpp)
  target_link_libraries(telemetryd PRIVATE pdh)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(telemetryd PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()

include(CTest)
option(TELEMETRY_BUILD_TESTS "Build unit tests" ON)

if (TELEMETRY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()


