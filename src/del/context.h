#pragma once
#include "macro.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <utility>

namespace del {

class SymbolTable;

struct Environment {
  Environment const*                              parent{nullptr};
  std::unordered_map<std::string, nlohmann::json> vars; // 局部闭包变量表 (lambda)

  inline const nlohmann::json* Lookup(std::string_view name) const {
    auto it = vars.find(std::string(name));
    if (it != vars.end()) {
      return &it->second;
    }
    if (parent) {
      return parent->Lookup(name);
    }
    return nullptr;
  }
};

struct EvaluationContext {
  nlohmann::json const& source;
  nlohmann::json&       target;
  Environment           global; // 全局变量表
  SymbolTable const&    symbol_table;

  Environment const* linked_env{nullptr}; // 当前作用域

  explicit EvaluationContext(nlohmann::json const& src, nlohmann::json& tgt, SymbolTable const& sym, Environment e = {})
  : source(src),
    target(tgt),
    global(std::move(e)),
    symbol_table(sym),
    linked_env(&global) {}

  DEL_DISABLE_COPY_MOVE(EvaluationContext);

  EvaluationContext(nlohmann::json&&, nlohmann::json&, SymbolTable const&, Environment) = delete;
};


class ScopeGuard {
public:
  explicit ScopeGuard(EvaluationContext& ctx) : ctx_(ctx), prev_env_(ctx.linked_env) {
    child_env_.parent = prev_env_;
    ctx_.linked_env   = &child_env_; // enter
  }

  ~ScopeGuard() {
    ctx_.linked_env = prev_env_; // exit
  }

  inline void Bind(std::string const& name, nlohmann::json val) { child_env_.vars[name] = std::move(val); }

  DEL_DISABLE_COPY_MOVE(ScopeGuard);

private:
  EvaluationContext& ctx_;
  Environment const* prev_env_;
  Environment        child_env_;
};

} // namespace del