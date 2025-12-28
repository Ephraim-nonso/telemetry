#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

namespace telemetry::tests {

struct TestCase final {
  std::string_view name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

struct Register final {
  Register(std::string_view name, void (*fn)()) { registry().push_back(TestCase{name, fn}); }
};

inline int run_all() {
  int failed = 0;
  for (const auto& tc : registry()) {
    try {
      tc.fn();
      std::cerr << "[PASS] " << tc.name << "\n";
    } catch (const std::exception& e) {
      ++failed;
      std::cerr << "[FAIL] " << tc.name << ": " << e.what() << "\n";
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << tc.name << ": unknown exception\n";
    }
  }
  return failed == 0 ? 0 : 1;
}

struct RequireFailure final : public std::exception {
  explicit RequireFailure(const char* msg) : msg_(msg ? msg : "REQUIRE failed") {}
  const char* what() const noexcept override { return msg_; }
  const char* msg_;
};

}  // namespace telemetry::tests

#define TELEMETRY_CONCAT2(a, b) a##b
#define TELEMETRY_CONCAT(a, b) TELEMETRY_CONCAT2(a, b)

#define TELEMETRY_TEST_CASE(name)                                                             \
  static void TELEMETRY_CONCAT(telemetry_test_fn_, __LINE__)();                                \
  static ::telemetry::tests::Register TELEMETRY_CONCAT(telemetry_test_reg_, __LINE__)(         \
      name, &TELEMETRY_CONCAT(telemetry_test_fn_, __LINE__));                                  \
  static void TELEMETRY_CONCAT(telemetry_test_fn_, __LINE__)()

#define REQUIRE(expr)                                    \
  do {                                                   \
    if (!(expr)) throw ::telemetry::tests::RequireFailure(#expr); \
  } while (0)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))


