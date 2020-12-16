/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_COMPILER_DRIVER_COMPILER_DRIVER_INL_H_
#define ART_COMPILER_DRIVER_COMPILER_DRIVER_INL_H_

#include "compiler_driver.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "dex_compilation_unit.h"
#include "handle_scope-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

inline ObjPtr<mirror::Class> CompilerDriver::ResolveClass(
    const ScopedObjectAccess& soa,
    Handle<mirror::DexCache> dex_cache,
    Handle<mirror::ClassLoader> class_loader,
    dex::TypeIndex cls_index,
    const DexCompilationUnit* mUnit) {
  DCHECK_EQ(dex_cache->GetDexFile(), mUnit->GetDexFile());
  DCHECK_EQ(class_loader.Get(), mUnit->GetClassLoader().Get());
  ObjPtr<mirror::Class> cls =
      mUnit->GetClassLinker()->ResolveType(cls_index, dex_cache, class_loader);
  DCHECK_EQ(cls == nullptr, soa.Self()->IsExceptionPending());
  if (UNLIKELY(cls == nullptr)) {
    // Clean up any exception left by type resolution.
    soa.Self()->ClearException();
  }
  return cls;
}

inline ObjPtr<mirror::Class> CompilerDriver::ResolveCompilingMethodsClass(
    const ScopedObjectAccess& soa,
    Handle<mirror::DexCache> dex_cache,
    Handle<mirror::ClassLoader> class_loader,
    const DexCompilationUnit* mUnit) {
  DCHECK_EQ(dex_cache->GetDexFile(), mUnit->GetDexFile());
  DCHECK_EQ(class_loader.Get(), mUnit->GetClassLoader().Get());
  const DexFile::MethodId& referrer_method_id =
      mUnit->GetDexFile()->GetMethodId(mUnit->GetDexMethodIndex());
  return ResolveClass(soa, dex_cache, class_loader, referrer_method_id.class_idx_, mUnit);
}

inline ArtField* CompilerDriver::ResolveField(const ScopedObjectAccess& soa,
                                              Handle<mirror::DexCache> dex_cache,
                                              Handle<mirror::ClassLoader> class_loader,
                                              uint32_t field_idx,
                                              bool is_static) {
  ArtField* resolved_field = Runtime::Current()->GetClassLinker()->ResolveField(
      field_idx, dex_cache, class_loader, is_static);
  DCHECK_EQ(resolved_field == nullptr, soa.Self()->IsExceptionPending());
  if (UNLIKELY(resolved_field == nullptr)) {
    // Clean up any exception left by type resolution.
    soa.Self()->ClearException();
    return nullptr;
  }
  if (UNLIKELY(resolved_field->IsStatic() != is_static)) {
    // ClassLinker can return a field of the wrong kind directly from the DexCache.
    // Silently return null on such incompatible class change.
    return nullptr;
  }
  return resolved_field;
}

inline std::pair<bool, bool> CompilerDriver::IsFastInstanceField(
    ObjPtr<mirror::DexCache> dex_cache,
    ObjPtr<mirror::Class> referrer_class,
    ArtField* resolved_field,
    uint16_t field_idx) {
  DCHECK(!resolved_field->IsStatic());
  ObjPtr<mirror::Class> fields_class = resolved_field->GetDeclaringClass();
  bool fast_get = referrer_class != nullptr &&
      referrer_class->CanAccessResolvedField(fields_class,
                                             resolved_field,
                                             dex_cache,
                                             field_idx);
  bool fast_put = fast_get && (!resolved_field->IsFinal() || fields_class == referrer_class);
  return std::make_pair(fast_get, fast_put);
}

inline ArtMethod* CompilerDriver::ResolveMethod(
    ScopedObjectAccess& soa,
    Handle<mirror::DexCache> dex_cache,
    Handle<mirror::ClassLoader> class_loader,
    const DexCompilationUnit* mUnit,
    uint32_t method_idx,
    InvokeType invoke_type) {
  DCHECK_EQ(class_loader.Get(), mUnit->GetClassLoader().Get());
  ArtMethod* resolved_method =
      mUnit->GetClassLinker()->ResolveMethod<ClassLinker::ResolveMode::kCheckICCEAndIAE>(
          method_idx, dex_cache, class_loader, /* referrer */ nullptr, invoke_type);
  if (UNLIKELY(resolved_method == nullptr)) {
    DCHECK(soa.Self()->IsExceptionPending());
    // Clean up any exception left by type resolution.
    soa.Self()->ClearException();
  }
  return resolved_method;
}

inline VerificationResults* CompilerDriver::GetVerificationResults() const {
  DCHECK(Runtime::Current()->IsAotCompiler());
  return verification_results_;
}

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_DRIVER_INL_H_
