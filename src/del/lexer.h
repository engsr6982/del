#pragma once
#include <string>
#include <string_view>

namespace del {


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

  char CurrentChar() const;

  char PeekChar() const;

  void ConsumeChar();

  void SkipWhitespace();

  bool IsDelimiter(char c) const;

  bool IsPointerDelimiter(char c) const;

public:
  constexpr explicit Lexer(std::string_view input) : input_(input) {}

  [[nodiscard]] Token NextToken();
};


} // namespace del