#include "test_reporter.h"

#include <iomanip>
#include <iostream>

namespace TestReporting
{

//////////////////////////////////////////////////////////////////////////////
// TerminalReporter Implementation
//////////////////////////////////////////////////////////////////////////////

TerminalReporter::TerminalReporter(bool show_all_results, bool enable_colors)
  : colors_enabled(enable_colors)
  , show_matches(show_all_results)
{
}

void TerminalReporter::testSuiteStarted(const std::string& suite_name)
{
  if (colors_enabled)
  {
    std::cout << "\033[1;36m=== Running Test Suite: " << suite_name << " ===\033[0m\n" << std::endl;
  }
  else
  {
    std::cout << "=== Running Test Suite: " << suite_name << " ===" << std::endl;
  }
}

void TerminalReporter::testSuiteFinished()
{
  std::cout << std::endl;
}

void TerminalReporter::testCaseStarted(const std::string& test_name)
{
  current_test_name = test_name;
  current_test_failures = 0;
  has_sections_in_current_test = false;
}

void TerminalReporter::testCaseFinished()
{
  if (current_test_failures == 0 && !has_sections_in_current_test)
  {
    // Simple test with no failures and no sections - show success
    if (colors_enabled)
    {
      std::cout << "\033[32m✓\033[0m " << current_test_name << std::endl;
    }
    else
    {
      std::cout << "✓ " << current_test_name << std::endl;
    }
  }
  else if (current_test_failures == 0 && has_sections_in_current_test)
  {
    // Test with sections that all passed - show success
    if (colors_enabled)
    {
      std::cout << "\033[32m✓\033[0m " << current_test_name << " (all sections passed)" << std::endl;
    }
    else
    {
      std::cout << "✓ " << current_test_name << " (all sections passed)" << std::endl;
    }
  }
  // If there were failures, the header was already shown by ensureTestHeaderShown()

  current_test_name.clear();
  current_test_failures = 0;
  has_sections_in_current_test = false;
}

void TerminalReporter::sectionStarted(const std::string& section_name)
{
  current_section_name = section_name;
  current_section_failures = 0;
  has_sections_in_current_test = true;
}

void TerminalReporter::sectionFinished()
{
  if (current_section_failures == 0 && show_matches)
  {
    ensureTestHeaderShown();
    if (colors_enabled)
    {
      std::cout << "  \033[32m✓\033[0m Section: " << current_section_name << std::endl;
    }
    else
    {
      std::cout << "  ✓ Section: " << current_section_name << std::endl;
    }
  }
  current_section_name.clear();
  current_section_failures = 0;
}

void TerminalReporter::reportMatch(const std::string& field, const std::string& value)
{
  if (show_matches)
  {
    ensureTestHeaderShown();
    ensureSectionHeaderShown();

    if (colors_enabled)
    {
      std::cout << "    \033[32m✓\033[0m " << field << ": " << value << std::endl;
    }
    else
    {
      std::cout << "    ✓ " << field << ": " << value << std::endl;
    }
  }
}

void TerminalReporter::reportMismatch(const std::string& field, const std::string& expected, const std::string& actual)
{
  ensureTestHeaderShown();
  ensureSectionHeaderShown();

  current_test_failures++;
  current_section_failures++;

  if (colors_enabled)
  {
    std::cout << "    \033[31m✗\033[0m " << field << ": expected \033[33m" << expected << "\033[0m, got \033[31m"
              << actual << "\033[0m" << std::endl;
  }
  else
  {
    std::cout << "    ✗ " << field << ": expected " << expected << ", got " << actual << std::endl;
  }
}

void TerminalReporter::reportSummary(int total_tests, int passed_tests, int failed_tests)
{
  std::cout << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  if (colors_enabled)
  {
    std::cout << "\033[1;36mTEST SUMMARY\033[0m" << std::endl;
  }
  else
  {
    std::cout << "TEST SUMMARY" << std::endl;
  }

  std::cout << std::string(60, '=') << std::endl;
  std::cout << "Total tests:    " << total_tests << std::endl;

  if (colors_enabled)
  {
    std::cout << "Passed:         \033[32m" << passed_tests << "\033[0m" << std::endl;
    if (failed_tests > 0)
    {
      std::cout << "Failed:         \033[31m" << failed_tests << "\033[0m" << std::endl;
    }
    else
    {
      std::cout << "Failed:         " << failed_tests << std::endl;
    }
  }
  else
  {
    std::cout << "Passed:         " << passed_tests << std::endl;
    std::cout << "Failed:         " << failed_tests << std::endl;
  }

  double pass_rate = total_tests > 0 ? (static_cast<double>(passed_tests) / total_tests * 100.0) : 0.0;
  std::cout << "Pass rate:      " << std::fixed << std::setprecision(1) << pass_rate << "%" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  if (failed_tests == 0 && total_tests > 0)
  {
    if (colors_enabled)
    {
      std::cout << "\033[32m🎉 All tests passed!\033[0m" << std::endl;
    }
    else
    {
      std::cout << "🎉 All tests passed!" << std::endl;
    }
  }
}

void TerminalReporter::ensureTestHeaderShown()
{
  static std::string last_shown_test;
  if (last_shown_test != current_test_name)
  {
    if (colors_enabled)
    {
      std::cout << "\033[1;33m" << current_test_name << "\033[0m" << std::endl;
    }
    else
    {
      std::cout << current_test_name << std::endl;
    }
    last_shown_test = current_test_name;
  }
}

void TerminalReporter::ensureSectionHeaderShown()
{
  if (!current_section_name.empty())
  {
    static std::string last_shown_section;
    std::string section_key = current_test_name + "::" + current_section_name;
    if (last_shown_section != section_key)
    {
      if (colors_enabled)
      {
        std::cout << "  \033[36m" << current_section_name << "\033[0m" << std::endl;
      }
      else
      {
        std::cout << "  " << current_section_name << std::endl;
      }
      last_shown_section = section_key;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// JsonReporter Implementation
//////////////////////////////////////////////////////////////////////////////

JsonReporter::JsonReporter()
  : output_stream(&std::cout)
{
}

JsonReporter::JsonReporter(const std::string& filename)
{
  file_stream = std::make_unique<std::ofstream>(filename);
  if (!file_stream->is_open())
  {
    throw std::runtime_error("Failed to open output file: " + filename);
  }
  output_stream = file_stream.get();
}

JsonReporter::~JsonReporter()
{
  if (file_stream)
  {
    file_stream->close();
  }
}

void JsonReporter::testSuiteStarted(const std::string& suite_name)
{
  *output_stream << "{\n";
  *output_stream << "  \"suite_name\": ";
  writeEscapedString(suite_name);
  *output_stream << ",\n";
  *output_stream << "  \"test_cases\": [\n";
  first_test_case = true;
}

void JsonReporter::testSuiteFinished()
{
  *output_stream << "\n  ]\n";
  *output_stream << "}\n";
  output_stream->flush();
}

void JsonReporter::testCaseStarted(const std::string& test_name)
{
  if (!first_test_case)
  {
    *output_stream << ",\n";
  }
  first_test_case = false;

  *output_stream << "    {\n";
  *output_stream << "      \"test_name\": ";
  writeEscapedString(test_name);
  *output_stream << ",\n";
  *output_stream << "      \"sections\": [\n";

  current_test_name = test_name;
  first_section_in_case = true;
  in_section = false;
}

void JsonReporter::testCaseFinished()
{
  if (in_section)
  {
    *output_stream << "\n        ]\n";
    *output_stream << "      }\n";
    in_section = false;
  }
  *output_stream << "      ]\n";
  *output_stream << "    }";

  current_test_name.clear();
}

void JsonReporter::sectionStarted(const std::string& section_name)
{
  ensureSectionStarted();
  current_section_name = section_name;
}

void JsonReporter::sectionFinished()
{
  // Section closing handled in ensureSectionStarted or testCaseFinished
  current_section_name.clear();
}

void JsonReporter::reportMatch(const std::string& field, const std::string& value)
{
  ensureSectionStarted();

  if (!first_result_in_section)
  {
    *output_stream << ",\n";
  }
  first_result_in_section = false;

  *output_stream << "          {\n";
  *output_stream << "            \"field\": ";
  writeEscapedString(field);
  *output_stream << ",\n";
  *output_stream << "            \"status\": \"pass\",\n";
  *output_stream << "            \"value\": ";
  writeEscapedString(value);
  *output_stream << "\n";
  *output_stream << "          }";
}

void JsonReporter::reportMismatch(const std::string& field, const std::string& expected, const std::string& actual)
{
  ensureSectionStarted();

  if (!first_result_in_section)
  {
    *output_stream << ",\n";
  }
  first_result_in_section = false;

  *output_stream << "          {\n";
  *output_stream << "            \"field\": ";
  writeEscapedString(field);
  *output_stream << ",\n";
  *output_stream << "            \"status\": \"fail\",\n";
  *output_stream << "            \"expected\": ";
  writeEscapedString(expected);
  *output_stream << ",\n";
  *output_stream << "            \"actual\": ";
  writeEscapedString(actual);
  *output_stream << "\n";
  *output_stream << "          }";
}

void JsonReporter::reportSummary(int /*total_tests*/, int /*passed_tests*/, int /*failed_tests*/)
{
  // Summary is not included in the JSON output as it can be calculated from the data
  // This keeps the JSON focused on the raw test results
}

void JsonReporter::writeEscapedString(const std::string& str)
{
  *output_stream << "\"";
  for (char c : str)
  {
    switch (c)
    {
      case '"': *output_stream << "\\\""; break;
      case '\\': *output_stream << "\\\\"; break;
      case '\b': *output_stream << "\\b"; break;
      case '\f': *output_stream << "\\f"; break;
      case '\n': *output_stream << "\\n"; break;
      case '\r': *output_stream << "\\r"; break;
      case '\t': *output_stream << "\\t"; break;
      default:
        if (c < 0x20)
        {
          *output_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        }
        else
        {
          *output_stream << c;
        }
        break;
    }
  }
  *output_stream << "\"";
}

void JsonReporter::ensureSectionStarted()
{
  if (!in_section)
  {
    if (!first_section_in_case)
    {
      *output_stream << ",\n";
    }
    first_section_in_case = false;

    *output_stream << "        {\n";
    *output_stream << "          \"section_name\": ";
    writeEscapedString(current_section_name.empty() ? "default" : current_section_name);
    *output_stream << ",\n";
    *output_stream << "          \"results\": [\n";

    first_result_in_section = true;
    in_section = true;
  }
}

//////////////////////////////////////////////////////////////////////////////
// Factory Function
//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<TestReporter> createReporter(ReporterType type, bool enable_colors, const std::string& output_file)
{
  switch (type)
  {
    case ReporterType::Minimal: return std::make_unique<TerminalReporter>(false, enable_colors);

    case ReporterType::Verbose: return std::make_unique<TerminalReporter>(true, enable_colors);

    case ReporterType::Json:
      if (output_file.empty())
      {
        return std::make_unique<JsonReporter>();
      }
      else
      {
        return std::make_unique<JsonReporter>(output_file);
      }
  }
}

}  // namespace TestReporting
