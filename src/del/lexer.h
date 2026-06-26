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
  std::string_view path_key_; // for SyntaxError
  size_t           pos_{0};
  size_t           line_{1};
  size_t           col_{1};
  TokenType        last_token_type_{TokenType::kEof};

  static constexpr char kEofChar = '\0';

  char CurrentChar() const;

  char PeekChar() const;

  void ConsumeChar();

  void SkipWhitespace();

  bool IsDelimiter(char c) const;

  bool IsPointerDelimiter(char c) const;

  Token ReadNumberToken(size_t start_pos, size_t start_col);

public:
  constexpr explicit Lexer(std::string_view input, std::string_view path_key = "")
  : input_(input),
    path_key_(path_key) {}

  [[nodiscard]] inline std::string_view Input() const { return input_; }

  [[nodiscard]] inline std::string_view PathKey() const { return path_key_; }

  [[nodiscard]] Token NextToken();
};


} // namespace del