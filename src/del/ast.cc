#include "ast.h"
#include "context.h"
#include "exception.h"
#include "symbol_table.h"
#include <iostream>

namespace del {

inline void EnsureBoolean(nlohmann::json const& v, std::string_view context) {
  if (!v.is_boolean()) {
    throw RuntimeError(std::string(context) + " requires a boolean operand, but got: " + v.dump());
  }
}

inline void EnsureNumber(nlohmann::json const& v, std::string_view context) {
  if (!v.is_number()) {
    throw RuntimeError(std::string(context) + " requires a numeric operand, but got: " + v.dump());
  }
}

inline void EnsureSameType(nlohmann::json const& lhs, nlohmann::json const& rhs, std::string_view context) {
  if (lhs.type() != rhs.type()) {
    if (lhs.is_number() && rhs.is_number()) {
      return;
    }
    throw RuntimeError(
        std::string(context) + " requires both operands to be of the exact same type. Got " + lhs.dump() + " and "
        + rhs.dump()
    );
  }
}
const ASTNode* ASTNode::GetUnderlyingNode() const { return this; }


nlohmann::json NumberLiteralNode::Evaluate(EvaluationContext&) const {
  if (is_double_) return d_val_;
  return i_val_;
}
std::string NumberLiteralNode::ToString() const { return is_double_ ? std::to_string(d_val_) : std::to_string(i_val_); }


StringLiteralNode::StringLiteralNode(std::string val) : val_(std::move(val)) {}
nlohmann::json StringLiteralNode::Evaluate(EvaluationContext&) const { return val_; }
std::string    StringLiteralNode::ToString() const { return "\"" + val_ + "\""; }


nlohmann::json BooleanLiteralNode::Evaluate(EvaluationContext&) const { return val_; }
std::string    BooleanLiteralNode::ToString() const { return val_ ? "true" : "false"; }


nlohmann::json NullLiteralNode::Evaluate(EvaluationContext&) const { return nullptr; }
std::string    NullLiteralNode::ToString() const { return "null"; }


IdentifierNode::IdentifierNode(std::string name) : name_(std::move(name)) {}
const std::string& IdentifierNode::name() const { return name_; }

nlohmann::json IdentifierNode::Evaluate(EvaluationContext& ctx) const {
  if (ctx.linked_env == nullptr) [[unlikely]] {
    std::cerr << std::format(
        "[CRITICAL ERROR] Evaluation context has a null Environment pointer. "
        "Invariant violated during evaluating identifier '{}'.\n",
        name_
    );
    std::abort();
  }
  const auto* val = ctx.linked_env->Lookup(name_);
  if (val) {
    return *val;
  }
  throw RuntimeError(std::format("Undefined identifier: '{}'", name_));
}
std::string IdentifierNode::ToString() const { return name_; }


PointerNode::PointerNode(bool is_source, std::string path) : is_source_(is_source), path_(std::move(path)) {}

nlohmann::json PointerNode::Evaluate(EvaluationContext& ctx) const {
  const auto& root = is_source_ ? ctx.source : ctx.target;
  try {
    auto json_ptr = nlohmann::json::json_pointer(path_);
    if (root.contains(json_ptr)) {
      return root[json_ptr];
    }
  } catch (...) {
    // RFC 6901 寻址失败时，静默求值为 null
  }
  return nullptr;
}

std::string PointerNode::ToString() const { return (is_source_ ? "@" : "$") + path_; }


PrefixNode::PrefixNode(std::string op, std::unique_ptr<ASTNode> right) : op_(std::move(op)), right_(std::move(right)) {}

nlohmann::json PrefixNode::Evaluate(EvaluationContext& ctx) const {
  auto right_val = right_->Evaluate(ctx);
  if (op_ == "!") {
    EnsureBoolean(right_val, "Logical NOT operator (!)");
    return !right_val.get<bool>();
  } else if (op_ == "-") {
    EnsureNumber(right_val, "Unary minus operator (-)");
    if (right_val.is_number_integer()) {
      return -right_val.get<int64_t>();
    } else {
      return -right_val.get<double>();
    }
  }
  throw RuntimeError("Unknown prefix operator: " + op_);
}

std::string PrefixNode::ToString() const { return "(" + op_ + right_->ToString() + ")"; }


InfixNode::InfixNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
: op_(std::move(op)),
  left_(std::move(left)),
  right_(std::move(right)) {}

nlohmann::json InfixNode::Evaluate(EvaluationContext& ctx) const {
  // 1. 短路求值逻辑
  if (op_ == "&&") {
    auto left_val = left_->Evaluate(ctx);
    EnsureBoolean(left_val, "Logical AND (&&) left operand");
    if (!left_val.get<bool>()) return false;
    auto right_val = right_->Evaluate(ctx);
    EnsureBoolean(right_val, "Logical AND (&&) right operand");
    return right_val.get<bool>();
  }
  if (op_ == "||") {
    auto left_val = left_->Evaluate(ctx);
    EnsureBoolean(left_val, "Logical OR (||) left operand");
    if (left_val.get<bool>()) return true;
    auto right_val = right_->Evaluate(ctx);
    EnsureBoolean(right_val, "Logical OR (||) right operand");
    return right_val.get<bool>();
  }

  // 2. 空值合并操作符 ??
  if (op_ == "??") {
    auto left_val = left_->Evaluate(ctx);
    if (!left_val.is_null()) {
      return left_val;
    }
    return right_->Evaluate(ctx);
  }

  // 3. 常规严格求值 (Eager Evaluation)
  auto left_val  = left_->Evaluate(ctx);
  auto right_val = right_->Evaluate(ctx);

  if (op_ == "==") {
    EnsureSameType(left_val, right_val, "Equality (==)");
    return left_val == right_val;
  }
  if (op_ == "!=") {
    EnsureSameType(left_val, right_val, "Inequality (!=)");
    return left_val != right_val;
  }

  // 关系运算符
  if (op_ == "<" || op_ == ">" || op_ == "<=" || op_ == ">=") {
    EnsureSameType(left_val, right_val, "Relational comparison");
    if (!left_val.is_number() && !left_val.is_string()) {
      throw RuntimeError("Relational operators only support numbers or strings, but got: " + left_val.dump());
    }
    if (op_ == "<") return left_val < right_val;
    if (op_ == ">") return left_val > right_val;
    if (op_ == "<=") return left_val <= right_val;
    if (op_ == ">=") return left_val >= right_val;
  }

  // 算术运算符
  if (op_ == "+" || op_ == "-" || op_ == "*" || op_ == "/") {
    EnsureNumber(left_val, std::string("Arithmetic (") + op_ + ") left operand");
    EnsureNumber(right_val, std::string("Arithmetic (") + op_ + ") right operand");

    bool is_double = left_val.is_number_float() || right_val.is_number_float();
    if (is_double) {
      double l = left_val.get<double>();
      double r = right_val.get<double>();
      if (op_ == "+") return l + r;
      if (op_ == "-") return l - r;
      if (op_ == "*") return l * r;
      if (op_ == "/") {
        if (r == 0.0) throw RuntimeError("Division by zero");
        return l / r;
      }
    } else {
      int64_t l = left_val.get<int64_t>();
      int64_t r = right_val.get<int64_t>();
      if (op_ == "+") return l + r;
      if (op_ == "-") return l - r;
      if (op_ == "*") return l * r;
      if (op_ == "/") {
        if (r == 0) throw RuntimeError("Division by zero");
        return l / r;
      }
    }
  }

  throw RuntimeError("Unknown operator: " + op_);
}

std::string InfixNode::ToString() const { return "(" + left_->ToString() + " " + op_ + " " + right_->ToString() + ")"; }


TernaryNode::TernaryNode(
    std::unique_ptr<ASTNode> cond,
    std::unique_ptr<ASTNode> true_expr,
    std::unique_ptr<ASTNode> false_expr
)
: cond_(std::move(cond)),
  true_expr_(std::move(true_expr)),
  false_expr_(std::move(false_expr)) {}

nlohmann::json TernaryNode::Evaluate(EvaluationContext& ctx) const {
  auto cond_val = cond_->Evaluate(ctx);
  EnsureBoolean(cond_val, "Ternary condition");
  if (cond_val.get<bool>()) {
    return true_expr_->Evaluate(ctx);
  } else {
    return false_expr_->Evaluate(ctx);
  }
}

std::string TernaryNode::ToString() const {
  return "(" + cond_->ToString() + " ? " + true_expr_->ToString() + " : " + false_expr_->ToString() + ")";
}


TupleNode::TupleNode(std::vector<std::unique_ptr<ASTNode>> elements) : elements_(std::move(elements)) {}
const std::vector<std::unique_ptr<ASTNode>>& TupleNode::elements() const { return elements_; }

nlohmann::json TupleNode::Evaluate(EvaluationContext&) const {
  throw RuntimeError("Tuple expressions cannot be evaluated outside lambda parameter lists");
}
std::string TupleNode::ToString() const {
  std::string res = "(";
  for (size_t i = 0; i < elements_.size(); ++i) {
    if (i > 0) res += ", ";
    res += elements_[i]->ToString();
  }
  res += ")";
  return res;
}


LambdaNode::LambdaNode(std::vector<std::string> param_names, std::unique_ptr<ASTNode> body)
: param_names_(std::move(param_names)),
  body_(std::move(body)) {}

const std::vector<std::string>& LambdaNode::param_names() const { return param_names_; }
const ASTNode&                  LambdaNode::body() const { return *body_; }

nlohmann::json LambdaNode::Evaluate(EvaluationContext&) const {
  throw RuntimeError("Lambda expressions cannot be evaluated outside of high-order functions");
}

std::string LambdaNode::ToString() const {
  std::string res = "(";
  for (size_t i = 0; i < param_names_.size(); ++i) {
    if (i > 0) res += ", ";
    res += param_names_[i];
  }
  res += ") -> " + body_->ToString();
  return res;
}


CallNode::CallNode(std::string name, std::vector<std::unique_ptr<ASTNode>> args)
: name_(std::move(name)),
  args_(std::move(args)) {}

const std::string&                           CallNode::name() const { return name_; }
const std::vector<std::unique_ptr<ASTNode>>& CallNode::args() const { return args_; }

nlohmann::json CallNode::Evaluate(EvaluationContext& ctx) const {
  const auto* func = ctx.symbol_table.Lookup(name_);
  if (!func) {
    throw RuntimeError("Undefined function call: " + name_);
  }
  return (*func)(args_, ctx, [](const ASTNode& node, EvaluationContext& c) { return node.Evaluate(c); });
}
std::string CallNode::ToString() const {
  std::string res = name_ + "(";
  for (size_t i = 0; i < args_.size(); ++i) {
    if (i > 0) res += ", ";
    res += args_[i]->ToString();
  }
  res += ")";
  return res;
}


// 辅助注入节点：用于管道执行
PreEvaluatedNode::PreEvaluatedNode(nlohmann::json val) : val_(std::move(val)) {}
nlohmann::json PreEvaluatedNode::Evaluate(EvaluationContext&) const { return val_; }
std::string    PreEvaluatedNode::ToString() const { return val_.dump(); }


ReferenceNode::ReferenceNode(const ASTNode& ref) : ref_(ref) {}
nlohmann::json ReferenceNode::Evaluate(EvaluationContext& ctx) const { return ref_.Evaluate(ctx); }
std::string    ReferenceNode::ToString() const { return ref_.ToString(); }
const ASTNode* ReferenceNode::GetUnderlyingNode() const { return ref_.GetUnderlyingNode(); }


PipelineNode::PipelineNode(std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
: left_(std::move(left)),
  right_(std::move(right)) {}

nlohmann::json PipelineNode::Evaluate(EvaluationContext& ctx) const {
  auto left_val = left_->Evaluate(ctx);

  // 右侧如果是 CallNode，则将左侧计算结果作为第一个参数隐式注入
  if (auto* call_node = dynamic_cast<CallNode*>(right_.get())) {
    const auto* func = ctx.symbol_table.Lookup(call_node->name());
    if (!func) {
      throw RuntimeError("Undefined function in pipeline: " + call_node->name());
    }

    std::vector<std::unique_ptr<ASTNode>> virtual_args;
    virtual_args.push_back(std::make_unique<PreEvaluatedNode>(std::move(left_val)));
    for (const auto& arg : call_node->args()) {
      virtual_args.push_back(std::make_unique<ReferenceNode>(*arg));
    }

    return (*func)(virtual_args, ctx, [](const ASTNode& node, EvaluationContext& c) { return node.Evaluate(c); });
  }
  // 右侧如果是普通的标识符，则视为 0 个显式参数的函数调用
  else if (auto* ident_node = dynamic_cast<IdentifierNode*>(right_.get())) {
    const auto* func = ctx.symbol_table.Lookup(ident_node->name());
    if (!func) {
      throw RuntimeError("Undefined function in pipeline: " + ident_node->name());
    }

    std::vector<std::unique_ptr<ASTNode>> virtual_args;
    virtual_args.push_back(std::make_unique<PreEvaluatedNode>(std::move(left_val)));

    return (*func)(virtual_args, ctx, [](const ASTNode& node, EvaluationContext& c) { return node.Evaluate(c); });
  }

  throw RuntimeError("Pipeline right-hand side must be a function or function call");
}

std::string PipelineNode::ToString() const { return "(" + left_->ToString() + " |> " + right_->ToString() + ")"; }


} // namespace del