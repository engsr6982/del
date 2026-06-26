#include "exception.h"
#include <format>

namespace del {

SyntaxError::SyntaxError(
    std::string_view message,
    size_t           line,
    size_t           col,
    std::string_view path_key,
    std::string_view expr
)
: message_(message),
  line_(line),
  col_(col) {

  if (!expr.empty()) {
    static constexpr std::string_view kUnknownPathKey = "<unknown>";

    std::string at           = std::format("    at '{}': ", path_key.empty() ? kUnknownPathKey : path_key);
    size_t      spaces_count = at.length() + (col_ - 1);

    std::string caret_line(spaces_count, ' ');
    caret_line += "^\n";

    full_message_ = std::format("Syntax Error: {} (#{}:{})\n{}{}\n{}", message_, line_, col_, at, expr, caret_line);
  } else {
    full_message_ = std::format("Syntax Error: {} (#{}:{})", message_, line_, col_);
  }
}

const char*        SyntaxError::what() const { return full_message_.c_str(); }
std::string const& SyntaxError::message() const { return message_; }
size_t             SyntaxError::line() const { return line_; }
size_t             SyntaxError::col() const { return col_; }


RuntimeError::RuntimeError(std::string message, std::string_view path_key) : message_(std::move(message)) {
  if (!path_key.empty()) {
    full_message_ = std::format("Runtime Error at '{}': {}", path_key, message_);
  } else {
    full_message_ = std::format("Runtime Error: {}", message_);
  }
}

const char* RuntimeError::what() const { return full_message_.c_str(); }

} // namespace del