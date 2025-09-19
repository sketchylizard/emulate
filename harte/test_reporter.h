#pragma once

#include <fstream>
#include <memory>
#include <string>

#include "common/address.h"
#include "test_run.h"

namespace TestReporting
{

// Abstract base class for test reporting
class TestReporter
{
public:
  virtual ~TestReporter() = default;

  // Test lifecycle
  virtual void testSuiteStarted(const std::string& suite_name) = 0;
  virtual void testSuiteFinished() = 0;
  virtual void testCaseStarted(const std::string& test_name) = 0;
  virtual void testCaseFinished() = 0;

  // Section support for organized output
  virtual void sectionStarted(const std::string& section_name) = 0;
  virtual void sectionFinished() = 0;

  template<typename T>
  std::string toString(T val)
  {
    if constexpr (std::is_same_v<T, Common::Address>)
      return std::format("{:04x}", static_cast<uint16_t>(val));
    else if constexpr (std::is_same_v<T, uint8_t>)
      return std::format("{:02x}", val);
    else
      return std::to_string(val);
  }

  // Result reporting
  template<typename T>
  void report(const std::string& field, T expected, T actual)
  {
    auto expectedString = toString(expected);
    auto actualString = toString(actual);

    if (expected == actual)
    {
      reportMatch(field, expectedString);
    }
    else
    {
      reportMismatch(field, expectedString, actualString);
    }
  }

  template<typename T>
  void report(Common::Address address, uint16_t expected, uint16_t actual)
  {
    auto addressStr = std::format("{:04x}", address);

    if (expected == actual)
    {
      reportMatch(addressStr, std::to_string(actual));
    }
    else
    {
      reportMismatch(addressStr, std::to_string(expected), std::to_string(actual));
    }
  }

  void report(const Cycle& expected, const Cycle& actual)
  {
    static_cast<void>(expected);
    static_cast<void>(actual);
  }

  virtual void reportMatch(const std::string& field, const std::string& value) = 0;
  virtual void reportMismatch(const std::string& field, const std::string& expected, const std::string& actual) = 0;

  // Summary
  virtual void reportSummary(int total_tests, int passed_tests, int failed_tests) = 0;

  // Convenience method
  void reportResult(const std::string& field, const std::string& expected, const std::string& actual)
  {
    if (expected == actual)
    {
      reportMatch(field, actual);
    }
    else
    {
      reportMismatch(field, expected, actual);
    }
  }
};

// Unified terminal reporter - handles both minimal and verbose modes
class TerminalReporter : public TestReporter
{
private:
  bool colors_enabled;
  bool show_matches;  // true = verbose mode, false = minimal mode
  int current_test_failures = 0;
  int current_section_failures = 0;
  std::string current_test_name;
  std::string current_section_name;
  bool has_sections_in_current_test = false;

public:
  explicit TerminalReporter(bool show_all_results = false, bool enable_colors = true);

  void testSuiteStarted(const std::string& suite_name) override;
  void testSuiteFinished() override;
  void testCaseStarted(const std::string& test_name) override;
  void testCaseFinished() override;

  void sectionStarted(const std::string& section_name) override;
  void sectionFinished() override;

  void reportMatch(const std::string& field, const std::string& value) override;
  void reportMismatch(const std::string& field, const std::string& expected, const std::string& actual) override;
  void reportSummary(int total_tests, int passed_tests, int failed_tests) override;

private:
  void ensureTestHeaderShown();
  void ensureSectionHeaderShown();
};

// JSON reporter - outputs structured data for tooling
class JsonReporter : public TestReporter
{
private:
  std::ostream* output_stream;
  std::unique_ptr<std::ofstream> file_stream;
  bool first_test_case = true;
  bool first_section_in_case = true;
  bool first_result_in_section = true;
  std::string current_test_name;
  std::string current_section_name;
  bool in_section = false;

public:
  // Output to stdout
  JsonReporter();

  // Output to file
  explicit JsonReporter(const std::string& filename);

  ~JsonReporter();

  void testSuiteStarted(const std::string& suite_name) override;
  void testSuiteFinished() override;
  void testCaseStarted(const std::string& test_name) override;
  void testCaseFinished() override;

  void sectionStarted(const std::string& section_name) override;
  void sectionFinished() override;

  void reportMatch(const std::string& field, const std::string& value) override;
  void reportMismatch(const std::string& field, const std::string& expected, const std::string& actual) override;
  void reportSummary(int total_tests, int passed_tests, int failed_tests) override;

private:
  void writeEscapedString(const std::string& str);
  void ensureSectionStarted();
};

// Factory function to create reporters
enum class ReporterType
{
  Minimal,  // Terminal reporter, mismatches only
  Verbose,  // Terminal reporter, show all results
  Json  // JSON output
};

std::unique_ptr<TestReporter> createReporter(
    ReporterType type, bool enable_colors = true, const std::string& output_file = "");

// RAII helper for sections
class SectionScope
{
private:
  TestReporter* reporter;

public:
  SectionScope(TestReporter* r, const std::string& section_name)
    : reporter(r)
  {
    if (reporter)
      reporter->sectionStarted(section_name);
  }

  ~SectionScope()
  {
    if (reporter)
      reporter->sectionFinished();
  }

  // Non-copyable, non-movable
  SectionScope(const SectionScope&) = delete;
  SectionScope& operator=(const SectionScope&) = delete;
};

// Convenience macro for sections
#define TEST_SECTION(reporter, name) TestReporting::SectionScope _section_scope(reporter, name)

}  // namespace TestReporting
