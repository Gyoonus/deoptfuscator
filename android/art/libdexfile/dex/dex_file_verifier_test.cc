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

#include "dex_file_verifier.h"

#include <zlib.h>

#include <functional>
#include <memory>

#include "base/bit_utils.h"
#include "base/leb128.h"
#include "base/macros.h"
#include "base64_test_util.h"
#include "descriptors_names.h"
#include "dex_file-inl.h"
#include "dex_file_loader.h"
#include "dex_file_types.h"
#include "gtest/gtest.h"
#include "standard_dex_file.h"

namespace art {

static constexpr char kLocationString[] = "dex_file_location";

// Make the Dex file version 37.
static void MakeDexVersion37(DexFile* dex_file) {
  size_t offset = OFFSETOF_MEMBER(DexFile::Header, magic_) + 6;
  CHECK_EQ(*(dex_file->Begin() + offset), '5');
  *(const_cast<uint8_t*>(dex_file->Begin()) + offset) = '7';
}

static void FixUpChecksum(uint8_t* dex_file) {
  DexFile::Header* header = reinterpret_cast<DexFile::Header*>(dex_file);
  uint32_t expected_size = header->file_size_;
  uint32_t adler_checksum = adler32(0L, Z_NULL, 0);
  const uint32_t non_sum = sizeof(DexFile::Header::magic_) + sizeof(DexFile::Header::checksum_);
  const uint8_t* non_sum_ptr = dex_file + non_sum;
  adler_checksum = adler32(adler_checksum, non_sum_ptr, expected_size - non_sum);
  header->checksum_ = adler_checksum;
}

class DexFileVerifierTest : public testing::Test {
 protected:
  DexFile* GetDexFile(const uint8_t* dex_bytes, size_t length) {
    return new StandardDexFile(dex_bytes, length, "tmp", 0, nullptr, nullptr);
  }

  void VerifyModification(const char* dex_file_base64_content,
                          const char* location,
                          const std::function<void(DexFile*)>& f,
                          const char* expected_error) {
    size_t length;
    std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(dex_file_base64_content, &length));
    CHECK(dex_bytes != nullptr);
    // Note: `dex_file` will be destroyed before `dex_bytes`.
    std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
    f(dex_file.get());
    FixUpChecksum(const_cast<uint8_t*>(dex_file->Begin()));

    static constexpr bool kVerifyChecksum = true;
    std::string error_msg;
    bool success = DexFileVerifier::Verify(dex_file.get(),
                                           dex_file->Begin(),
                                           dex_file->Size(),
                                           location,
                                           kVerifyChecksum,
                                           &error_msg);
    if (expected_error == nullptr) {
      EXPECT_TRUE(success) << error_msg;
    } else {
      EXPECT_FALSE(success) << "Expected " << expected_error;
      if (!success) {
        EXPECT_NE(error_msg.find(expected_error), std::string::npos) << error_msg;
      }
    }
  }
};

static std::unique_ptr<const DexFile> OpenDexFileBase64(const char* base64,
                                                        const char* location,
                                                        std::string* error_msg) {
  // decode base64
  CHECK(base64 != nullptr);
  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(base64, &length));
  CHECK(dex_bytes.get() != nullptr);

  // read dex
  std::vector<std::unique_ptr<const DexFile>> tmp;
  const DexFileLoader dex_file_loader;
  bool success = dex_file_loader.OpenAll(dex_bytes.get(),
                                         length,
                                         location,
                                         /* verify */ true,
                                         /* verify_checksum */ true,
                                         error_msg,
                                         &tmp);
  CHECK(success) << *error_msg;
  EXPECT_EQ(1U, tmp.size());
  std::unique_ptr<const DexFile> dex_file = std::move(tmp[0]);
  return dex_file;
}

// To generate a base64 encoded Dex file (such as kGoodTestDex, below)
// from Smali files, use:
//
//   smali assemble -o classes.dex class1.smali [class2.smali ...]
//   base64 classes.dex >classes.dex.base64

// For reference.
static const char kGoodTestDex[] =
    "ZGV4CjAzNQDrVbyVkxX1HljTznNf95AglkUAhQuFtmKkAgAAcAAAAHhWNBIAAAAAAAAAAAQCAAAN"
    "AAAAcAAAAAYAAACkAAAAAgAAALwAAAABAAAA1AAAAAQAAADcAAAAAQAAAPwAAACIAQAAHAEAAFoB"
    "AABiAQAAagEAAIEBAACVAQAAqQEAAL0BAADDAQAAzgEAANEBAADVAQAA2gEAAN8BAAABAAAAAgAA"
    "AAMAAAAEAAAABQAAAAgAAAAIAAAABQAAAAAAAAAJAAAABQAAAFQBAAAEAAEACwAAAAAAAAAAAAAA"
    "AAAAAAoAAAABAAEADAAAAAIAAAAAAAAAAAAAAAEAAAACAAAAAAAAAAcAAAAAAAAA8wEAAAAAAAAB"
    "AAEAAQAAAOgBAAAEAAAAcBADAAAADgACAAAAAgAAAO0BAAAIAAAAYgAAABoBBgBuIAIAEAAOAAEA"
    "AAADAAY8aW5pdD4ABkxUZXN0OwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09i"
    "amVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AARUZXN0AAlUZXN0"
    "LmphdmEAAVYAAlZMAANmb28AA291dAAHcHJpbnRsbgABAAcOAAMABw54AAAAAgAAgYAEnAIBCbQC"
    "AAAADQAAAAAAAAABAAAAAAAAAAEAAAANAAAAcAAAAAIAAAAGAAAApAAAAAMAAAACAAAAvAAAAAQA"
    "AAABAAAA1AAAAAUAAAAEAAAA3AAAAAYAAAABAAAA/AAAAAEgAAACAAAAHAEAAAEQAAABAAAAVAEA"
    "AAIgAAANAAAAWgEAAAMgAAACAAAA6AEAAAAgAAABAAAA8wEAAAAQAAABAAAABAIAAA==";

TEST_F(DexFileVerifierTest, GoodDex) {
  std::string error_msg;
  std::unique_ptr<const DexFile> raw(OpenDexFileBase64(kGoodTestDex,
                                                       kLocationString,
                                                       &error_msg));
  ASSERT_TRUE(raw.get() != nullptr) << error_msg;
}

TEST_F(DexFileVerifierTest, MethodId) {
  // Class idx error.
  VerifyModification(
      kGoodTestDex,
      "method_id_class_idx",
      [](DexFile* dex_file) {
        DexFile::MethodId* method_id = const_cast<DexFile::MethodId*>(&dex_file->GetMethodId(0));
        method_id->class_idx_ = dex::TypeIndex(0xFF);
      },
      "could not find declaring class for direct method index 0");

  // Proto idx error.
  VerifyModification(
      kGoodTestDex,
      "method_id_proto_idx",
      [](DexFile* dex_file) {
        DexFile::MethodId* method_id = const_cast<DexFile::MethodId*>(&dex_file->GetMethodId(0));
        method_id->proto_idx_ = 0xFF;
      },
      "inter_method_id_item proto_idx");

  // Name idx error.
  VerifyModification(
      kGoodTestDex,
      "method_id_name_idx",
      [](DexFile* dex_file) {
        DexFile::MethodId* method_id = const_cast<DexFile::MethodId*>(&dex_file->GetMethodId(0));
        method_id->name_idx_ = dex::StringIndex(0xFF);
      },
      "String index not available for method flags verification");
}

// Method flags test class generated from the following smali code. The declared-synchronized
// flags are there to enforce a 3-byte uLEB128 encoding so we don't have to relayout
// the code, but we need to remove them before doing tests.
//
// .class public LMethodFlags;
// .super Ljava/lang/Object;
//
// .method public static constructor <clinit>()V
// .registers 1
//     return-void
// .end method
//
// .method public constructor <init>()V
// .registers 1
//     return-void
// .end method
//
// .method private declared-synchronized foo()V
// .registers 1
//     return-void
// .end method
//
// .method public declared-synchronized bar()V
// .registers 1
//     return-void
// .end method

static const char kMethodFlagsTestDex[] =
    "ZGV4CjAzNQCyOQrJaDBwiIWv5MIuYKXhxlLLsQcx5SwgAgAAcAAAAHhWNBIAAAAAAAAAAJgBAAAH"
    "AAAAcAAAAAMAAACMAAAAAQAAAJgAAAAAAAAAAAAAAAQAAACkAAAAAQAAAMQAAAA8AQAA5AAAAOQA"
    "AADuAAAA9gAAAAUBAAAZAQAAHAEAACEBAAACAAAAAwAAAAQAAAAEAAAAAgAAAAAAAAAAAAAAAAAA"
    "AAAAAAABAAAAAAAAAAUAAAAAAAAABgAAAAAAAAABAAAAAQAAAAAAAAD/////AAAAAHoBAAAAAAAA"
    "CDxjbGluaXQ+AAY8aW5pdD4ADUxNZXRob2RGbGFnczsAEkxqYXZhL2xhbmcvT2JqZWN0OwABVgAD"
    "YmFyAANmb28AAAAAAAAAAQAAAAAAAAAAAAAAAQAAAA4AAAABAAEAAAAAAAAAAAABAAAADgAAAAEA"
    "AQAAAAAAAAAAAAEAAAAOAAAAAQABAAAAAAAAAAAAAQAAAA4AAAADAQCJgASsAgGBgATAAgKCgAjU"
    "AgKBgAjoAgAACwAAAAAAAAABAAAAAAAAAAEAAAAHAAAAcAAAAAIAAAADAAAAjAAAAAMAAAABAAAA"
    "mAAAAAUAAAAEAAAApAAAAAYAAAABAAAAxAAAAAIgAAAHAAAA5AAAAAMQAAABAAAAKAEAAAEgAAAE"
    "AAAALAEAAAAgAAABAAAAegEAAAAQAAABAAAAmAEAAA==";

// Find the method data for the first method with the given name (from class 0). Note: the pointer
// is to the access flags, so that the caller doesn't have to handle the leb128-encoded method-index
// delta.
static const uint8_t* FindMethodData(const DexFile* dex_file,
                                     const char* name,
                                     /*out*/ uint32_t* method_idx = nullptr) {
  const DexFile::ClassDef& class_def = dex_file->GetClassDef(0);
  const uint8_t* class_data = dex_file->GetClassData(class_def);

  ClassDataItemIterator it(*dex_file, class_data);

  const uint8_t* trailing = class_data;
  // Need to manually decode the four entries. DataPointer() doesn't work for this, as the first
  // element has already been loaded into the iterator.
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);

  // Skip all fields.
  while (it.HasNextStaticField() || it.HasNextInstanceField()) {
    trailing = it.DataPointer();
    it.Next();
  }

  while (it.HasNextMethod()) {
    uint32_t method_index = it.GetMemberIndex();
    dex::StringIndex name_index = dex_file->GetMethodId(method_index).name_idx_;
    const DexFile::StringId& string_id = dex_file->GetStringId(name_index);
    const char* str = dex_file->GetStringData(string_id);
    if (strcmp(name, str) == 0) {
      if (method_idx != nullptr) {
        *method_idx = method_index;
      }
      DecodeUnsignedLeb128(&trailing);
      return trailing;
    }

    trailing = it.DataPointer();
    it.Next();
  }

  return nullptr;
}

// Set the method flags to the given value.
static void SetMethodFlags(DexFile* dex_file, const char* method, uint32_t mask) {
  uint8_t* method_flags_ptr = const_cast<uint8_t*>(FindMethodData(dex_file, method));
  CHECK(method_flags_ptr != nullptr) << method;

    // Unroll this, as we only have three bytes, anyways.
  uint8_t base1 = static_cast<uint8_t>(mask & 0x7F);
  *(method_flags_ptr++) = (base1 | 0x80);
  mask >>= 7;

  uint8_t base2 = static_cast<uint8_t>(mask & 0x7F);
  *(method_flags_ptr++) = (base2 | 0x80);
  mask >>= 7;

  uint8_t base3 = static_cast<uint8_t>(mask & 0x7F);
  *method_flags_ptr = base3;
}

static uint32_t GetMethodFlags(DexFile* dex_file, const char* method) {
  const uint8_t* method_flags_ptr = const_cast<uint8_t*>(FindMethodData(dex_file, method));
  CHECK(method_flags_ptr != nullptr) << method;
  return DecodeUnsignedLeb128(&method_flags_ptr);
}

// Apply the given mask to method flags.
static void ApplyMaskToMethodFlags(DexFile* dex_file, const char* method, uint32_t mask) {
  uint32_t value = GetMethodFlags(dex_file, method);
  value &= mask;
  SetMethodFlags(dex_file, method, value);
}

// Apply the given mask to method flags.
static void OrMaskToMethodFlags(DexFile* dex_file, const char* method, uint32_t mask) {
  uint32_t value = GetMethodFlags(dex_file, method);
  value |= mask;
  SetMethodFlags(dex_file, method, value);
}

// Set code_off to 0 for the method.
static void RemoveCode(DexFile* dex_file, const char* method) {
  const uint8_t* ptr = FindMethodData(dex_file, method);
  // Next is flags, pass.
  DecodeUnsignedLeb128(&ptr);

  // Figure out how many bytes the code_off is.
  const uint8_t* tmp = ptr;
  DecodeUnsignedLeb128(&tmp);
  size_t bytes = tmp - ptr;

  uint8_t* mod = const_cast<uint8_t*>(ptr);
  for (size_t i = 1; i < bytes; ++i) {
    *(mod++) = 0x80;
  }
  *mod = 0x00;
}

TEST_F(DexFileVerifierTest, MethodAccessFlagsBase) {
  // Check that it's OK when the wrong declared-synchronized flag is removed from "foo."
  VerifyModification(
      kMethodFlagsTestDex,
      "method_flags_ok",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);
      },
      nullptr);
}

TEST_F(DexFileVerifierTest, MethodAccessFlagsConstructors) {
  // Make sure we still accept constructors without their flags.
  VerifyModification(
      kMethodFlagsTestDex,
      "method_flags_missing_constructor_tag_ok",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "<init>", ~kAccConstructor);
        ApplyMaskToMethodFlags(dex_file, "<clinit>", ~kAccConstructor);
      },
      nullptr);

  constexpr const char* kConstructors[] = { "<clinit>", "<init>"};
  for (size_t i = 0; i < 2; ++i) {
    // Constructor with code marked native.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_constructor_native",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kConstructors[i], kAccNative);
        },
        "has code, but is marked native or abstract");
    // Constructor with code marked abstract.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_constructor_abstract",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kConstructors[i], kAccAbstract);
        },
        "has code, but is marked native or abstract");
    // Constructor as-is without code.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_constructor_nocode",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          RemoveCode(dex_file, kConstructors[i]);
        },
        "has no code, but is not marked native or abstract");
    // Constructor without code marked native.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_constructor_native_nocode",
        [&](DexFile* dex_file) {
          MakeDexVersion37(dex_file);
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kConstructors[i], kAccNative);
          RemoveCode(dex_file, kConstructors[i]);
        },
        "must not be abstract or native");
    // Constructor without code marked abstract.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_constructor_abstract_nocode",
        [&](DexFile* dex_file) {
          MakeDexVersion37(dex_file);
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kConstructors[i], kAccAbstract);
          RemoveCode(dex_file, kConstructors[i]);
        },
        "must not be abstract or native");
  }
  // <init> may only have (modulo ignored):
  // kAccPrivate | kAccProtected | kAccPublic | kAccStrict | kAccVarargs | kAccSynthetic
  static constexpr uint32_t kInitAllowed[] = {
      0,
      kAccPrivate,
      kAccProtected,
      kAccPublic,
      kAccStrict,
      kAccVarargs,
      kAccSynthetic
  };
  for (size_t i = 0; i < arraysize(kInitAllowed); ++i) {
    VerifyModification(
        kMethodFlagsTestDex,
        "init_allowed_flags",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          ApplyMaskToMethodFlags(dex_file, "<init>", ~kAccPublic);
          OrMaskToMethodFlags(dex_file, "<init>", kInitAllowed[i]);
        },
        nullptr);
  }
  // Only one of public-private-protected.
  for (size_t i = 1; i < 8; ++i) {
    if (POPCOUNT(i) < 2) {
      continue;
    }
    // Technically the flags match, but just be defensive here.
    uint32_t mask = ((i & 1) != 0 ? kAccPrivate : 0) |
                    ((i & 2) != 0 ? kAccProtected : 0) |
                    ((i & 4) != 0 ? kAccPublic : 0);
    VerifyModification(
        kMethodFlagsTestDex,
        "init_one_of_ppp",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          ApplyMaskToMethodFlags(dex_file, "<init>", ~kAccPublic);
          OrMaskToMethodFlags(dex_file, "<init>", mask);
        },
        "Method may have only one of public/protected/private");
  }
  // <init> doesn't allow
  // kAccStatic | kAccFinal | kAccSynchronized | kAccBridge
  // Need to handle static separately as it has its own error message.
  VerifyModification(
      kMethodFlagsTestDex,
      "init_not_allowed_flags",
      [&](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "<init>", ~kAccPublic);
        OrMaskToMethodFlags(dex_file, "<init>", kAccStatic);
      },
      "Constructor 1(LMethodFlags;.<init>) is not flagged correctly wrt/ static");
  static constexpr uint32_t kInitNotAllowed[] = {
      kAccFinal,
      kAccSynchronized,
      kAccBridge
  };
  for (size_t i = 0; i < arraysize(kInitNotAllowed); ++i) {
    VerifyModification(
        kMethodFlagsTestDex,
        "init_not_allowed_flags",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          ApplyMaskToMethodFlags(dex_file, "<init>", ~kAccPublic);
          OrMaskToMethodFlags(dex_file, "<init>", kInitNotAllowed[i]);
        },
        "Constructor 1(LMethodFlags;.<init>) flagged inappropriately");
  }
}

TEST_F(DexFileVerifierTest, MethodAccessFlagsMethods) {
  constexpr const char* kMethods[] = { "foo", "bar"};
  for (size_t i = 0; i < arraysize(kMethods); ++i) {
    // Make sure we reject non-constructors marked as constructors.
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_non_constructor",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kMethods[i], kAccConstructor);
        },
        "is marked constructor, but doesn't match name");

    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_native_with_code",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kMethods[i], kAccNative);
        },
        "has code, but is marked native or abstract");

    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_abstract_with_code",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kMethods[i], kAccAbstract);
        },
        "has code, but is marked native or abstract");

    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_non_abstract_native_no_code",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          RemoveCode(dex_file, kMethods[i]);
        },
        "has no code, but is not marked native or abstract");

    // Abstract methods may not have the following flags.
    constexpr uint32_t kAbstractDisallowed[] = {
        kAccPrivate,
        kAccStatic,
        kAccFinal,
        kAccNative,
        kAccStrict,
        kAccSynchronized,
    };
    for (size_t j = 0; j < arraysize(kAbstractDisallowed); ++j) {
      VerifyModification(
          kMethodFlagsTestDex,
          "method_flags_abstract_and_disallowed_no_code",
          [&](DexFile* dex_file) {
            ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
            ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

            RemoveCode(dex_file, kMethods[i]);

            // Can't check private and static with foo, as it's in the virtual list and gives a
            // different error.
            if (((GetMethodFlags(dex_file, kMethods[i]) & kAccPublic) != 0) &&
                ((kAbstractDisallowed[j] & (kAccPrivate | kAccStatic)) != 0)) {
              // Use another breaking flag.
              OrMaskToMethodFlags(dex_file, kMethods[i], kAccAbstract | kAccFinal);
            } else {
              OrMaskToMethodFlags(dex_file, kMethods[i], kAccAbstract | kAbstractDisallowed[j]);
            }
          },
          "has disallowed access flags");
    }

    // Only one of public-private-protected.
    for (size_t j = 1; j < 8; ++j) {
      if (POPCOUNT(j) < 2) {
        continue;
      }
      // Technically the flags match, but just be defensive here.
      uint32_t mask = ((j & 1) != 0 ? kAccPrivate : 0) |
                      ((j & 2) != 0 ? kAccProtected : 0) |
                      ((j & 4) != 0 ? kAccPublic : 0);
      VerifyModification(
          kMethodFlagsTestDex,
          "method_flags_one_of_ppp",
          [&](DexFile* dex_file) {
            ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
            ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

            ApplyMaskToMethodFlags(dex_file, kMethods[i], ~kAccPublic);
            OrMaskToMethodFlags(dex_file, kMethods[i], mask);
          },
          "Method may have only one of public/protected/private");
    }
  }
}

TEST_F(DexFileVerifierTest, MethodAccessFlagsIgnoredOK) {
  constexpr const char* kMethods[] = { "<clinit>", "<init>", "foo", "bar"};
  for (size_t i = 0; i < arraysize(kMethods); ++i) {
    // All interesting method flags, other flags are to be ignored.
    constexpr uint32_t kAllMethodFlags =
        kAccPublic |
        kAccPrivate |
        kAccProtected |
        kAccStatic |
        kAccFinal |
        kAccSynchronized |
        kAccBridge |
        kAccVarargs |
        kAccNative |
        kAccAbstract |
        kAccStrict |
        kAccSynthetic;
    constexpr uint32_t kIgnoredMask = ~kAllMethodFlags & 0xFFFF;
    VerifyModification(
        kMethodFlagsTestDex,
        "method_flags_ignored",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToMethodFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToMethodFlags(dex_file, kMethods[i], kIgnoredMask);
        },
        nullptr);
  }
}

TEST_F(DexFileVerifierTest, B28552165) {
  // Regression test for bad error string retrieval in different situations.
  // Using invalid access flags to trigger the error.
  VerifyModification(
      kMethodFlagsTestDex,
      "b28552165",
      [](DexFile* dex_file) {
        OrMaskToMethodFlags(dex_file, "foo", kAccPublic | kAccProtected);
      },
      "Method may have only one of public/protected/private, LMethodFlags;.foo");
}

// Set of dex files for interface method tests. As it's not as easy to mutate method names, it's
// just easier to break up bad cases.

// Standard interface. Use declared-synchronized again for 3B encoding.
//
// .class public interface LInterfaceMethodFlags;
// .super Ljava/lang/Object;
//
// .method public static constructor <clinit>()V
// .registers 1
//     return-void
// .end method
//
// .method public abstract declared-synchronized foo()V
// .end method
static const char kMethodFlagsInterface[] =
    "ZGV4CjAzNQCOM0odZ5bws1d9GSmumXaK5iE/7XxFpOm8AQAAcAAAAHhWNBIAAAAAAAAAADQBAAAF"
    "AAAAcAAAAAMAAACEAAAAAQAAAJAAAAAAAAAAAAAAAAIAAACcAAAAAQAAAKwAAADwAAAAzAAAAMwA"
    "AADWAAAA7gAAAAIBAAAFAQAAAQAAAAIAAAADAAAAAwAAAAIAAAAAAAAAAAAAAAAAAAAAAAAABAAA"
    "AAAAAAABAgAAAQAAAAAAAAD/////AAAAACIBAAAAAAAACDxjbGluaXQ+ABZMSW50ZXJmYWNlTWV0"
    "aG9kRmxhZ3M7ABJMamF2YS9sYW5nL09iamVjdDsAAVYAA2ZvbwAAAAAAAAABAAAAAAAAAAAAAAAB"
    "AAAADgAAAAEBAImABJACAYGICAAAAAALAAAAAAAAAAEAAAAAAAAAAQAAAAUAAABwAAAAAgAAAAMA"
    "AACEAAAAAwAAAAEAAACQAAAABQAAAAIAAACcAAAABgAAAAEAAACsAAAAAiAAAAUAAADMAAAAAxAA"
    "AAEAAAAMAQAAASAAAAEAAAAQAQAAACAAAAEAAAAiAQAAABAAAAEAAAA0AQAA";

// To simplify generation of interesting "sub-states" of src_value, allow a "simple" mask to apply
// to a src_value, such that mask bit 0 applies to the lowest set bit in src_value, and so on.
static uint32_t ApplyMaskShifted(uint32_t src_value, uint32_t mask) {
  uint32_t result = 0;
  uint32_t mask_index = 0;
  while (src_value != 0) {
    uint32_t index = CTZ(src_value);
    if (((src_value & (1 << index)) != 0) &&
        ((mask & (1 << mask_index)) != 0)) {
      result |= (1 << index);
    }
    src_value &= ~(1 << index);
    mask_index++;
  }
  return result;
}

TEST_F(DexFileVerifierTest, MethodAccessFlagsInterfaces) {
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_ok",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      nullptr);
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_ok37",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      nullptr);

  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_non_public",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_non_public",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
      },
      "Interface virtual method 1(LInterfaceMethodFlags;.foo) is not public");

  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_non_abstract",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccAbstract);
      },
      "Method 1(LInterfaceMethodFlags;.foo) has no code, but is not marked native or abstract");

  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_static",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        OrMaskToMethodFlags(dex_file, "foo", kAccStatic);
      },
      "Direct/virtual method 1(LInterfaceMethodFlags;.foo) not in expected list 0");
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_private",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToMethodFlags(dex_file, "foo", kAccPrivate);
      },
      "Direct/virtual method 1(LInterfaceMethodFlags;.foo) not in expected list 0");

  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_non_public",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_non_public",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
      },
      "Interface virtual method 1(LInterfaceMethodFlags;.foo) is not public");

  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_protected",
      [](DexFile* dex_file) {
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToMethodFlags(dex_file, "foo", kAccProtected);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kMethodFlagsInterface,
      "method_flags_interface_protected",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToMethodFlags(dex_file, "foo", kAccProtected);
      },
      "Interface virtual method 1(LInterfaceMethodFlags;.foo) is not public");

  constexpr uint32_t kAllMethodFlags =
      kAccPublic |
      kAccPrivate |
      kAccProtected |
      kAccStatic |
      kAccFinal |
      kAccSynchronized |
      kAccBridge |
      kAccVarargs |
      kAccNative |
      kAccAbstract |
      kAccStrict |
      kAccSynthetic;
  constexpr uint32_t kInterfaceMethodFlags =
      kAccPublic | kAccAbstract | kAccVarargs | kAccBridge | kAccSynthetic;
  constexpr uint32_t kInterfaceDisallowed = kAllMethodFlags &
                                            ~kInterfaceMethodFlags &
                                            // Already tested, needed to be separate.
                                            ~kAccStatic &
                                            ~kAccPrivate &
                                            ~kAccProtected;
  static_assert(kInterfaceDisallowed != 0, "There should be disallowed flags.");

  uint32_t bits = POPCOUNT(kInterfaceDisallowed);
  for (uint32_t i = 1; i < (1u << bits); ++i) {
    VerifyModification(
        kMethodFlagsInterface,
        "method_flags_interface_non_abstract",
        [&](DexFile* dex_file) {
          ApplyMaskToMethodFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

          uint32_t mask = ApplyMaskShifted(kInterfaceDisallowed, i);
          if ((mask & kAccProtected) != 0) {
            mask &= ~kAccProtected;
            ApplyMaskToMethodFlags(dex_file, "foo", ~kAccPublic);
          }
          OrMaskToMethodFlags(dex_file, "foo", mask);
        },
        "Abstract method 1(LInterfaceMethodFlags;.foo) has disallowed access flags");
  }
}

///////////////////////////////////////////////////////////////////

// Field flags.

// Find the method data for the first method with the given name (from class 0). Note: the pointer
// is to the access flags, so that the caller doesn't have to handle the leb128-encoded method-index
// delta.
static const uint8_t* FindFieldData(const DexFile* dex_file, const char* name) {
  const DexFile::ClassDef& class_def = dex_file->GetClassDef(0);
  const uint8_t* class_data = dex_file->GetClassData(class_def);

  ClassDataItemIterator it(*dex_file, class_data);

  const uint8_t* trailing = class_data;
  // Need to manually decode the four entries. DataPointer() doesn't work for this, as the first
  // element has already been loaded into the iterator.
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);
  DecodeUnsignedLeb128(&trailing);

  while (it.HasNextStaticField() || it.HasNextInstanceField()) {
    uint32_t field_index = it.GetMemberIndex();
    dex::StringIndex name_index = dex_file->GetFieldId(field_index).name_idx_;
    const DexFile::StringId& string_id = dex_file->GetStringId(name_index);
    const char* str = dex_file->GetStringData(string_id);
    if (strcmp(name, str) == 0) {
      DecodeUnsignedLeb128(&trailing);
      return trailing;
    }

    trailing = it.DataPointer();
    it.Next();
  }

  return nullptr;
}

// Set the method flags to the given value.
static void SetFieldFlags(DexFile* dex_file, const char* field, uint32_t mask) {
  uint8_t* field_flags_ptr = const_cast<uint8_t*>(FindFieldData(dex_file, field));
  CHECK(field_flags_ptr != nullptr) << field;

    // Unroll this, as we only have three bytes, anyways.
  uint8_t base1 = static_cast<uint8_t>(mask & 0x7F);
  *(field_flags_ptr++) = (base1 | 0x80);
  mask >>= 7;

  uint8_t base2 = static_cast<uint8_t>(mask & 0x7F);
  *(field_flags_ptr++) = (base2 | 0x80);
  mask >>= 7;

  uint8_t base3 = static_cast<uint8_t>(mask & 0x7F);
  *field_flags_ptr = base3;
}

static uint32_t GetFieldFlags(DexFile* dex_file, const char* field) {
  const uint8_t* field_flags_ptr = const_cast<uint8_t*>(FindFieldData(dex_file, field));
  CHECK(field_flags_ptr != nullptr) << field;
  return DecodeUnsignedLeb128(&field_flags_ptr);
}

// Apply the given mask to method flags.
static void ApplyMaskToFieldFlags(DexFile* dex_file, const char* field, uint32_t mask) {
  uint32_t value = GetFieldFlags(dex_file, field);
  value &= mask;
  SetFieldFlags(dex_file, field, value);
}

// Apply the given mask to method flags.
static void OrMaskToFieldFlags(DexFile* dex_file, const char* field, uint32_t mask) {
  uint32_t value = GetFieldFlags(dex_file, field);
  value |= mask;
  SetFieldFlags(dex_file, field, value);
}

// Standard class. Use declared-synchronized again for 3B encoding.
//
// .class public LFieldFlags;
// .super Ljava/lang/Object;
//
// .field declared-synchronized public foo:I
//
// .field declared-synchronized public static bar:I

static const char kFieldFlagsTestDex[] =
    "ZGV4CjAzNQBtLw7hydbfv4TdXidZyzAB70W7w3vnYJRwAQAAcAAAAHhWNBIAAAAAAAAAAAABAAAF"
    "AAAAcAAAAAMAAACEAAAAAAAAAAAAAAACAAAAkAAAAAAAAAAAAAAAAQAAAKAAAACwAAAAwAAAAMAA"
    "AADDAAAA0QAAAOUAAADqAAAAAAAAAAEAAAACAAAAAQAAAAMAAAABAAAABAAAAAEAAAABAAAAAgAA"
    "AAAAAAD/////AAAAAPQAAAAAAAAAAUkADExGaWVsZEZsYWdzOwASTGphdmEvbGFuZy9PYmplY3Q7"
    "AANiYXIAA2ZvbwAAAAAAAAEBAAAAiYAIAYGACAkAAAAAAAAAAQAAAAAAAAABAAAABQAAAHAAAAAC"
    "AAAAAwAAAIQAAAAEAAAAAgAAAJAAAAAGAAAAAQAAAKAAAAACIAAABQAAAMAAAAADEAAAAQAAAPAA"
    "AAAAIAAAAQAAAPQAAAAAEAAAAQAAAAABAAA=";

TEST_F(DexFileVerifierTest, FieldAccessFlagsBase) {
  // Check that it's OK when the wrong declared-synchronized flag is removed from "foo."
  VerifyModification(
      kFieldFlagsTestDex,
      "field_flags_ok",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);
      },
      nullptr);
}

TEST_F(DexFileVerifierTest, FieldAccessFlagsWrongList) {
  // Mark the field so that it should appear in the opposite list (instance vs static).
  VerifyModification(
      kFieldFlagsTestDex,
      "field_flags_wrong_list",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

        OrMaskToFieldFlags(dex_file, "foo", kAccStatic);
      },
      "Static/instance field not in expected list");
  VerifyModification(
      kFieldFlagsTestDex,
      "field_flags_wrong_list",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
        ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "bar", ~kAccStatic);
      },
      "Static/instance field not in expected list");
}

TEST_F(DexFileVerifierTest, FieldAccessFlagsPPP) {
  static const char* kFields[] = { "foo", "bar" };
  for (size_t i = 0; i < arraysize(kFields); ++i) {
    // Should be OK to remove public.
    VerifyModification(
        kFieldFlagsTestDex,
        "field_flags_non_public",
        [&](DexFile* dex_file) {
          ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          ApplyMaskToFieldFlags(dex_file, kFields[i], ~kAccPublic);
        },
        nullptr);
    constexpr uint32_t kAccFlags = kAccPublic | kAccPrivate | kAccProtected;
    uint32_t bits = POPCOUNT(kAccFlags);
    for (uint32_t j = 1; j < (1u << bits); ++j) {
      if (POPCOUNT(j) < 2) {
        continue;
      }
      VerifyModification(
           kFieldFlagsTestDex,
           "field_flags_ppp",
           [&](DexFile* dex_file) {
             ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
             ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

             ApplyMaskToFieldFlags(dex_file, kFields[i], ~kAccPublic);
             uint32_t mask = ApplyMaskShifted(kAccFlags, j);
             OrMaskToFieldFlags(dex_file, kFields[i], mask);
           },
           "Field may have only one of public/protected/private");
    }
  }
}

TEST_F(DexFileVerifierTest, FieldAccessFlagsIgnoredOK) {
  constexpr const char* kFields[] = { "foo", "bar"};
  for (size_t i = 0; i < arraysize(kFields); ++i) {
    // All interesting method flags, other flags are to be ignored.
    constexpr uint32_t kAllFieldFlags =
        kAccPublic |
        kAccPrivate |
        kAccProtected |
        kAccStatic |
        kAccFinal |
        kAccVolatile |
        kAccTransient |
        kAccSynthetic |
        kAccEnum;
    constexpr uint32_t kIgnoredMask = ~kAllFieldFlags & 0xFFFF;
    VerifyModification(
        kFieldFlagsTestDex,
        "field_flags_ignored",
        [&](DexFile* dex_file) {
          ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToFieldFlags(dex_file, kFields[i], kIgnoredMask);
        },
        nullptr);
  }
}

TEST_F(DexFileVerifierTest, FieldAccessFlagsVolatileFinal) {
  constexpr const char* kFields[] = { "foo", "bar"};
  for (size_t i = 0; i < arraysize(kFields); ++i) {
    VerifyModification(
        kFieldFlagsTestDex,
        "field_flags_final_and_volatile",
        [&](DexFile* dex_file) {
          ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
          ApplyMaskToFieldFlags(dex_file, "bar", ~kAccDeclaredSynchronized);

          OrMaskToFieldFlags(dex_file, kFields[i], kAccVolatile | kAccFinal);
        },
        "Fields may not be volatile and final");
  }
}

// Standard interface. Needs to be separate from class as interfaces do not allow instance fields.
// Use declared-synchronized again for 3B encoding.
//
// .class public interface LInterfaceFieldFlags;
// .super Ljava/lang/Object;
//
// .field declared-synchronized public static final foo:I

static const char kFieldFlagsInterfaceTestDex[] =
    "ZGV4CjAzNQCVMHfEimR1zZPk6hl6O9GPAYqkl3u0umFkAQAAcAAAAHhWNBIAAAAAAAAAAPQAAAAE"
    "AAAAcAAAAAMAAACAAAAAAAAAAAAAAAABAAAAjAAAAAAAAAAAAAAAAQAAAJQAAACwAAAAtAAAALQA"
    "AAC3AAAAzgAAAOIAAAAAAAAAAQAAAAIAAAABAAAAAwAAAAEAAAABAgAAAgAAAAAAAAD/////AAAA"
    "AOwAAAAAAAAAAUkAFUxJbnRlcmZhY2VGaWVsZEZsYWdzOwASTGphdmEvbGFuZy9PYmplY3Q7AANm"
    "b28AAAAAAAABAAAAAJmACAkAAAAAAAAAAQAAAAAAAAABAAAABAAAAHAAAAACAAAAAwAAAIAAAAAE"
    "AAAAAQAAAIwAAAAGAAAAAQAAAJQAAAACIAAABAAAALQAAAADEAAAAQAAAOgAAAAAIAAAAQAAAOwA"
    "AAAAEAAAAQAAAPQAAAA=";

TEST_F(DexFileVerifierTest, FieldAccessFlagsInterface) {
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      nullptr);
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      nullptr);

  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_non_public",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_non_public",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
      },
      "Interface field is not public final static");

  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_non_final",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccFinal);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_non_final",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccFinal);
      },
      "Interface field is not public final static");

  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_protected",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToFieldFlags(dex_file, "foo", kAccProtected);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_protected",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToFieldFlags(dex_file, "foo", kAccProtected);
      },
      "Interface field is not public final static");

  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_private",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToFieldFlags(dex_file, "foo", kAccPrivate);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_private",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
        OrMaskToFieldFlags(dex_file, "foo", kAccPrivate);
      },
      "Interface field is not public final static");

  VerifyModification(
      kFieldFlagsInterfaceTestDex,
      "field_flags_interface_synthetic",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

        OrMaskToFieldFlags(dex_file, "foo", kAccSynthetic);
      },
      nullptr);

  constexpr uint32_t kAllFieldFlags =
      kAccPublic |
      kAccPrivate |
      kAccProtected |
      kAccStatic |
      kAccFinal |
      kAccVolatile |
      kAccTransient |
      kAccSynthetic |
      kAccEnum;
  constexpr uint32_t kInterfaceFieldFlags = kAccPublic | kAccStatic | kAccFinal | kAccSynthetic;
  constexpr uint32_t kInterfaceDisallowed = kAllFieldFlags &
                                            ~kInterfaceFieldFlags &
                                            ~kAccProtected &
                                            ~kAccPrivate;
  static_assert(kInterfaceDisallowed != 0, "There should be disallowed flags.");

  uint32_t bits = POPCOUNT(kInterfaceDisallowed);
  for (uint32_t i = 1; i < (1u << bits); ++i) {
    VerifyModification(
        kFieldFlagsInterfaceTestDex,
        "field_flags_interface_disallowed",
        [&](DexFile* dex_file) {
          ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

          uint32_t mask = ApplyMaskShifted(kInterfaceDisallowed, i);
          if ((mask & kAccProtected) != 0) {
            mask &= ~kAccProtected;
            ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
          }
          OrMaskToFieldFlags(dex_file, "foo", mask);
        },
        nullptr);  // Should be allowed in older dex versions for backwards compatibility.
    VerifyModification(
        kFieldFlagsInterfaceTestDex,
        "field_flags_interface_disallowed",
        [&](DexFile* dex_file) {
          MakeDexVersion37(dex_file);
          ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);

          uint32_t mask = ApplyMaskShifted(kInterfaceDisallowed, i);
          if ((mask & kAccProtected) != 0) {
            mask &= ~kAccProtected;
            ApplyMaskToFieldFlags(dex_file, "foo", ~kAccPublic);
          }
          OrMaskToFieldFlags(dex_file, "foo", mask);
        },
        "Interface field has disallowed flag");
  }
}

// Standard bad interface. Needs to be separate from class as interfaces do not allow instance
// fields. Use declared-synchronized again for 3B encoding.
//
// .class public interface LInterfaceFieldFlags;
// .super Ljava/lang/Object;
//
// .field declared-synchronized public final foo:I

static const char kFieldFlagsInterfaceBadTestDex[] =
    "ZGV4CjAzNQByMUnqYKHBkUpvvNp+9CnZ2VyDkKnRN6VkAQAAcAAAAHhWNBIAAAAAAAAAAPQAAAAE"
    "AAAAcAAAAAMAAACAAAAAAAAAAAAAAAABAAAAjAAAAAAAAAAAAAAAAQAAAJQAAACwAAAAtAAAALQA"
    "AAC3AAAAzgAAAOIAAAAAAAAAAQAAAAIAAAABAAAAAwAAAAEAAAABAgAAAgAAAAAAAAD/////AAAA"
    "AOwAAAAAAAAAAUkAFUxJbnRlcmZhY2VGaWVsZEZsYWdzOwASTGphdmEvbGFuZy9PYmplY3Q7AANm"
    "b28AAAAAAAAAAQAAAJGACAkAAAAAAAAAAQAAAAAAAAABAAAABAAAAHAAAAACAAAAAwAAAIAAAAAE"
    "AAAAAQAAAIwAAAAGAAAAAQAAAJQAAAACIAAABAAAALQAAAADEAAAAQAAAOgAAAAAIAAAAQAAAOwA"
    "AAAAEAAAAQAAAPQAAAA=";

TEST_F(DexFileVerifierTest, FieldAccessFlagsInterfaceNonStatic) {
  VerifyModification(
      kFieldFlagsInterfaceBadTestDex,
      "field_flags_interface_non_static",
      [](DexFile* dex_file) {
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      nullptr);  // Should be allowed in older dex versions for backwards compatibility.
  VerifyModification(
      kFieldFlagsInterfaceBadTestDex,
      "field_flags_interface_non_static",
      [](DexFile* dex_file) {
        MakeDexVersion37(dex_file);
        ApplyMaskToFieldFlags(dex_file, "foo", ~kAccDeclaredSynchronized);
      },
      "Interface field is not public final static");
}

// Generated from:
//
// .class public LTest;
// .super Ljava/lang/Object;
// .source "Test.java"
//
// .method public constructor <init>()V
//     .registers 1
//
//     .prologue
//     .line 1
//     invoke-direct {p0}, Ljava/lang/Object;-><init>()V
//
//     return-void
// .end method
//
// .method public static main()V
//     .registers 2
//
//     const-string v0, "a"
//     const-string v0, "b"
//     const-string v0, "c"
//     const-string v0, "d"
//     const-string v0, "e"
//     const-string v0, "f"
//     const-string v0, "g"
//     const-string v0, "h"
//     const-string v0, "i"
//     const-string v0, "j"
//     const-string v0, "k"
//
//     .local v1, "local_var":Ljava/lang/String;
//     const-string v1, "test"
// .end method

static const char kDebugInfoTestDex[] =
    "ZGV4CjAzNQCHRkHix2eIMQgvLD/0VGrlllZLo0Rb6VyUAgAAcAAAAHhWNBIAAAAAAAAAAAwCAAAU"
    "AAAAcAAAAAQAAADAAAAAAQAAANAAAAAAAAAAAAAAAAMAAADcAAAAAQAAAPQAAACAAQAAFAEAABQB"
    "AAAcAQAAJAEAADgBAABMAQAAVwEAAFoBAABdAQAAYAEAAGMBAABmAQAAaQEAAGwBAABvAQAAcgEA"
    "AHUBAAB4AQAAewEAAIYBAACMAQAAAQAAAAIAAAADAAAABQAAAAUAAAADAAAAAAAAAAAAAAAAAAAA"
    "AAAAABIAAAABAAAAAAAAAAAAAAABAAAAAQAAAAAAAAAEAAAAAAAAAPwBAAAAAAAABjxpbml0PgAG"
    "TFRlc3Q7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAJVGVzdC5qYXZh"
    "AAFWAAFhAAFiAAFjAAFkAAFlAAFmAAFnAAFoAAFpAAFqAAFrAAlsb2NhbF92YXIABG1haW4ABHRl"
    "c3QAAAABAAcOAAAAARYDARIDAAAAAQABAAEAAACUAQAABAAAAHAQAgAAAA4AAgAAAAAAAACZAQAA"
    "GAAAABoABgAaAAcAGgAIABoACQAaAAoAGgALABoADAAaAA0AGgAOABoADwAaABAAGgETAAAAAgAA"
    "gYAEpAMBCbwDAAALAAAAAAAAAAEAAAAAAAAAAQAAABQAAABwAAAAAgAAAAQAAADAAAAAAwAAAAEA"
    "AADQAAAABQAAAAMAAADcAAAABgAAAAEAAAD0AAAAAiAAABQAAAAUAQAAAyAAAAIAAACUAQAAASAA"
    "AAIAAACkAQAAACAAAAEAAAD8AQAAABAAAAEAAAAMAgAA";

TEST_F(DexFileVerifierTest, DebugInfoTypeIdxTest) {
  {
    // The input dex file should be good before modification.
    std::string error_msg;
    std::unique_ptr<const DexFile> raw(OpenDexFileBase64(kDebugInfoTestDex,
                                                         kLocationString,
                                                         &error_msg));
    ASSERT_TRUE(raw.get() != nullptr) << error_msg;
  }

  // Modify the debug information entry.
  VerifyModification(
      kDebugInfoTestDex,
      "debug_start_type_idx",
      [](DexFile* dex_file) {
        *(const_cast<uint8_t*>(dex_file->Begin()) + 416) = 0x14U;
      },
      "DBG_START_LOCAL type_idx");
}

TEST_F(DexFileVerifierTest, SectionAlignment) {
  {
    // The input dex file should be good before modification. Any file is fine, as long as it
    // uses all sections.
    std::string error_msg;
    std::unique_ptr<const DexFile> raw(OpenDexFileBase64(kGoodTestDex,
                                                         kLocationString,
                                                         &error_msg));
    ASSERT_TRUE(raw.get() != nullptr) << error_msg;
  }

  // Modify all section offsets to be unaligned.
  constexpr size_t kSections = 7;
  for (size_t i = 0; i < kSections; ++i) {
    VerifyModification(
        kGoodTestDex,
        "section_align",
        [&](DexFile* dex_file) {
          DexFile::Header* header = const_cast<DexFile::Header*>(
              reinterpret_cast<const DexFile::Header*>(dex_file->Begin()));
          uint32_t* off_ptr;
          switch (i) {
            case 0:
              off_ptr = &header->map_off_;
              break;
            case 1:
              off_ptr = &header->string_ids_off_;
              break;
            case 2:
              off_ptr = &header->type_ids_off_;
              break;
            case 3:
              off_ptr = &header->proto_ids_off_;
              break;
            case 4:
              off_ptr = &header->field_ids_off_;
              break;
            case 5:
              off_ptr = &header->method_ids_off_;
              break;
            case 6:
              off_ptr = &header->class_defs_off_;
              break;

            static_assert(kSections == 7, "kSections is wrong");
            default:
              LOG(FATAL) << "Unexpected section";
              UNREACHABLE();
          }
          ASSERT_TRUE(off_ptr != nullptr);
          ASSERT_NE(*off_ptr, 0U) << i;  // Should already contain a value (in use).
          (*off_ptr)++;                  // Add one, which should misalign it (all the sections
                                         // above are aligned by 4).
        },
        "should be aligned by 4 for");
  }
}

// Generated from
//
// .class LOverloading;
//
// .super Ljava/lang/Object;
//
// .method public static foo()V
// .registers 1
//     return-void
// .end method
//
// .method public static foo(I)V
// .registers 1
//     return-void
// .end method
static const char kProtoOrderingTestDex[] =
    "ZGV4CjAzNQA1L+ABE6voQ9Lr4Ci//efB53oGnDr5PinsAQAAcAAAAHhWNBIAAAAAAAAAAFgBAAAG"
    "AAAAcAAAAAQAAACIAAAAAgAAAJgAAAAAAAAAAAAAAAIAAACwAAAAAQAAAMAAAAAMAQAA4AAAAOAA"
    "AADjAAAA8gAAAAYBAAAJAQAADQEAAAAAAAABAAAAAgAAAAMAAAADAAAAAwAAAAAAAAAEAAAAAwAA"
    "ABQBAAABAAAABQAAAAEAAQAFAAAAAQAAAAAAAAACAAAAAAAAAP////8AAAAASgEAAAAAAAABSQAN"
    "TE92ZXJsb2FkaW5nOwASTGphdmEvbGFuZy9PYmplY3Q7AAFWAAJWSQADZm9vAAAAAQAAAAAAAAAA"
    "AAAAAAAAAAEAAAAAAAAAAAAAAAEAAAAOAAAAAQABAAAAAAAAAAAAAQAAAA4AAAACAAAJpAIBCbgC"
    "AAAMAAAAAAAAAAEAAAAAAAAAAQAAAAYAAABwAAAAAgAAAAQAAACIAAAAAwAAAAIAAACYAAAABQAA"
    "AAIAAACwAAAABgAAAAEAAADAAAAAAiAAAAYAAADgAAAAARAAAAEAAAAUAQAAAxAAAAIAAAAcAQAA"
    "ASAAAAIAAAAkAQAAACAAAAEAAABKAQAAABAAAAEAAABYAQAA";

TEST_F(DexFileVerifierTest, ProtoOrdering) {
  {
    // The input dex file should be good before modification.
    std::string error_msg;
    std::unique_ptr<const DexFile> raw(OpenDexFileBase64(kProtoOrderingTestDex,
                                                         kLocationString,
                                                         &error_msg));
    ASSERT_TRUE(raw.get() != nullptr) << error_msg;
  }

  // Modify the order of the ProtoIds for two overloads of "foo" with the
  // same return type and one having longer parameter list than the other.
  for (size_t i = 0; i != 2; ++i) {
    VerifyModification(
        kProtoOrderingTestDex,
        "proto_ordering",
        [i](DexFile* dex_file) {
          uint32_t method_idx;
          const uint8_t* data = FindMethodData(dex_file, "foo", &method_idx);
          CHECK(data != nullptr);
          // There should be 2 methods called "foo".
          CHECK_LT(method_idx + 1u, dex_file->NumMethodIds());
          CHECK_EQ(dex_file->GetMethodId(method_idx).name_idx_,
                   dex_file->GetMethodId(method_idx + 1).name_idx_);
          CHECK_EQ(dex_file->GetMethodId(method_idx).proto_idx_ + 1u,
                   dex_file->GetMethodId(method_idx + 1).proto_idx_);
          // Their return types should be the same.
          uint32_t proto1_idx = dex_file->GetMethodId(method_idx).proto_idx_;
          const DexFile::ProtoId& proto1 = dex_file->GetProtoId(proto1_idx);
          const DexFile::ProtoId& proto2 = dex_file->GetProtoId(proto1_idx + 1u);
          CHECK_EQ(proto1.return_type_idx_, proto2.return_type_idx_);
          // And the first should not have any parameters while the second should have some.
          CHECK(!DexFileParameterIterator(*dex_file, proto1).HasNext());
          CHECK(DexFileParameterIterator(*dex_file, proto2).HasNext());
          if (i == 0) {
            // Swap the proto parameters and shorties to break the ordering.
            std::swap(const_cast<uint32_t&>(proto1.parameters_off_),
                      const_cast<uint32_t&>(proto2.parameters_off_));
            std::swap(const_cast<dex::StringIndex&>(proto1.shorty_idx_),
                      const_cast<dex::StringIndex&>(proto2.shorty_idx_));
          } else {
            // Copy the proto parameters and shorty to create duplicate proto id.
            const_cast<uint32_t&>(proto1.parameters_off_) = proto2.parameters_off_;
            const_cast<dex::StringIndex&>(proto1.shorty_idx_) = proto2.shorty_idx_;
          }
        },
        "Out-of-order proto_id arguments");
  }
}

// To generate a base64 encoded Dex file version 037 from Smali files, use:
//
//   smali assemble --api 24 -o classes.dex class1.smali [class2.smali ...]
//   base64 classes.dex >classes.dex.base64

// Dex file version 037 generated from:
//
//   .class public LB28685551;
//   .super LB28685551;

static const char kClassExtendsItselfTestDex[] =
    "ZGV4CjAzNwDeGbgRg1kb6swszpcTWrrOAALB++F4OPT0AAAAcAAAAHhWNBIAAAAAAAAAAKgAAAAB"
    "AAAAcAAAAAEAAAB0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAHgAAABcAAAAmAAAAJgA"
    "AAAAAAAAAAAAAAEAAAAAAAAAAAAAAP////8AAAAAAAAAAAAAAAALTEIyODY4NTU1MTsAAAAABgAA"
    "AAAAAAABAAAAAAAAAAEAAAABAAAAcAAAAAIAAAABAAAAdAAAAAYAAAABAAAAeAAAAAIgAAABAAAA"
    "mAAAAAAQAAABAAAAqAAAAA==";

TEST_F(DexFileVerifierTest, ClassExtendsItself) {
  VerifyModification(
      kClassExtendsItselfTestDex,
      "class_extends_itself",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Class with same type idx as its superclass: '0'");
}

// Dex file version 037 generated from:
//
//   .class public LFoo;
//   .super LBar;
//
// and:
//
//    .class public LBar;
//    .super LFoo;

static const char kClassesExtendOneAnotherTestDex[] =
    "ZGV4CjAzNwBXHSrwpDMwRBkg+L+JeQCuFNRLhQ86duEcAQAAcAAAAHhWNBIAAAAAAAAAANAAAAAC"
    "AAAAcAAAAAIAAAB4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAIAAAABcAAAAwAAAAMAA"
    "AADHAAAAAAAAAAEAAAABAAAAAQAAAAAAAAAAAAAA/////wAAAAAAAAAAAAAAAAAAAAABAAAAAQAA"
    "AAAAAAD/////AAAAAAAAAAAAAAAABUxCYXI7AAVMRm9vOwAAAAYAAAAAAAAAAQAAAAAAAAABAAAA"
    "AgAAAHAAAAACAAAAAgAAAHgAAAAGAAAAAgAAAIAAAAACIAAAAgAAAMAAAAAAEAAAAQAAANAAAAA=";

TEST_F(DexFileVerifierTest, ClassesExtendOneAnother) {
  VerifyModification(
      kClassesExtendOneAnotherTestDex,
      "classes_extend_one_another",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Invalid class definition ordering: class with type idx: '1' defined before"
      " superclass with type idx: '0'");
}

// Dex file version 037 generated from:
//
//   .class public LAll;
//   .super LYour;
//
// and:
//
//   .class public LYour;
//   .super LBase;
//
// and:
//
//   .class public LBase;
//   .super LAll;

static const char kCircularClassInheritanceTestDex[] =
    "ZGV4CjAzNwBMJxgP0SJz6oLXnKfl+J7lSEORLRwF5LNMAQAAcAAAAHhWNBIAAAAAAAAAAAABAAAD"
    "AAAAcAAAAAMAAAB8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwAAAIgAAABkAAAA6AAAAOgA"
    "AADvAAAA9wAAAAAAAAABAAAAAgAAAAEAAAABAAAAAAAAAAAAAAD/////AAAAAAAAAAAAAAAAAgAA"
    "AAEAAAABAAAAAAAAAP////8AAAAAAAAAAAAAAAAAAAAAAQAAAAIAAAAAAAAA/////wAAAAAAAAAA"
    "AAAAAAVMQWxsOwAGTEJhc2U7AAZMWW91cjsAAAYAAAAAAAAAAQAAAAAAAAABAAAAAwAAAHAAAAAC"
    "AAAAAwAAAHwAAAAGAAAAAwAAAIgAAAACIAAAAwAAAOgAAAAAEAAAAQAAAAABAAA=";

TEST_F(DexFileVerifierTest, CircularClassInheritance) {
  VerifyModification(
      kCircularClassInheritanceTestDex,
      "circular_class_inheritance",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Invalid class definition ordering: class with type idx: '1' defined before"
      " superclass with type idx: '0'");
}

// Dex file version 037 generated from:
//
//   .class public abstract interface LInterfaceImplementsItself;
//   .super Ljava/lang/Object;
//   .implements LInterfaceImplementsItself;

static const char kInterfaceImplementsItselfTestDex[] =
    "ZGV4CjAzNwCKKrjatp8XbXl5S/bEVJnqaBhjZkQY4440AQAAcAAAAHhWNBIAAAAAAAAAANwAAAAC"
    "AAAAcAAAAAIAAAB4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAIAAAACUAAAAoAAAAKAA"
    "AAC9AAAAAAAAAAEAAAAAAAAAAQYAAAEAAADUAAAA/////wAAAAAAAAAAAAAAABtMSW50ZXJmYWNl"
    "SW1wbGVtZW50c0l0c2VsZjsAEkxqYXZhL2xhbmcvT2JqZWN0OwAAAAABAAAAAAAAAAcAAAAAAAAA"
    "AQAAAAAAAAABAAAAAgAAAHAAAAACAAAAAgAAAHgAAAAGAAAAAQAAAIAAAAACIAAAAgAAAKAAAAAB"
    "EAAAAQAAANQAAAAAEAAAAQAAANwAAAA=";

TEST_F(DexFileVerifierTest, InterfaceImplementsItself) {
  VerifyModification(
      kInterfaceImplementsItselfTestDex,
      "interface_implements_itself",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Class with same type idx as implemented interface: '0'");
}

// Dex file version 037 generated from:
//
//   .class public abstract interface LPing;
//   .super Ljava/lang/Object;
//   .implements LPong;
//
// and:
//
//   .class public abstract interface LPong;
//   .super Ljava/lang/Object;
//   .implements LPing;

static const char kInterfacesImplementOneAnotherTestDex[] =
    "ZGV4CjAzNwD0Kk9sxlYdg3Dy1Cff0gQCuJAQfEP6ohZUAQAAcAAAAHhWNBIAAAAAAAAAAPwAAAAD"
    "AAAAcAAAAAMAAAB8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAIgAAACMAAAAyAAAAMgA"
    "AADQAAAA2AAAAAAAAAABAAAAAgAAAAEAAAABBgAAAgAAAOwAAAD/////AAAAAAAAAAAAAAAAAAAA"
    "AAEGAAACAAAA9AAAAP////8AAAAAAAAAAAAAAAAGTFBpbmc7AAZMUG9uZzsAEkxqYXZhL2xhbmcv"
    "T2JqZWN0OwABAAAAAAAAAAEAAAABAAAABwAAAAAAAAABAAAAAAAAAAEAAAADAAAAcAAAAAIAAAAD"
    "AAAAfAAAAAYAAAACAAAAiAAAAAIgAAADAAAAyAAAAAEQAAACAAAA7AAAAAAQAAABAAAA/AAAAA==";

TEST_F(DexFileVerifierTest, InterfacesImplementOneAnother) {
  VerifyModification(
      kInterfacesImplementOneAnotherTestDex,
      "interfaces_implement_one_another",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Invalid class definition ordering: class with type idx: '1' defined before"
      " implemented interface with type idx: '0'");
}

// Dex file version 037 generated from:
//
//   .class public abstract interface LA;
//   .super Ljava/lang/Object;
//   .implements LB;
//
// and:
//
//   .class public abstract interface LB;
//   .super Ljava/lang/Object;
//   .implements LC;
//
// and:
//
//   .class public abstract interface LC;
//   .super Ljava/lang/Object;
//   .implements LA;

static const char kCircularInterfaceImplementationTestDex[] =
    "ZGV4CjAzNwCzKmD5Fol6XAU6ichYHcUTIP7Z7MdTcEmEAQAAcAAAAHhWNBIAAAAAAAAAACwBAAAE"
    "AAAAcAAAAAQAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwAAAJAAAACUAAAA8AAAAPAA"
    "AAD1AAAA+gAAAP8AAAAAAAAAAQAAAAIAAAADAAAAAgAAAAEGAAADAAAAHAEAAP////8AAAAAAAAA"
    "AAAAAAABAAAAAQYAAAMAAAAUAQAA/////wAAAAAAAAAAAAAAAAAAAAABBgAAAwAAACQBAAD/////"
    "AAAAAAAAAAAAAAAAA0xBOwADTEI7AANMQzsAEkxqYXZhL2xhbmcvT2JqZWN0OwAAAQAAAAIAAAAB"
    "AAAAAAAAAAEAAAABAAAABwAAAAAAAAABAAAAAAAAAAEAAAAEAAAAcAAAAAIAAAAEAAAAgAAAAAYA"
    "AAADAAAAkAAAAAIgAAAEAAAA8AAAAAEQAAADAAAAFAEAAAAQAAABAAAALAEAAA==";

TEST_F(DexFileVerifierTest, CircularInterfaceImplementation) {
  VerifyModification(
      kCircularInterfaceImplementationTestDex,
      "circular_interface_implementation",
      [](DexFile* dex_file ATTRIBUTE_UNUSED) { /* empty */ },
      "Invalid class definition ordering: class with type idx: '2' defined before"
      " implemented interface with type idx: '0'");
}

TEST_F(DexFileVerifierTest, Checksum) {
  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kGoodTestDex, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;

  // Good checksum: all pass.
  EXPECT_TRUE(DexFileVerifier::Verify(dex_file.get(),
                                      dex_file->Begin(),
                                      dex_file->Size(),
                                       "good checksum, no verify",
                                      /*verify_checksum*/ false,
                                      &error_msg));
  EXPECT_TRUE(DexFileVerifier::Verify(dex_file.get(),
                                      dex_file->Begin(),
                                      dex_file->Size(),
                                      "good checksum, verify",
                                      /*verify_checksum*/ true,
                                      &error_msg));

  // Bad checksum: !verify_checksum passes verify_checksum fails.
  DexFile::Header* header = reinterpret_cast<DexFile::Header*>(
      const_cast<uint8_t*>(dex_file->Begin()));
  header->checksum_ = 0;
  EXPECT_TRUE(DexFileVerifier::Verify(dex_file.get(),
                                      dex_file->Begin(),
                                      dex_file->Size(),
                                      "bad checksum, no verify",
                                      /*verify_checksum*/ false,
                                      &error_msg));
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad checksum, verify",
                                       /*verify_checksum*/ true,
                                       &error_msg));
  EXPECT_NE(error_msg.find("Bad checksum"), std::string::npos) << error_msg;
}

TEST_F(DexFileVerifierTest, BadStaticMethodName) {
  // Generated DEX file version (037) from:
  //
  // .class public LBadName;
  // .super Ljava/lang/Object;
  //
  // .method public static <bad_name> (II)V
  //    .registers 2
  //    .prologue
  //    return-void
  // .end method
  //
  // .method public constructor <init>()V
  //     .registers 1
  //     .prologue
  //     .line 1
  // invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //     return-void
  // .end method
  //
  static const char kDexBase64[] =
      "ZGV4CjAzNwC2NYlwyxEc/h6hv+hMeUVQPtiX6MQBcfgwAgAAcAAAAHhWNBIAAAAAAAAAAJABAAAI"
      "AAAAcAAAAAQAAACQAAAAAgAAAKAAAAAAAAAAAAAAAAMAAAC4AAAAAQAAANAAAABAAQAA8AAAAPAA"
      "AAD8AAAABAEAABIBAAAVAQAAIAEAADQBAAA3AQAAAwAAAAQAAAAFAAAABgAAAAYAAAADAAAAAAAA"
      "AAcAAAADAAAAPAEAAAEAAQAAAAAAAQAAAAEAAAACAAAAAQAAAAEAAAABAAAAAgAAAAAAAAACAAAA"
      "AAAAAIABAAAAAAAACjxiYWRfbmFtZT4ABjxpbml0PgAMQmFkTmFtZS5qYXZhAAFJAAlMQmFkTmFt"
      "ZTsAEkxqYXZhL2xhbmcvT2JqZWN0OwABVgADVklJAAIAAAAAAAAAAAAAAAACAAAHAAEABw4AAAIA"
      "AgAAAAAASAEAAAEAAAAOAAAAAQABAAEAAABOAQAABAAAAHAQAgAAAA4AAAACAAAJ1AIBgYAE6AIA"
      "AA0AAAAAAAAAAQAAAAAAAAABAAAACAAAAHAAAAACAAAABAAAAJAAAAADAAAAAgAAAKAAAAAFAAAA"
      "AwAAALgAAAAGAAAAAQAAANAAAAACIAAACAAAAPAAAAABEAAAAQAAADwBAAADEAAAAQAAAEQBAAAD"
      "IAAAAgAAAEgBAAABIAAAAgAAAFQBAAAAIAAAAQAAAIABAAAAEAAAAQAAAJABAAA=";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad static method name",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

TEST_F(DexFileVerifierTest, BadVirtualMethodName) {
  // Generated DEX file version (037) from:
  //
  //  .class public LBadVirtualName;
  //  .super Ljava/lang/Object;
  //
  //  .method public <bad_name> (II)V
  //     .registers 2
  //     return-void
  //  .end method
  //
  //  .method public constructor <init>()V
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      return-void
  //  .end method
  //
  static const char kDexBase64[] =
      "ZGV4CjAzNwDcPC8B2E7kYTZmeHX2u2IqrpWV9EXBHpE8AgAAcAAAAHhWNBIAAAAAAAAAAJwBAAAI"
      "AAAAcAAAAAQAAACQAAAAAgAAAKAAAAAAAAAAAAAAAAMAAAC4AAAAAQAAANAAAABMAQAA8AAAAPAA"
      "AAD8AAAABAEAABkBAAAcAQAALgEAAEIBAABFAQAAAwAAAAQAAAAFAAAABgAAAAYAAAADAAAAAAAA"
      "AAcAAAADAAAATAEAAAEAAQAAAAAAAQAAAAEAAAACAAAAAQAAAAEAAAABAAAAAgAAAAAAAAACAAAA"
      "AAAAAI4BAAAAAAAACjxiYWRfbmFtZT4ABjxpbml0PgATQmFkVmlydHVhbE5hbWUuamF2YQABSQAQ"
      "TEJhZFZpcnR1YWxOYW1lOwASTGphdmEvbGFuZy9PYmplY3Q7AAFWAANWSUkAAAACAAAAAAAAAAAA"
      "AAABAAcOAAACAAAHAAABAAEAAQAAAFgBAAAEAAAAcBACAAAADgADAAMAAAAAAF0BAAABAAAADgAA"
      "AAEBAYGABOQCAAH8Ag0AAAAAAAAAAQAAAAAAAAABAAAACAAAAHAAAAACAAAABAAAAJAAAAADAAAA"
      "AgAAAKAAAAAFAAAAAwAAALgAAAAGAAAAAQAAANAAAAACIAAACAAAAPAAAAABEAAAAQAAAEwBAAAD"
      "EAAAAQAAAFQBAAADIAAAAgAAAFgBAAABIAAAAgAAAGQBAAAAIAAAAQAAAI4BAAAAEAAAAQAAAJwB"
      "AAA=";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad virtual method name",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

TEST_F(DexFileVerifierTest, BadClinitSignature) {
  // Generated DEX file version (037) from:
  //
  //  .class public LOneClinitBadSig;
  //  .super Ljava/lang/Object;
  //
  //  .method public static constructor <clinit>(II)V
  //     .registers 2
  //     return-void
  //  .end method
  //
  //  .method public constructor <init>()V
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      return-void
  //  .end method
  //
  static const char kDexBase64[] =
      "ZGV4CjAzNwBNOwTbfJmWq5eMOlxUY4EICGiEGJMVg8RAAgAAcAAAAHhWNBIAAAAAAAAAAKABAAAI"
      "AAAAcAAAAAQAAACQAAAAAgAAAKAAAAAAAAAAAAAAAAMAAAC4AAAAAQAAANAAAABQAQAA8AAAAPAA"
      "AAD6AAAAAgEAAAUBAAAYAQAALAEAAEIBAABFAQAAAgAAAAMAAAAEAAAABgAAAAYAAAADAAAAAAAA"
      "AAcAAAADAAAATAEAAAEAAQAAAAAAAQAAAAEAAAACAAAAAQAAAAEAAAABAAAAAgAAAAAAAAAFAAAA"
      "AAAAAJABAAAAAAAACDxjbGluaXQ+AAY8aW5pdD4AAUkAEUxPbmVDbGluaXRCYWRTaWc7ABJMamF2"
      "YS9sYW5nL09iamVjdDsAFE9uZUNsaW5pdEJhZFNpZy5qYXZhAAFWAANWSUkAAAACAAAAAAAAAAAA"
      "AAAAAgAABwABAAcOAAACAAIAAAAAAFgBAAABAAAADgAAAAEAAQABAAAAXgEAAAQAAABwEAIAAAAO"
      "AAAAAgAAiYAE5AIBgYAE+AINAAAAAAAAAAEAAAAAAAAAAQAAAAgAAABwAAAAAgAAAAQAAACQAAAA"
      "AwAAAAIAAACgAAAABQAAAAMAAAC4AAAABgAAAAEAAADQAAAAAiAAAAgAAADwAAAAARAAAAEAAABM"
      "AQAAAxAAAAEAAABUAQAAAyAAAAIAAABYAQAAASAAAAIAAABkAQAAACAAAAEAAACQAQAAABAAAAEA"
      "AACgAQAA";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad clinit signature",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

TEST_F(DexFileVerifierTest, BadClinitSignatureAgain) {
  // Generated DEX file version (037) from:
  //
  //  .class public LOneClinitBadSigAgain;
  //  .super Ljava/lang/Object;
  //
  //  .method public static constructor <clinit>()I
  //     .registers 1
  //     const/4 v0, 1
  //     return v0
  //  .end method
  //
  //  .method public constructor <init>()V
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      return-void
  //  .end method
  //
  static const char kDexBase64[] =
      "ZGV4CjAzNwBfPcPu5NVwKUqZIu/YR8xqVlVD5UzTk0gEAgAAcAAAAHhWNBIAAAAAAAAAAIgBAAAH"
      "AAAAcAAAAAQAAACMAAAAAgAAAJwAAAAAAAAAAAAAAAMAAAC0AAAAAQAAAMwAAAAYAQAA7AAAAOwA"
      "AAD2AAAA/gAAAAEBAAAZAQAALQEAAEgBAAACAAAAAwAAAAQAAAAGAAAAAgAAAAAAAAAAAAAABgAA"
      "AAMAAAAAAAAAAQAAAAAAAAABAAEAAQAAAAIAAQABAAAAAQAAAAEAAAACAAAAAAAAAAUAAAAAAAAA"
      "eAEAAAAAAAAIPGNsaW5pdD4ABjxpbml0PgABSQAWTE9uZUNsaW5pdEJhZFNpZ0FnYWluOwASTGph"
      "dmEvbGFuZy9PYmplY3Q7ABlPbmVDbGluaXRCYWRTaWdBZ2Fpbi5qYXZhAAFWAAABAAAAAAAAAAAA"
      "AAACAAAAEhAPAAEAAQABAAAAAAAAAAQAAABwEAIAAAAOAAAAAgAAiYAEzAIBgYAE4AIKAAAAAAAA"
      "AAEAAAAAAAAAAQAAAAcAAABwAAAAAgAAAAQAAACMAAAAAwAAAAIAAACcAAAABQAAAAMAAAC0AAAA"
      "BgAAAAEAAADMAAAAAiAAAAcAAADsAAAAASAAAAIAAABMAQAAACAAAAEAAAB4AQAAABAAAAEAAACI"
      "AQAA";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad clinit signature",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

TEST_F(DexFileVerifierTest, BadInitSignature) {
  // Generated DEX file version (037) from:
  //
  //  .class public LBadInitSig;
  //  .super Ljava/lang/Object;
  //
  //  .method public constructor <init>()I
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      const v0, 1
  //      return v0
  //  .end method
  //
  static const char kDexBase64[] =
      "ZGV4CjAzNwCdMdeh1KoHWamF2Prq32LF39YZ78fV7q+wAQAAcAAAAHhWNBIAAAAAAAAAADQBAAAF"
      "AAAAcAAAAAQAAACEAAAAAgAAAJQAAAAAAAAAAAAAAAIAAACsAAAAAQAAALwAAADUAAAA3AAAANwA"
      "AADkAAAA5wAAAPUAAAAJAQAAAQAAAAIAAAADAAAABAAAAAEAAAAAAAAAAAAAAAQAAAADAAAAAAAA"
      "AAEAAAAAAAAAAgABAAAAAAABAAAAAQAAAAIAAAAAAAAA/////wAAAAAqAQAAAAAAAAY8aW5pdD4A"
      "AUkADExCYWRJbml0U2lnOwASTGphdmEvbGFuZy9PYmplY3Q7AAFWAAEAAQABAAAAAAAAAAcAAABw"
      "EAEAAAAUAAEAAAAPAAAAAQAAgYAEjAIKAAAAAAAAAAEAAAAAAAAAAQAAAAUAAABwAAAAAgAAAAQA"
      "AACEAAAAAwAAAAIAAACUAAAABQAAAAIAAACsAAAABgAAAAEAAAC8AAAAAiAAAAUAAADcAAAAASAA"
      "AAEAAAAMAQAAACAAAAEAAAAqAQAAABAAAAEAAAA0AQAA";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad init signature",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

static const char* kInvokeCustomDexFiles[] = {
  // TODO(oth): Revisit this test when we have smali / dx support.
  // https://cs.corp.google.com/android/toolchain/jack/jack-tests/tests/com/android/jack/java7/invokecustom/test001/Tests.java
  "ZGV4CjAzOAAEj12s/acmmdGuDL92SWSBh6iLBjxgomWkCAAAcAAAAHhWNBIAAAAAAAAAALwHAAAx"
  "AAAAcAAAABYAAAA0AQAACQAAAIwBAAADAAAA+AEAAAsAAAAQAgAAAQAAAHACAAAMBgAAmAIAAMID"
  "AADKAwAAzQMAANIDAADhAwAA5AMAAOoDAAAfBAAAUgQAAIMEAAC4BAAA1AQAAOsEAAD+BAAAEgUA"
  "ACYFAAA6BQAAUQUAAG4FAACTBQAAtAUAAN0FAAD/BQAAHgYAADgGAABKBgAAVgYAAFkGAABdBgAA"
  "YgYAAGYGAAB7BgAAgAYAAI8GAACdBgAAtAYAAMMGAADSBgAA3gYAAPIGAAD4BgAABgcAAA4HAAAU"
  "BwAAGgcAAB8HAAAoBwAANAcAADoHAAABAAAABgAAAAcAAAAIAAAACQAAAAoAAAALAAAADAAAAA0A"
  "AAAOAAAADwAAABAAAAARAAAAEgAAABMAAAAUAAAAFQAAABYAAAAXAAAAGAAAABoAAAAeAAAAAgAA"
  "AAAAAACMAwAABQAAAAwAAACUAwAABQAAAA4AAACgAwAABAAAAA8AAAAAAAAAGgAAABQAAAAAAAAA"
  "GwAAABQAAACsAwAAHAAAABQAAACMAwAAHQAAABQAAAC0AwAAHQAAABQAAAC8AwAAAwADAAMAAAAE"
  "AAwAJAAAAAoABgAsAAAABAAEAAAAAAAEAAAAHwAAAAQAAQAoAAAABAAIACoAAAAEAAQALwAAAAYA"
  "BQAtAAAACAAEAAAAAAANAAcAAAAAAA8AAgAlAAAAEAADACkAAAASAAYAIQAAAJYHAACWBwAABAAA"
  "AAEAAAAIAAAAAAAAABkAAABkAwAAnQcAAAAAAAAEAAAAAgAAAAEAAABjBwAAAQAAAIsHAAACAAAA"
  "iwcAAJMHAAABAAEAAQAAAEEHAAAEAAAAcBAGAAAADgADAAIAAAAAAEYHAAADAAAAkAABAg8AAAAF"
  "AAMABAAAAE0HAAAQAAAAcQAJAAAADAAcAQQAbkAIABBDDAAiAQ0AcCAHAAEAEQEEAAEAAgAAAFYH"
  "AAAMAAAAYgACABIhEjL8IAAAIQAKAW4gBQAQAA4AAwABAAIAAABdBwAACwAAABIgEjH8IAEAEAAK"
  "ABJRcSAKAAEADgAAAAAAAAAAAAAAAwAAAAAAAAABAAAAmAIAAAIAAACgAgAABAAAAKgCAAACAAAA"
  "AAAAAAMAAAAPAAkAEQAAAAMAAAAHAAkAEQAAAAEAAAAAAAAAAQAAAA4AAAABAAAAFQAGPGluaXQ+"
  "AAFJAANJSUkADUlOVk9LRV9TVEFUSUMAAUwABExMTEwAM0xjb20vYW5kcm9pZC9qYWNrL2Fubm90"
  "YXRpb25zL0NhbGxlZEJ5SW52b2tlQ3VzdG9tOwAxTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlv"
  "bnMvTGlua2VyTWV0aG9kSGFuZGxlOwAvTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlvbnMvTWV0"
  "aG9kSGFuZGxlS2luZDsAM0xjb20vYW5kcm9pZC9qYWNrL2phdmE3L2ludm9rZWN1c3RvbS90ZXN0"
  "MDAxL1Rlc3RzOwAaTGRhbHZpay9hbm5vdGF0aW9uL1Rocm93czsAFUxqYXZhL2lvL1ByaW50U3Ry"
  "ZWFtOwARTGphdmEvbGFuZy9DbGFzczsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9T"
  "dHJpbmc7ABJMamF2YS9sYW5nL1N5c3RlbTsAFUxqYXZhL2xhbmcvVGhyb3dhYmxlOwAbTGphdmEv"
  "bGFuZy9pbnZva2UvQ2FsbFNpdGU7ACNMamF2YS9sYW5nL2ludm9rZS9Db25zdGFudENhbGxTaXRl"
  "OwAfTGphdmEvbGFuZy9pbnZva2UvTWV0aG9kSGFuZGxlOwAnTGphdmEvbGFuZy9pbnZva2UvTWV0"
  "aG9kSGFuZGxlcyRMb29rdXA7ACBMamF2YS9sYW5nL2ludm9rZS9NZXRob2RIYW5kbGVzOwAdTGph"
  "dmEvbGFuZy9pbnZva2UvTWV0aG9kVHlwZTsAGExqdW5pdC9mcmFtZXdvcmsvQXNzZXJ0OwAQTG9y"
  "Zy9qdW5pdC9UZXN0OwAKVGVzdHMuamF2YQABVgACVkkAA1ZJSQACVkwAE1tMamF2YS9sYW5nL1N0"
  "cmluZzsAA2FkZAANYXJndW1lbnRUeXBlcwAMYXNzZXJ0RXF1YWxzABVlbWl0dGVyOiBqYWNrLTQu"
  "MC1lbmcADWVuY2xvc2luZ1R5cGUADWZpZWxkQ2FsbFNpdGUACmZpbmRTdGF0aWMAEmludm9rZU1l"
  "dGhvZEhhbmRsZQAEa2luZAAMbGlua2VyTWV0aG9kAAZsb29rdXAABG1haW4ABG5hbWUAA291dAAH"
  "cHJpbnRsbgAKcmV0dXJuVHlwZQAEdGVzdAAFdmFsdWUAIgAHDgAvAgAABw4ANQMAAAAHDqUAPwEA"
  "Bw60ADsABw6lAAABBCAcAhgAGAAmHAEdAgQgHAMYDxgJGBEjGAQnGwArFygrFx8uGAACBQEwHAEY"
  "CwETAAMWABcfFQABAAQBAQkAgYAEtAUBCswFAQrkBQEJlAYEAbwGAAAAEwAAAAAAAAABAAAAAAAA"
  "AAEAAAAxAAAAcAAAAAIAAAAWAAAANAEAAAMAAAAJAAAAjAEAAAQAAAADAAAA+AEAAAUAAAALAAAA"
  "EAIAAAcAAAACAAAAaAIAAAYAAAABAAAAcAIAAAgAAAABAAAAkAIAAAMQAAADAAAAmAIAAAEgAAAF"
  "AAAAtAIAAAYgAAABAAAAZAMAAAEQAAAGAAAAjAMAAAIgAAAxAAAAwgMAAAMgAAAFAAAAQQcAAAQg"
  "AAADAAAAYwcAAAUgAAABAAAAlgcAAAAgAAABAAAAnQcAAAAQAAABAAAAvAcAAA==",
  // https://cs.corp.google.com/android/toolchain/jack/jack-tests/tests/com/android/jack/java7/invokecustom/test002/Tests.java
  "ZGV4CjAzOAAzq3aGAwKhT4QQj4lqNfZJAO8Tm24uTyNICQAAcAAAAHhWNBIAAAAAAAAAAGAIAAA2"
  "AAAAcAAAABgAAABIAQAACQAAAKgBAAAEAAAAFAIAAA0AAAA0AgAAAQAAAKQCAAB8BgAAzAIAACYE"
  "AAAwBAAAOAQAAEQEAABHBAAATAQAAE8EAABVBAAAigQAALwEAADtBAAAIgUAAD4FAABVBQAAaAUA"
  "AH0FAACRBQAApQUAALkFAADQBQAA7QUAABIGAAAzBgAAXAYAAH4GAACdBgAAtwYAAMkGAADPBgAA"
  "2wYAAN4GAADiBgAA5wYAAOsGAAD/BgAAFAcAABkHAAAoBwAANgcAAE0HAABcBwAAawcAAH4HAACK"
  "BwAAkAcAAJgHAACeBwAAqgcAALAHAAC1BwAAxgcAAM8HAADbBwAA4QcAAAMAAAAHAAAACAAAAAkA"
  "AAAKAAAACwAAAAwAAAANAAAADgAAAA8AAAAQAAAAEQAAABIAAAATAAAAFAAAABUAAAAWAAAAFwAA"
  "ABgAAAAZAAAAGgAAAB0AAAAhAAAAIgAAAAQAAAAAAAAA8AMAAAYAAAAPAAAA+AMAAAUAAAAQAAAA"
  "AAAAAAYAAAASAAAABAQAAB0AAAAVAAAAAAAAAB4AAAAVAAAAEAQAAB8AAAAVAAAA8AMAACAAAAAV"
  "AAAAGAQAACAAAAAVAAAAIAQAAAMAAwACAAAABAANACgAAAAIAAcAGwAAAAsABgAwAAAABAAEAAAA"
  "AAAEAAQAAQAAAAQAAAAjAAAABAAIAC0AAAAEAAQANAAAAAYABQAyAAAACQAEAAEAAAAMAAQAMQAA"
  "AA4ABwABAAAAEAABACoAAAARAAIALAAAABIAAwAuAAAAEwAGACUAAAA4CAAAOAgAAAQAAAABAAAA"
  "CQAAAAAAAAAcAAAA0AMAAD8IAAAAAAAAAQAAAAEAAAABAAAADggAAAIAAAAtCAAANQgAAAgAAAAE"
  "AAEA6AcAACoAAABxAAoAAAAMABwBBAAbAiMAAABiAwIAYgQCABIVI1UWAGIGAgASB00GBQdxMAsA"
  "QwUMA25ACQAQMgwAIgEOAHAgCAABAGkBAQAOAA0AbhAHAAAAKPsAAAAAJAABAAEBDCUBAAEAAQAA"
  "APUHAAAEAAAAcBAGAAAADgADAAIAAAAAAPoHAAADAAAAkAABAg8AAAAEAAEAAgAAAAEIAAAMAAAA"
  "YgADABIhEjL8IAAAIQAKAW4gBQAQAA4AAwABAAIAAAAICAAACwAAABIgEjH8IAEAEAAKABJRcSAM"
  "AAEADgAAAAAAAAAAAAAAAgAAAAAAAAACAAAAzAIAAAQAAADUAgAAAgAAAAAAAAADAAAABwAKABIA"
  "AAADAAAABwAHABYAAAABAAAAAAAAAAEAAAAPAAAAAQAAABcACDxjbGluaXQ+AAY8aW5pdD4ACkdF"
  "VF9TVEFUSUMAAUkAA0lJSQABTAAETExMTAAzTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlvbnMv"
  "Q2FsbGVkQnlJbnZva2VDdXN0b207ADBMY29tL2FuZHJvaWQvamFjay9hbm5vdGF0aW9ucy9MaW5r"
  "ZXJGaWVsZEhhbmRsZTsAL0xjb20vYW5kcm9pZC9qYWNrL2Fubm90YXRpb25zL01ldGhvZEhhbmRs"
  "ZUtpbmQ7ADNMY29tL2FuZHJvaWQvamFjay9qYXZhNy9pbnZva2VjdXN0b20vdGVzdDAwMi9UZXN0"
  "czsAGkxkYWx2aWsvYW5ub3RhdGlvbi9UaHJvd3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEUxq"
  "YXZhL2xhbmcvQ2xhc3M7ABNMamF2YS9sYW5nL0ludGVnZXI7ABJMamF2YS9sYW5nL09iamVjdDsA"
  "EkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07ABVMamF2YS9sYW5nL1Rocm93"
  "YWJsZTsAG0xqYXZhL2xhbmcvaW52b2tlL0NhbGxTaXRlOwAjTGphdmEvbGFuZy9pbnZva2UvQ29u"
  "c3RhbnRDYWxsU2l0ZTsAH0xqYXZhL2xhbmcvaW52b2tlL01ldGhvZEhhbmRsZTsAJ0xqYXZhL2xh"
  "bmcvaW52b2tlL01ldGhvZEhhbmRsZXMkTG9va3VwOwAgTGphdmEvbGFuZy9pbnZva2UvTWV0aG9k"
  "SGFuZGxlczsAHUxqYXZhL2xhbmcvaW52b2tlL01ldGhvZFR5cGU7ABhManVuaXQvZnJhbWV3b3Jr"
  "L0Fzc2VydDsAEExvcmcvanVuaXQvVGVzdDsABFRZUEUAClRlc3RzLmphdmEAAVYAAlZJAANWSUkA"
  "AlZMABJbTGphdmEvbGFuZy9DbGFzczsAE1tMamF2YS9sYW5nL1N0cmluZzsAA2FkZAANYXJndW1l"
  "bnRUeXBlcwAMYXNzZXJ0RXF1YWxzABVlbWl0dGVyOiBqYWNrLTQuMC1lbmcADWVuY2xvc2luZ1R5"
  "cGUADWZpZWxkQ2FsbFNpdGUAEWZpZWxkTWV0aG9kSGFuZGxlAApmaW5kU3RhdGljAARraW5kAAZs"
  "b29rdXAABG1haW4ACm1ldGhvZFR5cGUABG5hbWUAA291dAAPcHJpbnRTdGFja1RyYWNlAAdwcmlu"
  "dGxuAApyZXR1cm5UeXBlAAR0ZXN0AAV2YWx1ZQAoAAcOAR0PAnh3Jh4AIQAHDgA2AgAABw4APwEA"
  "Bw60ADsABw6lAAABBCQcAhgAGAApHAEdAgMnGAQrGwAvFygvFyMzGAACBQE1HAEYDAEUAAMWABcj"
  "FQABAAQBAQkAiIAE4AUBgYAE0AYBCugGAQmABwQBqAcAAAATAAAAAAAAAAEAAAAAAAAAAQAAADYA"
  "AABwAAAAAgAAABgAAABIAQAAAwAAAAkAAACoAQAABAAAAAQAAAAUAgAABQAAAA0AAAA0AgAABwAA"
  "AAIAAACcAgAABgAAAAEAAACkAgAACAAAAAEAAADEAgAAAxAAAAIAAADMAgAAASAAAAUAAADgAgAA"
  "BiAAAAEAAADQAwAAARAAAAYAAADwAwAAAiAAADYAAAAmBAAAAyAAAAUAAADoBwAABCAAAAMAAAAO"
  "CAAABSAAAAEAAAA4CAAAACAAAAEAAAA/CAAAABAAAAEAAABgCAAA",
  // https://cs.corp.google.com/android/toolchain/jack/jack-tests/tests/com/android/jack/java7/invokecustom/test003/Tests.java
  "ZGV4CjAzOABjnhkFatj30/7cHTCJsfr7vAjz9/p+Y+TcCAAAcAAAAHhWNBIAAAAAAAAAAPQHAAAx"
  "AAAAcAAAABYAAAA0AQAACQAAAIwBAAADAAAA+AEAAAsAAAAQAgAAAQAAAHACAABEBgAAmAIAAOoD"
  "AADyAwAA9QMAAP4DAAANBAAAEAQAABYEAABLBAAAfgQAAK8EAADkBAAAAAUAABcFAAAqBQAAPgUA"
  "AFIFAABmBQAAfQUAAJoFAAC/BQAA4AUAAAkGAAArBgAASgYAAGQGAAB2BgAAggYAAIUGAACJBgAA"
  "jgYAAJIGAACnBgAArAYAALsGAADJBgAA4AYAAO8GAAD+BgAACgcAAB4HAAAkBwAAMgcAADoHAABA"
  "BwAARgcAAEsHAABUBwAAYAcAAGYHAAABAAAABgAAAAcAAAAIAAAACQAAAAoAAAALAAAADAAAAA0A"
  "AAAOAAAADwAAABAAAAARAAAAEgAAABMAAAAUAAAAFQAAABYAAAAXAAAAGAAAABoAAAAeAAAAAgAA"
  "AAAAAACkAwAABQAAAAwAAAC0AwAABQAAAA4AAADAAwAABAAAAA8AAAAAAAAAGgAAABQAAAAAAAAA"
  "GwAAABQAAADMAwAAHAAAABQAAADUAwAAHQAAABQAAADcAwAAHQAAABQAAADkAwAAAwADAAMAAAAE"
  "AAwAJAAAAAoABgAsAAAABAAEAAAAAAAEAAAAHwAAAAQAAQAoAAAABAAIACoAAAAEAAQALwAAAAYA"
  "BQAtAAAACAAEAAAAAAANAAcAAAAAAA8AAgAlAAAAEAADACkAAAASAAYAIQAAAM4HAADOBwAABAAA"
  "AAEAAAAIAAAAAAAAABkAAAB8AwAA1QcAAAAAAAAEAAAAAgAAAAEAAACTBwAAAQAAAMMHAAACAAAA"
  "wwcAAMsHAAABAAEAAQAAAG0HAAAEAAAAcBAGAAAADgAHAAYAAAAAAHIHAAAHAAAAkAABArAwsECw"
  "ULBgDwAAAAUAAwAEAAAAfQcAABAAAABxAAkAAAAMABwBBABuQAgAEEMMACIBDQBwIAcAAQARAQgA"
  "AQACAAAAhgcAABAAAABiBgIAEhASIRIyEkMSVBJl/QYAAAAACgBuIAUABgAOAAcAAQACAAAAjQcA"
  "ABAAAAASEBIhEjISQxJUEmX9BgEAAAAKABMBFQBxIAoAAQAOAAAAAAAAAAAAAwAAAAAAAAABAAAA"
  "mAIAAAIAAACgAgAABAAAAKgCAAAGAAAAAAAAAAAAAAAAAAAAAwAAAA8ACQARAAAAAwAAAAcACQAR"
  "AAAAAQAAAAAAAAACAAAAAAAAAAEAAAAOAAAAAQAAABUABjxpbml0PgABSQAHSUlJSUlJSQANSU5W"
  "T0tFX1NUQVRJQwABTAAETExMTAAzTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlvbnMvQ2FsbGVk"
  "QnlJbnZva2VDdXN0b207ADFMY29tL2FuZHJvaWQvamFjay9hbm5vdGF0aW9ucy9MaW5rZXJNZXRo"
  "b2RIYW5kbGU7AC9MY29tL2FuZHJvaWQvamFjay9hbm5vdGF0aW9ucy9NZXRob2RIYW5kbGVLaW5k"
  "OwAzTGNvbS9hbmRyb2lkL2phY2svamF2YTcvaW52b2tlY3VzdG9tL3Rlc3QwMDMvVGVzdHM7ABpM"
  "ZGFsdmlrL2Fubm90YXRpb24vVGhyb3dzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABFMamF2YS9s"
  "YW5nL0NsYXNzOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZh"
  "L2xhbmcvU3lzdGVtOwAVTGphdmEvbGFuZy9UaHJvd2FibGU7ABtMamF2YS9sYW5nL2ludm9rZS9D"
  "YWxsU2l0ZTsAI0xqYXZhL2xhbmcvaW52b2tlL0NvbnN0YW50Q2FsbFNpdGU7AB9MamF2YS9sYW5n"
  "L2ludm9rZS9NZXRob2RIYW5kbGU7ACdMamF2YS9sYW5nL2ludm9rZS9NZXRob2RIYW5kbGVzJExv"
  "b2t1cDsAIExqYXZhL2xhbmcvaW52b2tlL01ldGhvZEhhbmRsZXM7AB1MamF2YS9sYW5nL2ludm9r"
  "ZS9NZXRob2RUeXBlOwAYTGp1bml0L2ZyYW1ld29yay9Bc3NlcnQ7ABBMb3JnL2p1bml0L1Rlc3Q7"
  "AApUZXN0cy5qYXZhAAFWAAJWSQADVklJAAJWTAATW0xqYXZhL2xhbmcvU3RyaW5nOwADYWRkAA1h"
  "cmd1bWVudFR5cGVzAAxhc3NlcnRFcXVhbHMAFWVtaXR0ZXI6IGphY2stNC4wLWVuZwANZW5jbG9z"
  "aW5nVHlwZQANZmllbGRDYWxsU2l0ZQAKZmluZFN0YXRpYwASaW52b2tlTWV0aG9kSGFuZGxlAARr"
  "aW5kAAxsaW5rZXJNZXRob2QABmxvb2t1cAAEbWFpbgAEbmFtZQADb3V0AAdwcmludGxuAApyZXR1"
  "cm5UeXBlAAR0ZXN0AAV2YWx1ZQAiAAcOAC8GAAAAAAAABw4ANQMAAAAHDqUAPwEABw7wADsABw7w"
  "AAABBCAcBhgAGAAYABgAGAAYACYcAR0CBCAcAxgPGAkYESMYBCcbACsXKCsXHy4YAAIFATAcARgL"
  "ARMAAxYAFx8VAAEABAEBCQCBgAS0BQEKzAUBCuwFAQmcBgQBzAYAAAATAAAAAAAAAAEAAAAAAAAA"
  "AQAAADEAAABwAAAAAgAAABYAAAA0AQAAAwAAAAkAAACMAQAABAAAAAMAAAD4AQAABQAAAAsAAAAQ"
  "AgAABwAAAAIAAABoAgAABgAAAAEAAABwAgAACAAAAAEAAACQAgAAAxAAAAMAAACYAgAAASAAAAUA"
  "AAC0AgAABiAAAAEAAAB8AwAAARAAAAcAAACkAwAAAiAAADEAAADqAwAAAyAAAAUAAABtBwAABCAA"
  "AAMAAACTBwAABSAAAAEAAADOBwAAACAAAAEAAADVBwAAABAAAAEAAAD0BwAA",
  // https://cs.corp.google.com/android/toolchain/jack/jack-tests/tests/com/android/jack/java7/invokecustom/test004/Tests.java
  "ZGV4CjAzOABvUVfbV74qWbSOEsgKP+EzahlNQLW2/8TMDAAAcAAAAHhWNBIAAAAAAAAAAOQLAABS"
  "AAAAcAAAAB8AAAC4AQAAEAAAADQCAAADAAAA9AIAABIAAAAMAwAAAQAAAKQDAAAACQAAzAMAANYF"
  "AADZBQAA4QUAAOkFAADsBQAA7wUAAPIFAAD1BQAA/AUAAP8FAAAEBgAAEwYAABYGAAAZBgAAHwYA"
  "AC8GAABkBgAAjQYAAMAGAADxBgAAJgcAAEUHAABhBwAAeAcAAIoHAACdBwAAsQcAAMUHAADZBwAA"
  "8AcAAA0IAAAyCAAAUwgAAHwIAACeCAAAvQgAANcIAADpCAAA7AgAAPgIAAD7CAAAAAkAAAYJAAAM"
  "CQAAEAkAABUJAAAaCQAAHgkAACMJAAAnCQAAKgkAADMJAABICQAATQkAAFwJAABqCQAAdgkAAIQJ"
  "AACPCQAAmgkAAKYJAACzCQAAygkAANkJAADoCQAA9AkAAAAKAAAKCgAAHgoAACQKAAAyCgAAPQoA"
  "AEUKAABLCgAAYgoAAGgKAABtCgAAdgoAAIIKAACOCgAAmwoAAKEKAAADAAAABAAAAAUAAAAGAAAA"
  "CAAAAAsAAAAPAAAAEAAAABEAAAASAAAAEwAAABQAAAAVAAAAFgAAABgAAAAZAAAAGgAAABsAAAAc"
  "AAAAHQAAAB4AAAAfAAAAIAAAACEAAAAiAAAAIwAAACQAAAAlAAAAJwAAADEAAAAzAAAACQAAAAQA"
  "AABMBQAADgAAABMAAABUBQAADQAAABUAAAB0BQAADAAAABYAAAAAAAAAJwAAABwAAAAAAAAAKAAA"
  "ABwAAACABQAAKQAAABwAAACIBQAAKgAAABwAAACUBQAAKwAAABwAAACgBQAALAAAABwAAABMBQAA"
  "LQAAABwAAACoBQAALwAAABwAAACwBQAALwAAABwAAAC4BQAALgAAABwAAADABQAAMAAAABwAAADI"
  "BQAALgAAABwAAADQBQAACQAJAAoAAAAKABMAPwAAABEADQBLAAAACgAEAAIAAAAKAAAANAAAAAoA"
  "AQBFAAAACgAPAEgAAAAKAAQAUAAAAA0ACABMAAAADwAEAAIAAAAUAA0AAgAAABYAAgBAAAAAFwAD"
  "AEcAAAAZAAUANgAAABkABgA2AAAAGQAHADYAAAAZAAkANgAAABkACgA2AAAAGQALADYAAAAZAAwA"
  "NgAAABkADgA3AAAAnQsAAJ0LAAAKAAAAAQAAAA8AAAAAAAAAJgAAACQFAADGCwAAAAAAAAQAAAAC"
  "AAAAAQAAAN4KAAACAAAAegsAAJILAAACAAAAkgsAAJoLAAABAAEAAQAAAKgKAAAEAAAAcBAGAAAA"
  "DgADAAIAAAAAAK0KAAADAAAAkAABAg8AAAAYAA8ABgAAALQKAABTAAAAcRARAAwAEhJxIA0A0gAT"
  "AmEAcSAKAOIAEwIABHEgDQDyABISAgAQAHEgDQACABICFAOamTFBAgARAHEwDAADAhYGAAAYApqZ"
  "mZmZmQFABQQSAHcGCwACABsCBwAAAAgAFABxIBAAAgAcAgoACAAVAHEgDwACABcCFc1bBwUAFgBx"
  "QA4AMhBxAAkAAAAMAhwDCgBuQAgAMroMAiIDFABwIAcAIwARAwAABAABAAIAAADRCgAADAAAAGIA"
  "AgASIRIy/CAAACEACgFuIAUAEAAOAAMAAQACAAAA2AoAAAsAAAASIBIx/CABABAACgASUXEgDQAB"
  "AA4AAAAAAAAAAAAAAAMAAAAAAAAAAQAAAMwDAAACAAAA1AMAAAQAAADgAwAAAgAAAAQABAANAAAA"
  "FgAQABgAHQAAAAEAGwAEAAMAAgAQAA4ABQAAAAMAAAAOABAAGAAAAAIAAAABAAEAAwAAAAIAAgAC"
  "AAAAAwAAAAMAAwADAAAAAQAAAAQAAAACAAAABQAFAAIAAAAPAA8AAgAAABAAEAABAAAAFQAAAAEA"
  "AAAdAAAAAQAAAB4AASgABjwqPjtKKQAGPGluaXQ+AAFCAAFDAAFEAAFGAAVIZWxsbwABSQADSUlJ"
  "AA1JTlZPS0VfU1RBVElDAAFKAAFMAARMTExMAA5MTExMWkJDU0lGRExMSgAzTGNvbS9hbmRyb2lk"
  "L2phY2svYW5ub3RhdGlvbnMvQ2FsbGVkQnlJbnZva2VDdXN0b207ACdMY29tL2FuZHJvaWQvamFj"
  "ay9hbm5vdGF0aW9ucy9Db25zdGFudDsAMUxjb20vYW5kcm9pZC9qYWNrL2Fubm90YXRpb25zL0xp"
  "bmtlck1ldGhvZEhhbmRsZTsAL0xjb20vYW5kcm9pZC9qYWNrL2Fubm90YXRpb25zL01ldGhvZEhh"
  "bmRsZUtpbmQ7ADNMY29tL2FuZHJvaWQvamFjay9qYXZhNy9pbnZva2VjdXN0b20vdGVzdDAwNC9U"
  "ZXN0czsAHUxkYWx2aWsvYW5ub3RhdGlvbi9TaWduYXR1cmU7ABpMZGFsdmlrL2Fubm90YXRpb24v"
  "VGhyb3dzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABBMamF2YS9sYW5nL0NsYXNzABFMamF2YS9s"
  "YW5nL0NsYXNzOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZh"
  "L2xhbmcvU3lzdGVtOwAVTGphdmEvbGFuZy9UaHJvd2FibGU7ABtMamF2YS9sYW5nL2ludm9rZS9D"
  "YWxsU2l0ZTsAI0xqYXZhL2xhbmcvaW52b2tlL0NvbnN0YW50Q2FsbFNpdGU7AB9MamF2YS9sYW5n"
  "L2ludm9rZS9NZXRob2RIYW5kbGU7ACdMamF2YS9sYW5nL2ludm9rZS9NZXRob2RIYW5kbGVzJExv"
  "b2t1cDsAIExqYXZhL2xhbmcvaW52b2tlL01ldGhvZEhhbmRsZXM7AB1MamF2YS9sYW5nL2ludm9r"
  "ZS9NZXRob2RUeXBlOwAYTGp1bml0L2ZyYW1ld29yay9Bc3NlcnQ7ABBMb3JnL2p1bml0L1Rlc3Q7"
  "AAFTAApUZXN0cy5qYXZhAAFWAANWQ0MABFZEREQABFZGRkYAAlZJAANWSUkAA1ZKSgACVkwAA1ZM"
  "TAACVloAAVoAB1pCQ1NJRkQAE1tMamF2YS9sYW5nL1N0cmluZzsAA2FkZAANYXJndW1lbnRUeXBl"
  "cwAMYXNzZXJ0RXF1YWxzAAphc3NlcnRUcnVlAAxib29sZWFuVmFsdWUACWJ5dGVWYWx1ZQAJY2hh"
  "clZhbHVlAApjbGFzc1ZhbHVlAAtkb3VibGVWYWx1ZQAVZW1pdHRlcjogamFjay00LjAtZW5nAA1l"
  "bmNsb3NpbmdUeXBlAA1maWVsZENhbGxTaXRlAApmaW5kU3RhdGljAApmbG9hdFZhbHVlAAhpbnRW"
  "YWx1ZQASaW52b2tlTWV0aG9kSGFuZGxlAARraW5kAAxsaW5rZXJNZXRob2QACWxvbmdWYWx1ZQAG"
  "bG9va3VwAARtYWluABVtZXRob2RIYW5kbGVFeHRyYUFyZ3MABG5hbWUAA291dAAHcHJpbnRsbgAK"
  "cmV0dXJuVHlwZQAKc2hvcnRWYWx1ZQALc3RyaW5nVmFsdWUABHRlc3QABXZhbHVlACMABw4ANwIA"
  "AAcOAD4NAAAAAAAAAAAAAAAAAAcOPEtaWmmWw4d4h6UAUgEABw60AE4ABw6lAAAGBTUcAhgEGARD"
  "HAEdCAQ1HA0YFhgQGBgYHRgAGAEYGxgEGAMYAhgQGA4YBT4YCkQbAEoXRUkcCh0HATgcAT8dBwE5"
  "HAEAAR0HATocAQNhHQcBThwBIgAEHQcBQhwBBAEdBwFBHAFwmpkxQR0HATwcAfGamZmZmZkBQB0H"
  "AU8cARcHHQcBOxwBGAodBwFGHAFmFc1bB0oXNE0YBAILAVEcCRcAFyAXGhciFzIXGhcXFwEXHQIM"
  "AVEcARgSARoADRYAFzQVAAQBBAEEYSQABAQBcJqZMUHxmpmZmZmZAUAXBxgKZhXNWwcBAAQBAQkA"
  "gYAE7AcBCoQIAQqcCAEJ1AkEAfwJAAATAAAAAAAAAAEAAAAAAAAAAQAAAFIAAABwAAAAAgAAAB8A"
  "AAC4AQAAAwAAABAAAAA0AgAABAAAAAMAAAD0AgAABQAAABIAAAAMAwAABwAAAAIAAACcAwAABgAA"
  "AAEAAACkAwAACAAAAAEAAADEAwAAAxAAAAMAAADMAwAAASAAAAUAAADsAwAABiAAAAEAAAAkBQAA"
  "ARAAAA0AAABMBQAAAiAAAFIAAADWBQAAAyAAAAUAAACoCgAABCAAAAQAAADeCgAABSAAAAEAAACd"
  "CwAAACAAAAEAAADGCwAAABAAAAEAAADkCwAA"
};

TEST_F(DexFileVerifierTest, InvokeCustomDexSamples) {
  for (size_t i = 0; i < arraysize(kInvokeCustomDexFiles); ++i) {
    size_t length;
    std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kInvokeCustomDexFiles[i], &length));
    CHECK(dex_bytes != nullptr);
    // Note: `dex_file` will be destroyed before `dex_bytes`.
    std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
    std::string error_msg;
    EXPECT_TRUE(DexFileVerifier::Verify(dex_file.get(),
                                        dex_file->Begin(),
                                        dex_file->Size(),
                                        "good checksum, verify",
                                        /*verify_checksum*/ true,
                                        &error_msg));
    // TODO(oth): Test corruptions (b/35308502)
  }
}

TEST_F(DexFileVerifierTest, BadStaticFieldInitialValuesArray) {
  // Generated DEX file version (037) from:
  //
  // .class public LBadStaticFieldInitialValuesArray;
  // .super Ljava/lang/Object;
  //
  //  # static fields
  //  .field static final c:C = 'c'
  //  .field static final i:I = 0x1
  //  .field static final s:Ljava/lang/String; = "s"
  //
  //  # direct methods
  //  .method public constructor <init>()V
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      return-void
  //  .end method
  //
  // Output file was hex edited so that static field "i" has string typing in initial values array.
  static const char kDexBase64[] =
      "ZGV4CjAzNQBrMi4cCPcMvvXNRw0uI6RRubwMPwgEYXIsAgAAcAAAAHhWNBIAAAAAAAAAAIwBAAAL"
      "AAAAcAAAAAYAAACcAAAAAQAAALQAAAADAAAAwAAAAAIAAADYAAAAAQAAAOgAAAAkAQAACAEAACAB"
      "AAAoAQAAMAEAADMBAAA2AQAAOwEAAE8BAABjAQAAZgEAAGkBAABsAQAAAgAAAAMAAAAEAAAABQAA"
      "AAYAAAAHAAAABwAAAAUAAAAAAAAAAgAAAAgAAAACAAEACQAAAAIABAAKAAAAAgAAAAAAAAADAAAA"
      "AAAAAAIAAAABAAAAAwAAAAAAAAABAAAAAAAAAHsBAAB0AQAAAQABAAEAAABvAQAABAAAAHAQAQAA"
      "AA4ABjxpbml0PgAGQS5qYXZhAAFDAAFJAANMQTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEv"
      "bGFuZy9TdHJpbmc7AAFWAAFjAAFpAAFzAAEABw4AAwNjFwoXCgMAAQAAGAEYARgAgYAEiAIADQAA"
      "AAAAAAABAAAAAAAAAAEAAAALAAAAcAAAAAIAAAAGAAAAnAAAAAMAAAABAAAAtAAAAAQAAAADAAAA"
      "wAAAAAUAAAACAAAA2AAAAAYAAAABAAAA6AAAAAEgAAABAAAACAEAAAIgAAALAAAAIAEAAAMgAAAB"
      "AAAAbwEAAAUgAAABAAAAdAEAAAAgAAABAAAAewEAAAAQAAABAAAAjAEAAA==";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_FALSE(DexFileVerifier::Verify(dex_file.get(),
                                       dex_file->Begin(),
                                       dex_file->Size(),
                                       "bad static field initial values array",
                                       /*verify_checksum*/ true,
                                       &error_msg));
}

TEST_F(DexFileVerifierTest, GoodStaticFieldInitialValuesArray) {
  // Generated DEX file version (037) from:
  //
  //  .class public LGoodStaticFieldInitialValuesArray;
  //  .super Ljava/lang/Object;
  //
  //  # static fields
  //  .field static final b:B = 0x1t
  //  .field static final c:C = 'c'
  //  .field static final d:D = 0.6
  //  .field static final f:F = 0.5f
  //  .field static final i:I = 0x3
  //  .field static final j:J = 0x4L
  //  .field static final l1:Ljava/lang/String;
  //  .field static final l2:Ljava/lang/String; = "s"
  //  .field static final l3:Ljava/lang/Class; = Ljava/lang/String;
  //  .field static final s:S = 0x2s
  //  .field static final z:Z = true
  //
  //  # direct methods
  //  .method public constructor <init>()V
  //      .registers 1
  //      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //      return-void
  //  .end method
  static const char kDexBase64[] =
      "ZGV4CjAzNQAwWxLbdhFa1NGiFWjsy5fhUCHxe5QHtPY8AwAAcAAAAHhWNBIAAAAAAAAAAJwCAAAZ"
      "AAAAcAAAAA0AAADUAAAAAQAAAAgBAAALAAAAFAEAAAIAAABsAQAAAQAAAHwBAACgAQAAnAEAAJwB"
      "AACkAQAApwEAAKoBAACtAQAAsAEAALMBAAC2AQAA2wEAAO4BAAACAgAAFgIAABkCAAAcAgAAHwIA"
      "ACICAAAlAgAAKAIAACsCAAAuAgAAMQIAADUCAAA5AgAAPQIAAEACAAABAAAAAgAAAAMAAAAEAAAA"
      "BQAAAAYAAAAHAAAACAAAAAkAAAAKAAAACwAAAAwAAAANAAAADAAAAAsAAAAAAAAABgAAAA4AAAAG"
      "AAEADwAAAAYAAgAQAAAABgADABEAAAAGAAQAEgAAAAYABQATAAAABgAJABQAAAAGAAkAFQAAAAYA"
      "BwAWAAAABgAKABcAAAAGAAwAGAAAAAYAAAAAAAAACAAAAAAAAAAGAAAAAQAAAAgAAAAAAAAA////"
      "/wAAAAB8AgAARAIAAAY8aW5pdD4AAUIAAUMAAUQAAUYAAUkAAUoAI0xHb29kU3RhdGljRmllbGRJ"
      "bml0aWFsVmFsdWVzQXJyYXk7ABFMamF2YS9sYW5nL0NsYXNzOwASTGphdmEvbGFuZy9PYmplY3Q7"
      "ABJMamF2YS9sYW5nL1N0cmluZzsAAVMAAVYAAVoAAWIAAWMAAWQAAWYAAWkAAWoAAmwxAAJsMgAC"
      "bDMAAXMAAXoAAAsAAQNj8TMzMzMzM+M/ED8EAwYEHhcXGAkCAj8AAAAAAQABAAEAAAAAAAAABAAA"
      "AHAQAQAAAA4ACwABAAAYARgBGAEYARgBGAEYARgBGAEYARgAgYAE5AQNAAAAAAAAAAEAAAAAAAAA"
      "AQAAABkAAABwAAAAAgAAAA0AAADUAAAAAwAAAAEAAAAIAQAABAAAAAsAAAAUAQAABQAAAAIAAABs"
      "AQAABgAAAAEAAAB8AQAAAiAAABkAAACcAQAABSAAAAEAAABEAgAAAxAAAAEAAABgAgAAASAAAAEA"
      "AABkAgAAACAAAAEAAAB8AgAAABAAAAEAAACcAgAA";

  size_t length;
  std::unique_ptr<uint8_t[]> dex_bytes(DecodeBase64(kDexBase64, &length));
  CHECK(dex_bytes != nullptr);
  // Note: `dex_file` will be destroyed before `dex_bytes`.
  std::unique_ptr<DexFile> dex_file(GetDexFile(dex_bytes.get(), length));
  std::string error_msg;
  EXPECT_TRUE(DexFileVerifier::Verify(dex_file.get(),
                                      dex_file->Begin(),
                                      dex_file->Size(),
                                      "good static field initial values array",
                                      /*verify_checksum*/ true,
                                      &error_msg));
}

}  // namespace art
