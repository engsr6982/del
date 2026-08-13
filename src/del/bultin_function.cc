#include "bultin_function.h"
#include "del/context.h"
#include "del/exception.h"
#include "nlohmann/json_fwd.hpp"

#include <string>

namespace del {

void BultinFunction::RegisterAll(SymbolTable& table) {
  table.Register<&TrimStr>("trim");
  table.Register<&StrToLower>("to_lower");
  table.Register<&StrToUpper>("to_upper");
  table.Register<&RemoveSuffix>("remove_suffix");
  table.Register<&Replace>("replace");
  table.Register<&Substr>("substr");
  table.Register<&StartsWith>("starts_with");
  table.Register<&EndsWith>("ends_with");

  table.Register<&IsNull>("is_null");
  table.Register<&ToString>("to_str");

  table.Register<&Object>("object");
  table.Register<&Array>("array");
  table.Register<&Entry>("entry");
  table.Register<&Map>("map");
  table.Register<&MapObject>("map_object");
  table.Register<&Put>("put");
  table.Register<&Reduce>("reduce");
  table.Register<&Filter>("filter");
  table.Register<&Split>("split");
  table.Register<&Join>("join");
  table.Register<&Length>("length");
  table.Register<&Keys>("keys");
  table.Register<&Values>("values");
  table.Register<&Contains>("contains");
}


/// =================
/// String Interface:

// trim(str) -> str
nlohmann::json BultinFunction::TrimStr(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("trim expects exactly 1 argument");
  auto s = eval(*args[0], ctx);
  if (!s.is_string()) throw RuntimeError("trim argument must be a string");
  std::string str   = s.template get<std::string>();
  size_t      first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

// to_lower(str) -> str
nlohmann::json BultinFunction::StrToLower(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("to_lower expects exactly 1 argument");
  auto s = eval(*args[0], ctx);
  if (!s.is_string()) throw RuntimeError("to_lower argument must be a string");
  std::string str = s.template get<std::string>();
  for (char& c : str) c = (char)std::tolower(static_cast<unsigned char>(c));
  return str;
}

// to_upper(str) -> str
nlohmann::json BultinFunction::StrToUpper(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("to_upper expects exactly 1 argument");
  auto s = eval(*args[0], ctx);
  if (!s.is_string()) throw RuntimeError("to_upper argument must be a string");
  std::string str = s.template get<std::string>();
  for (char& c : str) c = (char)std::toupper(static_cast<unsigned char>(c));
  return str;
}

// remove_suffix(str, suffix_str) -> str
nlohmann::json BultinFunction::RemoveSuffix(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
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
}

// replace(str, from, to) -> str
nlohmann::json BultinFunction::Replace(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 3) throw RuntimeError("replace expects exactly 3 arguments");
  auto s = eval(*args[0], ctx);
  auto f = eval(*args[1], ctx);
  auto t = eval(*args[2], ctx);
  if (!s.is_string() || !f.is_string() || !t.is_string()) {
    throw RuntimeError("replace arguments must all be strings");
  }
  auto str  = s.template get<std::string>();
  auto from = f.template get<std::string>();
  auto to   = t.template get<std::string>();
  if (from.empty()) return str;

  std::size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }

  return str;
}

// substr(str, start, len) -> str
nlohmann::json BultinFunction::Substr(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 3) throw RuntimeError("substr expects exactly 3 arguments");
  auto s  = eval(*args[0], ctx);
  auto st = eval(*args[1], ctx);
  auto le = eval(*args[2], ctx);
  if (!s.is_string() || !st.is_number_integer() || !le.is_number_integer()) {
    throw RuntimeError("substr arguments must be a string, an integer, and an integer");
  }
  auto str       = s.template get<std::string>();
  auto start_val = st.template get<int64_t>();
  auto len_val   = le.template get<int64_t>();

  if (start_val < 0 || len_val < 0) [[unlikely]] {
    throw RuntimeError("substr start index and length must be non-negative");
  }

  auto start = static_cast<size_t>(start_val);
  auto len   = static_cast<size_t>(len_val);

  if (start > str.size()) [[unlikely]] {
    throw RuntimeError("substr start index out of range");
  }

  // std::string::substr(start, len) 会自动处理 (start + len > str.size()) 的情况，
  // 会自动截断到字符串末尾（例如 str 长度为 10，start=8, len=5，会安全返回最后 2 个字符）。
  // if (start + len > str.size()) throw RuntimeError("substr length out of range");

  return str.substr(start, len);
}

// starts_with(str, prefix) -> bool
nlohmann::json BultinFunction::StartsWith(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 2) throw RuntimeError("starts_with expects exactly 2 arguments");
  auto s = eval(*args[0], ctx);
  auto t = eval(*args[1], ctx);
  if (!s.is_string() || !t.is_string()) {
    throw RuntimeError("starts_with arguments must both be strings");
  }
  return s.template get<std::string>().starts_with(t.template get<std::string>());
}

// ends_with(str, suffix) -> bool
nlohmann::json BultinFunction::EndsWith(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 2) throw RuntimeError("ends_with expects exactly 2 arguments");
  auto s = eval(*args[0], ctx);
  auto t = eval(*args[1], ctx);
  if (!s.is_string() || !t.is_string()) {
    throw RuntimeError("ends_with arguments must both be strings");
  }
  return s.template get<std::string>().ends_with(t.template get<std::string>());
}


/// =================
/// Type Interface:

// is_null(val) -> bool
nlohmann::json BultinFunction::IsNull(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("is_null expects exactly 1 argument");
  auto val = eval(*args[0], ctx);
  return val.is_null();
}

// to_str(val) -> str
nlohmann::json BultinFunction::ToString(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("to_str expects exactly 1 argument");
  nlohmann::json val = eval(*args[0], ctx);
  if (val.is_string()) return val;
  return val.dump();
}

// to_number(str) -> number
nlohmann::json BultinFunction::ToNumber(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("to_number expects exactly 1 argument");

  auto val = eval(*args[0], ctx);
  if (val.is_number()) {
    return val;
  }

  if (val.is_string()) {
    auto str = val.template get<std::string>();
    try {
      val = std::stoll(str);
      return val;
    } catch (std::invalid_argument const&) {
      throw RuntimeError("to_number: invalid number string");
    }
  }

  [[unlikely]] throw RuntimeError("to_number: argument must be a number or a string");
}

/// =================
/// Container Interface:

// object() -> object {}
nlohmann::json BultinFunction::Object(Arguments const&, EvaluationContext&, ExprEvaluator const&) {
  return nlohmann::json::object();
}

// array() -> array []
nlohmann::json BultinFunction::Array(Arguments const&, EvaluationContext&, ExprEvaluator const&) {
  return nlohmann::json::array();
}

// entry(key, val) -> object {key: val}
nlohmann::json BultinFunction::Entry(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 2) throw RuntimeError("entry expects exactly 2 arguments");
  auto k = eval(*args[0], ctx);
  auto v = eval(*args[1], ctx);
  if (!k.is_string()) throw RuntimeError("entry key must be a string");
  nlohmann::json obj                 = nlohmann::json::object();
  obj[k.template get<std::string>()] = v;
  return obj;
}

// map(array, (element, idx) -> ...) -> array [T]
nlohmann::json BultinFunction::Map(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
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
}

// map_object(object, (key, value, index) -> ...) -> object {key: T}
nlohmann::json BultinFunction::MapObject(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
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
}

// put(container, key/index, value) -> container
nlohmann::json BultinFunction::Put(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
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
}

// reduce(array, init, (acc, item, idx) -> ...) -> T
nlohmann::json BultinFunction::Reduce(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
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
}

// filter(arr, (element, index) -> ...) -> array [T]
nlohmann::json BultinFunction::Filter(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 2) throw RuntimeError("filter expects exactly 2 arguments (array, lambda)");
  auto arr_val = eval(*args[0], ctx);
  if (!arr_val.is_array()) throw RuntimeError("filter's first argument must be an array");

  const auto* lambda = dynamic_cast<const LambdaNode*>(args[1]->GetUnderlyingNode());
  if (!lambda) throw RuntimeError("filter's second argument must be a lambda expression");

  const auto& params       = lambda->param_names();
  auto const  params_count = params.size();
  if (params_count < 1 || params_count > 2) [[unlikely]] {
    throw RuntimeError("filter's lambda must accept 1 or 2 parameters (element [, idx])");
  }

  ScopeGuard     scope(ctx);
  nlohmann::json result_arr = nlohmann::json::array();
  for (size_t idx = 0; idx < arr_val.size(); ++idx) {
    scope.Bind(params[0], arr_val[idx]);
    if (params_count > 1) {
      scope.Bind(params[1], idx);
    }

    auto cond = lambda->body().Evaluate(ctx);
    if (!cond.is_boolean()) [[unlikely]]
      throw RuntimeError("filter's lambda must return a boolean value");

    if (cond.template get<bool>()) {
      result_arr.push_back(arr_val[idx]);
    }
  }
  return result_arr;
}

// split(str, delimiter) -> array [str]
nlohmann::json BultinFunction::Split(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() < 1 || args.size() > 2) throw RuntimeError("split expects 1 or 2 arguments (string, delimiter)");
  auto s = eval(*args[0], ctx);
  if (!s.is_string()) {
    throw RuntimeError("split expects a string as first argument");
  }

  std::string delimiter = ","; // default delimiter
  std::string str       = s.template get<std::string>();

  if (args.size() == 2) {
    auto d = eval(*args[1], ctx);
    if (!d.is_string()) {
      throw RuntimeError("split expects a string as second argument");
    }
    delimiter = d.template get<std::string>();
  }

  auto arr = nlohmann::json::array();

  size_t start = 0;
  while (true) {
    auto pos = str.find(delimiter, start);
    if (pos == std::string::npos) break;
    arr.push_back(str.substr(start, pos - start));
    start = pos + delimiter.size();
  }
  arr.push_back(str.substr(start));
  return arr;
}

// join(array, separator) -> str
nlohmann::json BultinFunction::Join(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() < 1 || args.size() > 2) throw RuntimeError("join expects 1 or 2 arguments (array, separator)");

  auto arr = eval(*args[0], ctx);
  if (!arr.is_array()) {
    throw RuntimeError("join expects an array as first argument");
  }

  std::string separator = ","; // default separator
  if (args.size() == 2) {
    auto s = eval(*args[1], ctx);
    if (!s.is_string()) {
      throw RuntimeError("join expects a string as second argument");
    }
  }

  std::string result;

  auto iter = arr.begin();
  while (iter != arr.end()) {
    if (!iter->is_string()) [[unlikely]] {
      throw RuntimeError("join expects an array of strings");
    }
    result += iter->template get<std::string>();
    if (iter != arr.end() - 1) {
      result += separator;
    }
    ++iter;
  }
  return result;
}

// length(container) -> number
nlohmann::json BultinFunction::Length(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("length expects exactly 1 argument (container)");
  auto val = eval(*args[0], ctx);

  if (val.is_array() || val.is_object()) {
    return val.size();
  } else if (val.is_string()) {
    return val.template get<std::string>().size();
  }
  [[unlikely]] throw RuntimeError("length expects a container (array, object, string)");
}

// keys(object) -> array [str]
nlohmann::json BultinFunction::Keys(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("keys expects exactly 1 argument (object)");
  auto val = eval(*args[0], ctx);
  if (!val.is_object()) throw RuntimeError("keys expects an object as argument");

  auto arr = nlohmann::json::array();
  for (auto& [key, _] : val.items()) {
    arr.push_back(key);
  }
  return arr;
}

// values(object) -> array [T]
nlohmann::json BultinFunction::Values(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 1) throw RuntimeError("values expects exactly 1 argument (object)");
  auto val = eval(*args[0], ctx);
  if (!val.is_object()) throw RuntimeError("values expects an object as argument");

  auto arr = nlohmann::json::array();
  for (auto& [_, value] : val.items()) {
    arr.push_back(value);
  }
  return arr;
}

// contains(container, element) -> bool
nlohmann::json BultinFunction::Contains(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval) {
  if (args.size() != 2) throw RuntimeError("contains expects exactly 2 arguments (container, element)");
  auto container = eval(*args[0], ctx);
  auto element   = eval(*args[1], ctx);

  if (container.is_array()) {
    auto it = std::find(container.begin(), container.end(), element);
    return it != container.end();
  }

  if (container.is_object()) {
    if (!element.is_string()) {
      throw RuntimeError("element must be a string when container is an object");
    }
    return container.contains(element.get<std::string>());
  }

  if (container.is_string()) {
    if (!element.is_string()) {
      throw RuntimeError("element must be a string when container is an string");
    }
    auto c = container.template get<std::string>();
    auto e = element.template get<std::string>();
    return c.find(e) != std::string::npos;
  }

  [[unlikely]] throw RuntimeError("contains expects a container (array, object, string)");
}

} // namespace del