#include "template_engine.h"
#include "ast.h"
#include "exception.h"
#include "lexer.h"
#include "parser.h"


namespace del {

void RegisterBuiltins(SymbolTable& table) {
  // 1. remove_char(str, target_char)
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

  // 2. to_lower(str)
  table.Register("to_lower", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("to_lower expects exactly 1 argument");
    auto s = eval(*args[0], ctx);
    if (!s.is_string()) throw RuntimeError("to_lower argument must be a string");
    std::string str = s.template get<std::string>();
    for (char& c : str) c = (char)std::tolower(static_cast<unsigned char>(c));
    return str;
  });

  // 3. to_upper(str)
  table.Register("to_upper", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("to_upper expects exactly 1 argument");
    auto s = eval(*args[0], ctx);
    if (!s.is_string()) throw RuntimeError("to_upper argument must be a string");
    std::string str = s.template get<std::string>();
    for (char& c : str) c = (char)std::toupper(static_cast<unsigned char>(c));
    return str;
  });

  // 4. trim(str)
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

  // 5. remove_suffix(str, suffix)
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

  // 6. is_valid(val)
  table.Register("is_valid", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("is_valid expects exactly 1 argument");
    auto val = eval(*args[0], ctx);
    return !val.is_null();
  });

  // 7. is_null(val)
  table.Register("is_null", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("is_null expects exactly 1 argument");
    auto val = eval(*args[0], ctx);
    return val.is_null();
  });

  // 8. entry(key, val) -> 打包生成一个单键值对的 JSON 节点
  table.Register("entry", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("entry expects exactly 2 arguments");
    auto k = eval(*args[0], ctx);
    auto v = eval(*args[1], ctx);
    if (!k.is_string()) throw RuntimeError("entry key must be a string");
    nlohmann::json obj                 = nlohmann::json::object();
    obj[k.template get<std::string>()] = v;
    return obj;
  });

  // 9. key(kv) -> 获取 entry 的 Key
  table.Register("key", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("key expects exactly 1 argument");
    auto kv = eval(*args[0], ctx);
    if (!kv.is_object() || kv.size() != 1) {
      throw RuntimeError("key expects a single-entry object");
    }
    return kv.begin().key();
  });

  // 10. val(kv) -> 获取 entry 的 Value
  table.Register("val", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 1) throw RuntimeError("val expects exactly 1 argument");
    auto kv = eval(*args[0], ctx);
    if (!kv.is_object() || kv.size() != 1) {
      throw RuntimeError("val expects a single-entry object");
    }
    return kv.begin().value();
  });

  // 11. map(array, lambda) 高阶映射
  table.Register("map", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("map expects exactly 2 arguments (array, lambda)");
    auto arr_val = eval(*args[0], ctx);
    if (!arr_val.is_array()) throw RuntimeError("map's first argument must be an array");

    const auto* lambda = dynamic_cast<const LambdaNode*>(args[1].get());
    if (!lambda) throw RuntimeError("map's second argument must be a lambda expression");

    nlohmann::json   result_arr = nlohmann::json::array();
    std::string_view param      = lambda->param_name();

    for (const auto& item : arr_val) {
      // 本地作用域的动态局部绑定，保护外层同名局部变量
      bool           had_local = ctx.locals.contains(param);
      nlohmann::json prev_val;
      if (had_local) prev_val = std::move(ctx.locals[param]);

      ctx.locals[param] = item;
      result_arr.push_back(lambda->body().Evaluate(ctx));

      // 作用域析构还原
      if (had_local) {
        ctx.locals[param] = std::move(prev_val);
      } else {
        ctx.locals.erase(param);
      }
    }
    return result_arr;
  });

  // 12. map_object(object, lambda) 对象高阶映射
  table.Register("map_object", [](const auto& args, auto& ctx, auto eval) -> nlohmann::json {
    if (args.size() != 2) throw RuntimeError("map_object expects exactly 2 arguments (object, lambda)");
    auto obj_val = eval(*args[0], ctx);
    if (!obj_val.is_object()) throw RuntimeError("map_object's first argument must be an object");

    const auto* lambda = dynamic_cast<const LambdaNode*>(args[1].get());
    if (!lambda) throw RuntimeError("map_object's second argument must be a lambda expression");

    nlohmann::json   result_obj = nlohmann::json::object();
    std::string_view param      = lambda->param_name();

    for (auto it = obj_val.begin(); it != obj_val.end(); ++it) {
      // 将当前的 key-value 打包为单键对象传递给 Lambda
      nlohmann::json kv_entry = nlohmann::json::object();
      kv_entry[it.key()]      = it.value();

      bool           had_local = ctx.locals.contains(param);
      nlohmann::json prev_val;
      if (had_local) prev_val = std::move(ctx.locals[param]);

      ctx.locals[param]        = std::move(kv_entry);
      nlohmann::json entry_res = lambda->body().Evaluate(ctx);

      if (!entry_res.is_object() || entry_res.size() != 1) {
        throw RuntimeError("map_object lambda must return a single-entry object constructed via entry()");
      }

      result_obj[entry_res.begin().key()] = entry_res.begin().value();

      if (had_local) {
        ctx.locals[param] = std::move(prev_val);
      } else {
        ctx.locals.erase(param);
      }
    }
    return result_obj;
  });
}

TemplateEngine::TemplateEngine() { RegisterBuiltins(symbol_table_); }

void TemplateEngine::RegisterCustomFunction(std::string name, CustomFunction fn) {
  symbol_table_.Register(std::move(name), std::move(fn));
}

// 对模板执行一次编译
CompiledTemplate TemplateEngine::Compile(const nlohmann::ordered_json& template_json) {
  CompiledTemplate ct;
  for (auto it = template_json.begin(); it != template_json.end(); ++it) {
    const std::string& path     = it.key();
    const auto&        expr_val = it.value();

    CompiledPathExpr instr;
    instr.target_pointer_path = path;

    if (expr_val.is_string()) {
      std::string expr_str = expr_val.get<std::string>();
      Lexer       lexer(expr_str, path);
      Parser      parser(lexer);
      instr.compiled_ast = parser.ParseExpression(Precedence::kLowest);
    } else {
      instr.direct_value = expr_val;
    }
    ct.instructions.push_back(std::move(instr));
  }
  return ct;
}

// 极速运行：在转换几千条数据时，仅调用此函数执行 AST 遍历
nlohmann::json TemplateEngine::Execute(const CompiledTemplate& ct, const nlohmann::json& source_json) {
  nlohmann::json    target_json = nlohmann::json::object();
  EvaluationContext ctx{source_json, target_json, {}, symbol_table_};

  for (const auto& instr : ct.instructions) {
    if (instr.compiled_ast) {
      nlohmann::json result = instr.compiled_ast->Evaluate(ctx);
      MountTargetValue(target_json, instr.target_pointer_path, std::move(result));
    } else {
      MountTargetValue(target_json, instr.target_pointer_path, instr.direct_value);
    }
  }
  return target_json;
}

void TemplateEngine::MountTargetValue(nlohmann::json& target, const std::string& path_str, nlohmann::json value) {
  try {
    nlohmann::json::json_pointer ptr(path_str);
    // nlohmann::json 的 operator[] 遇到不存在的父路径会自动构建嵌套对象
    target[ptr] = std::move(value);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::format("Failed to mount target path '{}': {}", path_str, e.what()));
  }
}


} // namespace del