#include "template_engine.h"
#include "ast.h"
#include "bultin_function.h"
#include "context.h"
#include "exception.h"
#include "lexer.h"
#include "parser.h"

#include "nlohmann/json_fwd.hpp"

#include <memory>


namespace del {

TemplateEngine::TemplateEngine() { BultinFunction::RegisterAll(this->symbol_table_); }

void TemplateEngine::RegisterCustomFunction(std::string name, CustomFunction fn) {
  symbol_table_.Register(std::move(name), std::move(fn));
}

CompiledPathExpr TemplateEngine::CompileExprStr(std::string_view expr, std::string_view path_key) {
  Lexer            lexer(expr, path_key);
  Parser           parser(lexer);
  CompiledPathExpr instr;
  instr.target_pointer_path = path_key;
  instr.compiled_ast        = parser.ParseTopLevelExpression();
  return instr;
}
CompiledPathExpr TemplateEngine::CompileExpr(nlohmann::json const& expr, std::string_view path_key) {
  if (expr.is_string()) {
    auto str = expr.get<std::string>();
    return CompileExprStr(str, path_key);
  } else {
    CompiledPathExpr instr;
    instr.target_pointer_path = path_key;
    instr.direct_value        = expr;
    return instr;
  }
}
CompiledTemplate TemplateEngine::Compile(nlohmann::ordered_json const& template_json) {
  CompiledTemplate ct;
  for (auto it = template_json.begin(); it != template_json.end(); ++it) {
    const std::string& path     = it.key();
    const auto&        expr_val = it.value();

    auto instr = CompileExpr(expr_val, path);

    ct.instructions.push_back(std::move(instr));
  }
  return ct;
}

EvaluationContext TemplateEngine::CreateContext(nlohmann::json const& source_json, nlohmann::json& target) {
  return EvaluationContext{source_json, target, symbol_table_};
}


nlohmann::json TemplateEngine::Execute(CompiledTemplate const& ct, nlohmann::json const& source_json) {
  auto target_json = nlohmann::json::object();
  auto ctx         = CreateContext(source_json, target_json);
  Execute(ct, ctx);
  return target_json;
}
void TemplateEngine::Execute(CompiledTemplate const& ct, EvaluationContext& ctx) {
  for (const auto& instr : ct.instructions) {
    auto result = Execute(instr, ctx);
    if (!instr.target_pointer_path.empty()) {
      MountValue(ctx.target, instr.target_pointer_path, std::move(result));
    }
  }
}
nlohmann::json TemplateEngine::Execute(CompiledPathExpr const& instr, EvaluationContext& ctx) {
  if (instr.compiled_ast) {
    auto result = instr.compiled_ast->Evaluate(ctx);
    return result;
  } else {
    return instr.direct_value;
  }
}

void TemplateEngine::MountValue(nlohmann::json& target, std::string const& path_str, nlohmann::json value) {
  try {
    nlohmann::json::json_pointer ptr(path_str);
    // nlohmann::json 的 operator[] 遇到不存在的父路径会自动构建嵌套对象
    target[ptr] = std::move(value);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::format("Failed to mount target path '{}': {}", path_str, e.what()));
  }
}


} // namespace del