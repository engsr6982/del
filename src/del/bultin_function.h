#pragma once

#include "nlohmann/json.hpp"

#include "ast.h"
#include "symbol_table.h"

namespace del {
class SymbolTable;
struct EvaluationContext;
} // namespace del

namespace del {

struct BultinFunction {
  BultinFunction() = delete;

  static void RegisterAll(SymbolTable& table);

public:
  /// string interface:

  // trim(str) -> str
  static nlohmann::json TrimStr(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // to_lower(str) -> str
  static nlohmann::json StrToLower(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // to_upper(str) -> str
  static nlohmann::json StrToUpper(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // remove_suffix(str, suffix_str) -> str
  static nlohmann::json RemoveSuffix(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // replace(str, from, to) -> str
  static nlohmann::json Replace(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // substr(str, start, len) -> str
  static nlohmann::json Substr(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // starts_with(str, prefix) -> bool
  static nlohmann::json StartsWith(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // ends_with(str, suffix) -> bool
  static nlohmann::json EndsWith(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

public:
  /// type interface:

  // is_null(val) -> bool
  static nlohmann::json IsNull(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // to_str(val) -> str
  static nlohmann::json ToString(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // to_number(str) -> number
  static nlohmann::json ToNumber(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

public:
  /// container interface:

  // object() -> object {}
  static nlohmann::json Object(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // array() -> array []
  static nlohmann::json Array(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // entry(key, val) -> object {key: val}
  static nlohmann::json Entry(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // map(array, (element, idx) -> ...) -> array [T]
  static nlohmann::json Map(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // map_object(object, (key, value, index) -> ...) -> object {key: T}
  static nlohmann::json MapObject(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // put(container, key/index, value) -> container
  static nlohmann::json Put(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // get(obj, key) -> value
  static nlohmann::json Get(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // at(array, index) -> value
  static nlohmann::json At(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // reduce(array, init, (acc, item, idx) -> ...) -> T
  static nlohmann::json Reduce(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // filter(arr, (element) -> ...) -> array [T]
  static nlohmann::json Filter(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // split(str, delimiter) -> array [str]
  static nlohmann::json Split(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // join(array, separator) -> str
  static nlohmann::json Join(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // length(container) -> number
  static nlohmann::json Length(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // keys(object) -> array [str]
  static nlohmann::json Keys(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // values(object) -> array [T]
  static nlohmann::json Values(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);

  // contains(container, element) -> bool
  static nlohmann::json Contains(Arguments const& args, EvaluationContext& ctx, ExprEvaluator const& eval);
};

} // namespace del
