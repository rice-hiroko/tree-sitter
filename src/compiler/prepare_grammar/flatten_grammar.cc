#include "compiler/prepare_grammar/flatten_grammar.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include "compiler/prepare_grammar/extract_choices.h"
#include "compiler/prepare_grammar/initial_syntax_grammar.h"
#include "compiler/grammar.h"
#include "compiler/rule.h"

namespace tree_sitter {
namespace prepare_grammar {

using std::find;
using std::pair;
using std::string;
using std::vector;
using rules::Rule;

class FlattenRule {
 private:
  vector<int> precedence_stack;
  vector<rules::Associativity> associativity_stack;
  vector<rules::Alias> alias_stack;
  Production production;

  void apply(const Rule &rule, bool at_end) {
    rule.match(
      [&](const rules::Symbol &symbol) {
        production.steps.push_back(ProductionStep{
          symbol,
          precedence_stack.back(),
          associativity_stack.back(),
          alias_stack.back()
        });
      },

      [&](const rules::Metadata &metadata) {
        if (metadata.params.has_precedence) {
          precedence_stack.push_back(metadata.params.precedence);
        }

        if (metadata.params.has_associativity) {
          associativity_stack.push_back(metadata.params.associativity);
        }

        if (!metadata.params.alias.value.empty()) {
          alias_stack.push_back(metadata.params.alias);
        }

        if (abs(metadata.params.dynamic_precedence) > abs(production.dynamic_precedence)) {
          production.dynamic_precedence = metadata.params.dynamic_precedence;
        }

        apply(*metadata.rule, at_end);

        if (metadata.params.has_precedence) {
          precedence_stack.pop_back();
          if (!at_end) production.back().precedence = precedence_stack.back();
        }

        if (metadata.params.has_associativity) {
          associativity_stack.pop_back();
          if (!at_end) production.back().associativity = associativity_stack.back();
        }

        if (!metadata.params.alias.value.empty()) {
          alias_stack.pop_back();
        }
      },

      [&](const rules::Seq &sequence) {
        apply(*sequence.left, false);
        apply(*sequence.right, at_end);
      },

      [&](const rules::Blank &blank) {},

      [&](auto) {
        assert(!"Unexpected rule type");
      }
    );
  }

 public:
  FlattenRule() :
    precedence_stack({0}),
    associativity_stack({rules::AssociativityNone}),
    alias_stack({rules::Alias{}}) {}

  Production flatten(const Rule &rule) {
    apply(rule, true);
    return production;
  }
};

SyntaxVariable flatten_rule(const Variable &variable) {
  vector<Production> productions;

  for (const Rule &rule_component : extract_choices(variable.rule)) {
    Production production = FlattenRule().flatten(rule_component);
    auto end = productions.end();
    if (find(productions.begin(), end, production) == end) {
      productions.push_back(production);
    }
  }

  return SyntaxVariable{variable.name, variable.type, productions};
}

pair<SyntaxGrammar, CompileError> flatten_grammar(const InitialSyntaxGrammar &grammar) {
  SyntaxGrammar result;
  result.external_tokens = grammar.external_tokens;
  result.variables_to_inline = grammar.variables_to_inline;

  for (const auto &expected_conflict : grammar.expected_conflicts) {
    result.expected_conflicts.insert({
      expected_conflict.begin(),
      expected_conflict.end(),
    });
  }

  for (const rules::Symbol &extra_token : grammar.extra_tokens) {
    result.extra_tokens.insert(extra_token);
  }

  bool is_start = true;
  for (const auto &variable : grammar.variables) {
    SyntaxVariable syntax_variable = flatten_rule(variable);

    if (!is_start) {
      for (const Production &production : syntax_variable.productions) {
        if (production.empty()) {
          return {
            result,
            CompileError(
              TSCompileErrorTypeEpsilonRule,
              "The rule `" + variable.name + "` matches the empty string.\n" +
              "Tree-sitter currently does not support syntactic rules that match the empty string.\n"
            )
          };
        }
      }
    }

    result.variables.push_back(syntax_variable);
    is_start = false;
  }

  return {result, CompileError::none()};
}

}  // namespace prepare_grammar
}  // namespace tree_sitter
