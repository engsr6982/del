#include "del/parser.h"
#include "del/version.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>

namespace {

std::vector<std::string> AsVectorArgs(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return args;
}

void PrintHelp() {
  std::cout << "Usage: del_test.exe [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --help            Print this help message" << std::endl;
  std::cout << "  --version         Print the version number" << std::endl;
  std::cout << "  --template <file> Specify the template file" << std::endl;
  std::cout << "  --source <file>   Specify the source file" << std::endl;
  std::cout << "  --output <file>   Specify the output file" << std::endl;
  std::cout << "Example:" << std::endl;
  std::cout << "  del_test.exe --template template.json --source source.json --output output.json" << std::endl;
  std::cout << "  del_test.exe --version" << std::endl;
  std::cout << "  del_test.exe --help" << std::endl;
}

void PrintVersion() { std::cout << "del_test.exe " << del::kVersion << std::endl; }

struct Options {
  std::string template_file_;
  std::string source_file_;
  std::string output_file_;
};

Options ParseArgs(std::vector<std::string> args) {
  Options options;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--help") {
      PrintHelp();
      exit(0);
    }
    if (args[i] == "--version") {
      PrintVersion();
      exit(0);
    }
    if (args[i] == "--template") {
      options.template_file_ = args[++i];
    } else if (args[i] == "--source") {
      options.source_file_ = args[++i];
    } else if (args[i] == "--output") {
      options.output_file_ = args[++i];
    }
  }
  return options;
}

namespace fs = std::filesystem;
std::optional<nlohmann::json> LoadJsonFile(fs::path const& path) {
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


} // namespace


int main(int argc, char** argv) {
  system("chcp 65001");

  //   auto args    = AsVectorArgs(argc, argv);
  //   auto options = ParseArgs(args);

  //   std::cout << "Template file: " << options.template_file_ << std::endl;
  //   std::cout << "Source file: " << options.source_file_ << std::endl;
  //   std::cout << "Output file: " << options.output_file_ << std::endl;

  //   std::cout << "Loading JSON files..." << std::endl;
  //   auto template_json = LoadJsonFile(options.template_file_);
  //   auto source_json   = LoadJsonFile(options.source_file_);

  //   if (!template_json || !source_json) {
  //     std::cerr << "Failed to load JSON files" << std::endl;
  //     return 1;
  //   }

  //   std::cout << "Loading JSON files...OK" << std::endl;


  del::TemplateEngine engine;

  //   std::cout << "Compiling template..." << std::endl;

  //   std::optional<del::CompiledTemplate> compiled_template;
  //   try {
  //     compiled_template = engine.Compile(*template_json);
  //   } catch (std::exception const& e) {
  //     std::cerr << "Failed to compile template: " << e.what() << std::endl;
  //   }
  //   assert(compiled_template);

  //   std::cout << "Compiling template...OK" << std::endl;

  //   std::cout << "Executing template..." << std::endl;
  //   try {
  //     auto result = engine.Execute(*compiled_template, *source_json);
  //     if (options.output_file_.empty()) {
  //       std::cout << result.dump(2) << std::endl;
  //     } else {
  //       std::ofstream ofs(options.output_file_);
  //       ofs << result.dump(2) << std::endl;
  //       ofs.close();
  //     }
  //   } catch (std::exception const& e) {
  //     std::cerr << "Failed to execute template: " << e.what() << std::endl;
  //   }

  std::cout << "================= 动态对象转换 =================" << std::endl;

  nlohmann::json source_1 = nlohmann::json::parse(R"(
    {
      "player_permissions": {
        "4A2E-8F1A-99BC": 1,
        "1111-2222-3333": 2
      }
    }
  )");

  nlohmann::ordered_json template_1 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/mPermissions": "map_object(@/player_permissions, kv -> entry(key(kv) |> remove_char('-'), val(kv) == 1 ? \"admin\" : \"member\") ) "
    }
  )");

  try {
    // 编译一次模板
    auto compiled_t1 = engine.Compile(template_1);

    // 执行转换
    nlohmann::json result_1 = engine.Execute(compiled_t1, source_1);
    std::cout << result_1.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Scenario 1 Error: " << e.what() << std::endl;
  }

  std::cout << "\n================= 全场景清洗 =================" << std::endl;

  nlohmann::json source_2 = nlohmann::json::parse(R"(
    {
      "legacy_meta": {
        "uuid": "4A2E-8F1A-99BC",
        "display": "  我的领地-- ",
        "status": 1
      },
      "member_list": ["aaa-111", "bbb-222"]
    }
  )");

  nlohmann::ordered_json template_2 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/land_info/owner_id": "@/legacy_meta/uuid |> remove_char('-') |> to_lower() ",
      "/land_info/clean_name": "@/legacy_meta/display |> trim() |> remove_suffix('-') ",
      "/land_info/hold_type": "@/legacy_meta/status == 1 ? \"Bought\" : \"Leasing\"",
      "/flags/is_valid": "is_null($/land_info/owner_id) ? false : true",
      "/flags/need_repair": "$/flags/is_valid == false || $/land_info/clean_name == \"\" ? true : false",
      "/members": "map(@/member_list, user -> user |> remove_char('-') |> to_lower()) ",
      "/fallback_test": "@/non_existent_field ?? \"default_value\""
    }
  )");

  try {
    // 编译模板
    auto compiled_t2 = engine.Compile(template_2);

    // 运行执行（可以连续测试 6,000 遍）
    nlohmann::json result_2 = engine.Execute(compiled_t2, source_2);
    std::cout << result_2.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Scenario 2 Error: " << e.what() << std::endl;
  }

  std::cout << "\n================= 严格类型匹配 =================" << std::endl;

  nlohmann::ordered_json error_template = nlohmann::ordered_json::parse(R"(
    {
      "/error_case": "true || \"not_boolean\""
    }
  )");

  try {
    auto compiled_err = engine.Compile(error_template);
    // 这里会触发非布尔逻辑运算熔断，抛出易读的错误
    engine.Execute(compiled_err, source_2);
  } catch (const std::exception& e) {
    std::cout << "预期内异常捕获成功:\n" << e.what() << std::endl;
  }

  return 0;
}