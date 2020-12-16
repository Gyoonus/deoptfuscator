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

#ifndef ART_RUNTIME_ART_METHOD_INL_H_
#define ART_RUNTIME_ART_METHOD_INL_H_

#include "art_method.h"

#include "art_field.h"
#include "base/callee_save_type.h"
#include "base/utils.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_file_types.h"
#include "dex/invoke_type.h"
#include "dex/primitive.h"
#include "gc_root-inl.h"
#include "intrinsics_enum.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "mirror/string.h"
#include "oat.h"
#include "obj_ptr-inl.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier-inl.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

template <ReadBarrierOption kReadBarrierOption>
inline mirror::Class* ArtMethod::GetDeclaringClassUnchecked() {
  GcRootSource gc_root_source(this);
  return declaring_class_.Read<kReadBarrierOption>(&gc_root_source);
}

template <ReadBarrierOption kReadBarrierOption>
inline mirror::Class* ArtMethod::GetDeclaringClass() {
  mirror::Class* result = GetDeclaringClassUnchecked<kReadBarrierOption>();
  if (kIsDebugBuild) {
    if (!IsRuntimeMethod()) {
      CHECK(result != nullptr) << this;
      if (kCheckDeclaringClassState) {
        if (!(result->IsIdxLoaded() || result->IsErroneous())) {
          LOG(FATAL_WITHOUT_ABORT) << "Class status: " << result->GetStatus();
          LOG(FATAL) << result->PrettyClass();
        }
      }
    } else {
      CHECK(result == nullptr) << this;
    }
  }
  return result;
}

inline void ArtMethod::SetDeclaringClass(ObjPtr<mirror::Class> new_declaring_class) {
  declaring_class_ = GcRoot<mirror::Class>(new_declaring_class);
}

inline bool ArtMethod::CASDeclaringClass(mirror::Class* expected_class,
                                         mirror::Class* desired_class) {
  GcRoot<mirror::Class> expected_root(expected_class);
  GcRoot<mirror::Class> desired_root(desired_class);
  auto atomic_root_class = reinterpret_cast<Atomic<GcRoot<mirror::Class>>*>(&declaring_class_);
  return atomic_root_class->CompareAndSetStrongSequentiallyConsistent(expected_root, desired_root);
}

inline uint16_t ArtMethod::GetMethodIndex() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsResolved());
  return method_index_;
}

inline uint16_t ArtMethod::GetMethodIndexDuringLinking() {
  return method_index_;
}

template <ReadBarrierOption kReadBarrierOption>
inline uint32_t ArtMethod::GetDexMethodIndex() {
  if (kCheckDeclaringClassState) {
    CHECK(IsRuntimeMethod() ||
          GetDeclaringClass<kReadBarrierOption>()->IsIdxLoaded() ||
          GetDeclaringClass<kReadBarrierOption>()->IsErroneous());
  }
  return GetDexMethodIndexUnchecked();
}

inline ObjPtr<mirror::Class> ArtMethod::LookupResolvedClassFromTypeIndex(dex::TypeIndex type_idx) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ObjPtr<mirror::Class> type =
      Runtime::Current()->GetClassLinker()->LookupResolvedType(type_idx, this);
  DCHECK(!Thread::Current()->IsExceptionPending());
  return type;
}

inline ObjPtr<mirror::Class> ArtMethod::ResolveClassFromTypeIndex(dex::TypeIndex type_idx) {
  ObjPtr<mirror::Class> type = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, this);
  DCHECK_EQ(type == nullptr, Thread::Current()->IsExceptionPending());
  return type;
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      // We have an error if we are direct or a non-copied (i.e. not part of a real class) interface
      // method.
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsCopied());
    }
    case kSuper:
      // Constructors and static methods are called with invoke-direct.
      return IsConstructor() || IsStatic();
    case kInterface: {
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || !(methods_class->IsInterface() || methods_class->IsObjectClass());
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      UNREACHABLE();
  }
}

inline bool ArtMethod::IsCalleeSaveMethod() {
  if (!IsRuntimeMethod()) {
    return false;
  }
  Runtime* runtime = Runtime::Current();
  bool result = false;
  for (uint32_t i = 0; i < static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType); i++) {
    if (this == runtime->GetCalleeSaveMethod(CalleeSaveType(i))) {
      result = true;
      break;
    }
  }
  return result;
}

inline bool ArtMethod::IsResolutionMethod() {
  bool result = this == Runtime::Current()->GetResolutionMethod();
  // Check that if we do think it is phony it looks like the resolution method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline bool ArtMethod::IsImtUnimplementedMethod() {
  bool result = this == Runtime::Current()->GetImtUnimplementedMethod();
  // Check that if we do think it is phony it looks like the imt unimplemented method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline const DexFile* ArtMethod::GetDexFile() {
  // It is safe to avoid the read barrier here since the dex file is constant, so if we read the
  // from-space dex file pointer it will be equal to the to-space copy.
  return GetDexCache<kWithoutReadBarrier>()->GetDexFile();
}

inline const char* ArtMethod::GetDeclaringClassDescriptor() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (UNLIKELY(dex_method_idx == dex::kDexNoIndex)) {
    return "<runtime method>";
  }
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodDeclaringClassDescriptor(dex_file->GetMethodId(dex_method_idx));
}

inline const char* ArtMethod::GetShorty() {
  uint32_t unused_length;
  return GetShorty(&unused_length);
}

inline const char* ArtMethod::GetShorty(uint32_t* out_length) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  // Don't do a read barrier in the DCHECK() inside GetDexMethodIndex() as GetShorty()
  // can be called when the declaring class is about to be unloaded and cannot be added
  // to the mark stack (subsequent GC assertion would fail).
  // It is safe to avoid the read barrier as the ArtMethod is constructed with a declaring
  // Class already satisfying the DCHECK() inside GetDexMethodIndex(), so even if that copy
  // of declaring class becomes a from-space object, it shall satisfy the DCHECK().
  return dex_file->GetMethodShorty(dex_file->GetMethodId(GetDexMethodIndex<kWithoutReadBarrier>()),
                                   out_length);
}

inline const Signature ArtMethod::GetSignature() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (dex_method_idx != dex::kDexNoIndex) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodSignature(dex_file->GetMethodId(dex_method_idx));
  }
  return Signature::NoSignature();
}

inline const char* ArtMethod::GetName() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (LIKELY(dex_method_idx != dex::kDexNoIndex)) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodName(dex_file->GetMethodId(dex_method_idx));
  }
  Runtime* const runtime = Runtime::Current();
  if (this == runtime->GetResolutionMethod()) {
    return "<runtime internal resolution method>";
  } else if (this == runtime->GetImtConflictMethod()) {
    return "<runtime internal imt conflict method>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveAllCalleeSaves)) {
    return "<runtime internal callee-save all registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsOnly)) {
    return "<runtime internal callee-save reference registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsAndArgs)) {
    return "<runtime internal callee-save reference and argument registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverything)) {
    return "<runtime internal save-every-register method>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForClinit)) {
    return "<runtime internal save-every-register method for clinit>";
  } else if (this == runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForSuspendCheck)) {
    return "<runtime internal save-every-register method for suspend check>";
  } else {
    return "<unknown runtime internal method>";
  }
}

inline const DexFile::CodeItem* ArtMethod::GetCodeItem() {
  return GetDexFile()->GetCodeItem(GetCodeItemOffset());
}

inline bool ArtMethod::IsResolvedTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  return LookupResolvedClassFromTypeIndex(type_idx) != nullptr;
}

inline int32_t ArtMethod::GetLineNumFromDexPC(uint32_t dex_pc) {
  DCHECK(!IsProxyMethod());
  if (dex_pc == dex::kDexNoIndex) {
    return IsNative() ? -2 : -1;
  }
  return annotations::GetLineNumFromPC(GetDexFile(), this, dex_pc);
}

inline const DexFile::ProtoId& ArtMethod::GetPrototype() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodPrototype(dex_file->GetMethodId(GetDexMethodIndex()));
}

inline const DexFile::TypeList* ArtMethod::GetParameterTypeList() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::ProtoId& proto = dex_file->GetMethodPrototype(
      dex_file->GetMethodId(GetDexMethodIndex()));
  return dex_file->GetProtoParameters(proto);
}

inline const char* ArtMethod::GetDeclaringClassSourceFile() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetSourceFile();
}

inline uint16_t ArtMethod::GetClassDefIndex() {
  DCHECK(!IsProxyMethod());
  if (LIKELY(!IsObsolete())) {
    return GetDeclaringClass()->GetDexClassDefIndex();
  } else {
    return FindObsoleteDexClassDefIndex();
  }
}

inline const DexFile::ClassDef& ArtMethod::GetClassDef() {
  DCHECK(!IsProxyMethod());
  return GetDexFile()->GetClassDef(GetClassDefIndex());
}

inline size_t ArtMethod::GetNumberOfParameters() {
  constexpr size_t return_type_count = 1u;
  return strlen(GetShorty()) - return_type_count;
}

inline const char* ArtMethod::GetReturnTypeDescriptor() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(GetReturnTypeIndex()));
}

inline Primitive::Type ArtMethod::GetReturnTypePrimitive() {
  return Primitive::GetType(GetReturnTypeDescriptor()[0]);
}

inline const char* ArtMethod::GetTypeDescriptorFromTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(type_idx));
}

inline mirror::ClassLoader* ArtMethod::GetClassLoader() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetClassLoader();
}

template <ReadBarrierOption kReadBarrierOption>
inline mirror::DexCache* ArtMethod::GetDexCache() {
  if (LIKELY(!IsObsolete<kReadBarrierOption>())) {
    mirror::Class* klass = GetDeclaringClass<kReadBarrierOption>();
    return klass->GetDexCache<kDefaultVerifyFlags, kReadBarrierOption>();
  } else {
    DCHECK(!IsProxyMethod());
    return GetObsoleteDexCache();
  }
}

inline bool ArtMethod::IsProxyMethod() {
  DCHECK(!IsRuntimeMethod()) << "ArtMethod::IsProxyMethod called on a runtime method";
  // Avoid read barrier since the from-space version of the class will have the correct proxy class
  // flags since they are constant for the lifetime of the class.
  return GetDeclaringClass<kWithoutReadBarrier>()->IsProxyClass();
}

inline ArtMethod* ArtMethod::GetInterfaceMethodForProxyUnchecked(PointerSize pointer_size) {
  DCHECK(IsProxyMethod());
  // Do not check IsAssignableFrom() here as it relies on raw reference comparison
  // which may give false negatives while visiting references for a non-CC moving GC.
  return reinterpret_cast<ArtMethod*>(GetDataPtrSize(pointer_size));
}

inline ArtMethod* ArtMethod::GetInterfaceMethodIfProxy(PointerSize pointer_size) {
  if (LIKELY(!IsProxyMethod())) {
    return this;
  }
  ArtMethod* interface_method = GetInterfaceMethodForProxyUnchecked(pointer_size);
  // We can check that the proxy class implements the interface only if the proxy class
  // is resolved, otherwise the interface table is not yet initialized.
  DCHECK(!GetDeclaringClass()->IsResolved() ||
         interface_method->GetDeclaringClass()->IsAssignableFrom(GetDeclaringClass()));
  return interface_method;
}

inline dex::TypeIndex ArtMethod::GetReturnTypeIndex() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  return proto_id.return_type_idx_;
}

inline ObjPtr<mirror::Class> ArtMethod::LookupResolvedReturnType() {
  return LookupResolvedClassFromTypeIndex(GetReturnTypeIndex());
}

inline ObjPtr<mirror::Class> ArtMethod::ResolveReturnType() {
  return ResolveClassFromTypeIndex(GetReturnTypeIndex());
}

template <ReadBarrierOption kReadBarrierOption>
inline bool ArtMethod::HasSingleImplementation() {
  if (IsFinal<kReadBarrierOption>() || GetDeclaringClass<kReadBarrierOption>()->IsFinal()) {
    // We don't set kAccSingleImplementation for these cases since intrinsic
    // can use the flag also.
    return true;
  }
  return (GetAccessFlags<kReadBarrierOption>() & kAccSingleImplementation) != 0;
}

inline HiddenApiAccessFlags::ApiList ArtMethod::GetHiddenApiAccessFlags()
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(IsIntrinsic())) {
    switch (static_cast<Intrinsics>(GetIntrinsic())) {
      case Intrinsics::kSystemArrayCopyChar:
      case Intrinsics::kStringGetCharsNoCheck:
      case Intrinsics::kReferenceGetReferent:
        // These intrinsics are on the light greylist and will fail a DCHECK in
        // SetIntrinsic() if their flags change on the respective dex methods.
        // Note that the DCHECK currently won't fail if the dex methods are
        // whitelisted, e.g. in the core image (b/77733081). As a result, we
        // might print warnings but we won't change the semantics.
        return HiddenApiAccessFlags::kLightGreylist;
      case Intrinsics::kVarHandleFullFence:
      case Intrinsics::kVarHandleAcquireFence:
      case Intrinsics::kVarHandleReleaseFence:
      case Intrinsics::kVarHandleLoadLoadFence:
      case Intrinsics::kVarHandleStoreStoreFence:
      case Intrinsics::kVarHandleCompareAndExchange:
      case Intrinsics::kVarHandleCompareAndExchangeAcquire:
      case Intrinsics::kVarHandleCompareAndExchangeRelease:
      case Intrinsics::kVarHandleCompareAndSet:
      case Intrinsics::kVarHandleGet:
      case Intrinsics::kVarHandleGetAcquire:
      case Intrinsics::kVarHandleGetAndAdd:
      case Intrinsics::kVarHandleGetAndAddAcquire:
      case Intrinsics::kVarHandleGetAndAddRelease:
      case Intrinsics::kVarHandleGetAndBitwiseAnd:
      case Intrinsics::kVarHandleGetAndBitwiseAndAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseAndRelease:
      case Intrinsics::kVarHandleGetAndBitwiseOr:
      case Intrinsics::kVarHandleGetAndBitwiseOrAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseOrRelease:
      case Intrinsics::kVarHandleGetAndBitwiseXor:
      case Intrinsics::kVarHandleGetAndBitwiseXorAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseXorRelease:
      case Intrinsics::kVarHandleGetAndSet:
      case Intrinsics::kVarHandleGetAndSetAcquire:
      case Intrinsics::kVarHandleGetAndSetRelease:
      case Intrinsics::kVarHandleGetOpaque:
      case Intrinsics::kVarHandleGetVolatile:
      case Intrinsics::kVarHandleSet:
      case Intrinsics::kVarHandleSetOpaque:
      case Intrinsics::kVarHandleSetRelease:
      case Intrinsics::kVarHandleSetVolatile:
      case Intrinsics::kVarHandleWeakCompareAndSet:
      case Intrinsics::kVarHandleWeakCompareAndSetAcquire:
      case Intrinsics::kVarHandleWeakCompareAndSetPlain:
      case Intrinsics::kVarHandleWeakCompareAndSetRelease:
        // These intrinsics are on the blacklist and will fail a DCHECK in
        // SetIntrinsic() if their flags change on the respective dex methods.
        // Note that the DCHECK currently won't fail if the dex methods are
        // whitelisted, e.g. in the core image (b/77733081). Given that they are
        // exclusively VarHandle intrinsics, they should not be used outside
        // tests that do not enable hidden API checks.
        return HiddenApiAccessFlags::kBlacklist;
      default:
        // Remaining intrinsics are public API. We DCHECK that in SetIntrinsic().
        return HiddenApiAccessFlags::kWhitelist;
    }
  } else {
    return HiddenApiAccessFlags::DecodeFromRuntime(GetAccessFlags());
  }
}

inline void ArtMethod::SetIntrinsic(uint32_t intrinsic) {
  // Currently we only do intrinsics for static/final methods or methods of final
  // classes. We don't set kHasSingleImplementation for those methods.
  DCHECK(IsStatic() || IsFinal() || GetDeclaringClass()->IsFinal()) <<
      "Potential conflict with kAccSingleImplementation";
  static const int kAccFlagsShift = CTZ(kAccIntrinsicBits);
  DCHECK_LE(intrinsic, kAccIntrinsicBits >> kAccFlagsShift);
  uint32_t intrinsic_bits = intrinsic << kAccFlagsShift;
  uint32_t new_value = (GetAccessFlags() & ~kAccIntrinsicBits) | kAccIntrinsic | intrinsic_bits;
  if (kIsDebugBuild) {
    uint32_t java_flags = (GetAccessFlags() & kAccJavaFlagsMask);
    bool is_constructor = IsConstructor();
    bool is_synchronized = IsSynchronized();
    bool skip_access_checks = SkipAccessChecks();
    bool is_fast_native = IsFastNative();
    bool is_critical_native = IsCriticalNative();
    bool is_copied = IsCopied();
    bool is_miranda = IsMiranda();
    bool is_default = IsDefault();
    bool is_default_conflict = IsDefaultConflicting();
    bool is_compilable = IsCompilable();
    bool must_count_locks = MustCountLocks();
    HiddenApiAccessFlags::ApiList hidden_api_flags = GetHiddenApiAccessFlags();
    SetAccessFlags(new_value);
    DCHECK_EQ(java_flags, (GetAccessFlags() & kAccJavaFlagsMask));
    DCHECK_EQ(is_constructor, IsConstructor());
    DCHECK_EQ(is_synchronized, IsSynchronized());
    DCHECK_EQ(skip_access_checks, SkipAccessChecks());
    DCHECK_EQ(is_fast_native, IsFastNative());
    DCHECK_EQ(is_critical_native, IsCriticalNative());
    DCHECK_EQ(is_copied, IsCopied());
    DCHECK_EQ(is_miranda, IsMiranda());
    DCHECK_EQ(is_default, IsDefault());
    DCHECK_EQ(is_default_conflict, IsDefaultConflicting());
    DCHECK_EQ(is_compilable, IsCompilable());
    DCHECK_EQ(must_count_locks, MustCountLocks());
    // Only DCHECK that we have preserved the hidden API access flags if the
    // original method was not on the whitelist. This is because the core image
    // does not have the access flags set (b/77733081). It is fine to hard-code
    // these because (a) warnings on greylist do not change semantics, and
    // (b) only VarHandle intrinsics are blacklisted at the moment and they
    // should not be used outside tests with disabled API checks.
    if (hidden_api_flags != HiddenApiAccessFlags::kWhitelist) {
      DCHECK_EQ(hidden_api_flags, GetHiddenApiAccessFlags());
    }
  } else {
    SetAccessFlags(new_value);
  }
}

template<ReadBarrierOption kReadBarrierOption, typename RootVisitorType>
void ArtMethod::VisitRoots(RootVisitorType& visitor, PointerSize pointer_size) {
  if (LIKELY(!declaring_class_.IsNull())) {
    visitor.VisitRoot(declaring_class_.AddressWithoutBarrier());
    mirror::Class* klass = declaring_class_.Read<kReadBarrierOption>();
    if (UNLIKELY(klass->IsProxyClass())) {
      // For normal methods, dex cache shortcuts will be visited through the declaring class.
      // However, for proxies we need to keep the interface method alive, so we visit its roots.
      ArtMethod* interface_method = GetInterfaceMethodForProxyUnchecked(pointer_size);
      DCHECK(interface_method != nullptr);
      interface_method->VisitRoots(visitor, pointer_size);
    }
  }
}

template <typename Visitor>
inline void ArtMethod::UpdateObjectsForImageRelocation(const Visitor& visitor) {
  mirror::Class* old_class = GetDeclaringClassUnchecked<kWithoutReadBarrier>();
  mirror::Class* new_class = visitor(old_class);
  if (old_class != new_class) {
    SetDeclaringClass(new_class);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void ArtMethod::UpdateEntrypoints(const Visitor& visitor, PointerSize pointer_size) {
  if (IsNative<kReadBarrierOption>()) {
    const void* old_native_code = GetEntryPointFromJniPtrSize(pointer_size);
    const void* new_native_code = visitor(old_native_code);
    if (old_native_code != new_native_code) {
      SetEntryPointFromJniPtrSize(new_native_code, pointer_size);
    }
  } else {
    DCHECK(GetDataPtrSize(pointer_size) == nullptr);
  }
  const void* old_code = GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  const void* new_code = visitor(old_code);
  if (old_code != new_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(new_code, pointer_size);
  }
}

inline CodeItemInstructionAccessor ArtMethod::DexInstructions() {
  return CodeItemInstructionAccessor(*GetDexFile(), GetCodeItem());
}

inline CodeItemDataAccessor ArtMethod::DexInstructionData() {
  return CodeItemDataAccessor(*GetDexFile(), GetCodeItem());
}

inline CodeItemDebugInfoAccessor ArtMethod::DexInstructionDebugInfo() {
  return CodeItemDebugInfoAccessor(*GetDexFile(), GetCodeItem(), GetDexMethodIndex());
}

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_INL_H_
