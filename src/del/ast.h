#pragma once
#include "macro.h"

#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace del {

class ASTNode;
class SymbolTable;

struct EvaluationContext {
  nlohmann::json const&                                source;
  nlohmann::json&                                      target;
  std::unordered_map<std::string_view, nlohmann::json> locals;
  SymbolTable const&                                   symbol_table;
};

class ASTNode {
public:
  DEL_DISABLE_COPY(ASTNode);

  ASTNode() = default;

  virtual ~ASTNode() = default;

  [[nodiscard]] virtual nlohmann::json Evaluate(EvaluationContext& ctx) const = 0;

  [[nodiscard]] virtual std::string ToString() const = 0;
};


// impl

class NumberLiteralNode : public ASTNode {
public:
  constexpr explicit NumberLiteralNode(double val) : is_double_(true), d_val_(val) {}
  constexpr explicit NumberLiteralNode(int64_t val) : is_double_(false), i_val_(val) {}

  nlohmann::json Evaluate(EvaluationContext&) const override;
  std::string    ToString() const override;

private:
  bool    is_double_;
  double  d_val_ = 0.0;
  int64_t i_val_ = 0;
};

class StringLiteralNode : public ASTNode {
public:
  explicit StringLiteralNode(std::string val);
  nlohmann::json Evaluate(EvaluationContext&) const override;
  std::string    ToString() const override;

private:
  std::string val_;
};

class BooleanLiteralNode : public ASTNode {
public:
  constexpr explicit BooleanLiteralNode(bool val) : val_(val) {}
  nlohmann::json Evaluate(EvaluationContext&) const override;
  std::string    ToString() const override;

private:
  bool val_;
};

class NullLiteralNode : public ASTNode {
public:
  nlohmann::json Evaluate(EvaluationContext&) const override;
  std::string    ToString() const override;
};

class IdentifierNode : public ASTNode {
public:
  explicit IdentifierNode(std::string name);
  const std::string& name() const;

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;
  std::string    ToString() const override;

private:
  std::string name_;
};

class PointerNode : public ASTNode {
public:
  PointerNode(bool is_source, std::string path);

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  bool        is_source_;
  std::string path_;
};

class PrefixNode : public ASTNode {
public:
  PrefixNode(std::string op, std::unique_ptr<ASTNode> right);

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  std::string              op_;
  std::unique_ptr<ASTNode> right_;
};

class InfixNode : public ASTNode {
public:
  InfixNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right);

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  std::string              op_;
  std::unique_ptr<ASTNode> left_;
  std::unique_ptr<ASTNode> right_;
};

class TernaryNode : public ASTNode {
public:
  TernaryNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> true_expr, std::unique_ptr<ASTNode> false_expr);

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  std::unique_ptr<ASTNode> cond_;
  std::unique_ptr<ASTNode> true_expr_;
  std::unique_ptr<ASTNode> false_expr_;
};

class LambdaNode : public ASTNode {
public:
  LambdaNode(std::string param_name, std::unique_ptr<ASTNode> body);

  const std::string& param_name() const;
  const ASTNode&     body() const;

  nlohmann::json Evaluate(EvaluationContext&) const override;

  std::string ToString() const override;

private:
  std::string              param_name_;
  std::unique_ptr<ASTNode> body_;
};

class CallNode : public ASTNode {
public:
  CallNode(std::string name, std::vector<std::unique_ptr<ASTNode>> args);

  const std::string&                           name() const;
  const std::vector<std::unique_ptr<ASTNode>>& args() const;

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  std::string                           name_;
  std::vector<std::unique_ptr<ASTNode>> args_;
};

// 辅助注入节点：用于管道执行
class PreEvaluatedNode : public ASTNode {
public:
  explicit PreEvaluatedNode(nlohmann::json val);
  nlohmann::json Evaluate(EvaluationContext&) const override;
  std::string    ToString() const override;

private:
  nlohmann::json val_;
};

class ReferenceNode : public ASTNode {
public:
  explicit ReferenceNode(const ASTNode& ref);
  nlohmann::json Evaluate(EvaluationContext& ctx) const override;
  std::string    ToString() const override;

private:
  const ASTNode& ref_;
};

class PipelineNode : public ASTNode {
public:
  PipelineNode(std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right);

  nlohmann::json Evaluate(EvaluationContext& ctx) const override;

  std::string ToString() const override;

private:
  std::unique_ptr<ASTNode> left_;
  std::unique_ptr<ASTNode> right_;
};


} // namespace del