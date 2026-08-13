#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "macro.h"

#include "ast.h"

namespace del {

class ASTNode;
struct EvaluationContext;

using ExprEvaluator = std::function<nlohmann::json(ASTNode const&, EvaluationContext&)>;

using CustomFunction = std::function<nlohmann::json(
    Arguments const&     args,   //
    EvaluationContext&   ctx,    //
    ExprEvaluator const& eval_fn //
)>;

class SymbolTable {
public:
  DEL_DISABLE_COPY_MOVE(SymbolTable);

  SymbolTable() = default;

  void Register(std::string name, CustomFunction fn);

  template <auto T>
  void Register(std::string name) {
    using decay = std::decay_t<decltype(T)>;
    static_assert(std::is_convertible_v<decay, CustomFunction>, "The type is not a function");
    this->Register(std::move(name), static_cast<CustomFunction>(T));
  }

  CustomFunction const* Lookup(std::string_view name) const;

private:
  std::unordered_map<std::string, CustomFunction> functions_;
};

} // namespace del