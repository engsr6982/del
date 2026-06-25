#pragma once
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>

#include "macro.h"

#include "nlohmann/json.hpp"

namespace del {

// ===== Exceptions =====

class SyntaxError final : public std::exception {
  std::string message_;
  size_t      line_;
  size_t      col_;
  std::string full_message_;

public:
  explicit SyntaxError(std::string_view message, size_t line, size_t col) : message_(message), line_(line), col_(col) {
    full_message_ = "Syntax Error at Line " + std::to_string(line_) + ", Col " + std::to_string(col_) + ": " + message_;
  }

  const char* what() const override { return full_message_.c_str(); }

  inline size_t line() const { return line_; }

  inline size_t col() const { return col_; }
};

class RuntimeError final : public std::exception {
  std::string message_;

public:
  explicit RuntimeError(std::string message) : message_(std::move(message)) {}

  const char* what() const override { return message_.c_str(); }
};

// ===== Lexer =====

enum class TokenType {
  kEof,
  kNumber,
  kString,
  kIdentifier,
  kSourcePointer, // @/
  kTargetPointer, // $/
  kPlus,          // +
  kMinus,         // -
  kAsterisk,      // *
  kSlash,         // /
  kLparen,        // (
  kRparen,        // )
  kComma,         // ,
  kPipe,          // |>
  kQuestion,      // ?
  kColon,         // :
  kNullCoalesce,  // ??
  kArrow,         // ->
  kEqual,         // ==
  kNotEqual,      // !=
  kLess,          // <
  kLessEqual,     // <=
  kGreater,       // >
  kGreaterEqual,  // >=
  kAnd,           // &&
  kOr,            // ||
  kNot            // !
};

struct Token {
  TokenType        type{TokenType::kEof};
  std::string_view literal;
  size_t           line{1};
  size_t           column{1};
};

class Lexer {
  std::string_view input_;
  size_t           pos_{0};
  size_t           line_{1};
  size_t           col_{1};

  static constexpr char kEofChar = '\0';

  char CurrentChar() const {
    if (pos_ >= input_.size()) return kEofChar;
    return input_[pos_];
  }

  char PeekChar() const {
    if (pos_ + 1 >= input_.size()) return kEofChar;
    return input_[pos_ + 1];
  }

  void ConsumeChar() {
    if (pos_ < input_.size()) {
      if (input_[pos_] == '\n') {
        line_++;
        col_ = 1;
      } else {
        col_++;
      }
      pos_++;
    }
  }

  void SkipWhitespace() {
    while (pos_ < input_.size()) {
      char c = input_[pos_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ConsumeChar();
      } else {
        break;
      }
    }
  }

  bool IsDelimiter(char c) const {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '(' || c == ')' || c == '|' || c == '?'
        || c == ':' || c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '+' || c == '-' || c == '*'
        || c == '/' || c == kEofChar;
  }

public:
  constexpr explicit Lexer(std::string_view input) : input_(input) {}

  Token NextToken() {
    SkipWhitespace();
    if (pos_ >= input_.size()) {
      return Token{TokenType::kEof, "", line_, col_};
    }

    auto const start_pos = pos_;
    auto const start_col = col_;
    auto const ch        = CurrentChar();

    // @/
    if (ch == '@' && PeekChar() == '/') {
      ConsumeChar(); // '@'
      ConsumeChar(); // '/'
      while (pos_ < input_.size() && !IsDelimiter(CurrentChar())) {
        ConsumeChar();
      }
      return Token{TokenType::kSourcePointer, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    }
    // $/
    if (ch == '$' && PeekChar() == '/') {
      ConsumeChar(); // '$'
      ConsumeChar(); // '/'
      while (pos_ < input_.size() && !IsDelimiter(CurrentChar())) {
        ConsumeChar();
      }
      return Token{TokenType::kTargetPointer, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    }
    // |>
    if (ch == '|') {
      if (PeekChar() == '>') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kPipe, "|>", line_, start_col};
      }
    }
    // ? or ??
    if (ch == '?') {
      if (PeekChar() == '?') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kNullCoalesce, "??", line_, start_col};
      }
      ConsumeChar();
      return Token{TokenType::kQuestion, "?", line_, start_col};
    }
    // ->
    if (ch == '-') {
      if (PeekChar() == '>') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kArrow, "->", line_, start_col};
      }
    }
    // ==
    if (ch == '=') {
      if (PeekChar() == '=') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kEqual, "==", line_, start_col};
      }
    }
    // != or !
    if (ch == '!') {
      if (PeekChar() == '=') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kNotEqual, "!=", line_, start_col};
      }
      ConsumeChar();
      return Token{TokenType::kNot, "!", line_, start_col};
    }
    // <= or <
    if (ch == '<') {
      if (PeekChar() == '=') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kLessEqual, "<=", line_, start_col};
      }
      ConsumeChar();
      return Token{TokenType::kLess, "<", line_, start_col};
    }
    // >= or >
    if (ch == '>') {
      if (PeekChar() == '=') {
        ConsumeChar();
        ConsumeChar();
        return Token{TokenType::kGreaterEqual, ">=", line_, start_col};
      }
      ConsumeChar();
      return Token{TokenType::kGreater, ">", line_, start_col};
    }
    // &&
    if (ch == '&' && PeekChar() == '&') {
      ConsumeChar();
      ConsumeChar();
      return Token{TokenType::kAnd, "&&", line_, start_col};
    }
    // ||
    if (ch == '|' && PeekChar() == '|') {
      ConsumeChar();
      ConsumeChar();
      return Token{TokenType::kOr, "||", line_, start_col};
    }

    // operators
    if (ch == '+') {
      ConsumeChar();
      return Token{TokenType::kPlus, "+", line_, start_col};
    }
    if (ch == '-') {
      ConsumeChar();
      return Token{TokenType::kMinus, "-", line_, start_col};
    }
    if (ch == '*') {
      ConsumeChar();
      return Token{TokenType::kAsterisk, "*", line_, start_col};
    }
    if (ch == '/') {
      ConsumeChar();
      return Token{TokenType::kSlash, "/", line_, start_col};
    }
    if (ch == '(') {
      ConsumeChar();
      return Token{TokenType::kLparen, "(", line_, start_col};
    }
    if (ch == ')') {
      ConsumeChar();
      return Token{TokenType::kRparen, ")", line_, start_col};
    }
    if (ch == ',') {
      ConsumeChar();
      return Token{TokenType::kComma, ",", line_, start_col};
    }
    if (ch == ':') {
      ConsumeChar();
      return Token{TokenType::kColon, ":", line_, start_col};
    }

    // string literal
    if (ch == '"' || ch == '\'') {
      ConsumeChar(); // left " or '
      while (pos_ < input_.size() && CurrentChar() != ch) {
        if (CurrentChar() == '\\' && PeekChar() == ch) {
          ConsumeChar(); // skip \" or \'
        }
        ConsumeChar();
      }
      if (pos_ >= input_.size()) {
        throw SyntaxError("Unterminated string literal", line_, start_col);
      }
      ConsumeChar(); // right " or '
      return Token{TokenType::kString, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    }

    // number literal
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      bool has_dot = false;
      while (pos_ < input_.size()) {
        char next = CurrentChar();
        if (std::isdigit(static_cast<unsigned char>(next))) {
          ConsumeChar();
        } else if (next == '.' && !has_dot && std::isdigit(static_cast<unsigned char>(PeekChar()))) {
          has_dot = true;
          ConsumeChar();
        } else {
          break;
        }
      }
      return Token{TokenType::kNumber, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    }

    // identifier
    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      while (pos_ < input_.size()
             && (std::isalnum(static_cast<unsigned char>(CurrentChar())) || CurrentChar() == '_')) {
        ConsumeChar();
      }
      return Token{TokenType::kIdentifier, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    }

    throw SyntaxError(std::string("Unexpected character: ") + ch, line_, start_col);
  }
};

// ===== AST Node =====

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

// ===== Symbol Table =====

using CustomFunction = std::function<nlohmann::json(
    std::vector<std::unique_ptr<ASTNode>> const&                             args,   //
    EvaluationContext&                                                       ctx,    //
    std::function<nlohmann::json(ASTNode const&, EvaluationContext&)> const& eval_fn //
)>;

class SymbolTable {
public:
  DEL_DISABLE_COPY_MOVE(SymbolTable);

  SymbolTable() = default;

  void Register(std::string name, CustomFunction fn) { functions_[std::move(name)] = std::move(fn); }

  CustomFunction const* Lookup(std::string_view name) const {
    auto it = functions_.find(std::string(name));
    if (it != functions_.end()) {
      return &it->second;
    }
    return nullptr;
  }

private:
  std::unordered_map<std::string, CustomFunction> functions_;
};

// ===== Helper =====

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
    throw RuntimeError(
        std::string(context) + " requires both operands to be of the exact same type. Got " + lhs.dump() + " and "
        + rhs.dump()
    );
  }
}

// ===== AST Nodes =====

class NumberLiteralNode : public ASTNode {
public:
  explicit NumberLiteralNode(double val) : is_double_(true), d_val_(val) {}
  explicit NumberLiteralNode(int64_t val) : is_double_(false), i_val_(val) {}

  nlohmann::json Evaluate(EvaluationContext&) const override {
    if (is_double_) return d_val_;
    return i_val_;
  }
  std::string ToString() const override { return is_double_ ? std::to_string(d_val_) : std::to_string(i_val_); }

private:
  bool    is_double_;
  double  d_val_ = 0.0;
  int64_t i_val_ = 0;
};

class StringLiteralNode : public ASTNode {
public:
  explicit StringLiteralNode(std::string val) : val_(std::move(val)) {}
  nlohmann::json Evaluate(EvaluationContext&) const override { return val_; }
  std::string    ToString() const override { return "\"" + val_ + "\""; }

private:
  std::string val_;
};

class BooleanLiteralNode : public ASTNode {
public:
  explicit BooleanLiteralNode(bool val) : val_(val) {}
  nlohmann::json Evaluate(EvaluationContext&) const override { return val_; }
  std::string    ToString() const override { return val_ ? "true" : "false"; }

private:
  bool val_;
};

class NullLiteralNode : public ASTNode {
public:
  nlohmann::json Evaluate(EvaluationContext&) const override { return nullptr; }
  std::string    ToString() const override { return "null"; }
};

class IdentifierNode : public ASTNode {
public:
  explicit IdentifierNode(std::string name) : name_(std::move(name)) {}
  const std::string& name() const { return name_; }

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
    auto it = ctx.locals.find(name_);
    if (it != ctx.locals.end()) {
      return it->second;
    }
    throw RuntimeError("Undefined identifier: " + name_);
  }
  std::string ToString() const override { return name_; }

private:
  std::string name_;
};

class PointerNode : public ASTNode {
public:
  PointerNode(bool is_source, std::string path) : is_source_(is_source), path_(std::move(path)) {}

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
    const auto& root = is_source_ ? ctx.source : ctx.target;
    try {
      auto json_ptr = nlohmann::json::json_pointer(path_);
      if (root.contains(json_ptr)) {
        return root[json_ptr];
      }
    } catch (...) {
      // RFC 6901 寻址失败时，静默求值为 null
      // TODO: 如果是 target 考虑自动新建?
    }
    return nullptr;
  }

  std::string ToString() const override { return (is_source_ ? "@" : "$") + path_; }

private:
  bool        is_source_;
  std::string path_;
};

class PrefixNode : public ASTNode {
public:
  PrefixNode(std::string op, std::unique_ptr<ASTNode> right) : op_(std::move(op)), right_(std::move(right)) {}

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
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

  std::string ToString() const override { return "(" + op_ + right_->ToString() + ")"; }

private:
  std::string              op_;
  std::unique_ptr<ASTNode> right_;
};

class InfixNode : public ASTNode {
public:
  InfixNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
  : op_(std::move(op)),
    left_(std::move(left)),
    right_(std::move(right)) {}

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
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

  std::string ToString() const override { return "(" + left_->ToString() + " " + op_ + " " + right_->ToString() + ")"; }

private:
  std::string              op_;
  std::unique_ptr<ASTNode> left_;
  std::unique_ptr<ASTNode> right_;
};

class TernaryNode : public ASTNode {
public:
  TernaryNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> true_expr, std::unique_ptr<ASTNode> false_expr)
  : cond_(std::move(cond)),
    true_expr_(std::move(true_expr)),
    false_expr_(std::move(false_expr)) {}

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
    auto cond_val = cond_->Evaluate(ctx);
    EnsureBoolean(cond_val, "Ternary condition");
    if (cond_val.get<bool>()) {
      return true_expr_->Evaluate(ctx);
    } else {
      return false_expr_->Evaluate(ctx);
    }
  }

  std::string ToString() const override {
    return "(" + cond_->ToString() + " ? " + true_expr_->ToString() + " : " + false_expr_->ToString() + ")";
  }

private:
  std::unique_ptr<ASTNode> cond_;
  std::unique_ptr<ASTNode> true_expr_;
  std::unique_ptr<ASTNode> false_expr_;
};

class LambdaNode : public ASTNode {
public:
  LambdaNode(std::string param_name, std::unique_ptr<ASTNode> body)
  : param_name_(std::move(param_name)),
    body_(std::move(body)) {}

  const std::string& param_name() const { return param_name_; }
  const ASTNode&     body() const { return *body_; }

  nlohmann::json Evaluate(EvaluationContext&) const override {
    throw RuntimeError("Lambda expressions cannot be evaluated outside of high-order functions");
  }

  std::string ToString() const override { return param_name_ + " -> " + body_->ToString(); }

private:
  std::string              param_name_;
  std::unique_ptr<ASTNode> body_;
};

class CallNode : public ASTNode {
public:
  CallNode(std::string name, std::vector<std::unique_ptr<ASTNode>> args)
  : name_(std::move(name)),
    args_(std::move(args)) {}

  const std::string&                           name() const { return name_; }
  const std::vector<std::unique_ptr<ASTNode>>& args() const { return args_; }

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
    const auto* func = ctx.symbol_table.Lookup(name_);
    if (!func) {
      throw RuntimeError("Undefined function call: " + name_);
    }
    return (*func)(args_, ctx, [](const ASTNode& node, EvaluationContext& c) { return node.Evaluate(c); });
  }

  std::string ToString() const override {
    std::string res = name_ + "(";
    for (size_t i = 0; i < args_.size(); ++i) {
      if (i > 0) res += ", ";
      res += args_[i]->ToString();
    }
    res += ")";
    return res;
  }

private:
  std::string                           name_;
  std::vector<std::unique_ptr<ASTNode>> args_;
};

// 辅助注入节点：用于管道执行
class PreEvaluatedNode : public ASTNode {
public:
  explicit PreEvaluatedNode(nlohmann::json val) : val_(std::move(val)) {}
  nlohmann::json Evaluate(EvaluationContext&) const override { return val_; }
  std::string    ToString() const override { return val_.dump(); }

private:
  nlohmann::json val_;
};

class ReferenceNode : public ASTNode {
public:
  explicit ReferenceNode(const ASTNode& ref) : ref_(ref) {}
  nlohmann::json Evaluate(EvaluationContext& ctx) const override { return ref_.Evaluate(ctx); }
  std::string    ToString() const override { return ref_.ToString(); }

private:
  const ASTNode& ref_;
};

class PipelineNode : public ASTNode {
public:
  PipelineNode(std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
  : left_(std::move(left)),
    right_(std::move(right)) {}

  nlohmann::json Evaluate(EvaluationContext& ctx) const override {
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

  std::string ToString() const override { return "(" + left_->ToString() + " |> " + right_->ToString() + ")"; }

private:
  std::unique_ptr<ASTNode> left_;
  std::unique_ptr<ASTNode> right_;
};

// ===== Parser =====

enum class Precedence : int {
  kLowest       = -3,
  kArrow        = -2, // ->
  kTernary      = -1, // ? :
  kNullCoalesce = 0,  // ??
  kOr           = 1,  // ||
  kAnd          = 2,  // &&
  kEqual        = 3,  // ==, !=
  kRelational   = 4,  // <, >, <=, >=
  kPipeline     = 5,  // |>
  kPrefix       = 6,  // !, -
  kCall         = 7,  // ()
};

class Parser {
public:
  DEL_DISABLE_COPY(Parser);

  explicit Parser(Lexer lexer, const SymbolTable& symbol_table) : lexer_(lexer), symbol_table_(symbol_table) {
    NextToken(); // init cur_token_
    NextToken(); // init peek_token_
  }

  std::unique_ptr<ASTNode> ParseExpression(Precedence precedence) {
    auto prefix_fn = GetPrefixFn(cur_token_.type);
    if (!prefix_fn) {
      throw SyntaxError(
          "No prefix parse function for token: " + std::string(cur_token_.literal),
          cur_token_.line,
          cur_token_.column
      );
    }

    std::unique_ptr<ASTNode> left = (this->*prefix_fn)();

    while (peek_token_.type != TokenType::kEof && precedence < GetPeekPrecedence()) {
      auto infix_fn = GetInfixFn(peek_token_.type);
      if (!infix_fn) {
        return left;
      }
      NextToken(); // 将 peek_token_ 推进为当前的 cur_token_ 运算符
      left = (this->*infix_fn)(std::move(left));
    }

    return left;
  }

private:
  Lexer              lexer_;
  const SymbolTable& symbol_table_;
  Token              cur_token_;
  Token              peek_token_;

  void NextToken() {
    cur_token_  = peek_token_;
    peek_token_ = lexer_.NextToken();
  }

  Precedence GetPeekPrecedence() const {
    switch (peek_token_.type) {
    case TokenType::kQuestion:
      return Precedence::kTernary;
    case TokenType::kNullCoalesce:
      return Precedence::kNullCoalesce;
    case TokenType::kOr:
      return Precedence::kOr;
    case TokenType::kAnd:
      return Precedence::kAnd;
    case TokenType::kEqual:
    case TokenType::kNotEqual:
      return Precedence::kEqual;
    case TokenType::kLess:
    case TokenType::kLessEqual:
    case TokenType::kGreater:
    case TokenType::kGreaterEqual:
      return Precedence::kRelational;
    case TokenType::kPipe:
      return Precedence::kPipeline;
    case TokenType::kLparen:
      return Precedence::kCall;
    case TokenType::kArrow:
      return Precedence::kArrow;
    default:
      return Precedence::kLowest;
    }
  }

  using PrefixFn = std::unique_ptr<ASTNode> (Parser::*)();
  using InfixFn  = std::unique_ptr<ASTNode> (Parser::*)(std::unique_ptr<ASTNode>);

  PrefixFn GetPrefixFn(TokenType type) {
    switch (type) {
    case TokenType::kNumber:
      return &Parser::ParseNumberLiteral;
    case TokenType::kString:
      return &Parser::ParseStringLiteral;
    case TokenType::kIdentifier:
      return &Parser::ParseIdentifier;
    case TokenType::kSourcePointer:
    case TokenType::kTargetPointer:
      return &Parser::ParsePointer;
    case TokenType::kMinus:
    case TokenType::kNot:
      return &Parser::ParsePrefixExpression;
    case TokenType::kLparen:
      return &Parser::ParseGroupedExpression;
    default:
      return nullptr;
    }
  }

  InfixFn GetInfixFn(TokenType type) {
    switch (type) {
    case TokenType::kPlus:
    case TokenType::kMinus:
    case TokenType::kAsterisk:
    case TokenType::kSlash:
    case TokenType::kEqual:
    case TokenType::kNotEqual:
    case TokenType::kLess:
    case TokenType::kLessEqual:
    case TokenType::kGreater:
    case TokenType::kGreaterEqual:
    case TokenType::kAnd:
    case TokenType::kOr:
    case TokenType::kNullCoalesce:
      return &Parser::ParseInfixExpression;
    case TokenType::kQuestion:
      return &Parser::ParseTernaryExpression;
    case TokenType::kPipe:
      return &Parser::ParsePipelineExpression;
    case TokenType::kLparen:
      return &Parser::ParseCallExpression;
    case TokenType::kArrow:
      return &Parser::ParseLambdaExpression;
    default:
      return nullptr;
    }
  }

  // 前缀解析
  std::unique_ptr<ASTNode> ParseNumberLiteral() {
    std::string_view lit = cur_token_.literal;
    if (lit.find('.') != std::string_view::npos) {
      double val = std::stod(std::string(lit));
      return std::make_unique<NumberLiteralNode>(val);
    } else {
      int64_t val = std::stoll(std::string(lit));
      return std::make_unique<NumberLiteralNode>(val);
    }
  }

  std::unique_ptr<ASTNode> ParseStringLiteral() {
    std::string_view lit = cur_token_.literal;
    std::string      unescaped;
    if (lit.size() >= 2) {
      std::string_view inner = lit.substr(1, lit.size() - 2);
      for (size_t i = 0; i < inner.size(); ++i) {
        if (inner[i] == '\\' && i + 1 < inner.size()) {
          if (inner[i + 1] == '"') {
            unescaped += '"';
            i++;
          } else if (inner[i + 1] == '\\') {
            unescaped += '\\';
            i++;
          } else {
            unescaped += inner[i];
          }
        } else {
          unescaped += inner[i];
        }
      }
    }
    return std::make_unique<StringLiteralNode>(std::move(unescaped));
  }

  std::unique_ptr<ASTNode> ParseIdentifier() {
    std::string_view lit = cur_token_.literal;
    if (lit == "true") {
      return std::make_unique<BooleanLiteralNode>(true);
    } else if (lit == "false") {
      return std::make_unique<BooleanLiteralNode>(false);
    } else if (lit == "null") {
      return std::make_unique<NullLiteralNode>();
    }
    return std::make_unique<IdentifierNode>(std::string(lit));
  }

  std::unique_ptr<ASTNode> ParsePointer() {
    std::string_view lit       = cur_token_.literal;
    bool             is_source = (lit[0] == '@');
    std::string      path      = std::string(lit.substr(1)); // 保留路径开头的 '/'
    return std::make_unique<PointerNode>(is_source, std::move(path));
  }

  std::unique_ptr<ASTNode> ParsePrefixExpression() {
    std::string op = std::string(cur_token_.literal);
    NextToken(); // 消费操作符本身，进入右操作数上下文
    auto right = ParseExpression(Precedence::kPrefix);
    return std::make_unique<PrefixNode>(std::move(op), std::move(right));
  }

  std::unique_ptr<ASTNode> ParseGroupedExpression() {
    NextToken(); // 消费 '('
    auto expr = ParseExpression(Precedence::kLowest);
    if (peek_token_.type != TokenType::kRparen) {
      throw SyntaxError("Expected closing parenthesis ')'", peek_token_.line, peek_token_.column);
    }
    NextToken(); // 将 ')' 推进为当前的 cur_token_，完成括号闭合
    return expr;
  }

  // 中缀解析
  std::unique_ptr<ASTNode> ParseInfixExpression(std::unique_ptr<ASTNode> left) {
    std::string op   = std::string(cur_token_.literal);
    Precedence  prec = GetCurPrecedence();
    NextToken(); // 消费运算符本身
    auto right = ParseExpression(prec);
    return std::make_unique<InfixNode>(std::move(op), std::move(left), std::move(right));
  }

  Precedence GetCurPrecedence() const {
    switch (cur_token_.type) {
    case TokenType::kQuestion:
      return Precedence::kTernary;
    case TokenType::kNullCoalesce:
      return Precedence::kNullCoalesce;
    case TokenType::kOr:
      return Precedence::kOr;
    case TokenType::kAnd:
      return Precedence::kAnd;
    case TokenType::kEqual:
    case TokenType::kNotEqual:
      return Precedence::kEqual;
    case TokenType::kLess:
    case TokenType::kLessEqual:
    case TokenType::kGreater:
    case TokenType::kGreaterEqual:
      return Precedence::kRelational;
    case TokenType::kPipe:
      return Precedence::kPipeline;
    case TokenType::kLparen:
      return Precedence::kCall;
    case TokenType::kArrow:
      return Precedence::kArrow;
    default:
      return Precedence::kLowest;
    }
  }

  std::unique_ptr<ASTNode> ParseTernaryExpression(std::unique_ptr<ASTNode> left) {
    NextToken(); // 消费 '?'
    auto true_expr = ParseExpression(Precedence::kLowest);
    if (peek_token_.type != TokenType::kColon) {
      throw SyntaxError("Expected ':' in ternary condition", peek_token_.line, peek_token_.column);
    }
    NextToken(); // 推进使 cur_token_ 变为 ':'
    NextToken(); // 消费 ':'，使 cur_token_ 变为 false 分支的起点
    auto false_expr = ParseExpression(Precedence::kTernary);
    return std::make_unique<TernaryNode>(std::move(left), std::move(true_expr), std::move(false_expr));
  }

  std::unique_ptr<ASTNode> ParsePipelineExpression(std::unique_ptr<ASTNode> left) {
    NextToken(); // 消费 '|>'
    auto right = ParseExpression(Precedence::kPipeline);
    return std::make_unique<PipelineNode>(std::move(left), std::move(right));
  }

  std::unique_ptr<ASTNode> ParseCallExpression(std::unique_ptr<ASTNode> left) {
    auto ident_node = dynamic_cast<IdentifierNode*>(left.get());
    if (!ident_node) {
      throw SyntaxError("Call operator '(' must follow a function name", cur_token_.line, cur_token_.column);
    }
    std::string func_name = ident_node->name();
    NextToken(); // 消费 '('
    std::vector<std::unique_ptr<ASTNode>> args;
    if (cur_token_.type != TokenType::kRparen) {
      args.push_back(ParseExpression(Precedence::kLowest));
      while (peek_token_.type == TokenType::kComma) {
        NextToken(); // 消费参数之间的逗号的前置 Token，使 cur_token_ 变为 ','
        NextToken(); // 消费 ','，使 cur_token_ 变为下一个参数
        args.push_back(ParseExpression(Precedence::kLowest));
      }
    }
    if (peek_token_.type != TokenType::kRparen) {
      throw SyntaxError("Expected ')' at end of function arguments", peek_token_.line, peek_token_.column);
    }
    NextToken(); // 推进使 ')' 成为当前的 cur_token_
    return std::make_unique<CallNode>(std::move(func_name), std::move(args));
  }

  std::unique_ptr<ASTNode> ParseLambdaExpression(std::unique_ptr<ASTNode> left) {
    auto ident_node = dynamic_cast<IdentifierNode*>(left.get());
    if (!ident_node) {
      throw SyntaxError("Left side of lambda '->' must be a single identifier", cur_token_.line, cur_token_.column);
    }
    std::string param_name = ident_node->name();
    NextToken(); // 消费 '->'
    auto body = ParseExpression(Precedence::kLowest);
    return std::make_unique<LambdaNode>(std::move(param_name), std::move(body));
  }
};

// ===== Bult-in Functions =====

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

// ===== Template Engine =====

struct CompiledPathExpr {
  std::string              target_pointer_path;
  nlohmann::json           direct_value; // 非公式值直接存放
  std::unique_ptr<ASTNode> compiled_ast; // 公式编译后的 AST 树
};

class CompiledTemplate {
public:
  std::vector<CompiledPathExpr> instructions;

  CompiledTemplate()                              = default;
  CompiledTemplate(CompiledTemplate&&)            = default;
  CompiledTemplate& operator=(CompiledTemplate&&) = default;

  DEL_DISABLE_COPY(CompiledTemplate);
};

class TemplateEngine {
public:
  TemplateEngine() { RegisterBuiltins(symbol_table_); }

  void RegisterCustomFunction(std::string name, CustomFunction fn) {
    symbol_table_.Register(std::move(name), std::move(fn));
  }

  // 对模板执行一次编译
  CompiledTemplate Compile(const nlohmann::ordered_json& template_json) {
    CompiledTemplate ct;
    for (auto it = template_json.begin(); it != template_json.end(); ++it) {
      const std::string& path     = it.key();
      const auto&        expr_val = it.value();

      CompiledPathExpr instr;
      instr.target_pointer_path = path;

      if (expr_val.is_string()) {
        std::string expr_str = expr_val.get<std::string>();
        Lexer       lexer(expr_str);
        Parser      parser(lexer, symbol_table_);
        try {
          instr.compiled_ast = parser.ParseExpression(Precedence::kLowest);
        } catch (const SyntaxError& e) {
          throw std::runtime_error(
              std::format("Template compilation failed at target path '{}': {}\nSource: {}", path, e.what(), expr_str)
          );
        }
      } else {
        instr.direct_value = expr_val;
      }
      ct.instructions.push_back(std::move(instr));
    }
    return ct;
  }

  // 极速运行：在转换几千条数据时，仅调用此函数执行 AST 遍历
  nlohmann::json Execute(const CompiledTemplate& ct, const nlohmann::json& source_json) {
    nlohmann::json    target_json = nlohmann::json::object();
    EvaluationContext ctx{source_json, target_json, {}, symbol_table_};

    for (const auto& instr : ct.instructions) {
      try {
        if (instr.compiled_ast) {
          nlohmann::json result = instr.compiled_ast->Evaluate(ctx);
          MountTargetValue(target_json, instr.target_pointer_path, std::move(result));
        } else {
          MountTargetValue(target_json, instr.target_pointer_path, instr.direct_value);
        }
      } catch (const RuntimeError& e) {
        throw std::runtime_error(
            "Runtime execution failed at target path '" + instr.target_pointer_path + "': " + e.what()
        );
      }
    }
    return target_json;
  }

private:
  SymbolTable symbol_table_;

  void MountTargetValue(nlohmann::json& target, const std::string& path_str, nlohmann::json value) {
    try {
      nlohmann::json::json_pointer ptr(path_str);
      // nlohmann::json 的 operator[] 遇到不存在的父路径会自动构建嵌套对象
      target[ptr] = std::move(value);
    } catch (const std::exception& e) {
      throw std::runtime_error("Failed to mount target path '" + path_str + "': " + e.what());
    }
  }
};

} // namespace del