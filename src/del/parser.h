#pragma once
#include "lexer.h"
#include "macro.h"

#include <memory>

namespace del {

class ASTNode;

enum class Precedence : int {
  kLowest       = -3,
  kArrow        = -2, // ->
  kTernary      = -1, // ? :
  kNullCoalesce = 0,  // ??
  kOr           = 1,  // ||
  kAnd          = 2,  // &&
  kEqual        = 3,  // ==, !=
  kRelational   = 4,  // <, >, <=, >=
  kSum          = 5,  // +, -
  kProduct      = 6,  // *, /
  kPipeline     = 7,  // |> (优先级高于算术，确保 a |> b + c 为 (a |> b) + c)
  kPrefix       = 8,  // !, -
  kCall         = 9,  // ()
};

class Parser {
public:
  DEL_DISABLE_COPY(Parser);

  explicit Parser(Lexer lexer);

  std::unique_ptr<ASTNode> ParseTopLevelExpression();

  std::unique_ptr<ASTNode> ParseExpression(Precedence precedence);

private:
  Lexer lexer_;
  Token cur_token_;
  Token peek_token_;

  void NextToken();

  static Precedence ResolveTokenPrecedence(TokenType type);

  Precedence GetPeekPrecedence() const;
  Precedence GetCurPrecedence() const;

  using PrefixFn = std::unique_ptr<ASTNode> (Parser::*)();
  using InfixFn  = std::unique_ptr<ASTNode> (Parser::*)(std::unique_ptr<ASTNode>);

  PrefixFn GetPrefixFn(TokenType type);

  InfixFn GetInfixFn(TokenType type);

  // 前缀解析
  std::unique_ptr<ASTNode> ParseNumberLiteral();

  std::unique_ptr<ASTNode> ParseStringLiteral();

  std::unique_ptr<ASTNode> ParseIdentifier();

  std::unique_ptr<ASTNode> ParsePointer();

  std::unique_ptr<ASTNode> ParsePrefixExpression();

  std::unique_ptr<ASTNode> ParseGroupedExpression();

  // 中缀解析
  std::unique_ptr<ASTNode> ParseInfixExpression(std::unique_ptr<ASTNode> left);

  std::unique_ptr<ASTNode> ParseTernaryExpression(std::unique_ptr<ASTNode> left);

  std::unique_ptr<ASTNode> ParsePipelineExpression(std::unique_ptr<ASTNode> left);

  std::unique_ptr<ASTNode> ParseCallExpression(std::unique_ptr<ASTNode> left);

  std::unique_ptr<ASTNode> ParseLambdaExpression(std::unique_ptr<ASTNode> left);
};


} // namespace del