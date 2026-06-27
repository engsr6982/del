#include "del/ast.h"
#include "del/context.h"
#include "del/exception.h"
#include "del/lexer.h"
#include "del/parser.h"
#include "del/symbol_table.h"
#include "del/template_engine.h"
#include "del/version.h"
#include <iostream>

int main(int argc, char** argv) {
  del::TemplateEngine engine;


  std::cout << "\n==================================" << std::endl;

  nlohmann::json source_1 = nlohmann::json::parse(R"(
    {
      "player_permissions": {
        "4A2E-8F1A-99BC": 1,
        "1111-2222-3333": 2
      }
    }
  )");

  nlohmann::ordered_json template_1 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/mPermissions": "map_object(@/player_permissions, (key, val) -> entry(key |> remove_char('-'), val == 1 ? \"admin\" : \"member\") ) "
    }
  )");

  try {
    auto           compiled_t1 = engine.Compile(template_1);
    nlohmann::json result_1    = engine.Execute(compiled_t1, source_1);
    std::cout << result_1.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;

  nlohmann::json source_2 = nlohmann::json::parse(R"(
    {
      "legacy_meta": {
        "uuid": "4A2E-8F1A-99BC",
        "display": "  My Land-- ",
        "status": 1
      },
      "member_list": ["aaa-111", "bbb-222"]
    }
  )");

  nlohmann::ordered_json template_2 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/land_info/owner_id": "@/legacy_meta/uuid |> remove_char('-') |> to_lower() ",
      "/land_info/clean_name": "@/legacy_meta/display |> trim() |> remove_suffix('-') ",
      "/land_info/hold_type": "@/legacy_meta/status == 1 ? \"Bought\" : \"Leasing\"",
      "/flags/is_valid": "is_null($/land_info/owner_id) ? false : true",
      "/flags/need_repair": "$/flags/is_valid == false || $/land_info/clean_name == \"\" ? true : false",
      "/members": "map(@/member_list, user -> user |> remove_char('-') |> to_lower()) ",
      "/fallback_test": "@/non_existent_field ?? \"default_value\""
    }
  )");

  try {
    auto           compiled_t2 = engine.Compile(template_2);
    nlohmann::json result_2    = engine.Execute(compiled_t2, source_2);
    std::cout << result_2.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;

  nlohmann::ordered_json error_template = nlohmann::ordered_json::parse(R"(
    {
      "/error_case": "true == \"not_boolean\""
    }
  )");

  try {
    auto compiled_err = engine.Compile(error_template);
    engine.Execute(compiled_err, source_2);
  } catch (const std::exception& e) {
    std::cout << "Expected exception caught successfully:\n" << e.what() << std::endl;
  }

  return 0;
}