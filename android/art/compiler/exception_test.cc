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

#include <memory>

#include "base/arena_allocator.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/leb128.h"
#include "class_linker.h"
#include "common_runtime_test.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_exception_helpers.h"
#include "gtest/gtest.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/stack_trace_element.h"
#include "oat_quick_method_header.h"
#include "optimizing/stack_map_stream.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

class ExceptionTest : public CommonRuntimeTest {
 protected:
  // Since various dexers may differ in bytecode layout, we play
  // it safe and simply set the dex pc to the start of the method,
  // which always points to the first source statement.
  static constexpr const uint32_t kDexPc = 0;

  virtual void SetUp() {
    CommonRuntimeTest::SetUp();

    ScopedObjectAccess soa(Thread::Current());
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(LoadDex("ExceptionHandle"))));
    my_klass_ = class_linker_->FindClass(soa.Self(), "LExceptionHandle;", class_loader);
    ASSERT_TRUE(my_klass_ != nullptr);
    Handle<mirror::Class> klass(hs.NewHandle(my_klass_));
    class_linker_->EnsureInitialized(soa.Self(), klass, true, true);
    my_klass_ = klass.Get();

    dex_ = my_klass_->GetDexCache()->GetDexFile();

    uint32_t code_size = 12;
    for (size_t i = 0 ; i < code_size; i++) {
      fake_code_.push_back(0x70 | i);
    }

    ArenaPool pool;
    ArenaStack arena_stack(&pool);
    ScopedArenaAllocator allocator(&arena_stack);
    StackMapStream stack_maps(&allocator, kRuntimeISA);
    stack_maps.BeginStackMapEntry(kDexPc,
                                  /* native_pc_offset */ 3u,
                                  /* register_mask */ 0u,
                                  /* sp_mask */ nullptr,
                                  /* num_dex_registers */ 0u,
                                  /* inlining_depth */ 0u);
    stack_maps.EndStackMapEntry();
    size_t stack_maps_size = stack_maps.PrepareForFillIn();
    size_t stack_maps_offset = stack_maps_size +  sizeof(OatQuickMethodHeader);

    fake_header_code_and_maps_.resize(stack_maps_offset + fake_code_.size());
    MemoryRegion stack_maps_region(&fake_header_code_and_maps_[0], stack_maps_size);
    stack_maps.FillInCodeInfo(stack_maps_region);
    OatQuickMethodHeader method_header(stack_maps_offset, 0u, 4 * sizeof(void*), 0u, 0u, code_size);
    memcpy(&fake_header_code_and_maps_[stack_maps_size], &method_header, sizeof(method_header));
    std::copy(fake_code_.begin(),
              fake_code_.end(),
              fake_header_code_and_maps_.begin() + stack_maps_offset);

    // Align the code.
    const size_t alignment = GetInstructionSetAlignment(kRuntimeISA);
    fake_header_code_and_maps_.reserve(fake_header_code_and_maps_.size() + alignment);
    const void* unaligned_code_ptr =
        fake_header_code_and_maps_.data() + (fake_header_code_and_maps_.size() - code_size);
    size_t offset = dchecked_integral_cast<size_t>(reinterpret_cast<uintptr_t>(unaligned_code_ptr));
    size_t padding = RoundUp(offset, alignment) - offset;
    // Make sure no resizing takes place.
    CHECK_GE(fake_header_code_and_maps_.capacity(), fake_header_code_and_maps_.size() + padding);
    fake_header_code_and_maps_.insert(fake_header_code_and_maps_.begin(), padding, 0);
    const void* code_ptr = reinterpret_cast<const uint8_t*>(unaligned_code_ptr) + padding;
    CHECK_EQ(code_ptr,
             static_cast<const void*>(fake_header_code_and_maps_.data() +
                                          (fake_header_code_and_maps_.size() - code_size)));

    if (kRuntimeISA == InstructionSet::kArm) {
      // Check that the Thumb2 adjustment will be a NOP, see EntryPointToCodePointer().
      CHECK_ALIGNED(stack_maps_offset, 2);
    }

    method_f_ = my_klass_->FindClassMethod("f", "()I", kRuntimePointerSize);
    ASSERT_TRUE(method_f_ != nullptr);
    ASSERT_FALSE(method_f_->IsDirect());
    method_f_->SetEntryPointFromQuickCompiledCode(code_ptr);

    method_g_ = my_klass_->FindClassMethod("g", "(I)V", kRuntimePointerSize);
    ASSERT_TRUE(method_g_ != nullptr);
    ASSERT_FALSE(method_g_->IsDirect());
    method_g_->SetEntryPointFromQuickCompiledCode(code_ptr);
  }

  const DexFile* dex_;

  std::vector<uint8_t> fake_code_;
  std::vector<uint8_t> fake_header_code_and_maps_;

  ArtMethod* method_f_;
  ArtMethod* method_g_;

 private:
  mirror::Class* my_klass_;
};

TEST_F(ExceptionTest, FindCatchHandler) {
  ScopedObjectAccess soa(Thread::Current());
  CodeItemDataAccessor accessor(*dex_, dex_->GetCodeItem(method_f_->GetCodeItemOffset()));

  ASSERT_TRUE(accessor.HasCodeItem());

  ASSERT_EQ(2u, accessor.TriesSize());
  ASSERT_NE(0u, accessor.InsnsSizeInCodeUnits());

  const DexFile::TryItem& t0 = accessor.TryItems().begin()[0];
  const DexFile::TryItem& t1 = accessor.TryItems().begin()[1];
  EXPECT_LE(t0.start_addr_, t1.start_addr_);
  {
    CatchHandlerIterator iter(accessor, 4 /* Dex PC in the first try block */);
    EXPECT_STREQ("Ljava/io/IOException;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_STREQ("Ljava/lang/Exception;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_FALSE(iter.HasNext());
  }
  {
    CatchHandlerIterator iter(accessor, 8 /* Dex PC in the second try block */);
    EXPECT_STREQ("Ljava/io/IOException;", dex_->StringByTypeIdx(iter.GetHandlerTypeIndex()));
    ASSERT_TRUE(iter.HasNext());
    iter.Next();
    EXPECT_FALSE(iter.HasNext());
  }
  {
    CatchHandlerIterator iter(accessor, 11 /* Dex PC not in any try block */);
    EXPECT_FALSE(iter.HasNext());
  }
}

TEST_F(ExceptionTest, StackTraceElement) {
  Thread* thread = Thread::Current();
  thread->TransitionFromSuspendedToRunnable();
  bool started = runtime_->Start();
  CHECK(started);
  JNIEnv* env = thread->GetJniEnv();
  ScopedObjectAccess soa(env);

  std::vector<uintptr_t> fake_stack;
  Runtime* r = Runtime::Current();
  r->SetInstructionSet(kRuntimeISA);
  ArtMethod* save_method = r->CreateCalleeSaveMethod();
  r->SetCalleeSaveMethod(save_method, CalleeSaveType::kSaveAllCalleeSaves);
  QuickMethodFrameInfo frame_info = r->GetRuntimeMethodFrameInfo(save_method);

  ASSERT_EQ(kStackAlignment, 16U);
  // ASSERT_EQ(sizeof(uintptr_t), sizeof(uint32_t));

  // Create the stack frame for the callee save method, expected by the runtime.
  fake_stack.push_back(reinterpret_cast<uintptr_t>(save_method));
  for (size_t i = 0; i < frame_info.FrameSizeInBytes() - 2 * sizeof(uintptr_t);
       i += sizeof(uintptr_t)) {
    fake_stack.push_back(0);
  }

  fake_stack.push_back(method_g_->GetOatQuickMethodHeader(0)->ToNativeQuickPc(
      method_g_, kDexPc, /* is_catch_handler */ false));  // return pc

  // Create/push fake 16byte stack frame for method g
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_g_));
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(method_g_->GetOatQuickMethodHeader(0)->ToNativeQuickPc(
      method_g_, kDexPc, /* is_catch_handler */ false));  // return pc

  // Create/push fake 16byte stack frame for method f
  fake_stack.push_back(reinterpret_cast<uintptr_t>(method_f_));
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(0xEBAD6070);  // return pc

  // Push Method* of null to terminate the trace
  fake_stack.push_back(0);

  // Push null values which will become null incoming arguments.
  fake_stack.push_back(0);
  fake_stack.push_back(0);
  fake_stack.push_back(0);

  // Set up thread to appear as if we called out of method_g_ at given pc dex.
  thread->SetTopOfStack(reinterpret_cast<ArtMethod**>(&fake_stack[0]));

  jobject internal = thread->CreateInternalStackTrace<false>(soa);
  ASSERT_TRUE(internal != nullptr);
  jobjectArray ste_array = Thread::InternalStackTraceToStackTraceElementArray(soa, internal);
  ASSERT_TRUE(ste_array != nullptr);
  auto trace_array = soa.Decode<mirror::ObjectArray<mirror::StackTraceElement>>(ste_array);

  ASSERT_TRUE(trace_array != nullptr);
  ASSERT_TRUE(trace_array->Get(0) != nullptr);
  EXPECT_STREQ("ExceptionHandle",
               trace_array->Get(0)->GetDeclaringClass()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("ExceptionHandle.java",
               trace_array->Get(0)->GetFileName()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("g", trace_array->Get(0)->GetMethodName()->ToModifiedUtf8().c_str());
  EXPECT_EQ(36, trace_array->Get(0)->GetLineNumber());

  ASSERT_TRUE(trace_array->Get(1) != nullptr);
  EXPECT_STREQ("ExceptionHandle",
               trace_array->Get(1)->GetDeclaringClass()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("ExceptionHandle.java",
               trace_array->Get(1)->GetFileName()->ToModifiedUtf8().c_str());
  EXPECT_STREQ("f", trace_array->Get(1)->GetMethodName()->ToModifiedUtf8().c_str());
  EXPECT_EQ(22, trace_array->Get(1)->GetLineNumber());

  thread->SetTopOfStack(nullptr);  // Disarm the assertion that no code is running when we detach.
}

}  // namespace art
