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

#ifndef ART_CMDLINE_TOKEN_RANGE_H_
#define ART_CMDLINE_TOKEN_RANGE_H_

#include <assert.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "android-base/strings.h"

namespace art {
// A range of tokens to make token matching algorithms easier.
//
// We try really hard to avoid copying and store only a pointer and iterators to the
// interiors of the vector, so a typical copy constructor never ends up doing a deep copy.
// It is up to the user to play nice and not to mutate the strings in-place.
//
// Tokens are only copied if a mutating operation is performed (and even then only
// if it *actually* mutates the token).
struct TokenRange {
  // Short-hand for a vector of strings. A single string and a token is synonymous.
  using TokenList = std::vector<std::string>;

  // Copying-from-vector constructor.
  explicit TokenRange(const TokenList& token_list)
    : token_list_(new TokenList(token_list)),
      begin_(token_list_->begin()),
      end_(token_list_->end())
  {}

  // Copying-from-iterator constructor
  template <typename ForwardIterator>
  TokenRange(ForwardIterator it_begin, ForwardIterator it_end)
    : token_list_(new TokenList(it_begin, it_end)),
      begin_(token_list_->begin()),
      end_(token_list_->end())
  {}

#if 0
  // Copying-from-vector constructor.
  TokenRange(const TokenList& token_list ATTRIBUTE_UNUSED,
             TokenList::const_iterator it_begin,
             TokenList::const_iterator it_end)
    : token_list_(new TokenList(it_begin, it_end)),
      begin_(token_list_->begin()),
      end_(token_list_->end()) {
    assert(it_begin >= token_list.begin());
    assert(it_end <= token_list.end());
  }
#endif

  // Copying from char array constructor, convertings into tokens (strings) along the way.
  TokenRange(const char* token_list[], size_t length)
    : token_list_(new TokenList(&token_list[0], &token_list[length])),
      begin_(token_list_->begin()),
      end_(token_list_->end())
  {}

  // Non-copying move-from-vector constructor. Takes over the token vector.
  explicit TokenRange(TokenList&& token_list)
    : token_list_(new TokenList(std::forward<TokenList>(token_list))),
      begin_(token_list_->begin()),
      end_(token_list_->end())
  {}

  // Non-copying constructor. Retain reference to existing list of tokens.
  TokenRange(std::shared_ptr<TokenList> token_list,
             TokenList::const_iterator it_begin,
             TokenList::const_iterator it_end)
    : token_list_(token_list),
      begin_(it_begin),
      end_(it_end) {
    assert(it_begin >= token_list->begin());
    assert(it_end <= token_list->end());
  }

  // Non-copying copy constructor.
  TokenRange(const TokenRange&) = default;

  // Non-copying move constructor.
  TokenRange(TokenRange&&) = default;

  // Non-copying constructor. Retains reference to an existing list of tokens, with offset.
  explicit TokenRange(std::shared_ptr<TokenList> token_list)
    : token_list_(token_list),
      begin_(token_list_->begin()),
      end_(token_list_->end())
  {}

  // Iterator type for begin() and end(). Guaranteed to be a RandomAccessIterator.
  using iterator = TokenList::const_iterator;

  // Iterator type for const begin() and const end(). Guaranteed to be a RandomAccessIterator.
  using const_iterator = iterator;

  // Create a token range by splitting a string. Each separator gets their own token.
  // Since the separator are retained as tokens, it might be useful to call
  // RemoveToken afterwards.
  static TokenRange Split(const std::string& string, std::initializer_list<char> separators) {
    TokenList new_token_list;

    std::string tok;
    for (auto&& c : string) {
      for (char sep : separators) {
        if (c == sep) {
          // We spotted a separator character.
          // Push back everything before the last separator as a new token.
          // Push back the separator as a token.
          if (!tok.empty()) {
            new_token_list.push_back(tok);
            tok = "";
          }
          new_token_list.push_back(std::string() + sep);
        } else {
          // Build up the token with another character.
          tok += c;
        }
      }
    }

    if (!tok.empty()) {
      new_token_list.push_back(tok);
    }

    return TokenRange(std::move(new_token_list));
  }

  // A RandomAccessIterator to the first element in this range.
  iterator begin() const {
    return begin_;
  }

  // A RandomAccessIterator to one past the last element in this range.
  iterator end() const {
    return end_;
  }

  // The size of the range, i.e. how many tokens are in it.
  size_t Size() const {
    return std::distance(begin_, end_);
  }

  // Are there 0 tokens in this range?
  bool IsEmpty() const {
    return Size() > 0;
  }

  // Look up a token by it's offset.
  const std::string& GetToken(size_t offset) const {
    assert(offset < Size());
    return *(begin_ + offset);
  }

  // Does this token range equal the other range?
  // Equality is defined as having both the same size, and
  // each corresponding token being equal.
  bool operator==(const TokenRange& other) const {
    if (this == &other) {
      return true;
    }

    if (Size() != other.Size()) {
      return false;
    }

    return std::equal(begin(), end(), other.begin());
  }

  // Look up the token at the requested index.
  const std::string& operator[](int index) const {
    assert(index >= 0 && static_cast<size_t>(index) < Size());
    return *(begin() + index);
  }

  // Does this current range start with the other range?
  bool StartsWith(const TokenRange& other) const {
    if (this == &other) {
      return true;
    }

    if (Size() < other.Size()) {
      return false;
    }

    auto& smaller = Size() < other.Size() ? *this : other;
    auto& greater = Size() < other.Size() ? other : *this;

    return std::equal(smaller.begin(), smaller.end(), greater.begin());
  }

  // Remove all characters 'c' from each token, potentially copying the underlying tokens.
  TokenRange RemoveCharacter(char c) const {
    TokenList new_token_list(begin(), end());

    bool changed = false;
    for (auto&& token : new_token_list) {
      auto it = std::remove_if(token.begin(), token.end(), [&](char ch) {
        if (ch == c) {
          changed = true;
          return true;
        }
        return false;
      });
      token.erase(it, token.end());
    }

    if (!changed) {
      return *this;
    }

    return TokenRange(std::move(new_token_list));
  }

  // Remove all tokens matching this one, potentially copying the underlying tokens.
  TokenRange RemoveToken(const std::string& token) {
    return RemoveIf([&](const std::string& tok) { return tok == token; });
  }

  // Discard all empty tokens, potentially copying the underlying tokens.
  TokenRange DiscardEmpty() const {
    return RemoveIf([](const std::string& token) { return token.empty(); });
  }

  // Create a non-copying subset of this range.
  // Length is trimmed so that the Slice does not go out of range.
  TokenRange Slice(size_t offset, size_t length = std::string::npos) const {
    assert(offset < Size());

    if (length != std::string::npos && offset + length > Size()) {
      length = Size() - offset;
    }

    iterator it_end;
    if (length == std::string::npos) {
      it_end = end();
    } else {
      it_end = begin() + offset + length;
    }

    return TokenRange(token_list_, begin() + offset, it_end);
  }

  // Try to match the string with tokens from this range.
  // Each token is used to match exactly once (after which the next token is used, and so on).
  // The matching happens from left-to-right in a non-greedy fashion.
  // If the currently-matched token is the wildcard, then the new outputted token will
  // contain as much as possible until the next token is matched.
  //
  // For example, if this == ["a:", "_", "b:] and "_" is the match string, then
  // MatchSubstrings on "a:foob:" will yield: ["a:", "foo", "b:"]
  //
  // Since the string matching can fail (e.g. ["foo"] against "bar"), then this
  // function can fail, in which cause it will return null.
  std::unique_ptr<TokenRange> MatchSubstrings(const std::string& string,
                                              const std::string& wildcard) const {
    TokenList new_token_list;

    size_t wildcard_idx = std::string::npos;
    size_t string_idx = 0;

    // Function to push all the characters matched as a wildcard so far
    // as a brand new token. It resets the wildcard matching.
    // Empty wildcards are possible and ok, but only if wildcard matching was on.
    auto maybe_push_wildcard_token = [&]() {
      if (wildcard_idx != std::string::npos) {
        size_t wildcard_length = string_idx - wildcard_idx;
        std::string wildcard_substr = string.substr(wildcard_idx, wildcard_length);
        new_token_list.push_back(std::move(wildcard_substr));

        wildcard_idx = std::string::npos;
      }
    };

    for (iterator it = begin(); it != end(); ++it) {
      const std::string& tok = *it;

      if (tok == wildcard) {
        maybe_push_wildcard_token();
        wildcard_idx = string_idx;
        continue;
      }

      size_t next_token_idx = string.find(tok);
      if (next_token_idx == std::string::npos) {
        // Could not find token at all
        return nullptr;
      } else if (next_token_idx != string_idx && wildcard_idx == std::string::npos) {
        // Found the token at a non-starting location, and we weren't
        // trying to parse the wildcard.
        return nullptr;
      }

      new_token_list.push_back(string.substr(next_token_idx, tok.size()));
      maybe_push_wildcard_token();
      string_idx += tok.size();
    }

    size_t remaining = string.size() - string_idx;
    if (remaining > 0) {
      if (wildcard_idx == std::string::npos) {
        // Some characters were still remaining in the string,
        // but it wasn't trying to match a wildcard.
        return nullptr;
      }
    }

    // If some characters are remaining, the rest must be a wildcard.
    string_idx += remaining;
    maybe_push_wildcard_token();

    return std::unique_ptr<TokenRange>(new TokenRange(std::move(new_token_list)));
  }

  // Do a quick match token-by-token, and see if they match.
  // Any tokens with a wildcard in them are only matched up until the wildcard.
  // If this is true, then the wildcard matching later on can still fail, so this is not
  // a guarantee that the argument is correct, it's more of a strong hint that the
  // user-provided input *probably* was trying to match this argument.
  //
  // Returns how many tokens were either matched (or ignored because there was a
  // wildcard present). 0 means no match. If the size() tokens are returned.
  size_t MaybeMatches(const TokenRange& token_list, const std::string& wildcard) const {
    auto token_it = token_list.begin();
    auto token_end = token_list.end();
    auto name_it = begin();
    auto name_end = end();

    size_t matched_tokens = 0;

    while (token_it != token_end && name_it != name_end) {
      // Skip token matching when the corresponding name has a wildcard in it.
      const std::string& name = *name_it;

      size_t wildcard_idx = name.find(wildcard);
      if (wildcard_idx == std::string::npos) {  // No wildcard present
        // Did the definition token match the user token?
        if (name != *token_it) {
          return matched_tokens;
        }
      } else {
        std::string name_prefix = name.substr(0, wildcard_idx);

        // Did the user token start with the up-to-the-wildcard prefix?
        if (!StartsWith(*token_it, name_prefix)) {
          return matched_tokens;
        }
      }

      ++token_it;
      ++name_it;
      ++matched_tokens;
    }

    // If we got this far, it's either a full match or the token list was too short.
    return matched_tokens;
  }

  // Flatten the token range by joining every adjacent token with the separator character.
  // e.g. ["hello", "world"].join('$') == "hello$world"
  std::string Join(char separator) const {
    TokenList tmp(begin(), end());
    return android::base::Join(tmp, separator);
    // TODO: Join should probably take an offset or iterators
  }

 private:
  static bool StartsWith(const std::string& larger, const std::string& smaller) {
    if (larger.size() >= smaller.size()) {
      return std::equal(smaller.begin(), smaller.end(), larger.begin());
    }

    return false;
  }

  template <typename TPredicate>
  TokenRange RemoveIf(const TPredicate& predicate) const {
    // If any of the tokens in the token lists are empty, then
    // we need to remove them and compress the token list into a smaller one.
    bool remove = false;
    for (auto it = begin_; it != end_; ++it) {
      auto&& token = *it;

      if (predicate(token)) {
        remove = true;
        break;
      }
    }

    // Actually copy the token list and remove the tokens that don't match our predicate.
    if (remove) {
      auto token_list = std::make_shared<TokenList>(begin(), end());
      TokenList::iterator new_end =
          std::remove_if(token_list->begin(), token_list->end(), predicate);
      token_list->erase(new_end, token_list->end());

      assert(token_list_->size() > token_list->size() && "Nothing was actually removed!");

      return TokenRange(token_list);
    }

    return *this;
  }

  const std::shared_ptr<std::vector<std::string>> token_list_;
  const iterator begin_;
  const iterator end_;
};
}  // namespace art

#endif  // ART_CMDLINE_TOKEN_RANGE_H_
