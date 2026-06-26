#pragma once
#include "macro.h"
#include "symbol_table.h"

#include <nlohmann/json.hpp>

namespace del {

class ASTNode;

struct CompiledPathExpr {
  std::string              target_pointer_path;
  nlohmann::json           direct_value; // 非公式值直接存放
  std::unique_ptr<ASTNode> compiled_ast; // 公式编译后的 AST 树
};

class CompiledTemplate {
public:
  std::vector<CompiledPathExpr> instructions;

  CompiledTemplate()                              = default;
  CompiledTemplate(CompiledTemplate&&)            = default;
  CompiledTemplate& operator=(CompiledTemplate&&) = default;

  DEL_DISABLE_COPY(CompiledTemplate);
};

class TemplateEngine {
public:
  TemplateEngine();

  void RegisterCustomFunction(std::string name, CustomFunction fn);

  [[nodiscard]] CompiledPathExpr CompileExprStr(std::string_view expr, std::string_view path_key = "");
  [[nodiscard]] CompiledPathExpr CompileExpr(nlohmann::json const& expr, std::string_view path_key = "");

  [[nodiscard]] CompiledTemplate Compile(nlohmann::ordered_json const& template_json);

  [[nodiscard]] EvaluationContext CreateContext(nlohmann::json const& source_json, nlohmann::json& target);

  [[nodiscard]] nlohmann::json Execute(CompiledTemplate const& ct, nlohmann::json const& source_json);

  void Execute(CompiledTemplate const& ct, EvaluationContext& ctx);

  // execute expr only, return the result (not modify the target)
  [[nodiscard]] nlohmann::json Execute(CompiledPathExpr const& instr, EvaluationContext& ctx);

  static void MountValue(nlohmann::json& target, std::string const& path_str, nlohmann::json value);

private:
  SymbolTable symbol_table_;
};


} // namespace del