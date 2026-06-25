#include "lexer.h"
#include "exception.h"

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


Token Lexer::NextToken() {
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
    while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(CurrentChar())) || CurrentChar() == '_')) {
      ConsumeChar();
    }
    return Token{TokenType::kIdentifier, input_.substr(start_pos, pos_ - start_pos), line_, start_col};
  }

  throw SyntaxError(std::string("Unexpected character: ") + ch, line_, start_col);
}

} // namespace del