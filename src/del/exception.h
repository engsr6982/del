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
  explicit SyntaxError(std::string_view message, size_t line, size_t col);

  const char* what() const override;

  size_t line() const;

  size_t col() const;
};


class RuntimeError final : public std::exception {
  std::string message_;

public:
  explicit RuntimeError(std::string message);

  const char* what() const override;
};

} // namespace del