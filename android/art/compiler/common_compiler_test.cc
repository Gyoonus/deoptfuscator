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

#include "common_compiler_test.h"

#include "arch/instruction_set_features.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/utils.h"
#include "class_linker.h"
#include "compiled_method-inl.h"
#include "dex/descriptors_names.h"
#include "dex/quick_compiler_callbacks.h"
#include "dex/verification_results.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "interpreter/interpreter.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

CommonCompilerTest::CommonCompilerTest() {}
CommonCompilerTest::~CommonCompilerTest() {}

void CommonCompilerTest::MakeExecutable(ArtMethod* method) {
  CHECK(method != nullptr);

  const CompiledMethod* compiled_method = nullptr;
  if (!method->IsAbstract()) {
    mirror::DexCache* dex_cache = method->GetDeclaringClass()->GetDexCache();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    compiled_method =
        compiler_driver_->GetCompiledMethod(MethodReference(&dex_file,
                                                            method->GetDexMethodIndex()));
  }
  // If the code size is 0 it means the method was skipped due to profile guided compilation.
  if (compiled_method != nullptr && compiled_method->GetQuickCode().size() != 0u) {
    ArrayRef<const uint8_t> code = compiled_method->GetQuickCode();
    const uint32_t code_size = code.size();
    ArrayRef<const uint8_t> vmap_table = compiled_method->GetVmapTable();
    const uint32_t vmap_table_offset = vmap_table.empty() ? 0u
        : sizeof(OatQuickMethodHeader) + vmap_table.size();
    // The method info is directly before the vmap table.
    ArrayRef<const uint8_t> method_info = compiled_method->GetMethodInfo();
    const uint32_t method_info_offset = method_info.empty() ? 0u
        : vmap_table_offset + method_info.size();

    OatQuickMethodHeader method_header(vmap_table_offset,
                                       method_info_offset,
                                       compiled_method->GetFrameSizeInBytes(),
                                       compiled_method->GetCoreSpillMask(),
                                       compiled_method->GetFpSpillMask(),
                                       code_size);

    header_code_and_maps_chunks_.push_back(std::vector<uint8_t>());
    std::vector<uint8_t>* chunk = &header_code_and_maps_chunks_.back();
    const size_t max_padding = GetInstructionSetAlignment(compiled_method->GetInstructionSet());
    const size_t size = method_info.size() + vmap_table.size() + sizeof(method_header) + code_size;
    chunk->reserve(size + max_padding);
    chunk->resize(sizeof(method_header));
    memcpy(&(*chunk)[0], &method_header, sizeof(method_header));
    chunk->insert(chunk->begin(), vmap_table.begin(), vmap_table.end());
    chunk->insert(chunk->begin(), method_info.begin(), method_info.end());
    chunk->insert(chunk->end(), code.begin(), code.end());
    CHECK_EQ(chunk->size(), size);
    const void* unaligned_code_ptr = chunk->data() + (size - code_size);
    size_t offset = dchecked_integral_cast<size_t>(reinterpret_cast<uintptr_t>(unaligned_code_ptr));
    size_t padding = compiled_method->AlignCode(offset) - offset;
    // Make sure no resizing takes place.
    CHECK_GE(chunk->capacity(), chunk->size() + padding);
    chunk->insert(chunk->begin(), padding, 0);
    const void* code_ptr = reinterpret_cast<const uint8_t*>(unaligned_code_ptr) + padding;
    CHECK_EQ(code_ptr, static_cast<const void*>(chunk->data() + (chunk->size() - code_size)));
    MakeExecutable(code_ptr, code.size());
    const void* method_code = CompiledMethod::CodePointer(code_ptr,
                                                          compiled_method->GetInstructionSet());
    LOG(INFO) << "MakeExecutable " << method->PrettyMethod() << " code=" << method_code;
    method->SetEntryPointFromQuickCompiledCode(method_code);
  } else {
    // No code? You must mean to go into the interpreter.
    // Or the generic JNI...
    class_linker_->SetEntryPointsToInterpreter(method);
  }
}

void CommonCompilerTest::MakeExecutable(const void* code_start, size_t code_length) {
  CHECK(code_start != nullptr);
  CHECK_NE(code_length, 0U);
  uintptr_t data = reinterpret_cast<uintptr_t>(code_start);
  uintptr_t base = RoundDown(data, kPageSize);
  uintptr_t limit = RoundUp(data + code_length, kPageSize);
  uintptr_t len = limit - base;
  int result = mprotect(reinterpret_cast<void*>(base), len, PROT_READ | PROT_WRITE | PROT_EXEC);
  CHECK_EQ(result, 0);

  FlushInstructionCache(reinterpret_cast<char*>(base), reinterpret_cast<char*>(base + len));
}

void CommonCompilerTest::MakeExecutable(ObjPtr<mirror::ClassLoader> class_loader,
                                        const char* class_name) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::ClassLoader> loader(hs.NewHandle(class_loader));
  mirror::Class* klass = class_linker_->FindClass(self, class_descriptor.c_str(), loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  PointerSize pointer_size = class_linker_->GetImagePointerSize();
  for (auto& m : klass->GetMethods(pointer_size)) {
    MakeExecutable(&m);
  }
}

// Get the set of image classes given to the compiler-driver in SetUp. Note: the compiler
// driver assumes ownership of the set, so the test should properly release the set.
std::unordered_set<std::string>* CommonCompilerTest::GetImageClasses() {
  // Empty set: by default no classes are retained in the image.
  return new std::unordered_set<std::string>();
}

// Get the set of compiled classes given to the compiler-driver in SetUp. Note: the compiler
// driver assumes ownership of the set, so the test should properly release the set.
std::unordered_set<std::string>* CommonCompilerTest::GetCompiledClasses() {
  // Null, no selection of compiled-classes.
  return nullptr;
}

// Get the set of compiled methods given to the compiler-driver in SetUp. Note: the compiler
// driver assumes ownership of the set, so the test should properly release the set.
std::unordered_set<std::string>* CommonCompilerTest::GetCompiledMethods() {
  // Null, no selection of compiled-methods.
  return nullptr;
}

// Get ProfileCompilationInfo that should be passed to the driver.
ProfileCompilationInfo* CommonCompilerTest::GetProfileCompilationInfo() {
  // Null, profile information will not be taken into account.
  return nullptr;
}

void CommonCompilerTest::SetUp() {
  CommonRuntimeTest::SetUp();
  {
    ScopedObjectAccess soa(Thread::Current());

    const InstructionSet instruction_set = kRuntimeISA;
    // Take the default set of instruction features from the build.
    instruction_set_features_ = InstructionSetFeatures::FromCppDefines();

    runtime_->SetInstructionSet(instruction_set);
    for (uint32_t i = 0; i < static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType); ++i) {
      CalleeSaveType type = CalleeSaveType(i);
      if (!runtime_->HasCalleeSaveMethod(type)) {
        runtime_->SetCalleeSaveMethod(runtime_->CreateCalleeSaveMethod(), type);
      }
    }

    CreateCompilerDriver(compiler_kind_, instruction_set);
  }
}

void CommonCompilerTest::CreateCompilerDriver(Compiler::Kind kind,
                                              InstructionSet isa,
                                              size_t number_of_threads) {
  compiler_options_->boot_image_ = true;
  compiler_options_->SetCompilerFilter(GetCompilerFilter());
  compiler_driver_.reset(new CompilerDriver(compiler_options_.get(),
                                            verification_results_.get(),
                                            kind,
                                            isa,
                                            instruction_set_features_.get(),
                                            GetImageClasses(),
                                            GetCompiledClasses(),
                                            GetCompiledMethods(),
                                            number_of_threads,
                                            /* swap_fd */ -1,
                                            GetProfileCompilationInfo()));
  // We typically don't generate an image in unit tests, disable this optimization by default.
  compiler_driver_->SetSupportBootImageFixup(false);
}

void CommonCompilerTest::SetUpRuntimeOptions(RuntimeOptions* options) {
  CommonRuntimeTest::SetUpRuntimeOptions(options);

  compiler_options_.reset(new CompilerOptions);
  verification_results_.reset(new VerificationResults(compiler_options_.get()));
  QuickCompilerCallbacks* callbacks =
      new QuickCompilerCallbacks(CompilerCallbacks::CallbackMode::kCompileApp);
  callbacks->SetVerificationResults(verification_results_.get());
  callbacks_.reset(callbacks);
}

Compiler::Kind CommonCompilerTest::GetCompilerKind() const {
  return compiler_kind_;
}

void CommonCompilerTest::SetCompilerKind(Compiler::Kind compiler_kind) {
  compiler_kind_ = compiler_kind;
}

InstructionSet CommonCompilerTest::GetInstructionSet() const {
  DCHECK(compiler_driver_.get() != nullptr);
  return compiler_driver_->GetInstructionSet();
}

void CommonCompilerTest::TearDown() {
  compiler_driver_.reset();
  callbacks_.reset();
  verification_results_.reset();
  compiler_options_.reset();
  image_reservation_.reset();

  CommonRuntimeTest::TearDown();
}

void CommonCompilerTest::CompileClass(mirror::ClassLoader* class_loader, const char* class_name) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::ClassLoader> loader(hs.NewHandle(class_loader));
  mirror::Class* klass = class_linker_->FindClass(self, class_descriptor.c_str(), loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  auto pointer_size = class_linker_->GetImagePointerSize();
  for (auto& m : klass->GetMethods(pointer_size)) {
    CompileMethod(&m);
  }
}

void CommonCompilerTest::CompileMethod(ArtMethod* method) {
  CHECK(method != nullptr);
  TimingLogger timings("CommonTest::CompileMethod", false, false);
  TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
  compiler_driver_->CompileOne(Thread::Current(), method, &timings);
  TimingLogger::ScopedTiming t2("MakeExecutable", &timings);
  MakeExecutable(method);
}

void CommonCompilerTest::CompileDirectMethod(Handle<mirror::ClassLoader> class_loader,
                                             const char* class_name, const char* method_name,
                                             const char* signature) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  mirror::Class* klass = class_linker_->FindClass(self, class_descriptor.c_str(), class_loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  auto pointer_size = class_linker_->GetImagePointerSize();
  ArtMethod* method = klass->FindClassMethod(method_name, signature, pointer_size);
  CHECK(method != nullptr && method->IsDirect()) << "Direct method not found: "
      << class_name << "." << method_name << signature;
  CompileMethod(method);
}

void CommonCompilerTest::CompileVirtualMethod(Handle<mirror::ClassLoader> class_loader,
                                              const char* class_name, const char* method_name,
                                              const char* signature) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  mirror::Class* klass = class_linker_->FindClass(self, class_descriptor.c_str(), class_loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  auto pointer_size = class_linker_->GetImagePointerSize();
  ArtMethod* method = klass->FindClassMethod(method_name, signature, pointer_size);
  CHECK(method != nullptr && !method->IsDirect()) << "Virtual method not found: "
      << class_name << "." << method_name << signature;
  CompileMethod(method);
}

void CommonCompilerTest::ReserveImageSpace() {
  // Reserve where the image will be loaded up front so that other parts of test set up don't
  // accidentally end up colliding with the fixed memory address when we need to load the image.
  std::string error_msg;
  MemMap::Init();
  image_reservation_.reset(MemMap::MapAnonymous("image reservation",
                                                reinterpret_cast<uint8_t*>(ART_BASE_ADDRESS),
                                                (size_t)120 * 1024 * 1024,  // 120MB
                                                PROT_NONE,
                                                false /* no need for 4gb flag with fixed mmap*/,
                                                false /* not reusing existing reservation */,
                                                &error_msg));
  CHECK(image_reservation_.get() != nullptr) << error_msg;
}

void CommonCompilerTest::UnreserveImageSpace() {
  image_reservation_.reset();
}

}  // namespace art
