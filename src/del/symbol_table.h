#pragma once
#include <functional>
#include <memory>
#include <unordered_map>
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

  CustomFunction const* Lookup(std::string_view name) const;

private:
  std::unordered_map<std::string, CustomFunction> functions_;
};

} // namespace del