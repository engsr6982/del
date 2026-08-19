#pragma once
#include "macro.h"
#include "symbol_table.h"

#include <nlohmann/json.hpp>

namespace del {

class ASTNode;

// A single compiled template instruction, produced from one template entry
// (key = target path, value = expression).
//
// Created by CompileExpr / CompileExprStr. Holds one of two ways to produce a
// value, chosen at compile time:
//   - compiled_ast != nullptr: the entry value was a DEL expression string;
//     at execution the AST is evaluated to compute the result.
//   - compiled_ast == nullptr: the entry value was a plain JSON literal; it is
//     stored verbatim in direct_value and returned as-is at execution time
//     (no computation).
struct CompiledPathExpr {
  std::string
      target_pointer_path; // mount path for this instruction (template entry key, JSON Pointer, e.g. "/foo/bar")
  nlohmann::json           direct_value; // non-expression values are stored directly here
  std::unique_ptr<ASTNode> compiled_ast; // compiled AST tree for expression values
};

// Compilation result for a whole template: every instruction, in template
// insertion order.
class CompiledTemplate {
public:
  std::vector<CompiledPathExpr> instructions;

  CompiledTemplate()                              = default;
  CompiledTemplate(CompiledTemplate&&)            = default;
  CompiledTemplate& operator=(CompiledTemplate&&) = default;

  DEL_DISABLE_COPY(CompiledTemplate);
};

// Template engine: compiles a JSON template into a CompiledTemplate, then
// executes it against a source JSON to produce the target JSON.
//
// Typical usage:
//   1. Construct a TemplateEngine (the constructor registers all built-in
//      functions into the symbol table);
//   2. Optionally call RegisterCustomFunction to add user-defined functions;
//   3. Compile(template_json) to compile the whole template;
//   4. Execute(ct, source_json) for a one-shot result, or CreateContext +
//      Execute(ct, ctx) for incremental execution (e.g. REPL).
class TemplateEngine {
public:
  // Constructor: registers all built-in functions into the symbol table via
  // BultinFunction::RegisterAll.
  TemplateEngine();

  // Registers a user-defined function under the given name.
  // Takes ownership of (moves) both name and fn; afterwards the function can be
  // called from expressions just like a built-in.
  void RegisterCustomFunction(std::string name, CustomFunction fn);

  // Compiles a raw DEL expression string into an instruction.
  // Pipeline: Lexer (tokenize) -> Parser (Pratt) parses the top-level
  // expression into an AST, stored in instr.compiled_ast; path_key is stored in
  // instr.target_pointer_path. At execution the AST is evaluated and the result
  // is mounted to the target at target_pointer_path.
  [[nodiscard]] CompiledPathExpr CompileExprStr(std::string_view expr, std::string_view path_key = "");

  // Compiles a template entry "value" into an instruction (dispatches between
  // the expression and literal paths):
  //   - expr is a string      -> treated as a DEL expression, delegates to
  //                              CompileExprStr (compiled into an AST);
  //   - expr is any other JSON-> treated as a literal, stored verbatim in
  //                              direct_value (no computation).
  [[nodiscard]] CompiledPathExpr CompileExpr(nlohmann::json const& expr, std::string_view path_key = "");

  // Compiles a whole template: iterates the ordered_json entries in insertion
  // order, uses each key as the target mount path, compiles the value via
  // CompileExpr into a single instruction, and appends it to instructions.
  [[nodiscard]] CompiledTemplate Compile(nlohmann::ordered_json const& template_json);

  // Creates an evaluation context over the source JSON and a target JSON
  // reference. The target is owned by the caller; Execute(ct, ctx) mounts
  // results directly onto that object, which supports incrementally building
  // the target across multiple executions (e.g. REPL).
  [[nodiscard]] EvaluationContext CreateContext(nlohmann::json const& source_json, nlohmann::json& target);

  // Convenience entry point: internally creates a fresh empty target JSON
  // object and context, executes all instructions, and returns the target.
  // Equivalent to CreateContext + Execute(ct, ctx); fits one-shot batch runs.
  [[nodiscard]] nlohmann::json Execute(CompiledTemplate const& ct, nlohmann::json const& source_json);

  // Executes every instruction in order: evaluates it, and if its
  // target_pointer_path is non-empty, mounts the result at that path in
  // ctx.target (so later instructions can read it back via $/ pointers).
  void Execute(CompiledTemplate const& ct, EvaluationContext& ctx);

  // Executes a single instruction and returns the result without touching the
  // target: evaluates the AST if compiled_ast is set, otherwise returns
  // direct_value (literal passthrough).
  // execute expr only, return the result (not modify the target)
  [[nodiscard]] nlohmann::json Execute(CompiledPathExpr const& instr, EvaluationContext& ctx);

  // Static utility: mounts value at path_str (JSON Pointer) within target.
  // Backed by nlohmann::json's operator[]; missing parent paths are built
  // automatically as nested objects. Throws std::runtime_error (with the path
  // in the message) if the path is invalid or mounting fails.
  static void MountValue(nlohmann::json& target, std::string const& path_str, nlohmann::json value);

private:
  SymbolTable symbol_table_;
};


} // namespace del
