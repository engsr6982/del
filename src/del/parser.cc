#include "parser.h"
#include "ast.h"
#include "exception.h"
#include "lexer.h"
#include "symbol_table.h"


namespace del {

Parser::Parser(Lexer lexer) : lexer_(lexer) {
  NextToken(); // init cur_token_
  NextToken(); // init peek_token_
}

std::unique_ptr<ASTNode> Parser::ParseTopLevelExpression() {
  auto ast = ParseExpression(Precedence::kLowest);

  if (peek_token_.type != TokenType::kEof) {
    throw SyntaxError(
        std::format("Unexpected trailing token '{}' after expression", peek_token_.literal),
        peek_token_.line,
        peek_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
    );
  }

  return std::move(ast);
}

std::unique_ptr<ASTNode> Parser::ParseExpression(Precedence precedence) {
  auto prefix_fn = GetPrefixFn(cur_token_.type);
  if (!prefix_fn) {
    throw SyntaxError(
        std::format("No prefix parse function for token: '{}'", cur_token_.literal),
        cur_token_.line,
        cur_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
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

void Parser::NextToken() {
  cur_token_  = peek_token_;
  peek_token_ = lexer_.NextToken();
}

Precedence Parser::ResolveTokenPrecedence(TokenType type) {
  switch (type) {
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
  case TokenType::kPlus:
  case TokenType::kMinus:
    return Precedence::kSum;
  case TokenType::kAsterisk:
  case TokenType::kSlash:
    return Precedence::kProduct;
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
Precedence Parser::GetPeekPrecedence() const { return ResolveTokenPrecedence(peek_token_.type); }
Precedence Parser::GetCurPrecedence() const { return ResolveTokenPrecedence(cur_token_.type); }

Parser::PrefixFn Parser::GetPrefixFn(TokenType type) {
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

Parser::InfixFn Parser::GetInfixFn(TokenType type) {
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
std::unique_ptr<ASTNode> Parser::ParseNumberLiteral() {
  std::string_view lit = cur_token_.literal;
  if (lit.find('.') != std::string_view::npos) {
    double val = std::stod(std::string(lit));
    return std::make_unique<NumberLiteralNode>(val);
  } else {
    int64_t val = std::stoll(std::string(lit));
    return std::make_unique<NumberLiteralNode>(val);
  }
}

std::unique_ptr<ASTNode> Parser::ParseStringLiteral() {
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

std::unique_ptr<ASTNode> Parser::ParseIdentifier() {
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

std::unique_ptr<ASTNode> Parser::ParsePointer() {
  std::string_view lit       = cur_token_.literal;
  bool             is_source = (lit[0] == '@');
  std::string      path      = std::string(lit.substr(1)); // 保留路径开头的 '/'
  return std::make_unique<PointerNode>(is_source, std::move(path));
}

std::unique_ptr<ASTNode> Parser::ParsePrefixExpression() {
  std::string op = std::string(cur_token_.literal);
  NextToken(); // 消费操作符本身，进入右操作数上下文
  auto right = ParseExpression(Precedence::kPrefix);
  return std::make_unique<PrefixNode>(std::move(op), std::move(right));
}

std::unique_ptr<ASTNode> Parser::ParseGroupedExpression() {
  NextToken(); // 消费 '('
  std::vector<std::unique_ptr<ASTNode>> elements;
  elements.push_back(ParseExpression(Precedence::kLowest));

  // 循环解析逗号分隔的标识符或表达式
  while (peek_token_.type == TokenType::kComma) {
    NextToken(); // 移动到 ','
    NextToken(); // 消费 ','，移至下一个元素
    elements.push_back(ParseExpression(Precedence::kLowest));
  }

  if (peek_token_.type != TokenType::kRparen) {
    throw SyntaxError(
        "Expected closing parenthesis ')'",
        peek_token_.line,
        peek_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
    );
  }
  NextToken(); // 消费 ')'

  if (elements.size() == 1) {
    return std::move(elements[0]);
  }
  return std::make_unique<TupleNode>(std::move(elements));
}

// 中缀解析
std::unique_ptr<ASTNode> Parser::ParseInfixExpression(std::unique_ptr<ASTNode> left) {
  std::string op   = std::string(cur_token_.literal);
  Precedence  prec = GetCurPrecedence();
  NextToken(); // 消费运算符本身
  auto right = ParseExpression(prec);
  return std::make_unique<InfixNode>(std::move(op), std::move(left), std::move(right));
}


std::unique_ptr<ASTNode> Parser::ParseTernaryExpression(std::unique_ptr<ASTNode> left) {
  NextToken(); // 消费 '?'
  auto true_expr = ParseExpression(Precedence::kLowest);
  if (peek_token_.type != TokenType::kColon) {
    throw SyntaxError(
        "Expected ':' in ternary condition",
        peek_token_.line,
        peek_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
    );
  }
  NextToken(); // 推进使 cur_token_ 变为 ':'
  NextToken(); // 消费 ':'，使 cur_token_ 变为 false 分支的起点
  auto false_expr = ParseExpression(Precedence::kTernary);
  return std::make_unique<TernaryNode>(std::move(left), std::move(true_expr), std::move(false_expr));
}

std::unique_ptr<ASTNode> Parser::ParsePipelineExpression(std::unique_ptr<ASTNode> left) {
  NextToken(); // 消费 '|>'
  auto right = ParseExpression(Precedence::kPipeline);
  return std::make_unique<PipelineNode>(std::move(left), std::move(right));
}

std::unique_ptr<ASTNode> Parser::ParseCallExpression(std::unique_ptr<ASTNode> left) {
  auto ident_node = dynamic_cast<IdentifierNode*>(left.get());
  if (!ident_node) {
    throw SyntaxError(
        "Call operator '(' must follow a function name",
        cur_token_.line,
        cur_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
    );
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
    if (peek_token_.type != TokenType::kRparen) {
      throw SyntaxError(
          "Expected ')' at end of function arguments",
          peek_token_.line,
          peek_token_.column,
          lexer_.PathKey(),
          lexer_.Input()
      );
    }
    NextToken(); // 将 ')' 推进为 cur_token_
  }
  return std::make_unique<CallNode>(std::move(func_name), std::move(args));
}

std::unique_ptr<ASTNode> Parser::ParseLambdaExpression(std::unique_ptr<ASTNode> left) {
  std::vector<std::string> params;

  if (auto* ident_node = dynamic_cast<IdentifierNode*>(left.get())) {
    params.push_back(ident_node->name());
  } else if (auto* tuple_node = dynamic_cast<TupleNode*>(left.get())) {
    for (const auto& node : tuple_node->elements()) {
      auto* id = dynamic_cast<IdentifierNode*>(node.get());
      if (!id) {
        throw SyntaxError(
            "Lambda parameter must be a single identifier",
            cur_token_.line,
            cur_token_.column,
            lexer_.PathKey(),
            lexer_.Input()
        );
      }
      params.push_back(id->name());
    }
  } else {
    throw SyntaxError(
        "Left side of lambda '->' must be an identifier or parameter list",
        cur_token_.line,
        cur_token_.column,
        lexer_.PathKey(),
        lexer_.Input()
    );
  }

  NextToken(); // 消费 '->'
  auto body = ParseExpression(Precedence::kLowest);
  return std::make_unique<LambdaNode>(std::move(params), std::move(body));
}


} // namespace del