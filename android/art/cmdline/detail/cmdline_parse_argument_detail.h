/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_CMDLINE_DETAIL_CMDLINE_PARSE_ARGUMENT_DETAIL_H_
#define ART_CMDLINE_DETAIL_CMDLINE_PARSE_ARGUMENT_DETAIL_H_

#include <assert.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <type_traits>
#include <vector>

#include "android-base/strings.h"

#include "cmdline_parse_result.h"
#include "cmdline_types.h"
#include "token_range.h"
#include "unit.h"

namespace art {
// Implementation details for the parser. Do not look inside if you hate templates.
namespace detail {
// A non-templated base class for argument parsers. Used by the general parser
// to parse arguments, without needing to know the argument type at compile time.
//
// This is an application of the type erasure idiom.
struct CmdlineParseArgumentAny {
  virtual ~CmdlineParseArgumentAny() {}

  // Attempt to parse this argument starting at arguments[position].
  // If the parsing succeeds, the parsed value will be saved as a side-effect.
  //
  // In most situations, the parsing will not match by returning kUnknown. In this case,
  // no tokens were consumed and the position variable will not be updated.
  //
  // At other times, parsing may fail due to validation but the initial token was still matched
  // (for example an out of range value, or passing in a string where an int was expected).
  // In this case the tokens are still consumed, and the position variable will get incremented
  // by all the consumed tokens.
  //
  // The # of tokens consumed by the parse attempt will be set as an out-parameter into
  // consumed_tokens. The parser should skip this many tokens before parsing the next
  // argument.
  virtual CmdlineResult ParseArgument(const TokenRange& arguments, size_t* consumed_tokens) = 0;
  // How many tokens should be taken off argv for parsing this argument.
  // For example "--help" is just 1, "-compiler-option _" would be 2 (since there's a space).
  //
  // A [min,max] range is returned to represent argument definitions with multiple
  // value tokens. (e.g. {"-h", "-h " } would return [1,2]).
  virtual std::pair<size_t, size_t> GetNumTokens() const = 0;
  // Get the run-time typename of the argument type.
  virtual const char* GetTypeName() const = 0;
  // Try to do a close match, returning how many tokens were matched against this argument
  // definition. More tokens is better.
  //
  // Do a quick match token-by-token, and see if they match.
  // Any tokens with a wildcard in them are only matched up until the wildcard.
  // If this is true, then the wildcard matching later on can still fail, so this is not
  // a guarantee that the argument is correct, it's more of a strong hint that the
  // user-provided input *probably* was trying to match this argument.
  //
  // Returns how many tokens were either matched (or ignored because there was a
  // wildcard present). 0 means no match. If the Size() tokens are returned.
  virtual size_t MaybeMatches(const TokenRange& tokens) = 0;
};

template <typename T>
using EnableIfNumeric = std::enable_if<std::is_arithmetic<T>::value>;

template <typename T>
using DisableIfNumeric = std::enable_if<!std::is_arithmetic<T>::value>;

// Argument definition information, created by an ArgumentBuilder and an UntypedArgumentBuilder.
template <typename TArg>
struct CmdlineParserArgumentInfo {
  // This version will only be used if TArg is arithmetic and thus has the <= operators.
  template <typename T = TArg>  // Necessary to get SFINAE to kick in.
  bool CheckRange(const TArg& value, typename EnableIfNumeric<T>::type* = 0) {
    if (has_range_) {
      return min_ <= value && value <= max_;
    }
    return true;
  }

  // This version will be used at other times when TArg is not arithmetic.
  template <typename T = TArg>
  bool CheckRange(const TArg&, typename DisableIfNumeric<T>::type* = 0) {
    assert(!has_range_);
    return true;
  }

  // Do a quick match token-by-token, and see if they match.
  // Any tokens with a wildcard in them only match the prefix up until the wildcard.
  //
  // If this is true, then the wildcard matching later on can still fail, so this is not
  // a guarantee that the argument is correct, it's more of a strong hint that the
  // user-provided input *probably* was trying to match this argument.
  size_t MaybeMatches(const TokenRange& token_list) const {
    auto best_match = FindClosestMatch(token_list);

    return best_match.second;
  }

  // Attempt to find the closest match (see MaybeMatches).
  //
  // Returns the token range that was the closest match and the # of tokens that
  // this range was matched up until.
  std::pair<const TokenRange*, size_t> FindClosestMatch(const TokenRange& token_list) const {
    const TokenRange* best_match_ptr = nullptr;

    size_t best_match = 0;
    for (auto&& token_range : tokenized_names_) {
      size_t this_match = token_range.MaybeMatches(token_list, std::string("_"));

      if (this_match > best_match) {
        best_match_ptr = &token_range;
        best_match = this_match;
      }
    }

    return std::make_pair(best_match_ptr, best_match);
  }

  // Mark the argument definition as completed, do not mutate the object anymore after this
  // call is done.
  //
  // Performs several sanity checks and token calculations.
  void CompleteArgument() {
    assert(names_.size() >= 1);
    assert(!is_completed_);

    is_completed_ = true;

    size_t blank_count = 0;
    size_t token_count = 0;

    size_t global_blank_count = 0;
    size_t global_token_count = 0;
    for (auto&& name : names_) {
      std::string s(name);

      size_t local_blank_count = std::count(s.begin(), s.end(), '_');
      size_t local_token_count = std::count(s.begin(), s.end(), ' ');

      if (global_blank_count != 0) {
        assert(local_blank_count == global_blank_count
               && "Every argument descriptor string must have same amount of blanks (_)");
      }

      if (local_blank_count != 0) {
        global_blank_count = local_blank_count;
        blank_count++;

        assert(local_blank_count == 1 && "More than one blank is not supported");
        assert(s.back() == '_' && "The blank character must only be at the end of the string");
      }

      if (global_token_count != 0) {
        assert(local_token_count == global_token_count
               && "Every argument descriptor string must have same amount of tokens (spaces)");
      }

      if (local_token_count != 0) {
        global_token_count = local_token_count;
        token_count++;
      }

      // Tokenize every name, turning it from a string to a token list.
      tokenized_names_.clear();
      for (auto&& name1 : names_) {
        // Split along ' ' only, removing any duplicated spaces.
        tokenized_names_.push_back(
            TokenRange::Split(name1, {' '}).RemoveToken(" "));
      }

      // remove the _ character from each of the token ranges
      // we will often end up with an empty token (i.e. ["-XX", "_"] -> ["-XX", ""]
      // and this is OK because we still need an empty token to simplify
      // range comparisons
      simple_names_.clear();

      for (auto&& tokenized_name : tokenized_names_) {
        simple_names_.push_back(tokenized_name.RemoveCharacter('_'));
      }
    }

    if (token_count != 0) {
      assert(("Every argument descriptor string must have equal amount of tokens (spaces)" &&
          token_count == names_.size()));
    }

    if (blank_count != 0) {
      assert(("Every argument descriptor string must have an equal amount of blanks (_)" &&
          blank_count == names_.size()));
    }

    using_blanks_ = blank_count > 0;
    {
      size_t smallest_name_token_range_size =
          std::accumulate(tokenized_names_.begin(), tokenized_names_.end(), ~(0u),
                          [](size_t min, const TokenRange& cur) {
                            return std::min(min, cur.Size());
                          });
      size_t largest_name_token_range_size =
          std::accumulate(tokenized_names_.begin(), tokenized_names_.end(), 0u,
                          [](size_t max, const TokenRange& cur) {
                            return std::max(max, cur.Size());
                          });

      token_range_size_ = std::make_pair(smallest_name_token_range_size,
                                         largest_name_token_range_size);
    }

    if (has_value_list_) {
      assert(names_.size() == value_list_.size()
             && "Number of arg descriptors must match number of values");
      assert(!has_value_map_);
    }
    if (has_value_map_) {
      if (!using_blanks_) {
        assert(names_.size() == value_map_.size() &&
               "Since no blanks were specified, each arg is mapped directly into a mapped "
               "value without parsing; sizes must match");
      }

      assert(!has_value_list_);
    }

    if (!using_blanks_ && !CmdlineType<TArg>::kCanParseBlankless) {
      assert((has_value_map_ || has_value_list_) &&
             "Arguments without a blank (_) must provide either a value map or a value list");
    }

    TypedCheck();
  }

  // List of aliases for a single argument definition, e.g. {"-Xdex2oat", "-Xnodex2oat"}.
  std::vector<const char*> names_;
  // Is there at least 1 wildcard '_' in the argument definition?
  bool using_blanks_ = false;
  // [min, max] token counts in each arg def
  std::pair<size_t, size_t> token_range_size_;

  // contains all the names in a tokenized form, i.e. as a space-delimited list
  std::vector<TokenRange> tokenized_names_;

  // contains the tokenized names, but with the _ character stripped
  std::vector<TokenRange> simple_names_;

  // For argument definitions created with '.AppendValues()'
  // Meaning that parsing should mutate the existing value in-place if possible.
  bool appending_values_ = false;

  // For argument definitions created with '.WithRange(min, max)'
  bool has_range_ = false;
  TArg min_;
  TArg max_;

  // For argument definitions created with '.WithValueMap'
  bool has_value_map_ = false;
  std::vector<std::pair<const char*, TArg>> value_map_;

  // For argument definitions created with '.WithValues'
  bool has_value_list_ = false;
  std::vector<TArg> value_list_;

  // Make sure there's a default constructor.
  CmdlineParserArgumentInfo() = default;

  // Ensure there's a default move constructor.
  CmdlineParserArgumentInfo(CmdlineParserArgumentInfo&&) = default;

 private:
  // Perform type-specific checks at runtime.
  template <typename T = TArg>
  void TypedCheck(typename std::enable_if<std::is_same<Unit, T>::value>::type* = 0) {
    assert(!using_blanks_ &&
           "Blanks are not supported in Unit arguments; since a Unit has no parse-able value");
  }

  void TypedCheck() {}

  bool is_completed_ = false;
};

// A virtual-implementation of the necessary argument information in order to
// be able to parse arguments.
template <typename TArg>
struct CmdlineParseArgument : CmdlineParseArgumentAny {
  CmdlineParseArgument(CmdlineParserArgumentInfo<TArg>&& argument_info,
                       std::function<void(TArg&)>&& save_argument,
                       std::function<TArg&(void)>&& load_argument)
      : argument_info_(std::forward<decltype(argument_info)>(argument_info)),
        save_argument_(std::forward<decltype(save_argument)>(save_argument)),
        load_argument_(std::forward<decltype(load_argument)>(load_argument)) {
  }

  using UserTypeInfo = CmdlineType<TArg>;

  virtual CmdlineResult ParseArgument(const TokenRange& arguments, size_t* consumed_tokens) {
    assert(arguments.Size() > 0);
    assert(consumed_tokens != nullptr);

    auto closest_match_res = argument_info_.FindClosestMatch(arguments);
    size_t best_match_size = closest_match_res.second;
    const TokenRange* best_match_arg_def = closest_match_res.first;

    if (best_match_size > arguments.Size()) {
      // The best match has more tokens than were provided.
      // Shouldn't happen in practice since the outer parser does this check.
      return CmdlineResult(CmdlineResult::kUnknown, "Size mismatch");
    }

    assert(best_match_arg_def != nullptr);
    *consumed_tokens = best_match_arg_def->Size();

    if (!argument_info_.using_blanks_) {
      return ParseArgumentSingle(arguments.Join(' '));
    }

    // Extract out the blank value from arguments
    // e.g. for a def of "foo:_" and input "foo:bar", blank_value == "bar"
    std::string blank_value = "";
    size_t idx = 0;
    for (auto&& def_token : *best_match_arg_def) {
      auto&& arg_token = arguments[idx];

      // Does this definition-token have a wildcard in it?
      if (def_token.find('_') == std::string::npos) {
        // No, regular token. Match 1:1 against the argument token.
        bool token_match = def_token == arg_token;

        if (!token_match) {
          return CmdlineResult(CmdlineResult::kFailure,
                               std::string("Failed to parse ") + best_match_arg_def->GetToken(0)
                               + " at token " + std::to_string(idx));
        }
      } else {
        // This is a wild-carded token.
        TokenRange def_split_wildcards = TokenRange::Split(def_token, {'_'});

        // Extract the wildcard contents out of the user-provided arg_token.
        std::unique_ptr<TokenRange> arg_matches =
            def_split_wildcards.MatchSubstrings(arg_token, "_");
        if (arg_matches == nullptr) {
          return CmdlineResult(CmdlineResult::kFailure,
                               std::string("Failed to parse ") + best_match_arg_def->GetToken(0)
                               + ", with a wildcard pattern " + def_token
                               + " at token " + std::to_string(idx));
        }

        // Get the corresponding wildcard tokens from arg_matches,
        // and concatenate it to blank_value.
        for (size_t sub_idx = 0;
            sub_idx < def_split_wildcards.Size() && sub_idx < arg_matches->Size(); ++sub_idx) {
          if (def_split_wildcards[sub_idx] == "_") {
            blank_value += arg_matches->GetToken(sub_idx);
          }
        }
      }

      ++idx;
    }

    return ParseArgumentSingle(blank_value);
  }

 private:
  virtual CmdlineResult ParseArgumentSingle(const std::string& argument) {
    // TODO: refactor to use LookupValue for the value lists/maps

    // Handle the 'WithValueMap(...)' argument definition
    if (argument_info_.has_value_map_) {
      for (auto&& value_pair : argument_info_.value_map_) {
        const char* name = value_pair.first;

        if (argument == name) {
          return SaveArgument(value_pair.second);
        }
      }

      // Error case: Fail, telling the user what the allowed values were.
      std::vector<std::string> allowed_values;
      for (auto&& value_pair : argument_info_.value_map_) {
        const char* name = value_pair.first;
        allowed_values.push_back(name);
      }

      std::string allowed_values_flat = android::base::Join(allowed_values, ',');
      return CmdlineResult(CmdlineResult::kFailure,
                           "Argument value '" + argument + "' does not match any of known valid"
                            "values: {" + allowed_values_flat + "}");
    }

    // Handle the 'WithValues(...)' argument definition
    if (argument_info_.has_value_list_) {
      size_t arg_def_idx = 0;
      for (auto&& value : argument_info_.value_list_) {
        auto&& arg_def_token = argument_info_.names_[arg_def_idx];

        if (arg_def_token == argument) {
          return SaveArgument(value);
        }
        ++arg_def_idx;
      }

      assert(arg_def_idx + 1 == argument_info_.value_list_.size() &&
             "Number of named argument definitions must match number of values defined");

      // Error case: Fail, telling the user what the allowed values were.
      std::vector<std::string> allowed_values;
      for (auto&& arg_name : argument_info_.names_) {
        allowed_values.push_back(arg_name);
      }

      std::string allowed_values_flat = android::base::Join(allowed_values, ',');
      return CmdlineResult(CmdlineResult::kFailure,
                           "Argument value '" + argument + "' does not match any of known valid"
                            "values: {" + allowed_values_flat + "}");
    }

    // Handle the regular case where we parsed an unknown value from a blank.
    UserTypeInfo type_parser;

    if (argument_info_.appending_values_) {
      TArg& existing = load_argument_();
      CmdlineParseResult<TArg> result = type_parser.ParseAndAppend(argument, existing);

      assert(!argument_info_.has_range_);

      return result;
    }

    CmdlineParseResult<TArg> result = type_parser.Parse(argument);

    if (result.IsSuccess()) {
      TArg& value = result.GetValue();

      // Do a range check for 'WithRange(min,max)' argument definition.
      if (!argument_info_.CheckRange(value)) {
        return CmdlineParseResult<TArg>::OutOfRange(
            value, argument_info_.min_, argument_info_.max_);
      }

      return SaveArgument(value);
    }

    // Some kind of type-specific parse error. Pass the result as-is.
    CmdlineResult raw_result = std::move(result);
    return raw_result;
  }

 public:
  virtual const char* GetTypeName() const {
    // TODO: Obviate the need for each type specialization to hardcode the type name
    return UserTypeInfo::Name();
  }

  // How many tokens should be taken off argv for parsing this argument.
  // For example "--help" is just 1, "-compiler-option _" would be 2 (since there's a space).
  //
  // A [min,max] range is returned to represent argument definitions with multiple
  // value tokens. (e.g. {"-h", "-h " } would return [1,2]).
  virtual std::pair<size_t, size_t> GetNumTokens() const {
    return argument_info_.token_range_size_;
  }

  // See if this token range might begin the same as the argument definition.
  virtual size_t MaybeMatches(const TokenRange& tokens) {
    return argument_info_.MaybeMatches(tokens);
  }

 private:
  CmdlineResult SaveArgument(const TArg& value) {
    assert(!argument_info_.appending_values_
           && "If the values are being appended, then the updated parse value is "
               "updated by-ref as a side effect and shouldn't be stored directly");
    TArg val = value;
    save_argument_(val);
    return CmdlineResult(CmdlineResult::kSuccess);
  }

  CmdlineParserArgumentInfo<TArg> argument_info_;
  std::function<void(TArg&)> save_argument_;
  std::function<TArg&(void)> load_argument_;
};
}  // namespace detail  // NOLINT [readability/namespace] [5]
}  // namespace art

#endif  // ART_CMDLINE_DETAIL_CMDLINE_PARSE_ARGUMENT_DETAIL_H_
