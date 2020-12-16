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

#include "reference_table.h"

#include <regex>

#include "android-base/stringprintf.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "common_runtime_test.h"
#include "dex/primitive.h"
#include "handle_scope-inl.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/string.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

class ReferenceTableTest : public CommonRuntimeTest {};

static mirror::Object* CreateWeakReference(mirror::Object* referent)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  StackHandleScope<3> scope(self);
  Handle<mirror::Object> h_referent(scope.NewHandle<mirror::Object>(referent));

  Handle<mirror::Class> h_ref_class(scope.NewHandle<mirror::Class>(
      class_linker->FindClass(self,
                              "Ljava/lang/ref/WeakReference;",
                              ScopedNullHandle<mirror::ClassLoader>())));
  CHECK(h_ref_class != nullptr);
  CHECK(class_linker->EnsureInitialized(self, h_ref_class, true, true));

  Handle<mirror::Object> h_ref_instance(scope.NewHandle<mirror::Object>(
      h_ref_class->AllocObject(self)));
  CHECK(h_ref_instance != nullptr);

  ArtMethod* constructor = h_ref_class->FindConstructor(
      "(Ljava/lang/Object;)V", class_linker->GetImagePointerSize());
  CHECK(constructor != nullptr);

  uint32_t args[2];
  args[0] = PointerToLowMemUInt32(h_ref_instance.Get());
  args[1] = PointerToLowMemUInt32(h_referent.Get());
  JValue result;
  constructor->Invoke(self, args, sizeof(uint32_t), &result, constructor->GetShorty());
  CHECK(!self->IsExceptionPending());

  return h_ref_instance.Get();
}

TEST_F(ReferenceTableTest, Basics) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::Object* o1 = mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello");

  ReferenceTable rt("test", 0, 11);

  // Check dumping the empty table.
  {
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("(empty)"), std::string::npos) << oss.str();
    EXPECT_EQ(0U, rt.Size());
  }

  // Check removal of all nullss in a empty table is a no-op.
  rt.Remove(nullptr);
  EXPECT_EQ(0U, rt.Size());

  // Check removal of all o1 in a empty table is a no-op.
  rt.Remove(o1);
  EXPECT_EQ(0U, rt.Size());

  // Add o1 and check we have 1 element and can dump.
  {
    rt.Add(o1);
    EXPECT_EQ(1U, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("1 of java.lang.String"), std::string::npos) << oss.str();
    EXPECT_EQ(oss.str().find("short[]"), std::string::npos) << oss.str();
  }

  // Add a second object 10 times and check dumping is sane.
  mirror::Object* o2 = mirror::ShortArray::Alloc(soa.Self(), 0);
  for (size_t i = 0; i < 10; ++i) {
    rt.Add(o2);
    EXPECT_EQ(i + 2, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find(StringPrintf("Last %zd entries (of %zd):",
                                          i + 2 > 10 ? 10 : i + 2,
                                          i + 2)),
              std::string::npos) << oss.str();
    EXPECT_NE(oss.str().find("1 of java.lang.String"), std::string::npos) << oss.str();
    if (i == 0) {
      EXPECT_NE(oss.str().find("1 of short[]"), std::string::npos) << oss.str();
    } else {
      EXPECT_NE(oss.str().find(StringPrintf("%zd of short[] (1 unique instances)", i + 1)),
                std::string::npos) << oss.str();
    }
  }

  // Remove o1 (first element).
  {
    rt.Remove(o1);
    EXPECT_EQ(10U, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_EQ(oss.str().find("java.lang.String"), std::string::npos) << oss.str();
  }

  // Remove o2 ten times.
  for (size_t i = 0; i < 10; ++i) {
    rt.Remove(o2);
    EXPECT_EQ(9 - i, rt.Size());
    std::ostringstream oss;
    rt.Dump(oss);
    if (i == 9) {
      EXPECT_EQ(oss.str().find("short[]"), std::string::npos) << oss.str();
    } else if (i == 8) {
      EXPECT_NE(oss.str().find("1 of short[]"), std::string::npos) << oss.str();
    } else {
      EXPECT_NE(oss.str().find(StringPrintf("%zd of short[] (1 unique instances)", 10 - i - 1)),
                std::string::npos) << oss.str();
    }
  }

  // Add a reference and check that the type of the referent is dumped.
  {
    mirror::Object* empty_reference = CreateWeakReference(nullptr);
    ASSERT_TRUE(empty_reference->IsReferenceInstance());
    rt.Add(empty_reference);
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("java.lang.ref.WeakReference (referent is null)"), std::string::npos)
        << oss.str();
    rt.Remove(empty_reference);
  }

  {
    mirror::Object* string_referent = mirror::String::AllocFromModifiedUtf8(Thread::Current(), "A");
    mirror::Object* non_empty_reference = CreateWeakReference(string_referent);
    ASSERT_TRUE(non_empty_reference->IsReferenceInstance());
    rt.Add(non_empty_reference);
    std::ostringstream oss;
    rt.Dump(oss);
    EXPECT_NE(oss.str().find("java.lang.ref.WeakReference (referent is a java.lang.String)"),
              std::string::npos)
        << oss.str();
    rt.Remove(non_empty_reference);
  }

  // Add two objects. Enable allocation tracking for the latter.
  {
    StackHandleScope<3> hs(soa.Self());
    Handle<mirror::String> h_without_trace(hs.NewHandle(
        mirror::String::AllocFromModifiedUtf8(soa.Self(), "Without")));

    {
      ScopedThreadSuspension sts(soa.Self(), ThreadState::kSuspended);
      gc::AllocRecordObjectMap::SetAllocTrackingEnabled(true);
    }

    // To get a stack, actually make a call. Use substring, that's simple. Calling through JNI
    // avoids having to create the low-level args array ourselves.
    Handle<mirror::Object> h_with_trace;
    {
      jmethodID substr = soa.Env()->GetMethodID(WellKnownClasses::java_lang_String,
                                                "substring",
                                                "(II)Ljava/lang/String;");
      ASSERT_TRUE(substr != nullptr);
      jobject jobj = soa.Env()->AddLocalReference<jobject>(h_without_trace.Get());
      ASSERT_TRUE(jobj != nullptr);
      jobject result = soa.Env()->CallObjectMethod(jobj,
                                                   substr,
                                                   static_cast<jint>(0),
                                                   static_cast<jint>(4));
      ASSERT_TRUE(result != nullptr);
      h_with_trace = hs.NewHandle(soa.Self()->DecodeJObject(result));
    }

    Handle<mirror::Object> h_ref;
    {
      jclass weak_ref_class = soa.Env()->FindClass("java/lang/ref/WeakReference");
      ASSERT_TRUE(weak_ref_class != nullptr);
      jmethodID init = soa.Env()->GetMethodID(weak_ref_class,
                                              "<init>",
                                              "(Ljava/lang/Object;)V");
      ASSERT_TRUE(init != nullptr);
      jobject referent = soa.Env()->AddLocalReference<jobject>(h_with_trace.Get());
      jobject result = soa.Env()->NewObject(weak_ref_class, init, referent);
      ASSERT_TRUE(result != nullptr);
      h_ref = hs.NewHandle(soa.Self()->DecodeJObject(result));
    }

    rt.Add(h_without_trace.Get());
    rt.Add(h_with_trace.Get());
    rt.Add(h_ref.Get());

    std::ostringstream oss;
    rt.Dump(oss);

    constexpr const char* kStackTracePattern =
        R"(test reference table dump:\n)"
        R"(  Last 3 entries \(of 3\):\n)"  // NOLINT
        R"(        2: 0x[0-9a-f]* java.lang.ref.WeakReference \(referent is a java.lang.String\)\n)"  // NOLINT
        R"(          Allocated at:\n)"
        R"(            \(No managed frames\)\n)"  // NOLINT
        R"(          Referent allocated at:\n)"
        R"(            java.lang.String java.lang.String.fastSubstring\(int, int\):-2\n)"  // NOLINT
        R"(            java.lang.String java.lang.String.substring\(int, int\):[0-9]*\n)"  // NOLINT
        R"(        1: 0x[0-9a-f]* java.lang.String "With"\n)"
        R"(          Allocated at:\n)"
        R"(            java.lang.String java.lang.String.fastSubstring\(int, int\):-2\n)"  // NOLINT
        R"(            java.lang.String java.lang.String.substring\(int, int\):[0-9]*\n)"  // NOLINT
        R"(        0: 0x[0-9a-f]* java.lang.String "Without"\n)"
        R"(  Summary:\n)"
        R"(        2 of java.lang.String \(2 unique instances\)\n)"  // NOLINT
        R"(        1 of java.lang.ref.WeakReference\n)";
    std::regex stack_trace_regex(kStackTracePattern);
    std::smatch stack_trace_match;
    std::string str = oss.str();
    bool found = std::regex_search(str, stack_trace_match, stack_trace_regex);
    EXPECT_TRUE(found) << str;

    {
      ScopedThreadSuspension sts(soa.Self(), ThreadState::kSuspended);
      gc::AllocRecordObjectMap::SetAllocTrackingEnabled(false);
    }
  }
}

static std::vector<size_t> FindAll(const std::string& haystack, const char* needle) {
  std::vector<size_t> res;
  size_t start = 0;
  do {
    size_t pos = haystack.find(needle, start);
    if (pos == std::string::npos) {
      break;
    }
    res.push_back(pos);
    start = pos + 1;
  } while (start < haystack.size());
  return res;
}

TEST_F(ReferenceTableTest, SummaryOrder) {
  // Check that the summary statistics are sorted.
  ScopedObjectAccess soa(Thread::Current());

  ReferenceTable rt("test", 0, 20);

  {
    mirror::Object* s1 = mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello");
    mirror::Object* s2 = mirror::String::AllocFromModifiedUtf8(soa.Self(), "world");

    // 3 copies of s1, 2 copies of s2, interleaved.
    for (size_t i = 0; i != 2; ++i) {
      rt.Add(s1);
      rt.Add(s2);
    }
    rt.Add(s1);
  }

  {
    // Differently sized byte arrays. Should be sorted by identical (non-unique cound).
    mirror::Object* b1_1 = mirror::ByteArray::Alloc(soa.Self(), 1);
    rt.Add(b1_1);
    rt.Add(mirror::ByteArray::Alloc(soa.Self(), 2));
    rt.Add(b1_1);
    rt.Add(mirror::ByteArray::Alloc(soa.Self(), 2));
    rt.Add(mirror::ByteArray::Alloc(soa.Self(), 1));
    rt.Add(mirror::ByteArray::Alloc(soa.Self(), 2));
  }

  rt.Add(mirror::CharArray::Alloc(soa.Self(), 0));

  // Now dump, and ensure order.
  std::ostringstream oss;
  rt.Dump(oss);

  // Only do this on the part after Summary.
  std::string base = oss.str();
  size_t summary_pos = base.find("Summary:");
  ASSERT_NE(summary_pos, std::string::npos);

  std::string haystack = base.substr(summary_pos);

  std::vector<size_t> strCounts = FindAll(haystack, "java.lang.String");
  std::vector<size_t> b1Counts = FindAll(haystack, "byte[] (1 elements)");
  std::vector<size_t> b2Counts = FindAll(haystack, "byte[] (2 elements)");
  std::vector<size_t> cCounts = FindAll(haystack, "char[]");

  // Only one each.
  EXPECT_EQ(1u, strCounts.size());
  EXPECT_EQ(1u, b1Counts.size());
  EXPECT_EQ(1u, b2Counts.size());
  EXPECT_EQ(1u, cCounts.size());

  // Expect them to be in order.
  EXPECT_LT(strCounts[0], b1Counts[0]);
  EXPECT_LT(b1Counts[0], b2Counts[0]);
  EXPECT_LT(b2Counts[0], cCounts[0]);
}

}  // namespace art
