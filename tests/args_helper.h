#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class ArgsParser {
public:
  ArgsParser(int argc, char** argv) {
    // 跳过 argv[0] (程序名)
    for (int i = 1; i < argc; ++i) {
      std::string_view arg(argv[i]);

      if (arg.starts_with("--")) {
        // 处理形如 --key=value 的情况
        if (auto pos = arg.find('='); pos != std::string_view::npos) {
          std::string key(arg.substr(0, pos));
          std::string value(arg.substr(pos + 1));
          options_[key] = std::move(value);
        }
        // 处理标准的 --key value 情况
        else if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("-")) {
          options_[std::string(arg)] = argv[++i];
        }
        // 纯 Flag (如 --help, --repl)
        else {
          flags_.insert(std::string(arg));
        }
      } else if (arg.starts_with("-")) {
        // 兼容短参数
        if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("-")) {
          options_[std::string(arg)] = argv[++i];
        } else {
          flags_.insert(std::string(arg));
        }
      } else {
        // 位置参数
        positionals_.emplace_back(arg);
      }
    }
  }

  bool has_flag(std::string_view flag) const { return flags_.contains(std::string(flag)); }

  // 获取某个带值的选项，如果不存在则返回 std::nullopt
  std::optional<std::string> get_option(std::string_view option) const {
    if (auto it = options_.find(std::string(option)); it != options_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  // 获取位置参数列表
  const std::vector<std::string>& positionals() const { return positionals_; }

private:
  std::unordered_set<std::string>              flags_;
  std::unordered_map<std::string, std::string> options_;
  std::vector<std::string>                     positionals_;
};