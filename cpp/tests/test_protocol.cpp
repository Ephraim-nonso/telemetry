#include "minitest.h"
#include "telemetry/net/protocol.h"

using telemetry::net::CommandType;
using telemetry::net::parse_command;

TELEMETRY_TEST_CASE("parse_command handles basic commands") {
  REQUIRE(parse_command("PING").type == CommandType::kPing);
  REQUIRE(parse_command("GET").type == CommandType::kGet);
  REQUIRE(parse_command("RESTART").type == CommandType::kRestart);
}

TELEMETRY_TEST_CASE("parse_command handles throttle") {
  {
    const auto pc = parse_command("THROTTLE 0");
    REQUIRE(pc.type == CommandType::kThrottle);
    REQUIRE(pc.ok);
    REQUIRE(pc.throttle_ms == 0);
  }
  {
    const auto pc = parse_command("THROTTLE 500");
    REQUIRE(pc.type == CommandType::kThrottle);
    REQUIRE(pc.ok);
    REQUIRE(pc.throttle_ms == 500);
  }
}

TELEMETRY_TEST_CASE("parse_command rejects invalid throttle") {
  {
    const auto pc = parse_command("THROTTLE ");
    REQUIRE(pc.type == CommandType::kThrottle);
    REQUIRE_FALSE(pc.ok);
  }
  {
    const auto pc = parse_command("THROTTLE abc");
    REQUIRE(pc.type == CommandType::kThrottle);
    REQUIRE_FALSE(pc.ok);
  }
  {
    const auto pc = parse_command("THROTTLE 70000");
    REQUIRE(pc.type == CommandType::kThrottle);
    REQUIRE_FALSE(pc.ok);
  }
}

TELEMETRY_TEST_CASE("parse_command unknown") {
  REQUIRE(parse_command("HELLO").type == CommandType::kUnknown);
}


