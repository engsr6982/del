#include "args_helper.h"

#include "del/ast.h"
#include "del/context.h"
#include "del/exception.h"
#include "del/symbol_table.h"
#include "del/template_engine.h"
#include "del/version.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

void print_help() {
  std::cout << "Usage: del_repl.exe [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -h, --help            Print this help message" << std::endl;
  std::cout << "  -v, --version         Print the version number" << std::endl;
  std::cout << "  -s, --source <file>   Read source code from the specified file" << std::endl;
  std::cout << "  -e, --eval <expr>     Evaluate the specified expression" << std::endl;
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

void repl(nlohmann::json const& json) {
  std::cout << "REPL mode" << std::endl;
  std::cout << "Type 'exit' to quit" << std::endl;
  std::cout << "Type 'set('json pointer', expr)' to evaluate an expression and set target value" << std::endl;
  std::cout << "    e.g. set('/foo/bar', @/raw/data |> to_lower())" << std::endl;

  del::TemplateEngine engine;

  // 由于 REPL 交互环境，无法编写模板，且书写 JSON 也不方便，补充使用 set 函数来设置值
  engine.RegisterCustomFunction(
      "set",
      [](auto const& args, del::EvaluationContext& ctx, auto const& eval) -> nlohmann::json {
        if (args.size() != 2) {
          throw del::RuntimeError("set function requires two arguments");
        }
        auto arg1 = eval(*args[0], ctx);
        if (!arg1.is_string()) {
          throw del::RuntimeError("first argument to set function must be a string (JSON Pointer)");
        }

        auto arg2 = eval(*args[1], ctx);
        std::cout << "eval-value: " << arg2.dump() << std::endl;

        auto path = arg1.template get<std::string>();
        del::TemplateEngine::MountValue(ctx.target, path, arg2);
        std::cout << "snapshot(target): " << ctx.target.dump(2) << std::endl;
        return nullptr;
      }
  );

  auto target = nlohmann::json::object();
  auto ctx    = engine.CreateContext(json, target);

  while (true) {
    std::cout << std::endl << "del> ";
    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }

    if (line.empty()) {
      continue;
    }
    if (line == "exit") {
      break;
    }

    try {
      auto expr   = engine.CompileExprStr(line);
      auto result = engine.Execute(expr, ctx);
      std::cout << "result: " << result.dump() << std::endl;
    } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
    }
  }
}

int eval(nlohmann::json const& json, std::string const& expr) {
  std::cout << "eval: " << expr << std::endl;

  del::TemplateEngine engine;

  auto target = nlohmann::json::object();
  auto ctx    = engine.CreateContext(json, target);

  try {
    auto result = engine.Execute(engine.CompileExprStr(expr), ctx);
    std::cout << "result: " << result.dump() << std::endl;
  } catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  ArgsParser args{argc, argv};

  if (args.has_flag("-h") || args.has_flag("--help")) {
    print_help();
    return 0;
  }

  if (args.has_flag("-v") || args.has_flag("--version")) {
    std::cout << "del_repl.exe v" << del::kVersion << std::endl;
    return 0;
  }

  auto source_path = args.get_option("-s");
  if (!source_path) source_path = args.get_option("--source");

  std::optional<nlohmann::json> source_json;
  if (source_path) {
    source_json = load_json_file(*source_path);
  }

  auto eval = args.get_option("-e");
  if (!eval) eval = args.get_option("--eval");

  auto json = source_json.value_or(nlohmann::json::object());
  if (eval) {
    return ::eval(json, *eval);
  }

  repl(json);
  return 0;
}