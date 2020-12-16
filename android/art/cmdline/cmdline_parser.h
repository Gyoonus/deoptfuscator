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

#ifndef ART_CMDLINE_CMDLINE_PARSER_H_
#define ART_CMDLINE_CMDLINE_PARSER_H_

#define CMDLINE_NDEBUG 1  // Do not output any debugging information for parsing.

#include "detail/cmdline_debug_detail.h"
#include "detail/cmdline_parse_argument_detail.h"
#include "detail/cmdline_parser_detail.h"

#include "cmdline_parse_result.h"
#include "cmdline_result.h"
#include "cmdline_type_parser.h"
#include "cmdline_types.h"
#include "token_range.h"

#include "base/variant_map.h"

#include <memory>
#include <vector>

namespace art {
// Build a parser for command line arguments with a small domain specific language.
// Each parsed type must have a specialized CmdlineType<T> in order to do the string->T parsing.
// Each argument must also have a VariantMap::Key<T> in order to do the T storage.
template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
struct CmdlineParser {
  template <typename TArg>
  struct ArgumentBuilder;

  struct Builder;  // Build the parser.
  struct UntypedArgumentBuilder;  // Build arguments which weren't yet given a type.

 private:
  // Forward declare some functions that we need to use before fully-defining structs.
  template <typename TArg>
  static ArgumentBuilder<TArg> CreateArgumentBuilder(Builder& parent);
  static void AppendCompletedArgument(Builder& builder, detail::CmdlineParseArgumentAny* arg);

  // Allow argument definitions to save their values when they are parsed,
  // without having a dependency on CmdlineParser or any of the builders.
  //
  // A shared pointer to the save destination is saved into the load/save argument callbacks.
  //
  // This also allows the underlying storage (i.e. a variant map) to be released
  // to the user, without having to recreate all of the callbacks.
  struct SaveDestination {
    SaveDestination() : variant_map_(new TVariantMap()) {}

    // Save value to the variant map.
    template <typename TArg>
    void SaveToMap(const TVariantMapKey<TArg>& key, TArg& value) {
      variant_map_->Set(key, value);
    }

    // Get the existing value from a map, creating the value if it did not already exist.
    template <typename TArg>
    TArg& GetOrCreateFromMap(const TVariantMapKey<TArg>& key) {
      auto* ptr = variant_map_->Get(key);
      if (ptr == nullptr) {
        variant_map_->Set(key, TArg());
        ptr = variant_map_->Get(key);
        assert(ptr != nullptr);
      }

      return *ptr;
    }

   protected:
    // Release the map, clearing it as a side-effect.
    // Future saves will be distinct from previous saves.
    TVariantMap&& ReleaseMap() {
      return std::move(*variant_map_);
    }

    // Get a read-only reference to the variant map.
    const TVariantMap& GetMap() {
      return *variant_map_;
    }

    // Clear all potential save targets.
    void Clear() {
      variant_map_->Clear();
    }

   private:
    // Don't try to copy or move this. Just don't.
    SaveDestination(const SaveDestination&) = delete;
    SaveDestination(SaveDestination&&) = delete;
    SaveDestination& operator=(const SaveDestination&) = delete;
    SaveDestination& operator=(SaveDestination&&) = delete;

    std::shared_ptr<TVariantMap> variant_map_;

    // Allow the parser to change the underlying pointers when we release the underlying storage.
    friend struct CmdlineParser;
  };

 public:
  // Builder for the argument definition of type TArg. Do not use this type directly,
  // it is only a separate type to provide compile-time enforcement against doing
  // illegal builds.
  template <typename TArg>
  struct ArgumentBuilder {
    // Add a range check to this argument.
    ArgumentBuilder<TArg>& WithRange(const TArg& min, const TArg& max) {
      argument_info_.has_range_ = true;
      argument_info_.min_ = min;
      argument_info_.max_ = max;

      return *this;
    }

    // Map the list of names into the list of values. List of names must not have
    // any wildcards '_' in it.
    //
    // Do not use if a value map has already been set.
    ArgumentBuilder<TArg>& WithValues(std::initializer_list<TArg> value_list) {
      SetValuesInternal(value_list);
      return *this;
    }

    // When used with a single alias, map the alias into this value.
    // Same as 'WithValues({value})' , but allows the omission of the curly braces {}.
    ArgumentBuilder<TArg> WithValue(const TArg& value) {
      return WithValues({ value });
    }

    // Map the parsed string values (from _) onto a concrete value. If no wildcard
    // has been specified, then map the value directly from the arg name (i.e.
    // if there are multiple aliases, then use the alias to do the mapping).
    //
    // Do not use if a values list has already been set.
    ArgumentBuilder<TArg>& WithValueMap(
        std::initializer_list<std::pair<const char*, TArg>> key_value_list) {
      assert(!argument_info_.has_value_list_);

      argument_info_.has_value_map_ = true;
      argument_info_.value_map_ = key_value_list;

      return *this;
    }

    // If this argument is seen multiple times, successive arguments mutate the same value
    // instead of replacing it with a new value.
    ArgumentBuilder<TArg>& AppendValues() {
      argument_info_.appending_values_ = true;

      return *this;
    }

    // Convenience type alias for the variant map key type definition.
    using MapKey = TVariantMapKey<TArg>;

    // Write the results of this argument into the key.
    // To look up the parsed arguments, get the map and then use this key with VariantMap::Get
    CmdlineParser::Builder& IntoKey(const MapKey& key) {
      // Only capture save destination as a pointer.
      // This allows the parser to later on change the specific save targets.
      auto save_destination = save_destination_;
      save_value_ = [save_destination, &key](TArg& value) {
        save_destination->SaveToMap(key, value);
        CMDLINE_DEBUG_LOG << "Saved value into map '"
            << detail::ToStringAny(value) << "'" << std::endl;
      };

      load_value_ = [save_destination, &key]() -> TArg& {
        TArg& value = save_destination->GetOrCreateFromMap(key);
        CMDLINE_DEBUG_LOG << "Loaded value from map '" << detail::ToStringAny(value) << "'"
            << std::endl;

        return value;
      };

      save_value_specified_ = true;
      load_value_specified_ = true;

      CompleteArgument();
      return parent_;
    }

    // Ensure we always move this when returning a new builder.
    ArgumentBuilder(ArgumentBuilder&&) = default;

   protected:
    // Used by builder to internally ignore arguments by dropping them on the floor after parsing.
    CmdlineParser::Builder& IntoIgnore() {
      save_value_ = [](TArg& value) {
        CMDLINE_DEBUG_LOG << "Ignored value '" << detail::ToStringAny(value) << "'" << std::endl;
      };
      load_value_ = []() -> TArg& {
        assert(false && "Should not be appending values to ignored arguments");
        return *reinterpret_cast<TArg*>(0);  // Blow up.
      };

      save_value_specified_ = true;
      load_value_specified_ = true;

      CompleteArgument();
      return parent_;
    }

    void SetValuesInternal(const std::vector<TArg>&& value_list) {
      assert(!argument_info_.has_value_map_);

      argument_info_.has_value_list_ = true;
      argument_info_.value_list_ = value_list;
    }

    void SetNames(std::vector<const char*>&& names) {
      argument_info_.names_ = names;
    }

    void SetNames(std::initializer_list<const char*> names) {
      argument_info_.names_ = names;
    }

   private:
    // Copying is bad. Move only.
    ArgumentBuilder(const ArgumentBuilder&) = delete;

    // Called by any function that doesn't chain back into this builder.
    // Completes the argument builder and save the information into the main builder.
    void CompleteArgument() {
      assert(save_value_specified_ &&
             "No Into... function called, nowhere to save parsed values to");
      assert(load_value_specified_ &&
             "No Into... function called, nowhere to load parsed values from");

      argument_info_.CompleteArgument();

      // Appending the completed argument is destructive. The object is no longer
      // usable since all the useful information got moved out of it.
      AppendCompletedArgument(parent_,
                              new detail::CmdlineParseArgument<TArg>(
                                  std::move(argument_info_),
                                  std::move(save_value_),
                                  std::move(load_value_)));
    }

    friend struct CmdlineParser;
    friend struct CmdlineParser::Builder;
    friend struct CmdlineParser::UntypedArgumentBuilder;

    ArgumentBuilder(CmdlineParser::Builder& parser,
                    std::shared_ptr<SaveDestination> save_destination)
        : parent_(parser),
          save_value_specified_(false),
          load_value_specified_(false),
          save_destination_(save_destination) {
      save_value_ = [](TArg&) {
        assert(false && "No save value function defined");
      };

      load_value_ = []() -> TArg& {
        assert(false && "No load value function defined");
        return *reinterpret_cast<TArg*>(0);  // Blow up.
      };
    }

    CmdlineParser::Builder& parent_;
    std::function<void(TArg&)> save_value_;
    std::function<TArg&(void)> load_value_;
    bool save_value_specified_;
    bool load_value_specified_;
    detail::CmdlineParserArgumentInfo<TArg> argument_info_;

    std::shared_ptr<SaveDestination> save_destination_;
  };

  struct UntypedArgumentBuilder {
    // Set a type for this argument. The specific subcommand parser is looked up by the type.
    template <typename TArg>
    ArgumentBuilder<TArg> WithType() {
      return CreateTypedBuilder<TArg>();
    }

    // When used with multiple aliases, map the position of the alias to the value position.
    template <typename TArg>
    ArgumentBuilder<TArg> WithValues(std::initializer_list<TArg> values) {
      auto&& a = CreateTypedBuilder<TArg>();
      a.WithValues(values);
      return std::move(a);
    }

    // When used with a single alias, map the alias into this value.
    // Same as 'WithValues({value})' , but allows the omission of the curly braces {}.
    template <typename TArg>
    ArgumentBuilder<TArg> WithValue(const TArg& value) {
      return WithValues({ value });
    }

    // Set the current building argument to target this key.
    // When this command line argument is parsed, it can be fetched with this key.
    Builder& IntoKey(const TVariantMapKey<Unit>& key) {
      return CreateTypedBuilder<Unit>().IntoKey(key);
    }

    // Ensure we always move this when returning a new builder.
    UntypedArgumentBuilder(UntypedArgumentBuilder&&) = default;

   protected:
    void SetNames(std::vector<const char*>&& names) {
      names_ = std::move(names);
    }

    void SetNames(std::initializer_list<const char*> names) {
      names_ = names;
    }

   private:
    // No copying. Move instead.
    UntypedArgumentBuilder(const UntypedArgumentBuilder&) = delete;

    template <typename TArg>
    ArgumentBuilder<TArg> CreateTypedBuilder() {
      auto&& b = CreateArgumentBuilder<TArg>(parent_);
      InitializeTypedBuilder(&b);  // Type-specific initialization
      b.SetNames(std::move(names_));
      return std::move(b);
    }

    template <typename TArg = Unit>
    typename std::enable_if<std::is_same<TArg, Unit>::value>::type
    InitializeTypedBuilder(ArgumentBuilder<TArg>* arg_builder) {
      // Every Unit argument implicitly maps to a runtime value of Unit{}
      std::vector<Unit> values(names_.size(), Unit{});
      arg_builder->SetValuesInternal(std::move(values));
    }

    // No extra work for all other types
    void InitializeTypedBuilder(void*) {}

    template <typename TArg>
    friend struct ArgumentBuilder;
    friend struct Builder;

    explicit UntypedArgumentBuilder(CmdlineParser::Builder& parent) : parent_(parent) {}
    // UntypedArgumentBuilder(UntypedArgumentBuilder&& other) = default;

    CmdlineParser::Builder& parent_;
    std::vector<const char*> names_;
  };

  // Build a new parser given a chain of calls to define arguments.
  struct Builder {
    Builder() : save_destination_(new SaveDestination()) {}

    // Define a single argument. The default type is Unit.
    UntypedArgumentBuilder Define(const char* name) {
      return Define({name});
    }

    // Define a single argument with multiple aliases.
    UntypedArgumentBuilder Define(std::initializer_list<const char*> names) {
      auto&& b = UntypedArgumentBuilder(*this);
      b.SetNames(names);
      return std::move(b);
    }

    // Whether the parser should give up on unrecognized arguments. Not recommended.
    Builder& IgnoreUnrecognized(bool ignore_unrecognized) {
      ignore_unrecognized_ = ignore_unrecognized;
      return *this;
    }

    // Provide a list of arguments to ignore for backwards compatibility.
    Builder& Ignore(std::initializer_list<const char*> ignore_list) {
      for (auto&& ignore_name : ignore_list) {
        std::string ign = ignore_name;

        // Ignored arguments are just like a regular definition which have very
        // liberal parsing requirements (no range checks, no value checks).
        // Unlike regular argument definitions, when a value gets parsed into its
        // stronger type, we just throw it away.

        if (ign.find('_') != std::string::npos) {  // Does the arg-def have a wildcard?
          // pretend this is a string, e.g. -Xjitconfig:<anythinggoeshere>
          auto&& builder = Define(ignore_name).template WithType<std::string>().IntoIgnore();
          assert(&builder == this);
          (void)builder;  // Ignore pointless unused warning, it's used in the assert.
        } else {
          // pretend this is a unit, e.g. -Xjitblocking
          auto&& builder = Define(ignore_name).template WithType<Unit>().IntoIgnore();
          assert(&builder == this);
          (void)builder;  // Ignore pointless unused warning, it's used in the assert.
        }
      }
      ignore_list_ = ignore_list;
      return *this;
    }

    // Finish building the parser; performs sanity checks. Return value is moved, not copied.
    // Do not call this more than once.
    CmdlineParser Build() {
      assert(!built_);
      built_ = true;

      auto&& p = CmdlineParser(ignore_unrecognized_,
                               std::move(ignore_list_),
                               save_destination_,
                               std::move(completed_arguments_));

      return std::move(p);
    }

   protected:
    void AppendCompletedArgument(detail::CmdlineParseArgumentAny* arg) {
      auto smart_ptr = std::unique_ptr<detail::CmdlineParseArgumentAny>(arg);
      completed_arguments_.push_back(std::move(smart_ptr));
    }

   private:
    // No copying now!
    Builder(const Builder& other) = delete;

    template <typename TArg>
    friend struct ArgumentBuilder;
    friend struct UntypedArgumentBuilder;
    friend struct CmdlineParser;

    bool built_ = false;
    bool ignore_unrecognized_ = false;
    std::vector<const char*> ignore_list_;
    std::shared_ptr<SaveDestination> save_destination_;

    std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>> completed_arguments_;
  };

  CmdlineResult Parse(const std::string& argv) {
    std::vector<std::string> tokenized;
    Split(argv, ' ', &tokenized);

    return Parse(TokenRange(std::move(tokenized)));
  }

  // Parse the arguments; storing results into the arguments map. Returns success value.
  CmdlineResult Parse(const char* argv) {
    return Parse(std::string(argv));
  }

  // Parse the arguments; storing the results into the arguments map. Returns success value.
  // Assumes that argv[0] is a valid argument (i.e. not the program name).
  CmdlineResult Parse(const std::vector<const char*>& argv) {
    return Parse(TokenRange(argv.begin(), argv.end()));
  }

  // Parse the arguments; storing the results into the arguments map. Returns success value.
  // Assumes that argv[0] is a valid argument (i.e. not the program name).
  CmdlineResult Parse(const std::vector<std::string>& argv) {
    return Parse(TokenRange(argv.begin(), argv.end()));
  }

  // Parse the arguments (directly from an int main(argv,argc)). Returns success value.
  // Assumes that argv[0] is the program name, and ignores it.
  CmdlineResult Parse(const char* argv[], int argc) {
    return Parse(TokenRange(&argv[1], argc - 1));  // ignore argv[0] because it's the program name
  }

  // Look up the arguments that have been parsed; use the target keys to lookup individual args.
  const TVariantMap& GetArgumentsMap() const {
    return save_destination_->GetMap();
  }

  // Release the arguments map that has been parsed; useful for move semantics.
  TVariantMap&& ReleaseArgumentsMap() {
    return save_destination_->ReleaseMap();
  }

  // How many arguments were defined?
  size_t CountDefinedArguments() const {
    return completed_arguments_.size();
  }

  // Ensure we have a default move constructor.
  CmdlineParser(CmdlineParser&&) = default;
  // Ensure we have a default move assignment operator.
  CmdlineParser& operator=(CmdlineParser&&) = default;

 private:
  friend struct Builder;

  // Construct a new parser from the builder. Move all the arguments.
  CmdlineParser(bool ignore_unrecognized,
                std::vector<const char*>&& ignore_list,
                std::shared_ptr<SaveDestination> save_destination,
                std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>>&& completed_arguments)
    : ignore_unrecognized_(ignore_unrecognized),
      ignore_list_(std::move(ignore_list)),
      save_destination_(save_destination),
      completed_arguments_(std::move(completed_arguments)) {
    assert(save_destination != nullptr);
  }

  // Parse the arguments; storing results into the arguments map. Returns success value.
  // The parsing will fail on the first non-success parse result and return that error.
  //
  // All previously-parsed arguments are cleared out.
  // Otherwise, all parsed arguments will be stored into SaveDestination as a side-effect.
  // A partial parse will result only in a partial save of the arguments.
  CmdlineResult Parse(TokenRange&& arguments_list) {
    save_destination_->Clear();

    for (size_t i = 0; i < arguments_list.Size(); ) {
      TokenRange possible_name = arguments_list.Slice(i);

      size_t best_match_size = 0;  // How many tokens were matched in the best case.
      size_t best_match_arg_idx = 0;
      bool matched = false;  // At least one argument definition has been matched?

      // Find the closest argument definition for the remaining token range.
      size_t arg_idx = 0;
      for (auto&& arg : completed_arguments_) {
        size_t local_match = arg->MaybeMatches(possible_name);

        if (local_match > best_match_size) {
          best_match_size = local_match;
          best_match_arg_idx = arg_idx;
          matched = true;
        }
        arg_idx++;
      }

      // Saw some kind of unknown argument
      if (matched == false) {
        if (UNLIKELY(ignore_unrecognized_)) {  // This is usually off, we only need it for JNI.
          // Consume 1 token and keep going, hopefully the next token is a good one.
          ++i;
          continue;
        }
        // Common case:
        // Bail out on the first unknown argument with an error.
        return CmdlineResult(CmdlineResult::kUnknown,
                             std::string("Unknown argument: ") + possible_name[0]);
      }

      // Look at the best-matched argument definition and try to parse against that.
      auto&& arg = completed_arguments_[best_match_arg_idx];

      assert(arg->MaybeMatches(possible_name) == best_match_size);

      // Try to parse the argument now, if we have enough tokens.
      std::pair<size_t, size_t> num_tokens = arg->GetNumTokens();
      size_t min_tokens;
      size_t max_tokens;

      std::tie(min_tokens, max_tokens) = num_tokens;

      if ((i + min_tokens) > arguments_list.Size()) {
        // expected longer command line but it was too short
        // e.g. if the argv was only "-Xms" without specifying a memory option
        CMDLINE_DEBUG_LOG << "Parse failure, i = " << i << ", arg list " << arguments_list.Size() <<
            " num tokens in arg_def: " << min_tokens << "," << max_tokens << std::endl;
        return CmdlineResult(CmdlineResult::kFailure,
                             std::string("Argument ") +
                             possible_name[0] + ": incomplete command line arguments, expected "
                             + std::to_string(size_t(i + min_tokens) - arguments_list.Size()) +
                             " more tokens");
      }

      if (best_match_size > max_tokens || best_match_size < min_tokens) {
        // Even our best match was out of range, so parsing would fail instantly.
        return CmdlineResult(CmdlineResult::kFailure,
                             std::string("Argument ") + possible_name[0] + ": too few tokens "
                             "matched " + std::to_string(best_match_size)
                             + " but wanted " + std::to_string(num_tokens.first));
      }

      // We have enough tokens to begin exact parsing.
      TokenRange exact_range = possible_name.Slice(0, max_tokens);

      size_t consumed_tokens = 1;  // At least 1 if we ever want to try to resume parsing on error
      CmdlineResult parse_attempt = arg->ParseArgument(exact_range, &consumed_tokens);

      if (parse_attempt.IsError()) {
        // We may also want to continue parsing the other tokens to gather more errors.
        return parse_attempt;
      }  // else the value has been successfully stored into the map

      assert(consumed_tokens > 0);  // Don't hang in an infinite loop trying to parse
      i += consumed_tokens;

      // TODO: also handle ignoring arguments for backwards compatibility
    }  // for

    return CmdlineResult(CmdlineResult::kSuccess);
  }

  bool ignore_unrecognized_ = false;
  std::vector<const char*> ignore_list_;
  std::shared_ptr<SaveDestination> save_destination_;
  std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>> completed_arguments_;
};

// This has to be defined after everything else, since we want the builders to call this.
template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
template <typename TArg>
typename CmdlineParser<TVariantMap, TVariantMapKey>::template ArgumentBuilder<TArg>
CmdlineParser<TVariantMap, TVariantMapKey>::CreateArgumentBuilder(
    CmdlineParser<TVariantMap, TVariantMapKey>::Builder& parent) {
  return CmdlineParser<TVariantMap, TVariantMapKey>::ArgumentBuilder<TArg>(
      parent, parent.save_destination_);
}

// This has to be defined after everything else, since we want the builders to call this.
template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
void CmdlineParser<TVariantMap, TVariantMapKey>::AppendCompletedArgument(
    CmdlineParser<TVariantMap, TVariantMapKey>::Builder& builder,
    detail::CmdlineParseArgumentAny* arg) {
  builder.AppendCompletedArgument(arg);
}

}  // namespace art

#endif  // ART_CMDLINE_CMDLINE_PARSER_H_
