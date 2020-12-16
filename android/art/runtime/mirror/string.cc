/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "string-inl.h"

#include "arch/memcmp16.h"
#include "array.h"
#include "base/array_ref.h"
#include "base/stl_util.h"
#include "class-inl.h"
#include "dex/descriptors_names.h"
#include "dex/utf-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "gc_root-inl.h"
#include "handle_scope-inl.h"
#include "intern_table.h"
#include "object-inl.h"
#include "runtime.h"
#include "string-inl.h"
#include "thread.h"

namespace art {
namespace mirror {

// TODO: get global references for these
GcRoot<Class> String::java_lang_String_;

int32_t String::FastIndexOf(int32_t ch, int32_t start) {
  int32_t count = GetLength();
  if (start < 0) {
    start = 0;
  } else if (start > count) {
    start = count;
  }
  if (IsCompressed()) {
    return FastIndexOf<uint8_t>(GetValueCompressed(), ch, start);
  } else {
    return FastIndexOf<uint16_t>(GetValue(), ch, start);
  }
}

void String::SetClass(ObjPtr<Class> java_lang_String) {
  CHECK(java_lang_String_.IsNull());
  CHECK(java_lang_String != nullptr);
  CHECK(java_lang_String->IsStringClass());
  java_lang_String_ = GcRoot<Class>(java_lang_String);
}

void String::ResetClass() {
  CHECK(!java_lang_String_.IsNull());
  java_lang_String_ = GcRoot<Class>(nullptr);
}

int String::ComputeHashCode() {
  int32_t hash_code = 0;
  if (IsCompressed()) {
    hash_code = ComputeUtf16Hash(GetValueCompressed(), GetLength());
  } else {
    hash_code = ComputeUtf16Hash(GetValue(), GetLength());
  }
  SetHashCode(hash_code);
  return hash_code;
}

int32_t String::GetUtfLength() {
  if (IsCompressed()) {
    return GetLength();
  } else {
    return CountUtf8Bytes(GetValue(), GetLength());
  }
}

inline bool String::AllASCIIExcept(const uint16_t* chars, int32_t length, uint16_t non_ascii) {
  DCHECK(!IsASCII(non_ascii));
  for (int32_t i = 0; i < length; ++i) {
    if (!IsASCII(chars[i]) && chars[i] != non_ascii) {
      return false;
    }
  }
  return true;
}

ObjPtr<String> String::DoReplace(Thread* self, Handle<String> src, uint16_t old_c, uint16_t new_c) {
  int32_t length = src->GetLength();
  DCHECK(src->IsCompressed()
             ? ContainsElement(ArrayRef<uint8_t>(src->value_compressed_, length), old_c)
             : ContainsElement(ArrayRef<uint16_t>(src->value_, length), old_c));
  bool compressible =
      kUseStringCompression &&
      IsASCII(new_c) &&
      (src->IsCompressed() || (!IsASCII(old_c) && AllASCIIExcept(src->value_, length, old_c)));
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const int32_t length_with_flag = String::GetFlaggedCount(length, compressible);
  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    auto replace = [old_c, new_c](uint16_t c) {
      return dchecked_integral_cast<uint8_t>((old_c != c) ? c : new_c);
    };
    uint8_t* out = string->value_compressed_;
    if (LIKELY(src->IsCompressed())) {  // LIKELY(compressible == src->IsCompressed())
      std::transform(src->value_compressed_, src->value_compressed_ + length, out, replace);
    } else {
      std::transform(src->value_, src->value_ + length, out, replace);
    }
    DCHECK(kUseStringCompression && AllASCII(out, length));
  } else {
    auto replace = [old_c, new_c](uint16_t c) {
      return (old_c != c) ? c : new_c;
    };
    uint16_t* out = string->value_;
    if (UNLIKELY(src->IsCompressed())) {  // LIKELY(compressible == src->IsCompressed())
      std::transform(src->value_compressed_, src->value_compressed_ + length, out, replace);
    } else {
      std::transform(src->value_, src->value_ + length, out, replace);
    }
    DCHECK(!kUseStringCompression || !AllASCII(out, length));
  }
  return string;
}

String* String::AllocFromStrings(Thread* self, Handle<String> string, Handle<String> string2) {
  int32_t length = string->GetLength();
  int32_t length2 = string2->GetLength();
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression &&
      (string->IsCompressed() && string2->IsCompressed());
  const int32_t length_with_flag = String::GetFlaggedCount(length + length2, compressible);

  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> new_string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(new_string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    uint8_t* new_value = new_string->GetValueCompressed();
    memcpy(new_value, string->GetValueCompressed(), length * sizeof(uint8_t));
    memcpy(new_value + length, string2->GetValueCompressed(), length2 * sizeof(uint8_t));
  } else {
    uint16_t* new_value = new_string->GetValue();
    if (string->IsCompressed()) {
      for (int i = 0; i < length; ++i) {
        new_value[i] = string->CharAt(i);
      }
    } else {
      memcpy(new_value, string->GetValue(), length * sizeof(uint16_t));
    }
    if (string2->IsCompressed()) {
      for (int i = 0; i < length2; ++i) {
        new_value[i+length] = string2->CharAt(i);
      }
    } else {
      memcpy(new_value + length, string2->GetValue(), length2 * sizeof(uint16_t));
    }
  }
  return new_string.Ptr();
}

String* String::AllocFromUtf16(Thread* self, int32_t utf16_length, const uint16_t* utf16_data_in) {
  CHECK(utf16_data_in != nullptr || utf16_length == 0);
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression &&
                            String::AllASCII<uint16_t>(utf16_data_in, utf16_length);
  int32_t length_with_flag = String::GetFlaggedCount(utf16_length, compressible);
  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    for (int i = 0; i < utf16_length; ++i) {
      string->GetValueCompressed()[i] = static_cast<uint8_t>(utf16_data_in[i]);
    }
  } else {
    uint16_t* array = string->GetValue();
    memcpy(array, utf16_data_in, utf16_length * sizeof(uint16_t));
  }
  return string.Ptr();
}

String* String::AllocFromModifiedUtf8(Thread* self, const char* utf) {
  DCHECK(utf != nullptr);
  size_t byte_count = strlen(utf);
  size_t char_count = CountModifiedUtf8Chars(utf, byte_count);
  return AllocFromModifiedUtf8(self, char_count, utf, byte_count);
}

String* String::AllocFromModifiedUtf8(Thread* self,
                                      int32_t utf16_length,
                                      const char* utf8_data_in) {
  return AllocFromModifiedUtf8(self, utf16_length, utf8_data_in, strlen(utf8_data_in));
}

String* String::AllocFromModifiedUtf8(Thread* self,
                                      int32_t utf16_length,
                                      const char* utf8_data_in,
                                      int32_t utf8_length) {
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression && (utf16_length == utf8_length);
  const int32_t utf16_length_with_flag = String::GetFlaggedCount(utf16_length, compressible);
  SetStringCountVisitor visitor(utf16_length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, utf16_length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    memcpy(string->GetValueCompressed(), utf8_data_in, utf16_length * sizeof(uint8_t));
  } else {
    uint16_t* utf16_data_out = string->GetValue();
    ConvertModifiedUtf8ToUtf16(utf16_data_out, utf16_length, utf8_data_in, utf8_length);
  }
  return string.Ptr();
}

bool String::Equals(ObjPtr<String> that) {
  if (this == that) {
    // Quick reference equality test
    return true;
  } else if (that == nullptr) {
    // Null isn't an instanceof anything
    return false;
  } else if (this->GetLength() != that->GetLength()) {
    // Quick length inequality test
    return false;
  } else {
    // Note: don't short circuit on hash code as we're presumably here as the
    // hash code was already equal
    for (int32_t i = 0; i < that->GetLength(); ++i) {
      if (this->CharAt(i) != that->CharAt(i)) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const uint16_t* that_chars, int32_t that_offset, int32_t that_length) {
  if (this->GetLength() != that_length) {
    return false;
  } else {
    for (int32_t i = 0; i < that_length; ++i) {
      if (this->CharAt(i) != that_chars[that_offset + i]) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const char* modified_utf8) {
  const int32_t length = GetLength();
  int32_t i = 0;
  while (i < length) {
    const uint32_t ch = GetUtf16FromUtf8(&modified_utf8);
    if (ch == '\0') {
      return false;
    }

    if (GetLeadingUtf16Char(ch) != CharAt(i++)) {
      return false;
    }

    const uint16_t trailing = GetTrailingUtf16Char(ch);
    if (trailing != 0) {
      if (i == length) {
        return false;
      }

      if (CharAt(i++) != trailing) {
        return false;
      }
    }
  }
  return *modified_utf8 == '\0';
}

bool String::Equals(const StringPiece& modified_utf8) {
  const int32_t length = GetLength();
  const char* p = modified_utf8.data();
  for (int32_t i = 0; i < length; ++i) {
    uint32_t ch = GetUtf16FromUtf8(&p);

    if (GetLeadingUtf16Char(ch) != CharAt(i)) {
      return false;
    }

    const uint16_t trailing = GetTrailingUtf16Char(ch);
    if (trailing != 0) {
      if (i == (length - 1)) {
        return false;
      }

      if (CharAt(++i) != trailing) {
        return false;
      }
    }
  }
  return true;
}

// Create a modified UTF-8 encoded std::string from a java/lang/String object.
std::string String::ToModifiedUtf8() {
  size_t byte_count = GetUtfLength();
  std::string result(byte_count, static_cast<char>(0));
  if (IsCompressed()) {
    for (size_t i = 0; i < byte_count; ++i) {
      result[i] = static_cast<char>(CharAt(i));
    }
  } else {
    const uint16_t* chars = GetValue();
    ConvertUtf16ToModifiedUtf8(&result[0], byte_count, chars, GetLength());
  }
  return result;
}

int32_t String::CompareTo(ObjPtr<String> rhs) {
  // Quick test for comparison of a string with itself.
  ObjPtr<String> lhs = this;
  if (lhs == rhs) {
    return 0;
  }
  int32_t lhs_count = lhs->GetLength();
  int32_t rhs_count = rhs->GetLength();
  int32_t count_diff = lhs_count - rhs_count;
  int32_t min_count = (count_diff < 0) ? lhs_count : rhs_count;
  if (lhs->IsCompressed() && rhs->IsCompressed()) {
    const uint8_t* lhs_chars = lhs->GetValueCompressed();
    const uint8_t* rhs_chars = rhs->GetValueCompressed();
    for (int32_t i = 0; i < min_count; ++i) {
      int32_t char_diff = static_cast<int32_t>(lhs_chars[i]) - static_cast<int32_t>(rhs_chars[i]);
      if (char_diff != 0) {
        return char_diff;
      }
    }
  } else if (lhs->IsCompressed() || rhs->IsCompressed()) {
    const uint8_t* compressed_chars =
        lhs->IsCompressed() ? lhs->GetValueCompressed() : rhs->GetValueCompressed();
    const uint16_t* uncompressed_chars = lhs->IsCompressed() ? rhs->GetValue() : lhs->GetValue();
    for (int32_t i = 0; i < min_count; ++i) {
      int32_t char_diff =
          static_cast<int32_t>(compressed_chars[i]) - static_cast<int32_t>(uncompressed_chars[i]);
      if (char_diff != 0) {
        return lhs->IsCompressed() ? char_diff : -char_diff;
      }
    }
  } else {
    const uint16_t* lhs_chars = lhs->GetValue();
    const uint16_t* rhs_chars = rhs->GetValue();
    // FIXME: The MemCmp16() name is misleading. It returns the char difference on mismatch
    // where memcmp() only guarantees that the returned value has the same sign.
    int32_t char_diff = MemCmp16(lhs_chars, rhs_chars, min_count);
    if (char_diff != 0) {
      return char_diff;
    }
  }
  return count_diff;
}

void String::VisitRoots(RootVisitor* visitor) {
  java_lang_String_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

CharArray* String::ToCharArray(Thread* self) {
  StackHandleScope<1> hs(self);
  Handle<String> string(hs.NewHandle(this));
  ObjPtr<CharArray> result = CharArray::Alloc(self, GetLength());
  if (result != nullptr) {
    if (string->IsCompressed()) {
      int32_t length = string->GetLength();
      for (int i = 0; i < length; ++i) {
        result->GetData()[i] = string->CharAt(i);
      }
    } else {
      memcpy(result->GetData(), string->GetValue(), string->GetLength() * sizeof(uint16_t));
    }
  } else {
    self->AssertPendingOOMException();
  }
  return result.Ptr();
}

void String::GetChars(int32_t start, int32_t end, Handle<CharArray> array, int32_t index) {
  uint16_t* data = array->GetData() + index;
  if (IsCompressed()) {
    for (int i = start; i < end; ++i) {
      data[i-start] = CharAt(i);
    }
  } else {
    uint16_t* value = GetValue() + start;
    memcpy(data, value, (end - start) * sizeof(uint16_t));
  }
}

bool String::IsValueNull() {
  return (IsCompressed()) ? (GetValueCompressed() == nullptr) : (GetValue() == nullptr);
}

std::string String::PrettyStringDescriptor(ObjPtr<mirror::String> java_descriptor) {
  if (java_descriptor == nullptr) {
    return "null";
  }
  return java_descriptor->PrettyStringDescriptor();
}

std::string String::PrettyStringDescriptor() {
  return PrettyDescriptor(ToModifiedUtf8().c_str());
}

ObjPtr<String> String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

}  // namespace mirror
}  // namespace art
