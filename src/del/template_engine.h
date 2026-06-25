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

  // 对模板执行一次编译
  CompiledTemplate Compile(const nlohmann::ordered_json& template_json);

  // 极速运行：在转换几千条数据时，仅调用此函数执行 AST 遍历
  nlohmann::json Execute(const CompiledTemplate& ct, const nlohmann::json& source_json);

private:
  SymbolTable symbol_table_;

  void MountTargetValue(nlohmann::json& target, const std::string& path_str, nlohmann::json value);
};


} // namespace del