#include "lexer.h"
#include "exception.h"
#include <format>

namespace del {


char Lexer::CurrentChar() const {
  if (pos_ >= input_.size()) return kEofChar;
  return input_[pos_];
}

char Lexer::PeekChar() const {
  if (pos_ + 1 >= input_.size()) return kEofChar;
  return input_[pos_ + 1];
}

void Lexer::ConsumeChar() {
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

void Lexer::SkipWhitespace() {
  while (pos_ < input_.size()) {
    char c = input_[pos_];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ConsumeChar();
    } else {
      break;
    }
  }
}

bool Lexer::IsDelimiter(char c) const {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '(' || c == ')' || c == '|' || c == '?'
      || c == ':' || c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '+' || c == '-' || c == '*'
      || c == '/' || c == kEofChar;
}
bool Lexer::IsPointerDelimiter(char c) const {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '(' || c == ')' || c == '|' || c == '?'
      || c == ':' || c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '+' || c == '-' || c == '*'
      || c == kEofChar;
}
Token Lexer::ReadNumberToken(size_t start_pos, size_t start_col) {
  char first_digit = CurrentChar();

  // 前导零
  if (first_digit == '0') {
    ConsumeChar();
    // 如果首位是 '0'，其后绝不能紧跟其它数字（例如不允许 '05'）
    if (std::isdigit(static_cast<unsigned char>(CurrentChar()))) {
      throw SyntaxError("Leading zeros are not allowed in number literals", line_, start_col, path_key_, input_);
    }
  } else if (std::isdigit(static_cast<unsigned char>(first_digit))) {
    while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(CurrentChar()))) {
      ConsumeChar();
    }
  } else {
    throw SyntaxError("Invalid number literal structure", line_, start_col, path_key_, input_);
  }

  // 小数点校验
  if (CurrentChar() == '.') {
    // 规约：小数点后必须有至少一位数字（例如不允许 '5.' 这样的截断）
    if (!std::isdigit(static_cast<unsigned char>(PeekChar()))) {
      throw SyntaxError("Decimal point must be followed by at least one digit", line_, start_col, path_key_, input_);
    }
    ConsumeChar(); // 消费 '.'
    while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(CurrentChar()))) {
      ConsumeChar();
    }
  }

  // 非法后续：避免将 '5abc'、'5_foo' 或 '5.1.2' 隐式截断
  char next_ch = CurrentChar();
  if (std::isalnum(static_cast<unsigned char>(next_ch)) || next_ch == '_' || next_ch == '.') {
    throw SyntaxError("Invalid suffix or bad formatting after number literal", line_, start_col, path_key_, input_);
  }

  std::string_view lit = input_.substr(start_pos, pos_ - start_pos);
  return Token{TokenType::kNumber, lit, line_, start_col};
}


Token Lexer::NextToken() {
  SkipWhitespace();
  if (pos_ >= input_.size()) {
    auto const tok   = Token{TokenType::kEof, "", line_, col_};
    last_token_type_ = tok.type;
    return tok;
  }

  auto const start_pos = pos_;
  auto const start_col = col_;
  auto const ch        = CurrentChar();

  // number literal
  {
    // 上下文感知负号作为数字前缀的判定
    bool can_be_negative =
        (last_token_type_ == TokenType::kEof || last_token_type_ == TokenType::kPlus
         || last_token_type_ == TokenType::kMinus || last_token_type_ == TokenType::kAsterisk
         || last_token_type_ == TokenType::kSlash || last_token_type_ == TokenType::kLparen
         || last_token_type_ == TokenType::kComma || last_token_type_ == TokenType::kPipe
         || last_token_type_ == TokenType::kQuestion || last_token_type_ == TokenType::kColon
         || last_token_type_ == TokenType::kNullCoalesce || last_token_type_ == TokenType::kArrow
         || last_token_type_ == TokenType::kEqual || last_token_type_ == TokenType::kNotEqual
         || last_token_type_ == TokenType::kLess || last_token_type_ == TokenType::kLessEqual
         || last_token_type_ == TokenType::kGreater || last_token_type_ == TokenType::kGreaterEqual
         || last_token_type_ == TokenType::kAnd || last_token_type_ == TokenType::kOr
         || last_token_type_ == TokenType::kNot);

    // 负数整体解析 (如 "-5")
    if (ch == '-' && std::isdigit(static_cast<unsigned char>(PeekChar())) && can_be_negative) {
      ConsumeChar(); // 消费 '-'
      auto const tok   = ReadNumberToken(start_pos, start_col);
      last_token_type_ = tok.type;
      return tok;
    }

    // 正数解析 (如 "5")
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      auto const tok   = ReadNumberToken(start_pos, start_col);
      last_token_type_ = tok.type;
      return tok;
    }
  }

  // @/
  if (ch == '@' && PeekChar() == '/') {
    ConsumeChar(); // '@'
    ConsumeChar(); // '/'
    while (pos_ < input_.size() && !IsPointerDelimiter(CurrentChar())) {
      ConsumeChar();
    }
    auto const tok   = Token{TokenType::kSourcePointer, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // $/
  if (ch == '$' && PeekChar() == '/') {
    ConsumeChar(); // '$'
    ConsumeChar(); // '/'
    while (pos_ < input_.size() && !IsPointerDelimiter(CurrentChar())) {
      ConsumeChar();
    }
    auto const tok   = Token{TokenType::kTargetPointer, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // |>
  if (ch == '|') {
    if (PeekChar() == '>') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kPipe, "|>", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
  }
  // ? or ??
  if (ch == '?') {
    if (PeekChar() == '?') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kNullCoalesce, "??", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
    ConsumeChar();
    auto const tok   = Token{TokenType::kQuestion, "?", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // -> or -
  if (ch == '-') {
    if (PeekChar() == '>') { // lambda ->
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kArrow, "->", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
    // -
    ConsumeChar();
    auto const tok   = Token{TokenType::kMinus, "-", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // ==
  if (ch == '=') {
    if (PeekChar() == '=') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kEqual, "==", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
  }
  // != or !
  if (ch == '!') {
    if (PeekChar() == '=') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kNotEqual, "!=", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
    ConsumeChar();
    auto const tok   = Token{TokenType::kNot, "!", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // <= or <
  if (ch == '<') {
    if (PeekChar() == '=') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kLessEqual, "<=", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
    ConsumeChar();
    auto const tok   = Token{TokenType::kLess, "<", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // >= or >
  if (ch == '>') {
    if (PeekChar() == '=') {
      ConsumeChar();
      ConsumeChar();
      auto const tok   = Token{TokenType::kGreaterEqual, ">=", line_, start_col};
      last_token_type_ = tok.type;
      return tok;
    }
    ConsumeChar();
    auto const tok   = Token{TokenType::kGreater, ">", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // &&
  if (ch == '&' && PeekChar() == '&') {
    ConsumeChar();
    ConsumeChar();
    auto const tok   = Token{TokenType::kAnd, "&&", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  // ||
  if (ch == '|' && PeekChar() == '|') {
    ConsumeChar();
    ConsumeChar();
    auto const tok   = Token{TokenType::kOr, "||", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }

  // operators
  if (ch == '+') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kPlus, "+", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == '*') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kAsterisk, "*", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == '/') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kSlash, "/", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == '(') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kLparen, "(", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == ')') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kRparen, ")", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == ',') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kComma, ",", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }
  if (ch == ':') {
    ConsumeChar();
    auto const tok   = Token{TokenType::kColon, ":", line_, start_col};
    last_token_type_ = tok.type;
    return tok;
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
      throw SyntaxError("Unterminated string literal", line_, start_col, path_key_, input_);
    }
    ConsumeChar(); // right " or '
    auto const tok   = Token{TokenType::kString, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }

  // identifier
  if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
    while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(CurrentChar())) || CurrentChar() == '_')) {
      ConsumeChar();
    }
    auto const tok   = Token{TokenType::kIdentifier, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
    last_token_type_ = tok.type;
    return tok;
  }

  throw SyntaxError(std::format("Unexpected character: '{}'", ch), line_, start_col, path_key_, input_);
}

} // namespace del