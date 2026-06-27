#include "template_engine.h"
#include "ast.h"
#include "context.h"
#include "exception.h"
#include "lexer.h"
#include "parser.h"

#include "nlohmann/json_fwd.hpp"

#include <memory>


namespace del {

void RegisterBuiltins(SymbolTable& table) {
  // remove_char(str, target_char)
  table.Register("remove_char", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("remove_char expects exactly 2 arguments");
    auto s = eval(*args[0], ctx);
    auto c = eval(*args[1], ctx);
    if (!s.is_string() || !c.is_string()) {
      throw RuntimeError("remove_char arguments must both be strings");
    }
    std::string str       = s.template get<std::string>();
    std::string to_remove = c.template get<std::string>();
    if (to_remove.empty()) return str;

    std::string res;
    for (char ch : str) {
      if (to_remove.find(ch) == std::string::npos) {
        res += ch;
      }
    }
    return res;
  });

  // to_lower(str)
  table.Register("to_lower", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("to_lower expects exactly 1 argument");
    auto s = eval(*args[0], ctx);
    if (!s.is_string()) throw RuntimeError("to_lower argument must be a string");
    std::string str = s.template get<std::string>();
    for (char& c : str) c = (char)std::tolower(static_cast<unsigned char>(c));
    return str;
  });

  // to_upper(str)
  table.Register("to_upper", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("to_upper expects exactly 1 argument");
    auto s = eval(*args[0], ctx);
    if (!s.is_string()) throw RuntimeError("to_upper argument must be a string");
    std::string str = s.template get<std::string>();
    for (char& c : str) c = (char)std::toupper(static_cast<unsigned char>(c));
    return str;
  });

  // trim(str)
  table.Register("trim", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("trim expects exactly 1 argument");
    auto s = eval(*args[0], ctx);
    if (!s.is_string()) throw RuntimeError("trim argument must be a string");
    std::string str   = s.template get<std::string>();
    size_t      first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
  });

  // remove_suffix(str, suffix)
  table.Register("remove_suffix", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("remove_suffix expects exactly 2 arguments");
    auto s   = eval(*args[0], ctx);
    auto suf = eval(*args[1], ctx);
    if (!s.is_string() || !suf.is_string()) {
      throw RuntimeError("remove_suffix arguments must both be strings");
    }
    std::string str    = s.template get<std::string>();
    std::string suffix = suf.template get<std::string>();
    if (str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0) {
      return str.substr(0, str.size() - suffix.size());
    }
    return str;
  });

  // is_null(val)
  table.Register("is_null", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("is_null expects exactly 1 argument");
    auto val = eval(*args[0], ctx);
    return val.is_null();
  });

  // to_str(val)
  table.Register("to_str", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("to_str expects exactly 1 argument");
    nlohmann::json val = eval(*args[0], ctx);
    return val.dump();
  });

  // entry(key, val) -> 打包生成一个单键值对的 JSON 节点
  table.Register("entry", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("entry expects exactly 2 arguments");
    auto k = eval(*args[0], ctx);
    auto v = eval(*args[1], ctx);
    if (!k.is_string()) throw RuntimeError("entry key must be a string");
    nlohmann::json obj                 = nlohmann::json::object();
    obj[k.template get<std::string>()] = v;
    return obj;
  });

  // map(array, (element, idx) -> ...)
  table.Register("map", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("map expects exactly 2 arguments (array, lambda)");
    auto arr_val = eval(*args[0], ctx);
    if (!arr_val.is_array()) throw RuntimeError("map's first argument must be an array");

    const auto* lambda = dynamic_cast<const LambdaNode*>(args[1]->GetUnderlyingNode());
    if (!lambda) throw RuntimeError("map's second argument must be a lambda expression");

    const auto& params = lambda->param_names();
    if (params.empty() || params.size() > 2) {
      throw RuntimeError("map's lambda must accept 1 or 2 parameters (element [, idx])");
    }

    ScopeGuard scope(ctx);

    nlohmann::json result_arr = nlohmann::json::array();
    for (size_t idx = 0; idx < arr_val.size(); ++idx) {
      scope.Bind(params[0], arr_val[idx]);
      if (params.size() > 1) {
        scope.Bind(params[1], idx);
      }

      result_arr.push_back(lambda->body().Evaluate(ctx));
    }
    return result_arr;
  });

  // map_object(object, lambda) 对象高阶映射
  table.Register("map_object", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("map_object expects exactly 2 arguments (object, lambda)");
    auto obj_val = eval(*args[0], ctx);
    if (!obj_val.is_object()) throw RuntimeError("map_object's first argument must be an object");

    const auto* lambda = dynamic_cast<const LambdaNode*>(args[1]->GetUnderlyingNode());
    if (!lambda) throw RuntimeError("map_object's second argument must be a lambda expression");

    const auto& params = lambda->param_names();
    if (params.size() < 2 || params.size() > 3) {
      throw RuntimeError("map_object's lambda must accept 2 or 3 parameters (key, val [, idx])");
    }

    ScopeGuard scope(ctx);

    nlohmann::json result_obj = nlohmann::json::object();
    size_t         idx        = 0;
    for (auto it = obj_val.begin(); it != obj_val.end(); ++it, ++idx) {
      scope.Bind(params[0], it.key());
      scope.Bind(params[1], it.value());
      if (params.size() > 2) {
        scope.Bind(params[2], idx);
      }

      nlohmann::json entry_res = lambda->body().Evaluate(ctx);
      if (!entry_res.is_object() || entry_res.size() != 1) {
        throw RuntimeError("map_object lambda must return a single-entry object constructed via entry()");
      }
      result_obj[entry_res.begin().key()] = entry_res.begin().value();
    }
    return result_obj;
  });

  // 辅助函数：构造空容器
  table.Register("obj", [](const auto&, auto&, auto) -> nlohmann::json { return nlohmann::json::object(); });
  table.Register("arr", [](const auto&, auto&, auto) -> nlohmann::json { return nlohmann::json::array(); });

  // put(container, key/index, value)
  table.Register("put", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 3) throw RuntimeError("put expects exactly 3 arguments (container, key/idx, value)");
    auto container  = eval(*args[0], ctx);
    auto key_or_idx = eval(*args[1], ctx);
    auto val        = eval(*args[2], ctx);

    if (container.is_object()) {
      if (!key_or_idx.is_string()) throw RuntimeError("put on object expects a string key");
      container[key_or_idx.template get<std::string>()] = std::move(val);
    } else if (container.is_array()) {
      if (!key_or_idx.is_number_integer()) throw RuntimeError("put on array expects an integer index");
      size_t idx = key_or_idx.template get<size_t>();
      if (idx > container.size()) throw RuntimeError("put index out of bounds");
      if (idx == container.size()) {
        container.push_back(std::move(val));
      } else {
        container[idx] = std::move(val);
      }
    } else {
      throw RuntimeError("put requires an object or array as the first argument");
    }
    return container;
  });

  // reduce(array, init, (acc, item, idx) -> ...)
  table.Register("reduce", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 3) throw RuntimeError("reduce expects exactly 3 arguments (array, init, lambda)");
    auto arr_val = eval(*args[0], ctx);
    if (!arr_val.is_array()) throw RuntimeError("reduce's first argument must be an array");

    auto acc = eval(*args[1], ctx);

    const auto* lambda = dynamic_cast<const LambdaNode*>(args[2]->GetUnderlyingNode());
    if (!lambda) throw RuntimeError("reduce's third argument must be a lambda expression");

    const auto& params = lambda->param_names();
    if (params.size() < 2 || params.size() > 3) {
      throw RuntimeError("reduce's lambda must accept 2 or 3 parameters (acc, item [, idx])");
    }

    ScopeGuard scope(ctx);
    for (size_t idx = 0; idx < arr_val.size(); ++idx) {
      scope.Bind(params[0], acc);
      scope.Bind(params[1], arr_val[idx]);
      if (params.size() > 2) {
        scope.Bind(params[2], idx);
      }

      acc = lambda->body().Evaluate(ctx);
    }
    return acc;
  });
}

TemplateEngine::TemplateEngine() { RegisterBuiltins(symbol_table_); }

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