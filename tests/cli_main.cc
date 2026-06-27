#include "del/ast.h"
#include "del/context.h"
#include "del/exception.h"
#include "del/lexer.h"
#include "del/parser.h"
#include "del/symbol_table.h"
#include "del/template_engine.h"
#include "del/version.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>


#include "args_helper.h"

namespace {

void print_help() {
  std::cout << "Usage: del_cli.exe [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -h, --help            Print this help message" << std::endl;
  std::cout << "  -v, --version         Print the version number" << std::endl;
  std::cout << "  -t, --template <file> Specify the template file" << std::endl;
  std::cout << "  -s, --source <file>   Specify the source file" << std::endl;
  std::cout << "  -o, --output <file>   Specify the output file" << std::endl;
  std::cout << std::endl;
  std::cout << "Example:" << std::endl;
  std::cout << "  del_cli.exe --template template.json --source source.json --output output.json" << std::endl;
  std::cout << "  del_cli.exe --template template.json --source source.json" << std::endl;
  std::cout << "  del_cli.exe --version" << std::endl;
  std::cout << "  del_cli.exe --help" << std::endl;
  std::cout << std::endl;
}

namespace fs = std::filesystem;
std::optional<nlohmann::json> load_json_file(fs::path const& path) {
  if (!fs::exists(path)) {
    return std::nullopt;
  }
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return std::nullopt;
  }
  try {
    return nlohmann::json::parse(ifs);
  } catch (std::exception const& e) {
    std::cerr << "Failed to parse JSON file: " << e.what() << std::endl;
    return std::nullopt;
  }
}


int template_convert(
    std::string const&                template_file,
    std::string const&                source_file,
    std::optional<std::string> const& output_file
) {
  auto template_json = load_json_file(template_file);
  auto source_json   = load_json_file(source_file);
  if (!template_json || !source_json) {
    std::cerr << "Failed to load JSON files" << std::endl;
    return 1;
  }

  del::TemplateEngine engine;

  std::cout << "Compiling template..." << std::endl;

  std::optional<del::CompiledTemplate> compiled_template;
  try {
    compiled_template = engine.Compile(*template_json);
  } catch (std::exception const& e) {
    std::cerr << "Failed to compile template: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Compiling template...OK" << std::endl;

  std::cout << "Executing template..." << std::endl;

  try {
    auto result = engine.Execute(*compiled_template, *source_json);
    if (!output_file) {
      std::cout << result.dump(2) << std::endl;
    } else {
      std::ofstream ofs(*output_file);
      ofs << result.dump(2) << std::endl;
      ofs.close();
    }
  } catch (std::exception const& e) {
    std::cerr << "Failed to execute template: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}

int benchmark(std::string const& template_file, std::string const& source_file) {
  using Clock = std::chrono::high_resolution_clock;

  auto template_json = load_json_file(template_file);
  auto source_json   = load_json_file(source_file);

  if (!template_json || !source_json) {
    std::cerr << "Failed to load JSON files" << std::endl;
    return 1;
  }

  del::TemplateEngine engine;

  std::cout << "Compiling template..." << std::endl;

  std::optional<del::CompiledTemplate> compiled_template;
  try {
    auto compile_start = Clock::now(); // Start time
    compiled_template  = engine.Compile(*template_json);
    auto compile_end   = Clock::now(); // End time

    auto   total_us = std::chrono::duration_cast<std::chrono::microseconds>(compile_end - compile_start).count();
    double total_ms = static_cast<double>(total_us) / 1000.0;

    std::cout << "Compile time: " << total_us << " us" << std::endl;
    std::cout << "Compile time: " << total_ms << " ms" << std::endl;

  } catch (std::exception const& e) {
    std::cerr << "Failed to compile template: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Compiling template...OK" << std::endl;

  std::cout << "Executing template..." << std::endl;

  try {
    int iterations = 10000;

    auto execute_start = Clock::now(); // Start time

    auto& ct = *compiled_template;
    auto& sj = *source_json;
    for (int i = 0; i < iterations; ++i) {
      (void)engine.Execute(ct, sj);
    }
    auto execute_end = Clock::now(); // End time

    auto   total_us   = std::chrono::duration_cast<std::chrono::microseconds>(execute_end - execute_start).count();
    double total_ms   = static_cast<double>(total_us) / 1000.0;
    double average_us = static_cast<double>(total_us) / iterations;
    double ops        = (iterations / total_ms) * 1000.0;

    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Execution time: " << total_us << " us" << std::endl;
    std::cout << "Execution time: " << total_ms << " ms" << std::endl;
    std::cout << "Average execution time: " << average_us << " us" << std::endl;
    std::cout << "Operations per second: " << ops << std::endl;

  } catch (std::exception const& e) {
    std::cerr << "Failed to execute template: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}

} // namespace


int main(int argc, char** argv) {
  ArgsParser args{argc, argv};

  if (args.has_flag("-h") || args.has_flag("--help")) {
    print_help();
    return 0;
  }

  if (args.has_flag("-v") || args.has_flag("--version")) {
    std::cout << "del_cli.exe v" << del::kVersion << std::endl;
    return 0;
  }

  auto source_file = args.get_option("-s");
  if (!source_file) source_file = args.get_option("--source");
  if (!source_file) {
    std::cerr << "Error: No source file specified" << std::endl;
    return 1;
  }

  auto template_file = args.get_option("-t");
  if (!template_file) template_file = args.get_option("--template");
  if (!template_file) {
    std::cerr << "Error: No template file specified" << std::endl;
    return 1;
  }

  auto output_file = args.get_option("-o");
  if (!output_file) output_file = args.get_option("--output");

  std::cout << "Source file: " << *source_file << std::endl;
  std::cout << "Template file: " << *template_file << std::endl;
  std::cout << "Output file: " << output_file.value_or("") << std::endl;

  if (args.has_flag("--benchmark")) {
    return benchmark(*template_file, *source_file);
  }

  return template_convert(*template_file, *source_file, output_file);
}