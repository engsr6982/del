#pragma once
#include <exception>
#include <string>
#include <string_view>

namespace del {

class SyntaxError final : public std::exception {
  std::string message_;
  size_t      line_;
  size_t      col_;
  std::string full_message_;

public:
  explicit SyntaxError(
      std::string_view message,
      size_t           line,
      size_t           col,
      std::string_view path_key = "",
      std::string_view expr     = ""
  );

  const char* what() const override;

  std::string const& message() const;

  size_t line() const;

  size_t col() const;
};


class RuntimeError final : public std::exception {
  std::string message_;
  std::string full_message_;

public:
  explicit RuntimeError(std::string message, std::string_view path_key = "");

  const char* what() const override;
};

} // namespace del