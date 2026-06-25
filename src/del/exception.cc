#include "exception.h"

namespace del {

SyntaxError::SyntaxError(std::string_view message, size_t line, size_t col)
: message_(message),
  line_(line),
  col_(col) {
  full_message_ = "Syntax Error at Line " + std::to_string(line_) + ", Col " + std::to_string(col_) + ": " + message_;
}

const char* SyntaxError::what() const { return full_message_.c_str(); }

size_t SyntaxError::line() const { return line_; }

size_t SyntaxError::col() const { return col_; }


RuntimeError::RuntimeError(std::string message) : message_(std::move(message)) {}

const char* RuntimeError::what() const { return message_.c_str(); }

} // namespace del